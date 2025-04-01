#include "Database.h"
#include <iostream>

int main() {
    anudb::Database db("./my_database");
    anudb::Status status = db.open();

    if (!status.ok()) {
        std::cerr << "Failed to open database: " << status.message() << std::endl;
        return 1;
    }

    // Create a collection
    status = db.createCollection("users");

    // Get the collection
    anudb::Collection* users = db.getCollection("users");

    // Create a document
    nlohmann::json userData = {
        {"name", "Hash"},
        {"email", "hash@example.com"},
        {"age", 33}
    };
    anudb::Document doc("user001", userData);

    // Insert the document
    status = users->createDocument(doc);

    anudb::Document doc1;
    status = users->readDocument("user001", doc1);
    std::cout << doc1.data().dump(4) << std::endl;
    // Close the database
    db.close();

    return 0;
}
