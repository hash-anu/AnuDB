// WalTracker.h
#ifndef WAL_TRACKER_H
#define WAL_TRACKER_H

#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>
#include <unordered_map>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <iostream>
#include <regex>
#include <chrono>
#ifdef _WIN32
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
extern "C" uintptr_t __cdecl _beginthreadex(void*, unsigned int,
    unsigned int(__stdcall*)(void*), void*, unsigned int, unsigned int*);
#endif

namespace anudb {
    // Callback type for WAL operations
    using WalOperationCallback = std::function<void(const std::string& operation,
        const std::string& cf_name,
        const std::string& key,
        const std::string& value)>;

    class WalTracker {
    public:
        // Constructor - takes a RocksDB instance and optional CF mapping
        WalTracker(rocksdb::DB* db,
            const std::unordered_map<uint32_t, std::string>& cf_map = {});

        // Destructor - stops tracking thread
        ~WalTracker();

        // Start tracking WAL operations
        void StartTracking();

        // Stop tracking WAL operations
        void StopTracking();

        // Register a callback for WAL operations
        void RegisterCallback(WalOperationCallback callback);

        // Check if tracking is active
        bool IsTracking() const;

        // Get current sequence number
        uint64_t GetCurrentSequence() const;

        // Update column family mapping
        void UpdateColumnFamilyMap(uint32_t id, const std::string& name);

        // Delete id from column family mapping
        void DeleteColumnFamilyMap(uint32_t id, const std::string& name);

    private:
        // Handler for WAL operations
        class WalLogHandler : public rocksdb::WriteBatch::Handler {
        public:
            WalLogHandler(std::unordered_map<uint32_t, std::string>& cf_map,
                WalOperationCallback callback);

            rocksdb::Status PutCF(uint32_t column_family_id,
                const rocksdb::Slice& key,
                const rocksdb::Slice& value) override;

            rocksdb::Status DeleteCF(uint32_t column_family_id,
                const rocksdb::Slice& key) override;

        private:
            std::unordered_map<uint32_t, std::string>& cf_id_to_name;
            WalOperationCallback callback;

            std::string GetCFName(uint32_t id) const;
        };

        // Thread function for monitoring WAL
        void TrackingThread();

        // Read WAL logs from a sequence number
        bool ReadWalLogs(uint64_t& last_sequence);

        // Member variables
        rocksdb::DB* db_;
        std::unordered_map<uint32_t, std::string> cf_id_to_name_;
        std::unique_ptr<std::thread> tracking_thread_;
        std::atomic<bool> should_stop_;
        std::atomic<bool> is_tracking_;
        WalOperationCallback callback_;
        uint64_t current_sequence_;
    };
};
#endif // WAL_TRACKER_H