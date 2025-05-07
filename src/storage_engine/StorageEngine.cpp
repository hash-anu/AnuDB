#include "StorageEngine.h"

using namespace anudb;

Status StorageEngine::open() {
	rocksdb::Options options;
	RocksDBOptimizer::EmbeddedConfig config;

	// Edge-device optimized configuration for efficient single operations
	config.max_open_files = 64;                      // Limit file handles to conserve resources

	// Write optimizations for single operations
	config.max_write_buffer_number = 2;              // Fewer write buffers to reduce memory
	config.min_write_buffer_number = 1;              // Flush sooner for consistent latency
	config.level0_file_num_compaction_trigger = 2;   // Balance between compaction frequency and performance

	// Read optimizations - crucial for AnuDB's query advantage
	config.block_size = 4 * 1024;                    // 4KB blocks - better for random access
	config.bloom_filter_bits_per_key = 10;           // Efficient bloom filters for faster point lookups
	config.cache_index_and_filter_blocks = true;     // Keep index and filters in cache for fast queries

	// Lightweight background processing
	config.max_background_jobs = 2;                  // Limited background jobs to reduce resource usage
	config.max_background_compactions = 1;           // Single compaction thread to minimize CPU usage

	// Disable features that consume extra resources
	config.enable_pipelined_write = true;            // Optimize write path
	config.enable_direct_io = false;                 // Better for most flash storage on edge devices
	config.prefix_length = 8;                        // Efficient prefix length for indexing

	options = RocksDBOptimizer::getOptimizedOptions(config);

	// Additional edge-specific optimizations
	options.allow_concurrent_memtable_write = false; // Reduce memory contention for single operations
	options.enable_write_thread_adaptive_yield = true; // Better CPU utilization during writes
	options.avoid_flush_during_shutdown = true;      // Faster shutdown

	// Optimize for faster point operations (get/put)
	options.level_compaction_dynamic_level_bytes = false; // Simpler level management
	options.max_bytes_for_level_base = 16 * 1024 * 1024;  // 16MB for base level - smaller for edge devices
	options.max_bytes_for_level_multiplier = 8;           // Moderate level scaling
	options.optimize_filters_for_hits = true;             // Optimize bloom filters
	options.report_bg_io_stats = false;                   // Eliminate overhead from statistics
	options.skip_stats_update_on_db_open = true;
	options.skip_checking_sst_file_sizes_on_db_open = true;

	// Improve read performance with focused caching
	options.use_adaptive_mutex = true;               // Reduce mutex contention
	options.new_table_reader_for_compaction_inputs = false; // Save memory
	options.OptimizeForSmallDb();

	// Get list of existing column families
	std::vector<std::string> columnFamilies;
	rocksdb::Status s = rocksdb::DB::ListColumnFamilies(options, dbPath_, &columnFamilies);

	/*if (!s.ok() && !s.IsNotFound()) {
		return Status::IOError(s.ToString());
	}*/

	// If no column families exist, just add the default one
	if (columnFamilies.empty()) {
		columnFamilies.push_back(rocksdb::kDefaultColumnFamilyName);
	}

	std::vector<rocksdb::ColumnFamilyDescriptor> columnFamilyDescriptors;
	for (const auto& cf : columnFamilies) {
		columnFamilyDescriptors.emplace_back(cf, rocksdb::ColumnFamilyOptions());
	}

	// Open the database with column families
	std::vector<rocksdb::ColumnFamilyHandle*> handles;
	rocksdb::DB* dbRaw;
	s = rocksdb::DB::Open(options, dbPath_, columnFamilyDescriptors, &handles, &dbRaw);

	if (!s.ok()) {
		return Status::IOError(s.ToString());
	}

	db_ = dbRaw;

	// Store the handles
	for (size_t i = 0; i < columnFamilies.size(); i++) {
		columnFamilies_[columnFamilies[i]] = handles[i];
		ownedHandles_.emplace_back(handles[i], [this](rocksdb::ColumnFamilyHandle* h) {
			if (db_) db_->DestroyColumnFamilyHandle(h);
			});
	}
	// Print estimated memory usage
	size_t estimated_mem = RocksDBOptimizer::estimateMemoryUsage(config);
	//std::cout << "Estimated memory usage by storage engine: " << (estimated_mem >> 20) << "MB\n";

	return Status::OK();
}

