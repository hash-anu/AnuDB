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
#ifdef _WIN32
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
extern "C" uintptr_t __cdecl _beginthreadex(void*, unsigned int,
    unsigned int(__stdcall*)(void*), void*, unsigned int, unsigned int*);
#endif
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

    // Update column family mapping
    void UpdateColumnFamilyMap();

    // Member variables
    rocksdb::DB* db_;
    std::unordered_map<uint32_t, std::string> cf_id_to_name_;
    std::unique_ptr<std::thread> tracking_thread_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> is_tracking_;
    WalOperationCallback callback_;
    uint64_t current_sequence_;
};

#endif // WAL_TRACKER_H

// WalTracker.cpp
//#include "WalTracker.h"
#include <iostream>
#include <regex>
#include <chrono>

// Constructor
WalTracker::WalTracker(rocksdb::DB* db,
    const std::unordered_map<uint32_t, std::string>& cf_map)
    : db_(db),
    cf_id_to_name_(cf_map),
    should_stop_(false),
    is_tracking_(false),
    current_sequence_(0) {
    // Initialize column family mapping if empty
    if (cf_id_to_name_.empty()) {
        // Default column family always has ID 0
        cf_id_to_name_[0] = "default";

        // Try to get more column families from DB path
        std::vector<std::string> cf_names;
        rocksdb::Status s = rocksdb::DB::ListColumnFamilies(rocksdb::DBOptions(),
            db_->GetName(),
            &cf_names);
    }
}

void WalTracker::UpdateColumnFamilyMap(uint32_t id, const std::string& name) {
    callback_("CREATE_CF_MANUAL", name, std::to_string(id), "");
    cf_id_to_name_[id] = name;
}

// Destructor
WalTracker::~WalTracker() {
    StopTracking();
}

// Start tracking WAL operations
void WalTracker::StartTracking() {
    if (is_tracking_) {
        return;  // Already tracking
    }

    should_stop_ = false;
    is_tracking_ = true;

    // Create and start tracking thread
    tracking_thread_ = std::make_unique<std::thread>(&WalTracker::TrackingThread, this);
}

// Stop tracking WAL operations
void WalTracker::StopTracking() {
    if (!is_tracking_) {
        return;  // Not tracking
    }

    // Signal thread to stop and wait for it
    should_stop_ = true;
    if (tracking_thread_ && tracking_thread_->joinable()) {
        tracking_thread_->join();
    }

    is_tracking_ = false;
}

// Register a callback for WAL operations
void WalTracker::RegisterCallback(WalOperationCallback callback) {
    callback_ = callback;
}

// Check if tracking is active
bool WalTracker::IsTracking() const {
    return is_tracking_;
}

// Get current sequence number
uint64_t WalTracker::GetCurrentSequence() const {
    return current_sequence_;
}

