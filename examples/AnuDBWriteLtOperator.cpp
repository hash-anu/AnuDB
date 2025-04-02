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
    Database db("./lt_scan_db");
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
            {"name", "Budget Smartphone"},
            {"price", 299.99},
            {"stock", 65},
            {"rating", 3.9}
        }),
        Document("prod002", json{
            {"name", "Mid-range Smartphone"},
            {"price", 599.99},
            {"stock", 40},
            {"rating", 4.2}
        }),
        Document("prod003", json{
            {"name", "Premium Smartphone"},
            {"price", 999.99},
            {"stock", 25},
            {"rating", 4.6}
        }),
        Document("prod004", json{
            {"name", "Ultra Smartphone"},
            {"price", 1399.99},
            {"stock", 10},
            {"rating", 4.8}
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
    std::cout << "NOTE: Creating indexes is essential for efficient range scans!\n";
    
    for (const auto& field : {"price", "stock", "rating"}) {
        status = products->createIndex(field);
        if (!status.ok()) {
            if (status.message().find("already exists") != std::string::npos) {
                std::cout << "Index on '" << field << "' already exists." << std::endl;
            }
            else {
                std::cerr << "Failed to create index on " << field << ": " << status.message() << std::endl;
                std::cerr << "Range scanning will be extremely slow without indexes!" << std::endl;
            }
        }
        else {
            std::cout << "Index on '" << field << "' created successfully." << std::endl;
        }
    }
    
    // Perform $lt (less than) range scans
    std::cout << "\n===== Performing $lt Range Scans =====\n";
    
    // Query 1: Price < 600
    json ltPrice = {
        {"$lt", {
            {"price", 600.0}
        }}
    };
    executeQuery(products, ltPrice, "Price < 600.0");
    
    // Query 2: Stock < 30
    json ltStock = {
        {"$lt", {
            {"stock", 30}
        }}
    };
    executeQuery(products, ltStock, "Stock < 30");
    
    // Query 3: Rating < 4.5
    json ltRating = {
        {"$lt", {
            {"rating", 4.5}
        }}
    };
    executeQuery(products, ltRating, "Rating < 4.5");
    
    // Close database
    status = db.close();
    if (!status.ok()) {
        std::cerr << "Failed to close database: " << status.message() << std::endl;
    }

    return 0;
}