Status StorageEngine::close() {
	if (db_) {
		rocksdb::FlushOptions flush_options;
		for (auto it : columnFamilies_) {
			rocksdb::Status flush_status = db_->Flush(flush_options, it.second);
			if (!flush_status.ok()) {
				Status::IOError("Flush failed: " + flush_status.ToString());
			}
		}

		rocksdb::Status sync_status = db_->SyncWAL(); // Ensures WAL is persisted
		if (!sync_status.ok()) {
			Status::IOError("SyncWAL failed: " + sync_status.ToString());
		}

		// Clear the reference map first (this doesn't destroy handles)
		columnFamilies_.clear();

		// Clear the ownership vector which will destroy all handles properly
		ownedHandles_.clear();

		// Close and reset the DB
		if (db_) {
			rocksdb::Status s = db_->Close();
			if (!s.ok()) {
				return Status::IOError(s.ToString());
			}
			db_ = NULL;
			delete db_;
		}

		return Status::OK();
	}
	return Status::OK();
}

Status StorageEngine::createCollection(const std::string& name) {
	// Check if collection already exists
	if (columnFamilies_.find(name) != columnFamilies_.end()) {
		return Status::InvalidArgument("Collection already exists: " + name);
	}

	// Create column family for the collection
	rocksdb::ColumnFamilyHandle* handle;
	rocksdb::Status s = db_->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), name, &handle);

	if (!s.ok()) {
		return Status::IOError(s.ToString());
	}

	// Store the handle
	columnFamilies_[name] = handle;
	ownedHandles_.emplace_back(handle, [this](rocksdb::ColumnFamilyHandle* h) {
		if (db_) db_->DestroyColumnFamilyHandle(h);
		h = NULL;
		});
	return Status::OK();
}

Status StorageEngine::dropCollection(const std::string& name) {
	// Check if collection exists
	auto it = columnFamilies_.find(name);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + name);
	}
	// Get the handle
	auto handle = it->second;
	// Drop the column family
	rocksdb::Status s = db_->DropColumnFamily(handle);
	if (!s.ok()) {
		return Status::IOError(s.ToString());
	}
	// Remove from our map
	columnFamilies_.erase(it);
	return Status::OK();
}

std::unordered_map<std::string, rocksdb::ColumnFamilyHandle*> StorageEngine::getColumnFamilies() const {
	return columnFamilies_;
}

rocksdb::DB* StorageEngine::getDB() {
	return db_;
}

Status StorageEngine::put(const std::string& collection, const std::string& key, const std::vector<uint8_t>& value) {
	//std::lock_guard<std::mutex> lock(db_mutex_);

	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}

	rocksdb::Status s = db_->Put(RocksDBOptimizer::getWriteOptions(), it->second,
		rocksdb::Slice(key),
		rocksdb::Slice(reinterpret_cast<const char*>(value.data()), value.size()));

	if (!s.ok()) {
		return Status::IOError(s.ToString());
	}

	return Status::OK();
}

Status StorageEngine::putIndex(const std::string& collection, const std::string& key, const std::string& value) {
	//std::lock_guard<std::mutex> lock(db_mutex_);

	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}

	rocksdb::Status s = db_->Put(RocksDBOptimizer::getWriteOptions(), it->second,
		rocksdb::Slice(key),
		rocksdb::Slice(value.c_str(), value.size()));

	if (!s.ok()) {
		return Status::IOError(s.ToString());
	}

	return Status::OK();
}

