#include "Database.h"
#include "json.hpp"
#include <iostream>

using namespace anudb;
using json = nlohmann::json;

void printDocument(const Document& doc) {
    std::cout << "Document ID: " << doc.id() << "\nContent:\n" << doc.data().dump(4) << "\n" << std::endl;
}

int main() {
    // Initialize and open database
    Database db("./pull_example_db");
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
    
    // Create sample product document with arrays
    json product = {
        {"name", "Smart Watch"},
        {"price", 299.99},
        {"category", "Wearables"},
        {"brand", "FitTech"},
        {"features", {"heart-rate-monitor", "gps", "sleep-tracking", "water-resistant", "unused-feature"}},
        {"compatibility", {"ios", "android", "deprecated-os"}}
    };
    
    // Create document
    Document doc("watch1", product);
    status = products->createDocument(doc);
    if (!status.ok() && status.message().find("already exists") != std::string::npos) {
        // If exists, update it completely
        status = products->updateDocument(doc.id(), {{"$set", doc.data()}});
    }
    
    // Show original document
    Document readDoc;
    products->readDocument("watch1", readDoc);
    std::cout << "Original document:" << std::endl;
    printDocument(readDoc);
    
    // $pull operator usage
    std::cout << "\n===== Using $pull Operator =====\n";
    
    // Remove elements from features array
    json pullFeaturesOperation = {
        {"$pull", {
            {"features", "unused-feature"}  // Remove specific element from features array
        }}
    };
    
    status = products->updateDocument("watch1", pullFeaturesOperation);
    if (!status.ok()) {
        std::cerr << "Failed to update with $pull (features): " << status.message() << std::endl;
    } else {
        std::cout << "Document updated with $pull (removed feature)" << std::endl;
        products->readDocument("watch1", readDoc);
        printDocument(readDoc);
    }
    
    // Remove elements from compatibility array
    json pullCompatibilityOperation = {
        {"$pull", {
            {"compatibility", "deprecated-os"}  // Remove specific element from compatibility array
        }}
    };
    
    status = products->updateDocument("watch1", pullCompatibilityOperation);
    if (!status.ok()) {
        std::cerr << "Failed to update with $pull (compatibility): " << status.message() << std::endl;
    } else {
        std::cout << "Document updated with $pull (removed compatibility)" << std::endl;
        products->readDocument("watch1", readDoc);
        printDocument(readDoc);
    }
    
    db.close();
    return 0;
}
