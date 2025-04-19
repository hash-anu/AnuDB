#include "Database.h"

using namespace anudb;

Status Database::open() {
    isDbOpen_ = true;
    return engine_.open();
}

Status Database::close() {
    isDbOpen_ = false;
    collections_.clear();
    return engine_.close();
}

bool Database::isDbOpen() {
    return isDbOpen_;
}

Status Database::createCollection(const std::string& name) {
    // Create the collection in the storage engine
    Status status = engine_.createCollection(name);
    if (!status.ok()) {
        return status;
    }

    // Create and store the collection object
    collections_[name] = std::make_unique<Collection>(name, &engine_);
    return Status::OK();
}

Status Database::dropCollection(const std::string& name) {
    // Remove from our map
    auto it = collections_.find(name);
    if (it != collections_.end()) {
        collections_.erase(it);
    }
    // Removing collection indexes first
    Collection* col = getCollection(name);
    std::vector<std::string> indexes;
    Status status = col->getIndex(indexes);
    if (!status.ok()) {
        std::cerr << "Unable to fetch index list of collection : " << status.message() << std::endl;
    }
    for (std::string index : indexes) {
        status = col->deleteIndex(index);
        if (!status.ok()) {
            std::cerr << "Failed to drop index: " << index << " with this error message: " << status.message() << std::endl;
        }
    }
    // Drop collection from the storage engine
    return engine_.dropCollection(name);
}

Status Database::readDocument(const std::string& collectionName, const std::string& id, Document& doc) {
    Collection* collection = getCollection(collectionName);
    if (!collection) {
        return Status::NotFound("Collection not found: " + collectionName);
    }

    return collection->readDocument(id, doc);
}

Status Database::readAllDocuments(const std::string& collectionName, std::vector<Document>& docIds) {
    Collection* collection = getCollection(collectionName);
    if (!collection) {
        return Status::NotFound("Collection not found: " + collectionName);
    }
    return collection->readAllDocuments(docIds);
}

Status Database::exportAllToJsonAsync(const std::string& collectionName, const std::string& exportPath) {
    Collection* collection = getCollection(collectionName);
    if (!collection) {
        return Status::NotFound("Collection not found: " + collectionName);
    }
    return collection->exportAllToJsonAsync(exportPath);
}

Status Database::importFromJsonFile(const std::string& collectionName, const std::string& importFile) {
    Collection* collection = getCollection(collectionName);
    if (!collection) {
        return Status::NotFound("Collection not found: " + collectionName);
    }
    return collection->importFromJsonFile(importFile);
}

Collection* Database::getCollection(const std::string& name) {
    // Check if we already have the collection object
    auto it = collections_.find(name);
    if (it != collections_.end()) {
        return it->second.get();
    }

    // Check if the collection exists in the storage engine
    if (!engine_.collectionExists(name)) {
        return nullptr;
    }

    // Create and store the collection object
    collections_[name] = std::make_unique<Collection>(name, &engine_);
    return collections_[name].get();
}

std::vector<std::string> Database::getCollectionNames() const {
    return engine_.getCollectionNames();
}