Status StorageEngine::fetchDocIdsByOrder(const std::string& collection, const std::string& value, std::vector<std::string>& docIds) const {
	bool asc = (value == "asc" ? true : false);
	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}
	rocksdb::Iterator* iterator = db_->NewIterator(RocksDBOptimizer::getReadOptions(), it->second);
	if (asc) {
		for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
			docIds.push_back(iterator->value().ToString());
		}
	}
	else {
		for (iterator->SeekToLast(); iterator->Valid(); iterator->Prev()) {
			docIds.push_back(iterator->value().ToString());
		}
	}

	// delete iterator
	delete iterator;
	return Status::OK();
}

Status StorageEngine::fetchDocIdsForEqual(const std::string& collection, const std::string& prefix, std::vector<std::string>& docIds) const {
	//std::lock_guard<std::mutex> lock(db_mutex_);

	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}
	rocksdb::Iterator* iterator = db_->NewIterator(RocksDBOptimizer::getReadOptions(), it->second);
	for (iterator->Seek(prefix); iterator->Valid() && iterator->key().starts_with(prefix); iterator->Next()) {
		docIds.push_back(iterator->value().ToString());
	}
	// delete iterator
	delete iterator;
	return Status::OK();
}

Status StorageEngine::fetchDocIdsForGreater(const std::string& collection, const std::string& prefix, std::vector<std::string>& docIds) const {
	//std::lock_guard<std::mutex> lock(db_mutex_);

	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}
	rocksdb::Iterator* iterator = db_->NewIterator(RocksDBOptimizer::getReadOptions(), it->second);
	for (iterator->Seek(prefix); iterator->Valid() && iterator->key().starts_with(prefix); iterator->Next());
	for (; iterator->Valid(); iterator->Next()) {
		docIds.push_back(iterator->value().ToString());
	}

	// delete iterator
	delete iterator;
	return Status::OK();
}

Status StorageEngine::fetchDocIdsForLesser(const std::string& collection, const std::string& prefix, std::vector<std::string>& docIds) const {
	//std::lock_guard<std::mutex> lock(db_mutex_);

	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}
	rocksdb::Iterator* iterator = db_->NewIterator(RocksDBOptimizer::getReadOptions(), it->second);
	iterator->SeekForPrev(prefix);
	if (iterator->Valid() && iterator->key().starts_with(prefix)) {
		iterator->Prev();
	}
	for (; iterator->Valid(); iterator->Prev()) {
		std::string key = "";
		const char* p = iterator->key().data();
		while (*p != '#') {
			key.push_back(*p);
			p++;
		}
		docIds.push_back(iterator->value().ToString());
	}

	// delete iterator
	delete iterator;
	return Status::OK();
}

Status StorageEngine::get(const std::string& collection, const std::string& key, std::vector<uint8_t>* value) {
	//std::lock_guard<std::mutex> lock(db_mutex_);

	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}

	std::string result;
	rocksdb::Status s = db_->Get(RocksDBOptimizer::getReadOptions(), it->second,
		rocksdb::Slice(key), &result);

	if (s.IsNotFound()) {
		return Status::NotFound("Key not found: " + key);
	}
	else if (!s.ok()) {
		return Status::IOError(s.ToString());
	}

	value->assign(result.begin(), result.end());
	return Status::OK();
}

Status StorageEngine::getAll(const std::string& collection, std::vector<std::vector<uint8_t>>& values) {
	//std::lock_guard<std::mutex> lock(db_mutex_);

	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}
	values.clear();

	rocksdb::Iterator* iterator = db_->NewIterator(RocksDBOptimizer::getReadOptions(), it->second);

	for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
		rocksdb::Slice value_slice = iterator->value();

		std::vector<uint8_t> value_vec(value_slice.data(), value_slice.data() + value_slice.size());
		values.push_back(std::move(value_vec));
	}
	// delete iterator
	delete iterator;
	return Status::OK();
}

Status StorageEngine::remove(const std::string& collection, const std::string& key) {
	//std::lock_guard<std::mutex> lock(db_mutex_);

	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}

	rocksdb::Status s = db_->Delete(RocksDBOptimizer::getWriteOptions(), it->second, rocksdb::Slice(key));

	if (!s.ok() && !s.IsNotFound()) {
		return Status::IOError(s.ToString());
	}

	return Status::OK();
}

