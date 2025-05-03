#ifndef STORAGE_ENGINE_H
#define STORAGE_ENGINE_H

#include "json.hpp"

#include "Status.h"

#include "rocksdb/db.h"
#include "rocksdb/table.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/utilities/options_util.h"
#include "rocksdb/version.h"

#include <fstream>
#include <sys/stat.h>  // For stat() function
#ifdef _WIN32
#include <direct.h>  // For _mkdir on Windows
#else
#include <unistd.h>  // For Unix-specific functions
#include <sys/types.h>  // For mkdir on Unix
#endif
#include <iostream>
#include <set>
#include <mutex>
#include <thread>

using json = nlohmann::json;
namespace anudb {

	// StorageEngine class that wraps RocksDB
	class StorageEngine {
	public:
		StorageEngine(const std::string& dbPath) : dbPath_(dbPath), index_delimiter_("__index__"){}
		Status open();
		Status close();

		Status createCollection(const std::string& name);
		Status dropCollection(const std::string& name);
		Status put(const std::string& collection, const std::string& key, const std::vector<uint8_t>& value);
		Status putIndex(const std::string& collection, const std::string& key, const std::string& value);
		Status get(const std::string& collection, const std::string& key, std::vector<uint8_t>* value);
		Status getAll(const std::string& collection, std::vector<std::vector<uint8_t>>& value);
		Status remove(const std::string& collection, const std::string& key);
		bool collectionExists(const std::string& name) const;
		std::vector<std::string> getCollectionNames() const;
		std::set<std::string> getIndexNames(std::string);
		Status exportAllToJson(const std::string& collection, const std::string& exportPath);
		std::unordered_map<std::string, rocksdb::ColumnFamilyHandle*> getColumnFamilies() const;
		Status fetchDocIdsForEqual(const std::string& collection, const std::string& key, std::vector<std::string>& docIds) const;
		Status fetchDocIdsForGreater(const std::string& collection, const std::string& key, std::vector<std::string>& docIds) const;
		Status fetchDocIdsForLesser(const std::string& collection, const std::string& prefix, std::vector<std::string>& docIds) const;
		Status fetchDocIdsByOrder(const std::string& collection, const std::string& key, std::vector<std::string>& docIds) const;
		rocksdb::DB* getDB();
		virtual ~StorageEngine();
		// Decode a Big-Endian encoded integer from a RocksDB key
		static int64_t DecodeIntKey(const std::string& encoded) {
			if (encoded.size() != 8) return 0;

			uint64_t result = 0;
			for (int i = 0; i < 8; i++) {
				result = (result << 8) | static_cast<unsigned char>(encoded[i]);
			}

			// Remove the offset
			return result - INT64_MIN;
		}


	private:
		std::string dbPath_;
		rocksdb::DB* db_;
		std::unordered_map<std::string, rocksdb::ColumnFamilyHandle*> columnFamilies_;
		std::vector<std::unique_ptr<rocksdb::ColumnFamilyHandle, std::function<void(rocksdb::ColumnFamilyHandle*)>>> ownedHandles_;
		std::string index_delimiter_;
		//mutable std::mutex db_mutex_;
	};

	class RocksDBOptimizer {
	public:
		// Configuration structure with optimal defaults for v6.29.5
		struct EmbeddedConfig {
			// Memory-related
			size_t write_buffer_size = 4 << 20;     // 4MB
			size_t block_cache_size = 8 << 20;      // 8MB
			size_t max_open_files = 256;             // Unlimited for better performance in 6.29.5

			// Write-related
			size_t min_write_buffer_number = 2;
			size_t max_write_buffer_number = 3;
			size_t level0_file_num_compaction_trigger = 2;

			// Read-related
			size_t block_size = 16 * 1024;          // Optimal for 6.29.5
			int bloom_filter_bits_per_key = 8;
			bool cache_index_and_filter_blocks = true;

#ifdef ZSTD
			// Compression
			rocksdb::CompressionType compression = rocksdb::kZSTD;
			rocksdb::CompressionType bottommost_compression = rocksdb::kZSTD;
#else
			// Compression
			rocksdb::CompressionType compression = rocksdb::kNoCompression;
			rocksdb::CompressionType bottommost_compression = rocksdb::kNoCompression;
#endif

