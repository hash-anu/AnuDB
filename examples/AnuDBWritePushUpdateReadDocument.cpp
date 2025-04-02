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
    Database db("./push_example_db");
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
        {"name", "Gaming Laptop"},
        {"price", 1499.99},
        {"category", "Electronics"},
        {"brand", "GameMaster"},
        {"tags", {"gaming", "laptop", "high-performance"}},
        {"reviews", {
            {
                {"user", "gamer123"},
                {"rating", 5},
                {"comment", "Amazing performance!"}
            }
        }}
    };
    
    // Create document
    Document doc("laptop1", product);
    status = products->createDocument(doc);
    if (!status.ok() && status.message().find("already exists") != std::string::npos) {
        // If exists, update it completely
        status = products->updateDocument(doc.id(), {{"$set", doc.data()}});
    }
    
    // Show original document
    Document readDoc;
    products->readDocument("laptop1", readDoc);
    std::cout << "Original document:" << std::endl;
    printDocument(readDoc);
    
    // $push operator usage
    std::cout << "\n===== Using $push Operator =====\n";
    
    // 1. Push simple value to array
    json pushTagOperation = {
        {"$push", {
            {"tags", "vr-ready"}  // Add new tag to the tags array
        }}
    };
    
    status = products->updateDocument("laptop1", pushTagOperation, true);
    if (!status.ok()) {
        std::cerr << "Failed to update with $push (tags): " << status.message() << std::endl;
    } else {
        std::cout << "Document updated with $push (added tag)" << std::endl;
        products->readDocument("laptop1", readDoc);
        printDocument(readDoc);
    }
    
    // 2. Push object to array
    json newReview = {
        {"user", "techexpert"},
        {"rating", 4},
        {"comment", "Great laptop, but runs hot under heavy load"}
    };
    
    json pushReviewOperation = {
        {"$push", {
            {"reviews", newReview}  // Add new review object to reviews array
        }}
    };
    
    status = products->updateDocument("laptop1", pushReviewOperation, true);
    if (!status.ok()) {
        std::cerr << "Failed to update with $push (review): " << status.message() << std::endl;
    } else {
        std::cout << "Document updated with $push (added review)" << std::endl;
        products->readDocument("laptop1", readDoc);
        printDocument(readDoc);
    }
    
    db.close();
    return 0;
}
