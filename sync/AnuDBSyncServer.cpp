#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/transaction_log.h>
#include <rocksdb/write_batch.h>
#include <rocksdb/listener.h>

// Custom RocksDB event listener to detect log updates
class LogUpdateListener : public rocksdb::EventListener {
private:
    std::function<void()> updateCallback;
    
public:
    LogUpdateListener(std::function<void()> callback) : updateCallback(callback) {}
    
    void OnFlushBegin(rocksdb::DB* db, const rocksdb::FlushJobInfo& info) override {
        updateCallback();
    }
    
    void OnFlushCompleted(rocksdb::DB* db, const rocksdb::FlushJobInfo& info) override {
        updateCallback();
    }
    
    void OnBackgroundError(rocksdb::BackgroundErrorReason reason, rocksdb::Status* status) override {
        std::cerr << "RocksDB background error: " << status->ToString() << std::endl;
    }
};

class AnuDBSQLConverter {
private:
    std::string dbPath;
    uint64_t lastSequence;
    std::ofstream logFile;
    std::atomic<bool> running;
    std::mutex mtx;
    std::condition_variable cv;
    bool hasNewLogs;
    bool processAll;
    
    // Convert RocksDB operation to SQL query
    std::string operationToSQL(const std::string& opType, const std::string& key, 
                              const std::string& value, uint32_t cfId,
                              const std::map<uint32_t, std::string>& cfNameMap) {
        // Get column family name
        std::string cfName = "default";
        auto it = cfNameMap.find(cfId);
        if (it != cfNameMap.end()) {
            cfName = it->second;
        }
        
        // Document ID from key
        std::string docId = key;
        
        // Escape single quotes in values for SQL
        std::string escapedValue = value;
        size_t pos = 0;
        while ((pos = escapedValue.find("'", pos)) != std::string::npos) {
            escapedValue.replace(pos, 1, "''");
            pos += 2;
        }
        
        if (opType == "PUT") {
            return "INSERT INTO " + cfName + " (id, data) VALUES ('" + 
                   docId + "', '" + escapedValue + "') ON DUPLICATE KEY UPDATE data=VALUES(data);";
        } 
        else if (opType == "DELETE") {
            return "DELETE FROM " + cfName + " WHERE id = '" + docId + "';";
        }
        else if (opType == "CREATE_CF") {
            return "CREATE TABLE " + key + " (id VARCHAR(255) PRIMARY KEY, data TEXT);";
        }
        else if (opType == "DROP_CF") {
            return "DROP TABLE " + key + ";";
        }
        
        return "-- Unknown operation: " + opType;
    }
    
    // Load last sequence from state file
    void loadState() {
        if (processAll) {
            // Force starting from sequence 0 if processing all logs
            lastSequence = 0;
            std::cout << "Processing all logs from sequence 0" << std::endl;
            return;
        }
        
        std::ifstream stateFile(dbPath + "/sql_converter_state");
        if (stateFile) {
            stateFile >> lastSequence;
            stateFile.close();
            std::cout << "Loaded last processed sequence: " << lastSequence << std::endl;
        } else {
            lastSequence = 0;
            std::cout << "No previous state found. Starting from sequence 0" << std::endl;
        }
    }
    
    // Save last sequence to state file
    void saveState() {
        std::ofstream stateFile(dbPath + "/sql_converter_state");
        if (stateFile) {
            stateFile << lastSequence;
            stateFile.close();
        } else {
            std::cerr << "Failed to save state" << std::endl;
        }
    }

public:
    AnuDBSQLConverter(const std::string& path, const std::string& outputFile = "", bool processAllLogs = false)
        : dbPath(path), lastSequence(0), running(false), hasNewLogs(false), processAll(processAllLogs) {
        
        // Open log file if specified
        if (!outputFile.empty()) {
            logFile.open(outputFile, std::ios::app);
            if (!logFile) {
                std::cerr << "Failed to open log file: " << outputFile << std::endl;
            }
        }
        
        // Load previous state or start from 0 if processing all logs
        loadState();
    }

    ~AnuDBSQLConverter() {
        // Stop any running threads
        stop();
        
        // Close log file
        if (logFile.is_open()) {
            logFile.close();
        }
        
        // Save state
        saveState();
    }
    
