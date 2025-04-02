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
    Database db("./gt_scan_db");
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
            {"name", "Budget Laptop"},
            {"price", 499.99},
            {"stock", 25},
            {"rating", 3.8}
        }),
        Document("prod002", json{
            {"name", "Mid-range Laptop"},
            {"price", 899.99},
            {"stock", 50},
            {"rating", 4.2}
        }),
        Document("prod003", json{
            {"name", "Premium Laptop"},
            {"price", 1499.99},
            {"stock", 15},
            {"rating", 4.7}
        }),
        Document("prod004", json{
            {"name", "Ultra Laptop"},
            {"price", 2499.99},
            {"stock", 5},
            {"rating", 4.9}
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
    for (const auto& field : {"price", "stock", "rating"}) {
        status = products->createIndex(field);
        if (!status.ok()) {
            if (status.message().find("already exists") != std::string::npos) {
                std::cout << "Index on '" << field << "' already exists." << std::endl;
            }
            else {
                std::cerr << "Failed to create index on " << field << ": " << status.message() << std::endl;
                std::cerr << "Range scanning will be inefficient without proper indexes!" << std::endl;
            }
        }
        else {
            std::cout << "Index on '" << field << "' created successfully." << std::endl;
        }
    }
    
    // Perform $gt (greater than) range scans
    std::cout << "\n===== Performing $gt Range Scans =====\n";
    
    // Query 1: Price > 1000
    json gtPrice = {
        {"$gt", {
            {"price", 1000.0}
        }}
    };
    executeQuery(products, gtPrice, "Price > 1000.0");
    
    // Query 2: Stock > 20
    json gtStock = {
        {"$gt", {
            {"stock", 20}
        }}
    };
    executeQuery(products, gtStock, "Stock > 20");
    
    // Query 3: Rating > 4.5
    json gtRating = {
        {"$gt", {
            {"rating", 4.5}
        }}
    };
    executeQuery(products, gtRating, "Rating > 4.5");
    
    // Close database
    status = db.close();
    if (!status.ok()) {
        std::cerr << "Failed to close database: " << status.message() << std::endl;
    }

    return 0;
}
