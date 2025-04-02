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
    Database db("./unset_example_db");
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
    
    // Create sample product document
    json product = {
        {"name", "Smartphone X"},
        {"price", 899.99},
        {"stock", 120},
        {"category", "Electronics"},
        {"brand", "TechCorp"},
        {"promotion", "Limited Offer"},
        {"features", {
            {"screen", "6.5 inch OLED"},
            {"camera", "48MP triple camera"},
            {"storage", "256GB"},
            {"temporary_spec", "Test value"}
        }}
    };
    
    // Create document
    Document doc("smartphone1", product);
    status = products->createDocument(doc);
    if (!status.ok() && status.message().find("already exists") != std::string::npos) {
        // If exists, update it completely
        status = products->updateDocument(doc.id(), {{"$set", doc.data()}});
    }
    
    // Show original document
    Document readDoc;
    products->readDocument("smartphone1", readDoc);
    std::cout << "Original document:" << std::endl;
    printDocument(readDoc);
    
    // $unset operator usage
    std::cout << "\n===== Using $unset Operator =====\n";
    
    // 1. Remove top-level fields
    json unsetOperation = {
        {"$unset", {
            {"promotion", ""},     // Remove promotion field
            {"stock", ""}          // Remove stock field
        }}
    };
    
    status = products->updateDocument("smartphone1", unsetOperation);
    if (!status.ok()) {
        std::cerr << "Failed to update with $unset: " << status.message() << std::endl;
    } else {
        std::cout << "Document updated with $unset (top-level fields)" << std::endl;
        products->readDocument("smartphone1", readDoc);
        printDocument(readDoc);
    }
    
    // 2. Remove nested fields
    json unsetNestedOperation = {
        {"$unset", {
            {"features.temporary_spec", ""},  // Remove nested field
            {"features.storage", ""}          // Remove nested field
        }}
    };
    
    status = products->updateDocument("smartphone1", unsetNestedOperation);
    if (!status.ok()) {
        std::cerr << "Failed to update nested fields: " << status.message() << std::endl;
    } else {
        std::cout << "Document updated with $unset (nested fields)" << std::endl;
        products->readDocument("smartphone1", readDoc);
        printDocument(readDoc);
    }
    
    db.close();
    return 0;
}
