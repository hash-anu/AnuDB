// Include necessary headers
#include "Database.h"
#include <iostream>

int main() {
    // Initialize the database with a file path
    anudb::Database db("./my_database");

    // Open the database connection
    anudb::Status status = db.open();
    if (!status.ok()) {
        std::cerr << "Failed to open database: " << status.message() << std::endl;
        return 1;
    }

    // Create a collection named "users" to store user data
    status = db.createCollection("users");

    // Get a pointer to the users collection
    anudb::Collection* users = db.getCollection("users");

    // Create a JSON document with sample user data
    nlohmann::json userData = {
        {"name", "Hash"},
        {"email", "hash@example.com"},
        {"age", 33}
    };

    // Initialize a document with ID "user001" and the user data
    anudb::Document doc("user001", userData);

    // Insert the document into the users collection
    status = users->createDocument(doc);

    // Create a new document object to store retrieved data
    anudb::Document doc1;

    // Read the document with ID "user001" from the collection
    status = users->readDocument("user001", doc1);

    // Print the document data with pretty formatting (4-space indentation)
    std::cout << doc1.data().dump(4) << std::endl;

    // Close the database connection properly
    db.close();

    return 0;
}