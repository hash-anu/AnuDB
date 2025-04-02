#include "Database.h"
#include "json.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace anudb;
using json = nlohmann::json;

// Helper function to print document
void printDocument(const Document& doc) {
    std::cout << "Document ID: " << doc.id() << "\nContent:\n" << doc.data().dump(4) << "\n" << std::endl;
}

int main() {
    // Database initialization
    Database db("./product_db");
    Status status = db.open();

    if (!status.ok()) {
        std::cerr << "Failed to open database: " << status.message() << std::endl;
        return 1;
    }
    std::cout << "Database opened successfully." << std::endl;

    // Create products collection
    status = db.createCollection("products");
    if (!status.ok()) {
        // Check if collection already exists
        if (status.message().find("already exists") != std::string::npos) {
            std::cout << "Collection 'products' already exists, continuing..." << std::endl;
        }
        else {
            std::cerr << "Failed to create collection: " << status.message() << std::endl;
            return 1;
        }
    }
    else {
        std::cout << "Collection 'products' created successfully." << std::endl;
    }

    // Get the collection
    Collection* products = db.getCollection("products");
    if (!products) {
        std::cerr << "Failed to get collection." << std::endl;
        return 1;
    }

    // STEP 1: Insert sample documents
    std::cout << "\n===== Inserting Sample Documents =====\n";

    // Electronics product
    json product1 = {
        {"name", "Laptop"},
        {"price", 1299.99},
        {"stock", 45},
        {"category", "Electronics"},
        {"rating", 4.7},
        {"available", true}
    };

    // Another electronics product
    json product2 = {
        {"name", "Smartphone"},
        {"price", 799.99},
        {"stock", 160},
        {"category", "Electronics"},
        {"rating", 4.5},
        {"available", true}
    };

    // Book product
    json product3 = {
        {"name", "Programming in C++"},
        {"price", 49.99},
        {"stock", 75},
        {"category", "Books"},
        {"rating", 4.2},
        {"available", true}
    };

    // Food product
    json product4 = {
        {"name", "Organic Coffee"},
        {"price", 15.99},
        {"stock", 200},
        {"category", "Food"},
        {"rating", 4.8},
        {"available", false}
    };

    // Create document objects
    Document doc1("prod001", product1);
    Document doc2("prod002", product2);
    Document doc3("prod003", product3);
    Document doc4("prod004", product4);

    // Insert documents
    std::vector<Document> documents = { doc1, doc2, doc3, doc4 };

    for (Document& doc : documents) {
        status = products->createDocument(doc);
        if (!status.ok()) {
            if (status.message().find("already exists") != std::string::npos) {
                std::cout << "Document " << doc.id() << " already exists, updating instead..." << std::endl;

                // If document exists, update it instead
                json updateDoc = {
                    {"$set", doc.data()}
                };

                status = products->updateDocument(doc.id(), updateDoc);
                if (!status.ok()) {
                    std::cerr << "Failed to update existing document " << doc.id() << ": " << status.message() << std::endl;
                }
                else {
                    std::cout << "Document " << doc.id() << " updated." << std::endl;
                }
            }
            else {
                std::cerr << "Failed to create document " << doc.id() << ": " << status.message() << std::endl;
            }
        }
        else {
            std::cout << "Document " << doc.id() << " created successfully." << std::endl;
        }
    }

    // STEP 2: Create indexes for fields we'll query
    std::cout << "\n===== Creating Indexes =====\n";

    // Create index for category field
    status = products->createIndex("category");
    if (!status.ok()) {
        if (status.message().find("already exists") != std::string::npos) {
            std::cout << "Index on 'category' already exists." << std::endl;
        }
        else {
            std::cerr << "Failed to create index on category: " << status.message() << std::endl;
            return 1;
        }
    }
    else {
        std::cout << "Index on 'category' created successfully." << std::endl;
    }

    // Create index for available field
    status = products->createIndex("available");
    if (!status.ok()) {
        if (status.message().find("already exists") != std::string::npos) {
            std::cout << "Index on 'available' already exists." << std::endl;
        }
        else {
            std::cerr << "Failed to create index on available: " << status.message() << std::endl;
            return 1;
        }
    }
    else {
        std::cout << "Index on 'available' created successfully." << std::endl;
    }

    // STEP 3: Execute $eq queries
    std::cout << "\n===== $eq Operator Examples =====\n";

    // Example 1: Find products in Electronics category
    json eqCategoryQuery = {
        {"$eq", {
            {"category", "Electronics"}
        }}
    };

    std::vector<std::string> docIds;
    std::cout << "\n----- Query: category = Electronics -----\n";

    docIds = products->findDocument(eqCategoryQuery);

    std::cout << "Found " << docIds.size() << " document(s)" << std::endl;
    for (const std::string& docId : docIds) {
        Document doc;
        Status status = products->readDocument(docId, doc);
        if (status.ok()) {
            printDocument(doc);
        }
        else {
            std::cerr << "Failed to read document " << docId << ": " << status.message() << std::endl;
        }
    }

    // Example 2: Find products that are available
    json eqAvailableQuery = {
        {"$eq", {
            {"available", true}
        }}
    };

    docIds.clear();
    std::cout << "\n----- Query: available = true -----\n";

    docIds = products->findDocument(eqAvailableQuery);

    std::cout << "Found " << docIds.size() << " document(s)" << std::endl;
    for (const std::string& docId : docIds) {
        Document doc;
        Status status = products->readDocument(docId, doc);
        if (status.ok()) {
            printDocument(doc);
        }
        else {
            std::cerr << "Failed to read document " << docId << ": " << status.message() << std::endl;
        }
    }

    // Close database
    status = db.close();
    if (!status.ok()) {
        std::cerr << "Failed to close database: " << status.message() << std::endl;
        return 1;
    }

    std::cout << "\nDatabase closed successfully." << std::endl;

    return 0;
}