// Thread function for monitoring WAL
void WalTracker::TrackingThread() {
    // Start from the current sequence number of the database
    uint64_t last_sequence = 0;

    // First read historical WAL entries
    ReadWalLogs(last_sequence);

    // Then monitor for new entries
    while (!should_stop_) {
        uint64_t current_seq = db_->GetLatestSequenceNumber();
        current_sequence_ = current_seq;  // Update current sequence

        if (last_sequence < current_seq) {
            ReadWalLogs(last_sequence);
        }
        else {
            // No new sequences, wait a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// Read WAL logs from a sequence number
bool WalTracker::ReadWalLogs(uint64_t& last_sequence) {
    std::unique_ptr<rocksdb::TransactionLogIterator> iter;
    rocksdb::Status status = db_->GetUpdatesSince(last_sequence, &iter);

    if (!status.ok()) {
        return false;
    }

    bool has_new_entries = false;

    while (iter->Valid()) {
        const auto& result = iter->GetBatch();
        last_sequence = result.sequence + 1;

        // Process WAL entries if callback is registered
        if (callback_) {
            WalLogHandler handler(cf_id_to_name_, callback_);
            result.writeBatchPtr->Iterate(&handler);
        }

        iter->Next();
        has_new_entries = true;
    }

    return has_new_entries;
}

// Update column family mapping
void WalTracker::UpdateColumnFamilyMap() {
    // For older RocksDB versions that don't have GetColumnFamilyHandles()
    // We need to maintain our own list of column families

    // Default column family always has ID 0
    cf_id_to_name_[0] = "default";

    // We rely on WAL LogData entries to update our mapping
    // when new column families are created
}

// WalLogHandler implementation
WalTracker::WalLogHandler::WalLogHandler(
    std::unordered_map<uint32_t, std::string>& cf_map,
    WalOperationCallback callback)
    : cf_id_to_name(cf_map), callback(callback) {}

rocksdb::Status WalTracker::WalLogHandler::PutCF(
    uint32_t column_family_id,
    const rocksdb::Slice& key,
    const rocksdb::Slice& value) {
    std::string cf_name = GetCFName(column_family_id);

    if (callback) {
        callback("PUT", cf_name, key.ToString(), value.ToString());
    }

    return rocksdb::Status::OK();
}

rocksdb::Status WalTracker::WalLogHandler::DeleteCF(
    uint32_t column_family_id,
    const rocksdb::Slice& key) {
    std::string cf_name = GetCFName(column_family_id);

    if (callback) {
        callback("DELETE", cf_name, key.ToString(), "");
    }

    return rocksdb::Status::OK();
}

std::string WalTracker::WalLogHandler::GetCFName(uint32_t id) const {
    auto it = cf_id_to_name.find(id);
    return it != cf_id_to_name.end() ? it->second : "unknown";
}

// DatabaseOperations.h
#ifndef DATABASE_OPERATIONS_H
#define DATABASE_OPERATIONS_H

#include <rocksdb/db.h>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <random>
#include <functional>

class DatabaseOperations {
public:
    // Constructor
    DatabaseOperations(rocksdb::DB* db,
        const std::vector<rocksdb::ColumnFamilyHandle*>& cf_handles,WalTracker* wal_tracker);

    // Destructor
    ~DatabaseOperations();

    // Start random operations
    void StartRandomOperations(int operations_per_second = 10);

    // Stop random operations
    void StopRandomOperations();

    // Create a new column family
    rocksdb::ColumnFamilyHandle* CreateNewColumnFamily(const std::string& name);

    // Register callback for operation tracking (for logging purposes)
    using OperationCallback = std::function<void(const std::string& operation,
        const std::string& cf_name,
        const std::string& key,
        const std::string& value)>;
    void RegisterCallback(OperationCallback callback);

private:
    // Thread function for random operations
    void OperationsThread();

    // Generate random string
    std::string GenerateRandomString(int length);

    // Perform random put operation
    void RandomPut();

    // Perform random delete operation
    void RandomDelete();

    // Perform random get operation
    void RandomGet();

    // Perform random scan operation
    void RandomScan();

    // Member variables
    rocksdb::DB* db_;
    std::vector<rocksdb::ColumnFamilyHandle*> cf_handles_;
    std::vector<std::string> generated_keys_;
    std::unique_ptr<std::thread> operations_thread_;
    std::atomic<bool> should_stop_;
    std::atomic<bool> is_running_;
    int ops_per_second_;
    std::mt19937 random_generator_;
    OperationCallback callback_;
    WalTracker* waltracker_;
};

#endif // DATABASE_OPERATIONS_H

// DatabaseOperations.cpp
//#include "DatabaseOperations.h"
#include <iostream>
#include <chrono>
#include <algorithm>

// Constructor
DatabaseOperations::DatabaseOperations(rocksdb::DB* db,
    const std::vector<rocksdb::ColumnFamilyHandle*>& cf_handles,
    WalTracker* wal_tracker)
    : db_(db),
    cf_handles_(cf_handles),
    should_stop_(false),
    is_running_(false),
    ops_per_second_(10),
    waltracker_(wal_tracker){
    // Initialize random generator
    std::random_device rd;
    random_generator_.seed(rd());
}

// Destructor
DatabaseOperations::~DatabaseOperations() {
    StopRandomOperations();
}

// Start random operations
void DatabaseOperations::StartRandomOperations(int operations_per_second) {
    if (is_running_) {
        return;  // Already running
    }

    ops_per_second_ = operations_per_second;
    should_stop_ = false;
    is_running_ = true;

    // Create and start operations thread
    operations_thread_ = std::make_unique<std::thread>(&DatabaseOperations::OperationsThread, this);
}

// Stop random operations
void DatabaseOperations::StopRandomOperations() {
    if (!is_running_) {
        return;  // Not running
    }

    // Signal thread to stop and wait for it
    should_stop_ = true;
    if (operations_thread_ && operations_thread_->joinable()) {
        operations_thread_->join();
    }

    is_running_ = false;
}

// Create a new column family
rocksdb::ColumnFamilyHandle* DatabaseOperations::CreateNewColumnFamily(const std::string& name) {
    rocksdb::ColumnFamilyHandle* cf_handle = nullptr;
    rocksdb::Status status = db_->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), name, &cf_handle);

    if (status.ok() && cf_handle != nullptr) {
        cf_handles_.push_back(cf_handle);

        if (waltracker_ != NULL) {
            waltracker_->UpdateColumnFamilyMap(cf_handle->GetID(), cf_handle->GetName());
        }

        if (callback_) {
            callback_("CREATE_CF_MANUAL", name, std::to_string(cf_handle->GetID()), "");
        }
    }
    else {
        std::cerr << "Error creating column family: " << status.ToString() << std::endl;
    }

    return cf_handle;
}

// Register callback
void DatabaseOperations::RegisterCallback(OperationCallback callback) {
    callback_ = callback;
}

// Thread function for random operations
void DatabaseOperations::OperationsThread() {
    // Calculate sleep time between operations
    int sleep_ms = 1000 / ops_per_second_;

    while (!should_stop_) {
        // Select a random operation type
        std::uniform_int_distribution<> op_dist(0, 1);
        int operation = op_dist(random_generator_);

        switch (operation) {
        case 0:
            RandomPut();
            break;
        case 1:
            RandomDelete();
            break;
        case 2:
            //RandomGet();
            break;
        case 3:
            //RandomScan();
            break;
        }

        // Sleep between operations
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
}

// Generate random string
std::string DatabaseOperations::GenerateRandomString(int length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

    std::string result;
    result.reserve(length);
    for (int i = 0; i < length; ++i) {
        result += alphanum[dis(random_generator_)];
    }
    return result;
}

// Perform random put operation
void DatabaseOperations::RandomPut() {
    if (cf_handles_.empty()) {
        return;
    }

    // Select a random column family
    std::uniform_int_distribution<> cf_dist(0, cf_handles_.size() - 1);
    int cf_index = cf_dist(random_generator_);
    rocksdb::ColumnFamilyHandle* cf = cf_handles_[cf_index];

    // Create key and value
    std::string key = "key_" + GenerateRandomString(8);
    std::string value = "value_" + GenerateRandomString(16);

    // Store key for potential future delete/get operations
    generated_keys_.push_back(key);
    if (generated_keys_.size() > 1000) {
        generated_keys_.erase(generated_keys_.begin());
    }

    // Write to database
    rocksdb::WriteOptions write_options;
    write_options.disableWAL = false;  // Ensure WAL is enabled
    rocksdb::Status status = db_->Put(write_options, cf, key, value);

    if (status.ok() && callback_) {
        callback_("PUT_MANUAL", cf->GetName(), key, value);
    }
}

// Perform random delete operation
void DatabaseOperations::RandomDelete() {
    if (cf_handles_.empty() || generated_keys_.empty()) {
        return;
    }

    // Select a random column family
    std::uniform_int_distribution<> cf_dist(0, cf_handles_.size() - 1);
    int cf_index = cf_dist(random_generator_);
    rocksdb::ColumnFamilyHandle* cf = cf_handles_[cf_index];

    // Select a random key to delete
    std::uniform_int_distribution<> key_dist(0, generated_keys_.size() - 1);
    int key_index = key_dist(random_generator_);
    std::string key = generated_keys_[key_index];

    // Remove key from generated keys
    generated_keys_.erase(generated_keys_.begin() + key_index);

    // Delete from database
    rocksdb::WriteOptions write_options;
    rocksdb::Status status = db_->Delete(write_options, cf, key);

    if (status.ok() && callback_) {
        callback_("DELETE_MANUAL", cf->GetName(), key, "");
    }
}

// Perform random get operation
void DatabaseOperations::RandomGet() {
    if (cf_handles_.empty() || generated_keys_.empty()) {
        return;
    }

    // Select a random column family
    std::uniform_int_distribution<> cf_dist(0, cf_handles_.size() - 1);
    int cf_index = cf_dist(random_generator_);
    rocksdb::ColumnFamilyHandle* cf = cf_handles_[cf_index];

    // Select a random key to get
    std::uniform_int_distribution<> key_dist(0, generated_keys_.size() - 1);
    int key_index = key_dist(random_generator_);
    std::string key = generated_keys_[key_index];

    // Get from database
    std::string value;
    rocksdb::ReadOptions read_options;
    rocksdb::Status status = db_->Get(read_options, cf, key, &value);

    if (status.ok() && callback_) {
        callback_("GET", cf->GetName(), key, value);
    }
}

// Perform random scan operation
void DatabaseOperations::RandomScan() {
    if (cf_handles_.empty()) {
        return;
    }

    // Select a random column family
    std::uniform_int_distribution<> cf_dist(0, cf_handles_.size() - 1);
    int cf_index = cf_dist(random_generator_);
    rocksdb::ColumnFamilyHandle* cf = cf_handles_[cf_index];

    // Create iterator and scan some items
    rocksdb::ReadOptions read_options;
    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(read_options, cf));

    // Start from a random position or beginning
    std::uniform_int_distribution<> start_dist(0, 1);
    bool random_start = start_dist(random_generator_) == 1;

    if (random_start && !generated_keys_.empty()) {
        std::uniform_int_distribution<> key_dist(0, generated_keys_.size() - 1);
        int key_index = key_dist(random_generator_);
        std::string start_key = generated_keys_[key_index];
        it->Seek(start_key);
    }
    else {
        it->SeekToFirst();
    }

    // Scan a few items
    std::uniform_int_distribution<> count_dist(1, 10);
    int items_to_scan = count_dist(random_generator_);
    int items_scanned = 0;

    if (callback_) {
        callback_("SCAN_START", cf->GetName(), "", "");
    }

    for (; it->Valid() && items_scanned < items_to_scan; it->Next()) {
        std::string key = it->key().ToString();
        std::string value = it->value().ToString();

        if (callback_) {
            callback_("SCAN_ITEM", cf->GetName(), key, value);
        }

        items_scanned++;
    }

    if (callback_) {
        callback_("SCAN_END", cf->GetName(), "", "");
    }
}

// main.cpp - Usage example
//#include "WalTracker.h"
//#include "DatabaseOperations.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <mutex>
#include <sstream>

// Mutex for thread-safe console output
std::mutex console_mutex;

// Function to get current timestamp as string
std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// WAL operation handler
void WalOperationHandler(const std::string& operation,
    const std::string& cf_name,
    const std::string& key,
    const std::string& value) {
    std::lock_guard<std::mutex> lock(console_mutex);

    std::cout << "[" << GetTimestamp() << "] [WAL] ";
    std::cout << std::left << std::setw(10) << operation;
    std::cout << " | CF: " << std::setw(15) << cf_name;
    std::cout << " | Key: " << std::setw(20) << key;

    if (!value.empty()) {
        // Truncate value if it's too long
        std::string display_value = value;
        if (display_value.length() > 30) {
            display_value = display_value.substr(0, 27) + "...";
        }
        std::cout << " | Value: " << display_value;
    }

    std::cout << std::endl;
}

// DB operation handler
void DbOperationHandler(const std::string& operation,
    const std::string& cf_name,
    const std::string& key,
    const std::string& value) {
    std::lock_guard<std::mutex> lock(console_mutex);
#if 0
    std::cout << "[" << GetTimestamp() << "] [DB] ";
    std::cout << std::left << std::setw(12) << operation;
    std::cout << " | CF: " << std::setw(15) << cf_name;
    std::cout << " | Key: " << std::setw(20) << key;
#endif
    if (!value.empty() && operation != "SCAN_START" && operation != "SCAN_END") {
        // Truncate value if it's too long
        std::string display_value = value;
        if (display_value.length() > 30) {
            display_value = display_value.substr(0, 27) + "...";
        }
        std::cout << " | Value: " << display_value;
    }

    std::cout << std::endl;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <rocksdb_path>" << std::endl;
        return 1;
    }

    std::string db_path = argv[1];

    // Set up RocksDB options
    rocksdb::Options options;
    options.create_if_missing = true;
    options.create_missing_column_families = true;

    // Define initial column families
    std::vector<std::string> cf_names = { "default", "users", "products", "orders" };
    std::vector<rocksdb::ColumnFamilyDescriptor> cf_descriptors;

    for (const auto& name : cf_names) {
        rocksdb::ColumnFamilyOptions cf_options;
        cf_descriptors.emplace_back(name, cf_options);
    }

    // Open database
    std::vector<rocksdb::ColumnFamilyHandle*> cf_handles;
    rocksdb::DB* db_raw = nullptr;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, cf_descriptors, &cf_handles, &db_raw);

    if (!status.ok()) {
        std::cerr << "Error opening database: " << status.ToString() << std::endl;
        return 1;
    }

    // Use smart pointer for automatic cleanup
    std::unique_ptr<rocksdb::DB> db(db_raw);

    // Create column family ID to name mapping for WAL tracker
    std::unordered_map<uint32_t, std::string> cf_id_to_name;
    for (auto* handle : cf_handles) {
        cf_id_to_name[handle->GetID()] = handle->GetName();
        std::cout << "Column Family: " << handle->GetName() << " (ID: " << handle->GetID() << ")" << std::endl;
    }

    // Create WAL tracker
    WalTracker wal_tracker(db.get(), cf_id_to_name);
    wal_tracker.RegisterCallback(WalOperationHandler);

    // Create database operations manager
    DatabaseOperations db_ops(db.get(), cf_handles, &wal_tracker);
    db_ops.RegisterCallback(DbOperationHandler);

    // Start WAL tracking
    std::cout << "\nStarting WAL tracking..." << std::endl;
    wal_tracker.StartTracking();

    // Start random database operations
    std::cout << "Starting random database operations (5 ops/sec)..." << std::endl;
    db_ops.StartRandomOperations(5);  // 5 operations per second

    // Create a new column family after 5 seconds
    std::cout << "\nWill create a new column family after 5 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    rocksdb::ColumnFamilyHandle* analytics_cf = db_ops.CreateNewColumnFamily("analytics");
    if (analytics_cf != nullptr) {
        std::cout << "Created new column family: analytics (ID: " << analytics_cf->GetID() << ")" << std::endl;
    }

    // Wait for user input to stop
    std::cout << "\nPress Enter to stop..." << std::endl;
    std::cin.get();

    // Stop operations
    db_ops.StopRandomOperations();
    wal_tracker.StopTracking();

    std::cout << "Random operations and WAL tracking stopped." << std::endl;

    // Clean up column family handles
    for (auto* handle : cf_handles) {
        db->DestroyColumnFamilyHandle(handle);
    }

    return 0;
}