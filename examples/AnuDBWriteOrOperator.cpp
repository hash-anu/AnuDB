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
    Database db("./or_range_scan_db");
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
            {"name", "Entry Camera"},
            {"price", 149.99},
            {"stock", 95},
            {"rating", 3.6},
            {"category", "Photography"},
            {"onSale", false}
        }),
        Document("prod002", json{
            {"name", "Mid-level Camera"},
            {"price", 499.99},
            {"stock", 55},
            {"rating", 4.2},
            {"category", "Photography"},
            {"onSale", true}
        }),
        Document("prod003", json{
            {"name", "Professional Camera"},
            {"price", 1299.99},
            {"stock", 25},
            {"rating", 4.7},
            {"category", "Photography"},
            {"onSale", false}
        }),
        Document("prod004", json{
            {"name", "Basic Tripod"},
            {"price", 39.99},
            {"stock", 150},
            {"rating", 3.9},
            {"category", "Photography Accessories"},
            {"onSale", true}
        }),
        Document("prod005", json{
            {"name", "Camera Lens"},
            {"price", 599.99},
            {"stock", 30},
            {"rating", 4.5},
            {"category", "Photography"},
            {"onSale", true}
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
    std::cout << "NOTE: When using $or with range conditions, all fields need indexes for optimal performance!\n";
    
    for (const auto& field : {"price", "stock", "rating", "category", "onSale"}) {
        status = products->createIndex(field);
        if (!status.ok()) {
            if (status.message().find("already exists") != std::string::npos) {
                std::cout << "Index on '" << field << "' already exists." << std::endl;
            }
            else {
                std::cerr << "Failed to create index on " << field << ": " << status.message() << std::endl;
                std::cerr << "Range scanning with $or will be very slow without proper indexes!" << std::endl;
            }
        }
        else {
            std::cout << "Index on '" << field << "' created successfully." << std::endl;
        }
    }
    
    // Perform $or range scans
    std::cout << "\n===== Performing $or Range Scans =====\n";
    
    // Query 1: Price < 100 OR Price > 1000
    json orPriceRanges = {
        {"$or", {
            {{"$lt", {{"price", 100.0}}}},
            {{"$gt", {{"price", 1000.0}}}}
        }}
    };
    executeQuery(products, orPriceRanges, "Price < 100 OR Price > 1000");
    
    // Query 2: Rating > 4.5 OR Stock > 100
    json orRatingStock = {
        {"$or", {
            {{"$gt", {{"rating", 4.5}}}},
            {{"$gt", {{"stock", 100}}}}
        }}
    };
    executeQuery(products, orRatingStock, "Rating > 4.5 OR Stock > 100");
    
    // Close database
    status = db.close();
    if (!status.ok()) {
        std::cerr << "Failed to close database: " << status.message() << std::endl;
    }

    return 0;
}