			// Background jobs
			int max_background_jobs = 2;
			int max_background_compactions = 1;

			// Additional
			bool enable_pipelined_write = true;    // New in 6.29.5
			bool enable_direct_io = true;         // Hardware dependent

			int prefix_length = 8;
		};

		static rocksdb::Options getOptimizedOptions(const EmbeddedConfig& config) {
			rocksdb::Options options;

			// Basic options
			options.create_if_missing = true;
			options.paranoid_checks = false;

			options.max_log_file_size = 10 * 1024 * 1024; // 10MB max log size
			options.keep_log_file_num = 1;   // Keep only 1 log file (overwrite old)

			// Write buffer configuration
			options.write_buffer_size = config.write_buffer_size;
			options.min_write_buffer_number_to_merge = config.min_write_buffer_number;
			options.max_write_buffer_number = config.max_write_buffer_number;
			options.level0_file_num_compaction_trigger = config.level0_file_num_compaction_trigger;

			// Table options
			rocksdb::BlockBasedTableOptions table_options;

			// Configure block cache - optimal for 6.29.5
			table_options.block_cache = rocksdb::NewLRUCache(
				config.block_cache_size,
				6,    // shard bits
				false, // strict capacity
				0.5    // high priority ratio
			);

			// Block size and bloom filter
			table_options.block_size = config.block_size;
			table_options.filter_policy.reset(
				rocksdb::NewBloomFilterPolicy(config.bloom_filter_bits_per_key)
			);

			// Cache configuration
			table_options.cache_index_and_filter_blocks = config.cache_index_and_filter_blocks;
			table_options.pin_l0_filter_and_index_blocks_in_cache = true;
			table_options.pin_top_level_index_and_filter = true;

			// Format version (specific to 6.29.5)
			table_options.format_version = 5;

			//Set prefix extractor - THE most important option for prefix searches
			options.memtable_factory.reset(rocksdb::NewHashSkipListRepFactory());
			options.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(config.prefix_length));

			// Partition filters (better memory usage)
			table_options.partition_filters = true;
			table_options.index_type = rocksdb::BlockBasedTableOptions::kTwoLevelIndexSearch;

			table_options.whole_key_filtering = false;  // Only filter by prefix

			//options.table_factory.reset(NewBlockBasedTableFactory(table_options));
			options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));
			// Compression settings
			options.compression_opts.level = 5;
			options.compression = config.compression;
			options.bottommost_compression = config.bottommost_compression;

			// Background job configuration
			options.max_background_jobs = config.max_background_jobs;
			options.max_background_compactions = config.max_background_compactions;

			// Level-specific options
			options.num_levels = 4;  // Optimal for embedded
			options.target_file_size_base = 16 * 1024 * 1024;  // 16MB
			options.target_file_size_multiplier = 2;

			// Write optimizations
			options.enable_pipelined_write = config.enable_pipelined_write;
			options.use_direct_io_for_flush_and_compaction = config.enable_direct_io;

			// Additional optimizations
			options.skip_stats_update_on_db_open = true;
			options.skip_checking_sst_file_sizes_on_db_open = true;
			options.optimize_filters_for_hits = true;

			// Disable Stats Updates on Writes
			options.report_bg_io_stats = false;

			return options;
		}

		// Optimized read options
		static rocksdb::ReadOptions getReadOptions() {
			rocksdb::ReadOptions read_options;
			read_options.prefix_same_as_start = true;
			read_options.total_order_seek = false;
			read_options.verify_checksums = false;  // Better performance
			read_options.fill_cache = true;
			return read_options;
		}

		// Optimized write options
		static rocksdb::WriteOptions getWriteOptions() {
			rocksdb::WriteOptions write_options;
			write_options.sync = false;  // Better performance, but be careful
			write_options.disableWAL = false;  // Keep WAL for safety
			return write_options;
		}

		// Memory usage estimation
		static size_t estimateMemoryUsage(const EmbeddedConfig& config) {
			return config.write_buffer_size * config.max_write_buffer_number +
				config.block_cache_size +
				(1 << 20);  // Additional overhead
		}
	};
}

#endif // STORAGE_ENGINE_H