bool StorageEngine::collectionExists(const std::string& name) const {
	return columnFamilies_.find(name) != columnFamilies_.end();
}

std::vector<std::string> StorageEngine::getCollectionNames() const {
	std::vector<std::string> names;
	for (const auto& pair : columnFamilies_) {
		if (pair.first != rocksdb::kDefaultColumnFamilyName &&
			pair.first.find(index_delimiter_) == std::string::npos) {
			names.push_back(pair.first);
		}
	}
	return names;
}

std::set<std::string> StorageEngine::getIndexNames(std::string collectionName) {
	std::vector<std::string> names;
	std::map<std::string, std::set<std::string>> index;
	for (const auto& pair : columnFamilies_) {
		if (pair.first != rocksdb::kDefaultColumnFamilyName &&
			pair.first.find(index_delimiter_) != std::string::npos) {
			size_t pos = pair.first.find(index_delimiter_);
			std::string collectionName = pair.first.substr(0, pos);
			std::string indexName = pair.first.substr(pos + index_delimiter_.length());
			index[collectionName].insert(indexName);
		}
	}
	return index[collectionName];
}

Status StorageEngine::exportAllToJson(const std::string& collection, const std::string& exportPath) {
	// Create the directory if it doesn't exist
	std::string directory;
	struct stat info;
	size_t last_slash = exportPath.find_last_of("/\\");
	if (last_slash != std::string::npos) {
		directory = exportPath.substr(0, last_slash);
		// Check if directory exists and create it if it doesn't
		if (stat(directory.c_str(), &info) != 0) {
			// Directory doesn't exist, create it
#ifdef _WIN32
			int result = _mkdir(directory.c_str());
#else
			int result = mkdir(directory.c_str(), 0755);
#endif
			if (result != 0) {
				return Status::IOError("Failed to create directory: " + directory);
			}
		}
		else if (!(info.st_mode & S_IFDIR)) {
			// Path exists but is not a directory
			return Status::IOError("Path exists but is not a directory: " + directory);
		}
	}
	// Create a temporary dump file
	std::string temp_file = exportPath + "/" + collection + ".dump";
	std::string final_file = exportPath + "/" + collection + ".json";

	std::ofstream file(temp_file.c_str());
	if (!file.is_open()) {
		return Status::IOError("Failed to open file for writing: " + temp_file);
	}
	// Start JSON array
	file << "[\n";
	bool first_entry = true;
	auto it = columnFamilies_.find(collection);
	if (it == columnFamilies_.end()) {
		return Status::NotFound("Collection not found: " + collection);
	}
	rocksdb::Iterator* iterator = db_->NewIterator(RocksDBOptimizer::getReadOptions(), it->second);

	for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
		//std::unique_lock<std::mutex> lock(db_mutex_);
		rocksdb::Slice value_slice = iterator->value();

		// Write comma if not the first entry
		if (!first_entry) {
			file << ",\n";
		}
		else {
			first_entry = false;
		}

		std::vector<uint8_t> value_vec(value_slice.data(), value_slice.data() + value_slice.size());
		file << json::from_msgpack(value_vec)["data"].dump(4);
		//lock.unlock();
		// Add a microsecond sleep to prevent overwhelming system resources
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
	file << "\n]";
	file.close();

	// Check if iterator ended with an error
	rocksdb::Status status = iterator->status();
	delete iterator;

	if (!status.ok()) {
		// Remove the temporary file if there was an error
		std::remove(temp_file.c_str());
		return Status::InternalError("Unable to export json file");
	}
	else {
		std::remove(final_file.c_str());
	}

	// Rename the dump file to the final JSON file
	if (std::rename(temp_file.c_str(), final_file.c_str()) != 0) {
		return Status::IOError("Failed to rename dump file to final JSON file");
	}
	return Status::OK();
}

StorageEngine::~StorageEngine() {
}
