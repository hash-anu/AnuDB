// WalTracker.cpp
#include "WalTracker.h"

using namespace anudb;
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
    if (name.find("__index__") != std::string::npos) {
        return;
    }
    callback_("CREATE_CF_MANUAL", name, std::to_string(id), "");
    cf_id_to_name_[id] = name;
}

void WalTracker::DeleteColumnFamilyMap(uint32_t id, const std::string& name) {
    if (name.find("__index__") != std::string::npos) {
        return;
    }
    callback_("DELETE_CF_MANUAL", name, std::to_string(id), "");
    cf_id_to_name_.erase(id);
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
    if (cf_name == "unknown") {
        return rocksdb::Status::OK();
    }

    if (callback) {
        std::vector<uint8_t> value_vec(value.data(), value.data() + value.size());
        callback("PUT", cf_name, key.ToString(), json::from_msgpack(value_vec)["data"].dump());
    }

    return rocksdb::Status::OK();
}

rocksdb::Status WalTracker::WalLogHandler::DeleteCF(
    uint32_t column_family_id,
    const rocksdb::Slice& key) {
    std::string cf_name = GetCFName(column_family_id);
    if (cf_name == "unknown") {
        return rocksdb::Status::OK();
    }

    if (callback) {
        callback("DELETE", cf_name, key.ToString(), "");
    }

    return rocksdb::Status::OK();
}

std::string WalTracker::WalLogHandler::GetCFName(uint32_t id) const {
    auto it = cf_id_to_name.find(id);
    return it != cf_id_to_name.end() ? it->second : "unknown";
}
