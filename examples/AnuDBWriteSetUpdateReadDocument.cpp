#include "Database.h"
#include "json.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace anudb;
using json = nlohmann::json;

// Helper function to pretty-print documents
void printDocument(const Document& doc) {
    std::cout << "Document ID: " << doc.id() << "\nContent:\n" << doc.data().dump(4) << "\n" << std::endl;
}

int main() {
    // Initialize database
    Database db("./five_doc_example");
    Status status = db.open();
    if (!status.ok()) {
        std::cerr << "Failed to open database: " << status.message() << std::endl;
        return 1;
    }
    std::cout << "Database opened successfully." << std::endl;

    // Create collection
    status = db.createCollection("items");
    if (!status.ok() && status.message().find("already exists") == std::string::npos) {
        std::cerr << "Failed to create collection: " << status.message() << std::endl;
        return 1;
    }
    std::cout << "Collection 'items' ready." << std::endl;

    Collection* items = db.getCollection("items");
    if (!items) {
        std::cerr << "Failed to get collection." << std::endl;
        return 1;
    }

    // STEP 1: CREATE 5 DIFFERENT DOCUMENTS
    std::cout << "\n===== CREATING 5 DOCUMENTS =====\n";

    // Document 1: A movie
    json movie = {
        {"title", "The Matrix"},
        {"year", 1999},
        {"rating", 8.7},
        {"genre", {"Sci-Fi", "Action"}},
        {"director", "Wachowski Sisters"},
        {"cast", {
            {"name", "Keanu Reeves", "role", "Neo"},
            {"name", "Laurence Fishburne", "role", "Morpheus"},
            {"name", "Carrie-Anne Moss", "role", "Trinity"}
        }},
        {"inStock", true},
        {"price", 12.99}
    };

    // Document 2: A car
    json car = {
        {"make", "Tesla"},
        {"model", "Model 3"},
        {"year", 2022},
        {"features", {
            {"autopilot", true},
            {"range", "358 miles"},
            {"acceleration", "3.1 seconds"}
        }},
        {"colors", {"Red", "Black", "White", "Blue"}},
        {"price", 46990.00},
        {"inStock", true},
        {"reviews", {
            {{"user", "carfan42", "rating", 5, "comment", "Amazing car!"}},
            {{"user", "ecodrive", "rating", 4, "comment", "Great range but expensive"}}
        }}
    };

    // Document 3: A recipe
    json recipe = {
        {"name", "Chocolate Chip Cookies"},
        {"prepTime", "15 minutes"},
        {"cookTime", "10 minutes"},
        {"difficulty", "Easy"},
        {"rating", 4.8},
        {"ingredients", {
            "2 cups flour",
            "1/2 tsp baking soda",
            "1 cup butter",
            "1 cup sugar",
            "1 cup chocolate chips"
        }}/*,
        {"steps", {
            "Preheat oven to 350°F",
            "Mix dry ingredients",
            "Cream butter and sugar",
            "Combine all ingredients",
            "Drop spoonfuls onto baking sheet",
            "Bake for 10 minutes"
        }},
        {"nutrition", {
            {"calories", 150},
            {"fat", "7g"},
            {"sugar", "10g"}
        }}*/
    };

    // Document 4: A person
    json person = {
        {"firstName", "Jane"},
        {"lastName", "Smith"},
        {"age", 34},
        {"email", "jane.smith@example.com"},
        {"address", {
            {"street", "123 Main St"},
            {"city", "Boston"},
            {"state", "MA"},
            {"zipCode", "02108"}
        }},
        {"phoneNumbers", {
            {{"type", "home", "number", "555-1234"}},
            {{"type", "work", "number", "555-5678"}}
        }},
        {"hobbies", {"reading", "hiking", "photography"}},
        {"isEmployed", true}
    };

    // Document 5: A product
    json product = {
        {"name", "Wireless Headphones"},
        {"sku", "WH-1000XM4"},
        {"brand", "SoundMaster"},
        {"category", "Electronics"},
        {"price", 249.99},
        {"stock", 75},
        {"features", {
            {"noiseCancel", true},
            {"batteryLife", "30 hours"},
            {"waterproof", false}
        }},
        {"colors", {"Black", "Silver"}},
        {"rating", 4.6},
        {"onSale", false}
    };

    // Create document objects
    Document doc1("movie001", movie);
    Document doc2("car001", car);
    Document doc3("recipe001", recipe);
    Document doc4("person001", person);
    Document doc5("product001", product);

    // Insert documents
    std::vector<Document> documents = { doc1, doc2, doc3, doc4, doc5 };
    for (Document& doc : documents) {
        status = items->createDocument(doc);
        if (!status.ok()) {
            if (status.message().find("already exists") != std::string::npos) {
                std::cout << "Document " << doc.id() << " already exists, updating instead..." << std::endl;
                json updateDoc = { {"$set", doc.data()} };
                status = items->updateDocument(doc.id(), updateDoc);
                if (!status.ok()) {
                    std::cerr << "Failed to update document " << doc.id() << ": " << status.message() << std::endl;
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

    // Read all documents to verify creation
    std::cout << "\n===== READING ALL DOCUMENTS =====\n";
    for (const auto& docId : { "movie001", "car001", "recipe001", "person001", "product001" }) {
        Document doc;
        status = items->readDocument(docId, doc);
        if (status.ok()) {
            printDocument(doc);
        }
        else {
            std::cerr << "Failed to read document " << docId << ": " << status.message() << std::endl;
        }
    }

    // STEP 2: UPDATE ALL 5 DOCUMENTS USING DIFFERENT OPERATORS
    std::cout << "\n===== UPDATING DOCUMENTS USING DIFFERENT OPERATORS =====\n";

    // Example 1: $set - Update movie with new fields and modify existing ones
    std::cout << "\n----- Update Movie using $set -----\n";
    json updateMovie = {
        {"$set", {
            {"rating", 9.0},                    // Update existing field
            {"genre", {"Sci-Fi", "Action", "Cyberpunk"}},  // Replace array
        }}
    };

    status = items->updateDocument("movie001", updateMovie);
    if (!status.ok()) {
        std::cerr << "Failed to update movie: " << status.message() << std::endl;
    }
    else {
        Document updatedMovie;
        items->readDocument("movie001", updatedMovie);
        std::cout << "Movie updated with $set operator:" << std::endl;
        printDocument(updatedMovie);
    }

    // Example 2: $unset - Remove fields from car document
    std::cout << "\n----- Update Car using $unset -----\n";
    json updateCar = {
        {"$unset", {
            {"reviews", ""},        // Remove entire reviews array
            {"features.autopilot", ""}  // Remove nested field
        }}
    };

    status = items->updateDocument("car001", updateCar);
    if (!status.ok()) {
        std::cerr << "Failed to update car: " << status.message() << std::endl;
    }
    else {
        Document updatedCar;
        items->readDocument("car001", updatedCar);
        std::cout << "Car updated with $unset operator:" << std::endl;
        printDocument(updatedCar);
    }

    // Example 3: $push - Add new elements to arrays in recipe
    std::cout << "\n----- Update Recipe using $push -----\n";
    json updateRecipe = {
        {"$push", {
            {"ingredients", "1 tsp vanilla extract"},  // Add to ingredients array
            {"steps", "Let cool before serving"}       // Add to steps array
        }}
    };

    status = items->updateDocument("recipe001", updateRecipe, true);
    if (!status.ok()) {
        std::cerr << "Failed to update recipe: " << status.message() << std::endl;
    }
    else {
        Document updatedRecipe;
        items->readDocument("recipe001", updatedRecipe);
        std::cout << "Recipe updated with $push operator:" << std::endl;
        printDocument(updatedRecipe);
    }

    // Example 4: Combined $set and $push for person document
    std::cout << "\n----- Update Person using $set and $push -----\n";

    // First operation: $set
    json updatePerson1 = {
        {"$set", {
            {"age", 35},                   // Update age
            {"address.city", "Cambridge"}, // Update nested field
        }}
    };

    status = items->updateDocument("person001", updatePerson1);
    if (!status.ok()) {
        std::cerr << "Failed to update person ($set): " << status.message() << std::endl;
    }

    // Second operation: $push
    json updatePerson2 = {
        {"$push", {
            {"hobbies", "cooking"},  // Add to hobbies array
            {"phoneNumbers", {{"type", "mobile", "number", "555-9012"}}}  // Add to phone numbers array
        }}
    };

    status = items->updateDocument("person001", updatePerson2, true);
    if (!status.ok()) {
        std::cerr << "Failed to update person ($push): " << status.message() << std::endl;
    }
    else {
        Document updatedPerson;
        items->readDocument("person001", updatedPerson);
        std::cout << "Person updated with combined operators:" << std::endl;
        printDocument(updatedPerson);
    }

    // Example 5: $pull - Remove elements from arrays in product
    std::cout << "\n----- Update Product using $pull and $set -----\n";

    // First pull elements from array
    json updateProduct1 = {
        {"$pull", {
            {"colors", "Silver"}  // Remove Silver from colors array
        }}
    };

    status = items->updateDocument("product001", updateProduct1);
    if (!status.ok()) {
        std::cerr << "Failed to update product ($pull): " << status.message() << std::endl;
    }

    // Then update with $set
    json updateProduct2 = {
        {"$set", {
            {"price", 199.99},     // Reduce price
            {"onSale", true},      // Change boolean value
            {"stock", 50},         // Update stock
            {"features.waterproof", true},  // Change nested boolean
        }}
    };

    status = items->updateDocument("product001", updateProduct2);
    if (!status.ok()) {
        std::cerr << "Failed to update product ($set): " << status.message() << std::endl;
    }
    else {
        Document updatedProduct;
        items->readDocument("product001", updatedProduct);
        std::cout << "Product updated with $pull and $set operators:" << std::endl;
        printDocument(updatedProduct);
    }

    // STEP 3: READ ALL UPDATED DOCUMENTS
    std::cout << "\n===== READING ALL UPDATED DOCUMENTS =====\n";
    for (const auto& docId : { "movie001", "car001", "recipe001", "person001", "product001" }) {
        Document doc;
        status = items->readDocument(docId, doc);
        if (status.ok()) {
            printDocument(doc);
        }
        else {
            std::cerr << "Failed to read document " << docId << ": " << status.message() << std::endl;
        }
    }

    // STEP 4: ALTERNATIVE TO QUERIES - READ ALL DOCUMENTS AND FILTER IN CODE
    std::cout << "\n===== FILTERING DOCUMENTS IN APPLICATION CODE =====\n";

    std::cout << "\n--- Manual Filtering: Documents with price > 100 ---\n";
    int highPriceCount = 0;
    for (const auto& docId : { "movie001", "car001", "recipe001", "person001", "product001" }) {
        Document doc;
        status = items->readDocument(docId, doc);
        if (status.ok() && doc.data().contains("price")) {
            double price = doc.data()["price"];
            if (price > 100.0) {
                highPriceCount++;
                std::cout << "- " << doc.id();
                if (doc.data().contains("name")) {
                    std::cout << ": " << doc.data()["name"] << " ($" << price << ")";
                }
                std::cout << std::endl;
            }
        }
    }
    std::cout << "Found " << highPriceCount << " high-priced items" << std::endl;

    std::cout << "\n--- Manual Filtering: Documents with rating > 4.7 ---\n";
    int highRatingCount = 0;
    for (const auto& docId : { "movie001", "car001", "recipe001", "person001", "product001" }) {
        Document doc;
        status = items->readDocument(docId, doc);
        if (status.ok() && doc.data().contains("rating")) {
            double rating = doc.data()["rating"];
            if (rating > 4.7) {
                highRatingCount++;
                std::cout << "- " << doc.id() << ": Rating " << rating << std::endl;
            }
        }
    }
    std::cout << "Found " << highRatingCount << " high-rated items" << std::endl;

    std::cout << "\n--- Manual Filtering: Price > 50 AND onSale is true ---\n";
    int discountedCount = 0;
    for (const auto& docId : { "movie001", "car001", "recipe001", "person001", "product001" }) {
        Document doc;
        status = items->readDocument(docId, doc);
        if (status.ok() &&
            doc.data().contains("price") && doc.data().contains("onSale")) {

            double price = doc.data()["price"];
            bool onSale = doc.data()["onSale"];

            if (price > 50.0 && onSale) {
                discountedCount++;
                std::cout << "- " << doc.id();
                if (doc.data().contains("name")) {
                    std::cout << ": " << doc.data()["name"] << " ($" << price << ")";
                }
                std::cout << std::endl;
            }
        }
    }
    std::cout << "Found " << discountedCount << " on-sale expensive items" << std::endl;

    // Close database
    status = db.close();
    if (!status.ok()) {
        std::cerr << "Failed to close database: " << status.message() << std::endl;
        return 1;
    }
    std::cout << "\nDatabase closed successfully." << std::endl;

    return 0;
}

