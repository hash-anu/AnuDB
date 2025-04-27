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

// Helper function to execute a query and print results
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

#include <chrono>
// Mutex for thread-safe console output
std::mutex console_mutex;
// Function to get current timestamp as string
std::string GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
// WAL operation handler
void WalOperationHandler(const std::string& operation,
    const std::string& cf_name,
    const std::string& key,
    const std::string& value) {
    std::lock_guard<std::mutex> lock(console_mutex);

    std::cout << "[" << GetTimestamp() << "] [WAL] ";
    std::cout << std::left << std::setw(10) << operation;
    std::cout << " | CF: " << std::setw(15) << cf_name;
    std::cout << " | Key: " << std::setw(20) << key;

    if (!value.empty()) {
        // Truncate value if it's too long
        std::string display_value = value;
        
        std::cout << " | Value: " << display_value;
    }

    std::cout << std::endl;
}

int main() {
    bool walTracker = true;
    // Database initialization
    Database db("./product_db");
    Status status = db.open(walTracker);
    if (!status.ok()) {
        std::cerr << "Failed to open database: " << status.message() << std::endl;
        return 1;
    }
    std::cout << "Database opened successfully." << std::endl;

    if (walTracker) {
        db.registerCallback(WalOperationHandler);
    }

    // Create products collection
    status = db.createCollection("products");
    if (!status.ok()) {
        // Check if the collection already exists - if so, continue; otherwise, return
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

    // Create sample product documents with different structures
    // Electronics product - complex nested structure
    json product1 = {
        {"name", "Laptop"},
        {"price", 1299.99},
        {"stock", 45},
        {"category", "Electronics"},
        {"rating", 4.7},
        {"brand", "TechMaster"},
        {"specs", {
            {"processor", "i9"},
            {"ram", "32GB"},
            {"storage", "1TB SSD"}
        }},
        {"tags", {"laptop", "gaming", "high-performance"}},
        {"dimensions", {
            {"length", 35.8},
            {"width", 24.7},
            {"height", 1.9}
        }},
        {"available", true}
    };

    // Smartphone product - array of objects structure
    json product2 = {
        {"name", "Smartphone"},
        {"price", 799.99},
        {"stock", 160},
        {"category", "Electronics"},
        {"rating", 4.5},
        {"brand", "MobiTech"},
        {"colors", {"Black", "Silver", "Blue"}},
        {"features", {
            {"camera", "48MP"},
            {"display", "AMOLED"},
            {"battery", "5000mAh"}
        }},
        {"reviews", {
            {
                {"user", "user123"},
                {"rating", 5},
                {"comment", "Great phone!"}
            },
            {
                {"user", "tech_reviewer"},
                {"rating", 4},
                {"comment", "Good performance but battery drains quickly"}
            }
        }},
        {"available", true}
    };

    // Book product - simple flat structure
    json product3 = {
        {"name", "Programming in C++"},
        {"price", 49.99},
        {"stock", 75},
        {"category", "Books"},
        {"rating", 4.2},
        {"author", "John Smith"},
        {"publisher", "Tech Books Inc"},
        {"pages", 450},
        {"isbn", "978-3-16-148410-0"},
        {"published_date", "2023-03-15"},
        {"available", true}
    };

    // Food product - another structure
    json product4 = {
        {"name", "Organic Coffee"},
        {"price", 15.99},
        {"stock", 200},
        {"category", "Food"},
        {"rating", 4.8},
        {"brand", "BeanMaster"},
        {"weight", "500g"},
        {"origin", "Colombia"},
        {"expiry_date", "2025-06-30"},
        {"nutritional_info", {
            {"calories", 0},
            {"fat", "0g"},
            {"caffeine", "95mg per serving"}
        }},
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

    // Create indexes
    std::cout << "\n===== Creating Indexes =====\n";
    for (const auto& field : { "price", "stock", "category", "rating", "available", "name" }) {
        status = products->createIndex(field);
        if (!status.ok()) {
            if (status.message().find("already exists") != std::string::npos) {
                std::cout << "Index on '" << field << "' already exists." << std::endl;
            }
            else {
                std::cerr << "Failed to create index on " << field << ": " << status.message() << std::endl;
            }
        }
        else {
            std::cout << "Index on '" << field << "' created successfully." << std::endl;
        }
    }

    // ===== Query Examples =====

    // 1. OrderBy queries
    std::cout << "\n===== OrderBy Queries =====\n";

    json orderByPriceAsc = {
        {"$orderBy", {
            {"price", "asc"}
        }}
    };
    executeQuery(products, orderByPriceAsc, "Order By Price (Ascending)");

    json orderByRatingDesc = {
        {"$orderBy", {
            {"rating", "desc"}
        }}
    };
    executeQuery(products, orderByRatingDesc, "Order By Rating (Descending)");

    // 2. Equality queries
    std::cout << "\n===== Equality Queries =====\n";

    json eqCategory = {
        {"$eq", {
            {"category", "Electronics"}
        }}
    };
    executeQuery(products, eqCategory, "Equal Category: Electronics");

    json eqAvailable = {
        {"$eq", {
            {"available", true}
        }}
    };
    executeQuery(products, eqAvailable, "Equal Available: true");

    // 3. Greater than queries
    std::cout << "\n===== Greater Than Queries =====\n";

    json gtPrice = {
        {"$gt", {
            {"price", 50.0}
        }}
    };
    executeQuery(products, gtPrice, "Price > 50.0");

    json gtRating = {
        {"$gt", {
            {"rating", 4.5}
        }}
    };
    executeQuery(products, gtRating, "Rating > 4.5");

    // 4. Less than queries
    std::cout << "\n===== Less Than Queries =====\n";

    json ltStock = {
        {"$lt", {
            {"stock", 100}
        }}
    };
    executeQuery(products, ltStock, "Stock < 100");

    json ltPrice = {
        {"$lt", {
            {"price", 500.0}
        }}
    };
    executeQuery(products, ltPrice, "Price < 500.0");

    // 5. AND queries
    std::cout << "\n===== AND Queries =====\n";

    json andPriceRating = {
        {"$and", {
            {{"$gt", {{"price", 100.0}}}},
            {{"$lt", {{"price", 1000.0}}}}
        }}
    };
    executeQuery(products, andPriceRating, "100 < Price < 1000");

    json andCategoryAvailable = {
        {"$and", {
            {{"$eq", {{"category", "Electronics"}}}},
            {{"$eq", {{"available", true}}}}
        }}
    };
    executeQuery(products, andCategoryAvailable, "Category = Electronics AND Available = true");

    // 6. OR queries
    std::cout << "\n===== OR Queries =====\n";

    json orCategories = {
        {"$or", {
            {{"$eq", {{"category", "Books"}}}},
            {{"$eq", {{"category", "Food"}}}}
        }}
    };
    executeQuery(products, orCategories, "Category = Books OR Category = Food");

    json orRatingStock = {
        {"$or", {
            {{"$gt", {{"rating", 4.7}}}},
            {{"$gt", {{"stock", 150}}}}
        }}
    };
    executeQuery(products, orRatingStock, "Rating > 4.7 OR Stock > 150");

    // ===== Update Examples =====
    std::cout << "\n===== Update Operations =====\n";

    // Example 1: $set - Top-level fields
    std::cout << "\n----- $set Operation: Top-level Fields -----\n";

    // Show original document
    Document updateDoc1;
    status = products->readDocument("prod001", updateDoc1);
    if (status.ok()) {
        std::cout << "Original document:" << std::endl;
        printDocument(updateDoc1);
    }

    json setTopLevel = {
        {"$set", {
            {"price", 1399.99},
            {"stock", 50},
            {"rating", 4.8},
            {"promotion", "Summer Sale"}  // Add new field
        }}
    };

    status = products->updateDocument("prod001", setTopLevel);
    if (!status.ok()) {
        std::cerr << "Failed to update document: " << status.message() << std::endl;
    }
    else {
        std::cout << "Document updated with $set operator (top-level fields)" << std::endl;
        status = products->readDocument("prod001", updateDoc1);
        if (status.ok()) {
            printDocument(updateDoc1);
        }
    }

    // Example 2: $set - Nested fields
    std::cout << "\n----- $set Operation: Nested Fields -----\n";

    json setNested = {
        {"$set", {
            {"specs.processor", "i9-12900K"},
            {"specs.ram", "64GB"},
            {"specs.storage", "2TB SSD"},
        }}
    };

    status = products->updateDocument("prod001", setNested);
    if (!status.ok()) {
        std::cerr << "Failed to update document: " << status.message() << std::endl;
    }
    else {
        std::cout << "Document updated with $set operator (nested fields)" << std::endl;
        status = products->readDocument("prod001", updateDoc1);
        if (status.ok()) {
            printDocument(updateDoc1);
        }
    }

    // Example 3: $unset - Top-level fields
    std::cout << "\n----- $unset Operation: Top-level Fields -----\n";

    json unsetTopLevel = {
        {"$unset", {
            {"promotion", ""},
            {"available", ""}
        }}
    };

    status = products->updateDocument("prod001", unsetTopLevel);
    if (!status.ok()) {
        std::cerr << "Failed to update document: " << status.message() << std::endl;
    }
    else {
        std::cout << "Document updated with $unset operator (top-level fields)" << std::endl;
        status = products->readDocument("prod001", updateDoc1);
        if (status.ok()) {
            printDocument(updateDoc1);
        }
    }

    // Example 4: $unset - Nested fields
    std::cout << "\n----- $unset Operation: Nested Fields -----\n";

    json unsetNested = {
        {"$unset", {
            {"specs.storage", ""},
            {"dimensions.height", ""}
        }}
    };

    status = products->updateDocument("prod001", unsetNested);
    if (!status.ok()) {
        std::cerr << "Failed to update document: " << status.message() << std::endl;
    }
    else {
        std::cout << "Document updated with $unset operator (nested fields)" << std::endl;
        status = products->readDocument("prod001", updateDoc1);
        if (status.ok()) {
            printDocument(updateDoc1);
        }
    }

    // Example 5: $push - Top-level array field
    std::cout << "\n----- $push Operation: Top-level Array Fields -----\n";

    Document updateDoc2;
    status = products->readDocument("prod001", updateDoc2);
    if (status.ok()) {
        std::cout << "Before $push operation:" << std::endl;
        printDocument(updateDoc2);
    }

    json pushTopLevel = {
        {"$push", {
            {"tags", "limited-edition"},
            {"specs", {{"storage", "2 GB"}} }
        }}
    };

    status = products->updateDocument("prod001", pushTopLevel, true);
    if (!status.ok()) {
        std::cerr << "Failed to update document: " << status.message() << std::endl;
    }
    else {
        std::cout << "Document updated with $push operator" << std::endl;
        status = products->readDocument("prod001", updateDoc2);
        if (status.ok()) {
            printDocument(updateDoc2);
        }
    }

    // Example 6: $pull - Top-level array field
    std::cout << "\n----- $pull Operation: Top-level Array Fields -----\n";

    json pullTopLevel = {
        {"$pull", {
            {"tags", "limited-edition"}
        }}
    };

    status = products->updateDocument("prod001", pullTopLevel);
    if (!status.ok()) {
        std::cerr << "Failed to update document: " << status.message() << std::endl;
    }
    else {
        std::cout << "Document updated with $pull operator" << std::endl;
        status = products->readDocument("prod001", updateDoc2);
        if (status.ok()) {
            printDocument(updateDoc2);
        }
    }

    // Example 7: Updating smartphone product with reviews
    std::cout << "\n----- Updating Smartphone Document -----\n";

    Document smartphoneDoc;
    status = products->readDocument("prod002", smartphoneDoc);
    if (status.ok()) {
        std::cout << "Original smartphone document:" << std::endl;
        printDocument(smartphoneDoc);
    }

    // Setting new element is not allowed
    json setSmartphone = {
        {"$set", {
            {"features.waterproof", "IP68"},
            {"price", 849.99}
        }}
    };

    status = products->updateDocument("prod002", setSmartphone);
    if (!status.ok()) {
        std::cerr << "Failed to update smartphone document: " << status.message() << std::endl;
    }
    else {
        std::cout << "Smartphone document updated with new features" << std::endl;
        status = products->readDocument("prod002", smartphoneDoc);
        if (status.ok()) {
            printDocument(smartphoneDoc);
        }
    }

    // Add a new review
    json newReview = {
        {"user", "mobile_fan"},
        {"rating", 5},
        {"comment", "Best smartphone I've ever owned!"}
    };

    json pushReview = {
        {"$push", {
            {"reviews", newReview}
        }}
    };

    status = products->updateDocument("prod002", pushReview, true);
    if (!status.ok()) {
        std::cerr << "Failed to add review: " << status.message() << std::endl;
    }
    else {
        std::cout << "Review added to smartphone document" << std::endl;
        status = products->readDocument("prod002", smartphoneDoc);
        if (status.ok()) {
            printDocument(smartphoneDoc);
        }
    }

    // Example 8: Updating book product
    std::cout << "\n----- Updating Book Document -----\n";

    Document bookDoc;
    status = products->readDocument("prod003", bookDoc);
    if (status.ok()) {
        std::cout << "Original book document:" << std::endl;
        printDocument(bookDoc);
    }

    // Update book information
    json updateBook = {
        {"$set", {
            {"edition", "Second Edition"},
            {"price", 39.99},
            {"stock", 100}
        }}
    };

    status = products->updateDocument("prod003", updateBook);
    if (!status.ok()) {
        std::cerr << "Failed to update book document: " << status.message() << std::endl;
    }
    else {
        std::cout << "Book document updated" << std::endl;
        status = products->readDocument("prod003", bookDoc);
        if (status.ok()) {
            printDocument(bookDoc);
        }
    }

    // Example 9: Updating food product
    std::cout << "\n----- Updating Food Document -----\n";

    Document foodDoc;
    status = products->readDocument("prod004", foodDoc);
    if (status.ok()) {
        std::cout << "Original food document:" << std::endl;
        printDocument(foodDoc);
    }

    // Update food availability and add certifications
    json updateFood = {
        {"$set", {
            {"available", true},
            {"certifications", {"Organic", "Fair Trade", "Rainforest Alliance"}}
        }}
    };

    status = products->updateDocument("prod004", updateFood);
    if (!status.ok()) {
        std::cerr << "Failed to update food document: " << status.message() << std::endl;
    }
    else {
        std::cout << "Food document updated" << std::endl;
        status = products->readDocument("prod004", foodDoc);
        if (status.ok()) {
            printDocument(foodDoc);
        }
    }

    // Export all documents
    std::cout << "\n===== Exporting Documents =====\n";
    status = db.exportAllToJsonAsync("products", "./product_export/");
    if (!status.ok()) {
        std::cerr << "Failed to export documents: " << status.message() << std::endl;
    }
    else {
        std::cout << "Documents exported successfully to ./product_export/" << std::endl;
    }
    products->waitForExportOperation();

    status = db.createCollection("products_import");
    if (!status.ok()) {
        // Check if the collection already exists - if so, continue; otherwise, return
        if (status.message().find("already exists") != std::string::npos) {
            std::cout << "Collection 'products' already exists, continuing..." << std::endl;
        }
        else {
            std::cerr << "Failed to create collection: " << status.message() << std::endl;
            return 1;
        }
    }
    else {
        std::cout << "Collection 'products_import' created successfully." << std::endl;
    }

    status = db.importFromJsonFile("products_import", "./product_export/products.json");
    if (!status.ok()) {
        std::cerr << "Failed to import documents: " << status.message() << std::endl;
    }
    else {
        std::cout << "Documents imported successfully to products_import collection" << std::endl;
    }

    std::vector<Document> docIds;
    status = db.readAllDocuments("products_import", docIds);
    if (!status.ok()) {
        std::cerr << "Failed to import documents: " << status.message() << std::endl;
    }
    else {
        std::cout << "Documents imported successfully to products_import collection" << std::endl;
    }

    for (Document doc : docIds) {
        std::cout << doc.data().dump(4) << std::endl;
    }
    // Clean up - Delete documents
    std::cout << "\n===== Cleanup Operations =====\n";

    // Delete a document
    std::string docIdToDelete = "prod004";
    status = products->deleteDocument(docIdToDelete);
    if (!status.ok()) {
        std::cerr << "Failed to delete document " << docIdToDelete << ": " << status.message() << std::endl;
    }
    else {
        std::cout << "Document " << docIdToDelete << " deleted successfully." << std::endl;
    }

    // Delete indexes
    std::vector<std::string> indexesToDelete = { "stock", "rating", "available" };
    for (const auto& indexName : indexesToDelete) {
        status = products->deleteIndex(indexName);
        if (!status.ok()) {
            std::cerr << "Failed to delete index on " << indexName << ": " << status.message() << std::endl;
        }
        else {
            std::cout << "Index on " << indexName << " deleted successfully." << std::endl;
        }
    }

    // List remaining collections
    std::cout << "\n===== Collections in Database =====\n";
    for (const auto& name : db.getCollectionNames()) {
        std::cout << "- " << name << std::endl;
    }

    // Drop collection
    std::string collectionToDrop = "products";
    std::cout << "\nDropping collection '" << collectionToDrop << "'" << std::endl;
    status = db.dropCollection(collectionToDrop);
    if (!status.ok()) {
        std::cerr << "Failed to drop collection: " << status.message() << std::endl;
    }
    else {
        std::cout << "Collection dropped successfully." << std::endl;
    }

    // Close database
    status = db.close();
    if (!status.ok()) {
        std::cerr << "Failed to close database: " << status.message() << std::endl;
        return 1;
    }

    std::cout << "\nDatabase closed successfully." << std::endl;
    std::cout << "C++ version: " << __cplusplus << std::endl;

    return 0;
}