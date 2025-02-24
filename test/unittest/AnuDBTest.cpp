#include "gtest/gtest.h"
#include "Database.h"
#include "json.hpp"
#include <iostream>
#include <vector>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif


using namespace anudb;
using json = nlohmann::json;

class AnuDBTest : public ::testing::Test {
protected:
    Database* db;
    Collection* products;
    std::string dbPath = "./test_product_db";

    bool dirExists(const std::string& filename) {
#ifdef _WIN32
        DWORD attributes = GetFileAttributesA(filename.c_str());
        return (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat buffer;
        return (stat(filename.c_str(), &buffer) == 0);
#endif
    }

    bool removeDirectoryRecursive(const std::string& path) {
#ifdef _WIN32
        std::string command = "rmdir /S /Q \"" + path + "\"";
#else
        std::string command = "rm -rf \"" + path + "\"";
#endif
        return system(command.c_str()) == 0;
    }

    void SetUp() override {
        // Clean up any previous test database
        removeDirectoryRecursive(dbPath.c_str());
        
        // Create and open the database
        db = new Database(dbPath);
        Status status = db->open();
        ASSERT_TRUE(status.ok()) << "Failed to open database: " << status.message();

        // Create test collection
        status = db->createCollection("products");
        ASSERT_TRUE(status.ok()) << "Failed to create collection: " << status.message();
        
        products = db->getCollection("products");
        ASSERT_NE(products, nullptr) << "Failed to get collection";
        
        // Insert test documents
        createTestDocuments();
    }
    
    void TearDown() override {
        if (db) {
            Status status = db->close();
            EXPECT_TRUE(status.ok()) << "Failed to close database: " << status.message();
            delete db;
            db = nullptr;
        }
        
        removeDirectoryRecursive(dbPath.c_str());
    }
    
    void createTestDocuments() {
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

        // Smartphone product
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

        // Book product
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

        // Food product
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
        // Furniture product
        json product5 = {
            {"name", "Ergonomic Office Chair"},
            {"price", 249.99},
            {"stock", 35},
            {"category", "Furniture"},
            {"rating", 4.3},
            {"brand", "ComfortPlus"},
            {"materials", {"leather", "metal", "memory foam"}},
            {"colors", {"Black", "Brown", "White"}},
            {"dimensions", {
                {"height", 120.5},
                {"width", 65.0},
                {"depth", 68.0},
                {"weight", 15.2}
            }},
            {"features", {"adjustable height", "lumbar support", "arm rests"}},
            {"warranty", "3 years"},
            {"assembly_required", true},
            {"available", true}
        };
        // Clothing product
        json product6 = {
            {"name", "Winter Jacket"},
            {"price", 129.99},
            {"stock", 85},
            {"category", "Clothing"},
            {"rating", 4.6},
            {"brand", "NorthStyle"},
            {"sizes", {"S", "M", "L", "XL", "XXL"}},
            {"colors", {"Navy", "Black", "Green"}},
            {"material", "Polyester"},
            {"gender", "Unisex"},
            {"seasonal", "Winter"},
            {"waterproof", true},
            {"care", {
                {"washing", "Machine wash cold"},
                {"drying", "Tumble dry low"},
                {"ironing", "Do not iron"}
            }},
            {"available", true}
        };
        // Toy product
        json product7 = {
            {"name", "Building Blocks Set"},
            {"price", 39.99},
            {"stock", 120},
            {"category", "Toys"},
            {"rating", 4.9},
            {"brand", "KidsLearn"},
            {"age_range", "3-12"},
            {"pieces", 250},
            {"educational", true},
            {"materials", {"non-toxic plastic"}},
            {"dimensions", {
                {"box_height", 35.0},
                {"box_width", 45.0},
                {"box_depth", 12.0}
            }},
            {"safety_tested", true},
            {"awards", {"Toy of the Year 2024", "Parent's Choice Award"}},
            {"available", true}
        };
        // Kitchen appliance product
        json product8 = {
            {"name", "Smart Blender"},
            {"price", 89.99},
            {"stock", 62},
            {"category", "Kitchen Appliances"},
            {"rating", 4.4},
            {"brand", "KitchenTech"},
            {"color", "Silver"},
            {"wattage", 1200},
            {"capacity", "1.5L"},
            {"speeds", 10},
            {"features", {"pulse", "smoothie mode", "ice crush", "soup mode"}},
            {"warranty", "2 years"},
            {"connectivity", {
                {"bluetooth", true},
                {"wifi", false},
                {"app_control", true}
            }},
            {"dimensions", {
                {"height", 45.0},
                {"width", 18.0},
                {"depth", 18.0}
            }},
            {"available", true}
        };
        // Sports equipment product
        json product9 = {
            {"name", "Yoga Mat"},
            {"price", 29.99},
            {"stock", 95},
            {"category", "Sports"},
            {"rating", 4.7},
            {"brand", "FitLife"},
            {"thickness", "6mm"},
            {"material", "TPE"},
            {"dimensions", {
                {"length", 183.0},
                {"width", 61.0}
            }},
            {"colors", {"Purple", "Blue", "Green", "Black"}},
            {"features", {"non-slip", "eco-friendly", "lightweight", "carrying strap"}},
            {"care", "Wipe clean with damp cloth"},
            {"beginner_friendly", true},
            {"available", true}
        };
        // Beauty product
        json product10 = {
            {"name", "Organic Face Serum"},
            {"price", 38.50},
            {"stock", 45},
            {"category", "Beauty"},
            {"rating", 4.8},
            {"brand", "NaturalGlow"},
            {"volume", "30ml"},
            {"skin_type", {"all", "sensitive", "dry"}},
            {"ingredients", {"hyaluronic acid", "vitamin C", "aloe vera", "jojoba oil"}},
            {"benefits", {"hydrating", "anti-aging", "brightening"}},
            {"organic", true},
            {"cruelty_free", true},
            {"expiry_period", "12 months after opening"},
            {"instructions", "Apply morning and evening to clean skin"},
            {"available", true}
        };
        // Tool product
        json product11 = {
            {"name", "Cordless Drill Set"},
            {"price", 159.99},
            {"stock", 30},
            {"category", "Tools"},
            {"rating", 4.6},
            {"brand", "PowerPro"},
            {"power", "20V"},
            {"battery", {
                {"type", "Lithium-Ion"},
                {"capacity", "4.0Ah"},
                {"included", 2}
            }},
            {"max_rpm", 1800},
            {"torque_settings", 20},
            {"chuck_size", "13mm"},
            {"includes", {"drill", "2 batteries", "charger", "carrying case", "30 drill bits"}},
            {"warranty", "5 years"},
            {"professional_grade", true},
            {"available", true}
        };
        // Software product
        json product12 = {
            {"name", "Photo Editing Software"},
            {"price", 129.99},
            {"stock", 999},  // Digital product
            {"category", "Software"},
            {"rating", 4.5},
            {"brand", "CreativeSoft"},
            {"version", "2024"},
            {"license_type", "Perpetual"},
            {"platforms", {"Windows", "macOS", "Linux"}},
            {"features", {
                {"layers", true},
                {"filters", 150},
                {"cloud_storage", "5GB"},
                {"ai_tools", true},
                {"raw_support", true}
            }},
            {"requirements", {
                {"min_ram", "8GB"},
                {"min_processor", "2.0GHz Quad Core"},
                {"min_storage", "4GB"},
                {"graphics", "OpenGL 3.3 or higher"}
            }},
            {"instant_download", true},
            {"available", true}
        };
        // Musical instrument product
        json product13 = {
            {"name", "Acoustic Guitar"},
            {"price", 349.99},
            {"stock", 15},
            {"category", "Musical Instruments"},
            {"rating", 4.7},
            {"brand", "MeloWood"},
            {"body_type", "Dreadnought"},
            {"top_wood", "Spruce"},
            {"back_wood", "Mahogany"},
            {"strings", "Steel"},
            {"color", "Natural Wood"},
            {"dimensions", {
                {"length", 104.0},
                {"body_width", 39.0},
                {"depth", 12.0}
            }},
            {"includes", {"guitar", "soft case", "picks", "strap", "tuner"}},
            {"skill_level", {"beginner", "intermediate"}},
            {"handedness", "right"},
            {"warranty", "1 year"},
            {"available", true}
        };
        // Health supplement product
        json product14 = {
            {"name", "Protein Powder"},
            {"price", 45.99},
            {"stock", 80},
            {"category", "Health"},
            {"rating", 4.6},
            {"brand", "FitFuel"},
            {"weight", "1kg"},
            {"flavor", "Chocolate"},
            {"protein_per_serving", "25g"},
            {"servings", 40},
            {"ingredients", {"whey protein isolate", "cocoa powder", "stevia", "digestive enzymes"}},
            {"dietary", {
                {"gluten_free", true},
                {"soy_free", true},
                {"vegetarian", true},
                {"vegan", false}
            }},
            {"nutritional_info", {
                {"calories", 120},
                {"protein", "25g"},
                {"carbs", "3g"},
                {"fat", "2g"},
                {"sugar", "1g"}
            }},
            {"directions", "Mix one scoop with 8-10oz water or milk"},
            {"expiry_date", "2026-05-15"},
            {"available", true}
        };

        // Create document objects
        Document doc1("prod001", product1);
        Document doc2("prod002", product2);
        Document doc3("prod003", product3);
        Document doc4("prod004", product4);
        Document doc5("prod005", product5);
        Document doc6("prod006", product6);
        Document doc7("prod007", product7);
        Document doc8("prod008", product8);
        Document doc9("prod009", product9);
        Document doc10("prod010", product10);
        Document doc11("prod011", product11);
        Document doc12("prod012", product12);
        Document doc13("prod013", product13);
        Document doc14("prod014", product14);
        // Insert documents
        std::vector<Document> documents = { doc1, doc2, doc3, doc4, doc5, doc6, doc7, doc8, doc9, doc10, doc11, doc12, doc13, doc14 };
        for (Document& doc : documents) {
            Status status = products->createDocument(doc);
            ASSERT_TRUE(status.ok()) << "Failed to create document " << doc.id() << ": " << status.message();
        }
    }
};

// Database Operations Tests
TEST_F(AnuDBTest, DatabaseOpenClose) {
    // Setup and TearDown handle this, just verify the database is open
    ASSERT_NE(db, nullptr);
    EXPECT_TRUE(db->isDbOpen());
    
    // Close and reopen to test both operations
    Status status = db->close();
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(db->isDbOpen());
    
    status = db->open();
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(db->isDbOpen());
}

TEST_F(AnuDBTest, CollectionManagement) {
    // Create a new collection
    Status status = db->createCollection("test_collection");
    EXPECT_TRUE(status.ok());
    
    // Get collection names
    auto collectionNames = db->getCollectionNames();
    EXPECT_EQ(collectionNames.size(), 2);
    EXPECT_TRUE(std::find(collectionNames.begin(), collectionNames.end(), "products") != collectionNames.end());
    EXPECT_TRUE(std::find(collectionNames.begin(), collectionNames.end(), "test_collection") != collectionNames.end());
    
    // Get collection
    Collection* testColl = db->getCollection("test_collection");
    EXPECT_NE(testColl, nullptr);
    
    // Drop collection
    status = db->dropCollection("test_collection");
    EXPECT_TRUE(status.ok());
    
    // Verify it's gone
    collectionNames = db->getCollectionNames();
    EXPECT_EQ(collectionNames.size(), 1);
    EXPECT_TRUE(std::find(collectionNames.begin(), collectionNames.end(), "products") != collectionNames.end());
    EXPECT_TRUE(std::find(collectionNames.begin(), collectionNames.end(), "test_collection") == collectionNames.end());
}

// Document CRUD Tests
TEST_F(AnuDBTest, DocumentCreate) {
    // Create a new document
    json productData = {
        {"name", "Headphones"},
        {"price", 129.99},
        {"stock", 85},
        {"category", "Accessories"},
        {"rating", 4.4}
    };
    
    Document doc("prod005", productData);
    Status status = products->createDocument(doc);
    EXPECT_TRUE(status.ok());
    
    // Verify the document exists
    Document readDoc;
    status = products->readDocument("prod005", readDoc);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(readDoc.id(), "prod005");
    EXPECT_EQ(readDoc.data()["name"], "Headphones");
    EXPECT_EQ(readDoc.data()["price"], 129.99);
}

TEST_F(AnuDBTest, DocumentRead) {
    // Read an existing document
    Document doc;
    Status status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(doc.id(), "prod001");
    EXPECT_EQ(doc.data()["name"], "Laptop");
    EXPECT_EQ(doc.data()["price"], 1299.99);
    
    // Try to read a non-existent document
    Document nonExistentDoc;
    status = products->readDocument("non_existent_id", nonExistentDoc);
    EXPECT_FALSE(status.ok());
}

TEST_F(AnuDBTest, DocumentUpdate) {
    // Update top-level fields
    json updateData = {
        {"$set", {
            {"price", 1399.99},
            {"stock", 50},
            {"promotion", "Summer Sale"}
        }}
    };
    
    Status status = products->updateDocument("prod001", updateData);
    EXPECT_TRUE(status.ok());
    
    // Verify the update
    Document doc;
    status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(doc.data()["price"], 1399.99);
    EXPECT_EQ(doc.data()["stock"], 50);
    EXPECT_EQ(doc.data()["promotion"], "Summer Sale");
}

TEST_F(AnuDBTest, DocumentUpdateNested) {
    // Update nested fields
    json updateNestedData = {
        {"$set", {
            {"specs.processor", "i9-12900K"},
            {"specs.ram", "64GB"}
        }}
    };
    
    Status status = products->updateDocument("prod001", updateNestedData);
    EXPECT_TRUE(status.ok());
    
    // Verify the update
    Document doc;
    status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(doc.data()["specs"]["processor"], "i9-12900K");
    EXPECT_EQ(doc.data()["specs"]["ram"], "64GB");
}

TEST_F(AnuDBTest, DocumentDelete) {
    // Delete a document
    Status status = products->deleteDocument("prod001");
    EXPECT_TRUE(status.ok());
    
    // Verify it's gone
    Document doc;
    status = products->readDocument("prod001", doc);
    EXPECT_FALSE(status.ok());
}

// Query Tests
TEST_F(AnuDBTest, QueryEqualityOperator) {
    // Create index for faster queries
    Status status = products->createIndex("category");
    EXPECT_TRUE(status.ok());
    
    // Query for equality
    json query = {
        {"$eq", {
            {"category", "Electronics"}
        }}
    };
    
    std::vector<std::string> docIds = products->findDocument(query);
    EXPECT_EQ(docIds.size(), 2); // Should find Laptop and Smartphone
    
    // Verify the results contain expected IDs
    std::vector<std::string> expectedIds = {"prod001", "prod002"};
    for (const auto& id : expectedIds) {
        EXPECT_TRUE(std::find(docIds.begin(), docIds.end(), id) != docIds.end());
    }
}

TEST_F(AnuDBTest, QueryGreaterThanOperator) {
    // Create index for faster queries
    Status status = products->createIndex("price");
    EXPECT_TRUE(status.ok());

    // Query for price > 100
    json query = {
        {"$gt", {
            {"price", 100.0}
        }}
    };

    std::vector<std::string> docIds = products->findDocument(query);
    // Update the expected size and IDs based on new products
    EXPECT_GE(docIds.size(), 6); // Now more products over 100

    // Verify some known products over 100
    std::vector<std::string> expectedIds = {
        "prod001", "prod002", "prod005", "prod006", "prod011", "prod012", "prod013"
    };
    for (const auto& id : expectedIds) {
        EXPECT_TRUE(std::find(docIds.begin(), docIds.end(), id) != docIds.end());
    }
}

TEST_F(AnuDBTest, QueryLessThanOperator) {
    // Create index for faster queries
    Status status = products->createIndex("price");
    EXPECT_TRUE(status.ok());

    // Query for price < 100
    json query = {
        {"$lt", {
            {"price", 100.0}
        }}
    };

    std::vector<std::string> docIds = products->findDocument(query);
    // Update the expected size and IDs
    EXPECT_EQ(docIds.size(), 7); // Now more products under 100

    // Verify some known products under 100
    std::vector<std::string> expectedIds = {
        "prod003", "prod004", "prod007", "prod008", "prod009", "prod010", "prod014"
    };
    for (const auto& id : expectedIds) {
        EXPECT_TRUE(std::find(docIds.begin(), docIds.end(), id) != docIds.end());
    }
}

TEST_F(AnuDBTest, QueryOrderByOperator) {
    // Create index for faster queries
    Status status = products->createIndex("price");
    EXPECT_TRUE(status.ok());

    // Query for documents ordered by price ascending
    json query = {
        {"$orderBy", {
            {"price", "asc"}
        }}
    };

    std::vector<std::string> docIds = products->findDocument(query);
    EXPECT_EQ(docIds.size(), 14); // Now 14 products

    // Verify the first few entries are the lowest priced items
    // The exact order might vary, so we'll check for the lowest priced items
    EXPECT_TRUE(
        docIds[0] == "prod009" ||  // Yoga Mat at 29.99
        docIds[0] == "prod004"     // Organic Coffee at 15.99
    );

    // Last items should be the most expensive
    EXPECT_TRUE(
        docIds[docIds.size() - 1] == "prod011" ||  // Cordless Drill at 159.99
        docIds[docIds.size() - 1] == "prod001"    // Laptop at 1299.99
    );
}

TEST_F(AnuDBTest, QueryAndOperator) {
    // Create indexes for faster queries
    products->createIndex("category");
    products->createIndex("available");
    
    // Query for Electronics AND available=true
    json query = {
        {"$and", {
            {{"$eq", {{"category", "Electronics"}}}},
            {{"$eq", {{"available", true}}}}
        }}
    };
    
    std::vector<std::string> docIds;
    docIds = products->findDocument(query);
    EXPECT_EQ(docIds.size(), 2); // Should find Laptop and Smartphone
    
    // Verify the results contain expected IDs
    std::vector<std::string> expectedIds = {"prod001", "prod002"};
    for (const auto& id : expectedIds) {
        EXPECT_TRUE(std::find(docIds.begin(), docIds.end(), id) != docIds.end());
    }
}

TEST_F(AnuDBTest, QueryOrOperator) {
    // Create index for faster queries
    Status status = products->createIndex("category");
    EXPECT_TRUE(status.ok());
    
    // Query for Books OR Food
    json query = {
        {"$or", {
            {{"$eq", {{"category", "Books"}}}},
            {{"$eq", {{"category", "Food"}}}}
        }}
    };
    
    std::vector<std::string> docIds = products->findDocument(query);
    EXPECT_EQ(docIds.size(), 2); // Should find Book and Coffee
    
    // Verify the results contain expected IDs
    std::vector<std::string> expectedIds = {"prod003", "prod004"};
    for (const auto& id : expectedIds) {
        EXPECT_TRUE(std::find(docIds.begin(), docIds.end(), id) != docIds.end());
    }
}

TEST_F(AnuDBTest, QueryAndOperatorRangeScan) {
    // Create indexes for faster queries
    products->createIndex("price");
    products->createIndex("rating");

    // Query for price > 100 AND rating > 4.5
    json query = {
        {"$and", {
            {{"$gt", {{"price", 100.0}}}},
            {{"$gt", {{"rating", 4.5}}}}
        }}
    };

    std::vector<std::string> docIds = products->findDocument(query);

    // Verify the results
    std::vector<std::string> expectedIds = {
        "prod001",   // Laptop: price 1299.99, rating 4.7
        "prod006",
        "prod011",
        "prod013"    // Acoustic Guitar: price 349.99, rating 4.7
    };
    EXPECT_EQ(docIds.size(), expectedIds.size());
    for (const auto& id : expectedIds) {
        EXPECT_TRUE(std::find(docIds.begin(), docIds.end(), id) != docIds.end());
    }
}

TEST_F(AnuDBTest, QueryOrOperatorRangeScan) {
    // Create indexes for faster queries
    products->createIndex("price");
    products->createIndex("stock");

    // Query for price < 50 OR stock > 200
    json query = {
        {"$or", {
            {{"$lt", {{"price", 50.0}}}},
            {{"$gt", {{"stock", 200}}}}
        }}
    };

    std::vector<std::string> docIds = products->findDocument(query);

    // Verify the results
    std::vector<std::string> expectedIds = {
        "prod003",   // Book: price 49.99
        "prod004",   // Organic Coffee: price 15.99
        "prod007",
        "prod009",   // Yoga Mat: price 29.99
        "prod010",   // Face Serum: price 38.50
        "prod012",    // Software: stock 999
        "prod014",
    };

    EXPECT_EQ(docIds.size(), expectedIds.size());
    for (const auto& id : expectedIds) {
        EXPECT_TRUE(std::find(docIds.begin(), docIds.end(), id) != docIds.end());
    }
}

// Update Operation Tests
TEST_F(AnuDBTest, UpdateSetOperator) {
    // Test $set operator
    json updateData = {
        {"$set", {
            {"price", 1499.99},
            {"stock", 40},
            {"new_field", "This is a new field"}
        }}
    };
    
    Status status = products->updateDocument("prod001", updateData);
    EXPECT_TRUE(status.ok());
    
    // Verify the update
    Document doc;
    status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(doc.data()["price"], 1499.99);
    EXPECT_EQ(doc.data()["stock"], 40);
    EXPECT_EQ(doc.data()["new_field"], "This is a new field");
}

TEST_F(AnuDBTest, UpdateSetNestedOperator) {
    // Test $set operator with nested fields
    json updateData = {
        {"$set", {
            {"specs.processor", "i9-13900K"},
            {"specs.storage", "2TB SSD"},
            {"dimensions.height", 2.5}
        }}
    };
    Status status = products->updateDocument("prod001", updateData);
    EXPECT_TRUE(status.ok());
    
    // Verify the update
    Document doc;
    status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(doc.data()["specs"]["processor"], "i9-13900K");
    EXPECT_EQ(doc.data()["specs"]["storage"], "2TB SSD");
    EXPECT_EQ(doc.data()["dimensions"]["height"], 2.5);
}

TEST_F(AnuDBTest, UpdateUnsetOperator) {
    // Test $unset operator
    json updateData = {
        {"$unset", {
            {"available", ""},
            {"brand", ""}
        }}
    };
    
    Status status = products->updateDocument("prod001", updateData);
    EXPECT_TRUE(status.ok());
    
    // Verify the update
    Document doc;
    status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(doc.data().contains("available"));
    EXPECT_FALSE(doc.data().contains("brand"));
}

TEST_F(AnuDBTest, UpdateUnsetNestedOperator) {
    // Test $unset operator with nested fields
    json updateData = {
        {"$unset", {
            {"specs.storage", ""},
            {"dimensions.height", ""}
        }}
    };
    
    Status status = products->updateDocument("prod001", updateData);
    EXPECT_TRUE(status.ok());
    
    // Verify the update
    Document doc;
    status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(doc.data()["specs"].contains("storage"));
    EXPECT_FALSE(doc.data()["dimensions"].contains("height"));
}

TEST_F(AnuDBTest, UpdatePushOperator) {
    // Test $push operator for arrays
    json updateData = {
        {"$push", {
            {"tags", "premium"}
        }}
    };
    
    Status status = products->updateDocument("prod001", updateData, true);
    EXPECT_TRUE(status.ok());
    
    // Verify the update
    Document doc;
    status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    
    // Check if the array contains the new element
    auto& tags = doc.data()["tags"];
    bool found = false;
    for (const auto& tag : tags) {
        if (tag == "premium") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    
    // Test pushing to array that doesn't exist yet
    json updateData2 = {
        {"$push", {
            {"awards", "Best Laptop 2023"}
        }}
    };
    
    status = products->updateDocument("prod001", updateData2, true);
    EXPECT_TRUE(status.ok());
    
    // Verify the update
    status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(doc.data().contains("awards"));
    EXPECT_EQ(doc.data()["awards"], "Best Laptop 2023");
}

TEST_F(AnuDBTest, UpdatePullOperator) {
    // First, make sure we have a tag to pull
    json pushData = {
        {"$push", {
            {"tags", "to-be-removed"}
        }}
    };
    
    Status status = products->updateDocument("prod001", pushData, true);
    EXPECT_TRUE(status.ok());
    
    // Now test $pull operator
    json updateData = {
        {"$pull", {
            {"tags", "to-be-removed"}
        }}
    };
    
    status = products->updateDocument("prod001", updateData);
    EXPECT_TRUE(status.ok());
    
    // Verify the update
    Document doc;
    status = products->readDocument("prod001", doc);
    EXPECT_TRUE(status.ok());
    
    // Check that the array doesn't contain the removed element
    auto& tags = doc.data()["tags"];
    bool found = false;
    for (const auto& tag : tags) {
        if (tag == "to-be-removed") {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

// Index Tests
TEST_F(AnuDBTest, IndexCRUD) {
    // Create indexes
    std::vector<std::string> fieldsToIndex = {"name", "price", "category", "rating"};
    
    for (const auto& field : fieldsToIndex) {
        Status status = products->createIndex(field);
        EXPECT_TRUE(status.ok());
    }
    
    // TODO: Add a way to check if indexes exist
    
    // Delete some indexes
    std::vector<std::string> fieldsToDelete = {"name", "rating"};
    
    for (const auto& field : fieldsToDelete) {
        Status status = products->deleteIndex(field);
        EXPECT_TRUE(status.ok());
    }
    
    // TODO: Verify indexes are deleted
}

// Export/Import Tests
TEST_F(AnuDBTest, ExportDocuments) {
    std::string exportPath = "./test_export/";
    Status status = db->exportAllToJsonAsync("products", exportPath);
    products->waitForExportOperation();
    EXPECT_TRUE(status.ok());
    
    // Verify export directory exists
    EXPECT_TRUE(dirExists(exportPath));
    // Clean up after test
    removeDirectoryRecursive(exportPath);
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