    // Notify that there are new logs to process
    void notifyNewLogs() {
        std::lock_guard<std::mutex> lock(mtx);
        hasNewLogs = true;
        cv.notify_one();
    }

    // Process logs and convert to SQL
    void processLogs() {
        rocksdb::Options options;
        options.create_if_missing = false;
        
        // Add listener for log updates
        options.listeners.push_back(std::make_shared<LogUpdateListener>(
            [this]() { this->notifyNewLogs(); }
        ));
        
        // Get list of column families
        std::vector<std::string> cfNames;
        rocksdb::Status s = rocksdb::DB::ListColumnFamilies(options, dbPath, &cfNames);
        if (!s.ok()) {
            std::cerr << "Failed to list column families: " << s.ToString() << std::endl;
            return;
        }
        
        // Open DB with all column families
        std::vector<rocksdb::ColumnFamilyDescriptor> cfDescriptors;
        for (const auto& name : cfNames) {
            cfDescriptors.push_back(rocksdb::ColumnFamilyDescriptor(name, rocksdb::ColumnFamilyOptions()));
        }
        
        std::vector<rocksdb::ColumnFamilyHandle*> cfHandles;
        rocksdb::DB* dbRaw;
        
        // Open in read-only mode, but with listener for updates
        s = rocksdb::DB::OpenForReadOnly(options, dbPath, cfDescriptors, &cfHandles, &dbRaw);
        if (!s.ok()) {
            std::cerr << "Failed to open RocksDB: " << s.ToString() << std::endl;
            return;
        }
        
        std::shared_ptr<rocksdb::DB> db(dbRaw);
        
        // Build column family ID to name map
        std::map<uint32_t, std::string> cfNameMap;
        for (size_t i = 0; i < cfHandles.size(); i++) {
            cfNameMap[cfHandles[i]->GetID()] = cfNames[i];
            delete cfHandles[i];
        }

        // Process existing logs first (force notification)
        notifyNewLogs();
        
        std::cout << "RocksDB SQL converter started. Monitoring for log updates..." << std::endl;
        
        while (running) {
            // Wait for notification of new logs or timeout (to periodically check anyway)
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait_for(lock, std::chrono::seconds(5), [this]() { return hasNewLogs || !running; });
                
                if (!running) break;
                hasNewLogs = false;
            }
            
            // Get transaction logs since last processed sequence
            std::unique_ptr<rocksdb::TransactionLogIterator> iter;
            rocksdb::Status status = db->GetUpdatesSince(lastSequence, &iter);
            
            if (!status.ok()) {
                if (status.IsNotFound()) {
                    // WAL files might have been deleted, start from current
                    std::cout << "WAL files not found, resetting sequence number" << std::endl;
                    uint64_t currentSeq = db->GetLatestSequenceNumber();
                    lastSequence = currentSeq;
                    saveState();
                    continue;
                }
                
                std::cerr << "Failed to get transaction logs: " << status.ToString() << std::endl;
                continue;
            }

            int queryCount = 0;
            bool foundLogs = false;
            
            while (iter->Valid()) {
                foundLogs = true;
                rocksdb::BatchResult batch = iter->GetBatch();
                lastSequence = batch.sequence;
                
                class OperationExtractor : public rocksdb::WriteBatch::Handler {
                private:
                    AnuDBSQLConverter* converter;
                    const std::map<uint32_t, std::string>& cfMap;
                    std::vector<std::string> sqlQueries;
                    
                public:
                    OperationExtractor(AnuDBSQLConverter* conv, const std::map<uint32_t, std::string>& cfNameMap) 
                        : converter(conv), cfMap(cfNameMap) {}
                    
                    const std::vector<std::string>& getSqlQueries() const {
                        return sqlQueries;
                    }
                    
                    rocksdb::Status PutCF(uint32_t column_family_id, const rocksdb::Slice& key, 
                                         const rocksdb::Slice& value) override {
                        std::string sql = converter->operationToSQL("PUT", key.ToString(), value.ToString(), 
                                                                   column_family_id, cfMap);
                        sqlQueries.push_back(sql);
                        return rocksdb::Status::OK();
                    }
                    
                    rocksdb::Status DeleteCF(uint32_t column_family_id, const rocksdb::Slice& key) override {
                        std::string sql = converter->operationToSQL("DELETE", key.ToString(), "", 
                                                                   column_family_id, cfMap);
                        sqlQueries.push_back(sql);
                        return rocksdb::Status::OK();
                    }
                    
                    void LogData(const rocksdb::Slice& blob) override {
                        std::string data = blob.ToString();
                        
                        size_t createPos = data.find("CREATE_CF:");
                        if (createPos != std::string::npos) {
                            std::string cfName = data.substr(createPos + 10);
                            sqlQueries.push_back(converter->operationToSQL("CREATE_CF", cfName, "", 0, cfMap));
                            return;
                        }
                        
                        size_t dropPos = data.find("DROP_CF:");
                        if (dropPos != std::string::npos) {
                            std::string cfName = data.substr(dropPos + 8);
                            sqlQueries.push_back(converter->operationToSQL("DROP_CF", cfName, "", 0, cfMap));
                            return;
                        }
                    }
                    
                    // Other required handler methods
                    rocksdb::Status DeleteRangeCF(uint32_t column_family_id, const rocksdb::Slice& begin_key, 
                                                 const rocksdb::Slice& end_key) override {
                        return rocksdb::Status::OK();
                    }
                    
                    rocksdb::Status MergeCF(uint32_t column_family_id, const rocksdb::Slice& key, 
                                           const rocksdb::Slice& value) override {
                        return rocksdb::Status::OK();
                    }
                    
                    rocksdb::Status PutBlobIndexCF(uint32_t column_family_id, const rocksdb::Slice& key, 
                                                  const rocksdb::Slice& value) override {
                        return rocksdb::Status::OK();
                    }
                };
                
                // Process the write batch
                OperationExtractor extractor(this, cfNameMap);
                status = batch.writeBatchPtr->Iterate(&extractor);
                
                if (!status.ok()) {
                    std::cerr << "Error iterating batch: " << status.ToString() << std::endl;
                    break;
                }
                
                // Output SQL queries
                for (const auto& sql : extractor.getSqlQueries()) {
                    queryCount++;
                    
                    // Use timestamp for each SQL entry
                    auto now = std::chrono::system_clock::now();
                    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
                    char timestamp[100];
                    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
                    
                    std::string output = std::string(timestamp) + " | " + sql;
                    
                    // Log to file if open
                    if (logFile.is_open()) {
                        logFile << output << std::endl;
                        logFile.flush(); // Ensure it's written immediately
                    } else {
                        // Otherwise print to stdout
                        std::cout << output << std::endl;
                    }
                }
                
                iter->Next();
            }
            
            if (queryCount > 0) {
                std::cout << "Processed " << queryCount << " SQL queries. Latest sequence: " << lastSequence << std::endl;
                saveState(); // Save state after processing batch
            } else if (!foundLogs && processAll) {
                // If we were processing all logs and found none, switch to monitoring mode
                processAll = false;
                std::cout << "Finished processing existing logs. Now monitoring for new changes." << std::endl;
            }
        }
    }

    // Start monitoring in a separate thread
    void start() {
        if (running) return;
        
        running = true;
        std::thread([this]() {
            this->processLogs();
        }).detach();
    }

    // Stop monitoring
    void stop() {
        running = false;
        cv.notify_all();
    }

    // Get the latest sequence number
    uint64_t getLastSequence() const {
        return lastSequence;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <db_path> [output_file] [--all]" << std::endl;
        std::cerr << "  db_path: Path to RocksDB database" << std::endl;
        std::cerr << "  output_file: Optional file to write SQL queries (default: stdout)" << std::endl;
        std::cerr << "  --all: Process all logs from the beginning, ignoring previous state" << std::endl;
        return 1;
    }
    
    std::string dbPath = argv[1];
    std::string outputFile = "";
    bool processAll = false;
    
    // Parse arguments
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--all") {
            processAll = true;
        } else {
            outputFile = arg;
        }
    }
    
    try {
        AnuDBSQLConverter converter(dbPath, outputFile, processAll);
        
        // Start monitoring in real-time
        converter.start();
        
        std::cout << "SQL converter running for " << dbPath << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // Keep main thread alive
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
