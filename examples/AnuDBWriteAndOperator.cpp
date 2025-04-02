#include "Database.h"
#include "json.hpp"
#include <iostream>
#include <vector>

using namespace anudb;
using json = nlohmann::json;

// Helper function to print document
void printDocument(const Document& doc) {
    std::cout << "Document ID: " << doc.id() << "\nContent:\n" << doc.data().dump(4) << "\n" << std::endl;
}

// Helper function to execute query and print results
void executeQuery(Collection* collection, const json& query, const std::string& queryName) {
    std::vector<std::string> docIds;
    std::cout << "\n===== Executing " << queryName << " =====\n";

    docIds = collection->findDocument(query);

    std::cout << "Found " << docIds.size() << " document(s)" << std::endl;
    for (const std::string& docId : docIds) {
        Document doc;
        Status status = collection->readDocument(docId, doc);
        if (status.ok()) {
            printDocument(doc);
        }
        else {
            std::cerr << "Failed to read document " << docId << ": " << status.message() << std::endl;
        }
    }
}

int main() {
    // Initialize and open database
    Database db("./and_range_scan_db");
    Status status = db.open();
    if (!status.ok()) {
        std::cerr << "Failed to open database: " << status.message() << std::endl;
        return 1;
    }
    
    // Create collection
    status = db.createCollection("products");
    if (!status.ok() && status.message().find("already exists") == std::string::npos) {
        std::cerr << "Failed to create collection: " << status.message() << std::endl;
        return 1;
    }
    
    Collection* products = db.getCollection("products");
    
    // Create sample product documents
    std::vector<Document> documents = {
        Document("prod001", json{
            {"name", "Basic Tablet"},
            {"price", 199.99},
            {"stock", 85},
            {"rating", 3.5},
            {"category", "Electronics"},
            {"available", true}
        }),
        Document("prod002", json{
            {"name", "Standard Tablet"},
            {"price", 349.99},
            {"stock", 50},
            {"rating", 4.0},
            {"category", "Electronics"},
            {"available", true}
        }),
        Document("prod003", json{
            {"name", "Pro Tablet"},
            {"price", 599.99},
            {"stock", 30},
            {"rating", 4.5},
            {"category", "Electronics"},
            {"available", true}
        }),
        Document("prod004", json{
            {"name", "Ultra Tablet"},
            {"price", 899.99},
            {"stock", 15},
            {"rating", 4.8},
            {"category", "Electronics"},
            {"available", false}
        }),
        Document("prod005", json{
            {"name", "Budget Headphones"},
            {"price", 49.99},
            {"stock", 120},
            {"rating", 3.7},
            {"category", "Audio"},
            {"available", true}
        })
    };
    
    // Insert documents
    for (Document& doc : documents) {
        status = products->createDocument(doc);
        if (!status.ok()) {
            if (status.message().find("already exists") != std::string::npos) {
                // Update if exists
                status = products->updateDocument(doc.id(), {{"$set", doc.data()}});
                if (!status.ok()) {
                    std::cerr << "Failed to update document " << doc.id() << ": " << status.message() << std::endl;
                }
            }
            else {
                std::cerr << "Failed to create document " << doc.id() << ": " << status.message() << std::endl;
            }
        }
    }
    
    // IMPORTANT: Create indexes for range scan operations
    std::cout << "\n===== Creating Indexes for Range Scanning =====\n";
    std::cout << "NOTE: When using $and with range conditions, indexes are critical for performance!\n";
    
    for (const auto& field : {"price", "stock", "rating", "category", "available"}) {
        status = products->createIndex(field);
        if (!status.ok()) {
            if (status.message().find("already exists") != std::string::npos) {
                std::cout << "Index on '" << field << "' already exists." << std::endl;
            }
            else {
                std::cerr << "Failed to create index on " << field << ": " << status.message() << std::endl;
                std::cerr << "Range scanning with $and will be extremely inefficient without indexes!" << std::endl;
            }
        }
        else {
            std::cout << "Index on '" << field << "' created successfully." << std::endl;
        }
    }
    
    // Perform $and range scans
    std::cout << "\n===== Performing $and Range Scans =====\n";
    
    // Query 1: 200 < Price < 600 AND Available = true
    json andPriceAvailable = {
        {"$and", {
            {{"$gt", {{"price", 200.0}}}},
            {{"$lt", {{"price", 600.0}}}},
            {{"$eq", {{"available", true}}}}
        }}
    };
    executeQuery(products, andPriceAvailable, "200 < Price < 600 AND Available = true");
    
    // Query 2: Rating > 4.0 AND Stock < 40 AND Category = Electronics
    json andRatingStockCategory = {
        {"$and", {
            {{"$gt", {{"rating", 4.0}}}},
            {{"$lt", {{"stock", 40}}}},
            {{"$eq", {{"category", "Electronics"}}}}
        }}
    };
    executeQuery(products, andRatingStockCategory, "Rating > 4.0 AND Stock < 40 AND Category = Electronics");
    
    // Query 3: Price range and rating range
    json andMultipleRanges = {
        {"$and", {
            {{"$gt", {{"price", 300.0}}}},
            {{"$lt", {{"price", 1000.0}}}},
            {{"$gt", {{"rating", 4.0}}}},
            {{"$lt", {{"rating", 5.0}}}}
        }}
    };
    executeQuery(products, andMultipleRanges, "300 < Price < 1000 AND 4.0 < Rating < 5.0");
    
    // Close database
    status = db.close();
    if (!status.ok()) {
        std::cerr << "Failed to close database: " << status.message() << std::endl;
    }

    return 0;
}
