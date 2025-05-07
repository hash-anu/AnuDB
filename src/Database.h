#ifndef DATABASE_H
#define DATABASE_H

#include "Collection.h"

namespace anudb {

    // Database class representing the main ANUDB interface
    class Database {
    public:
        Database(const std::string& dbPath) : engine_(dbPath) {}
        Status open(bool walTracker = false);
        Status close();
        Status createCollection(const std::string& name);
        Status dropCollection(const std::string& name);
		Status readDocument(const std::string& collectionName, const std::string& id, Document& doc);
        // Not recommended for tight memory constraint devices.
        Status readAllDocuments(const std::string& collectionName, std::vector<Document>& docIds);
        Status exportAllToJsonAsync(const std::string& collectionName, const std::string& exportPath);
        Status importFromJsonFile(const std::string& collectionName, const std::string& importFile);
        void registerCallback(WalOperationCallback callback);

        Collection* getCollection(const std::string& name);
        std::vector<std::string> getCollectionNames() const;
        bool isDbOpen();
    private:
        bool isDbOpen_;
        StorageEngine engine_;
        std::unordered_map<std::string, std::unique_ptr<Collection>> collections_;
    };
}
#endif // DATABASE_H
