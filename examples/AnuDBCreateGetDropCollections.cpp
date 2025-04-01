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
    status = db.createCollection("users01");
    status = db.createCollection("users02");
    status = db.createCollection("users03");
    status = db.createCollection("users04");
    status = db.createCollection("users05");

    std::vector<std::string> cols = db.getCollectionNames();
    std::cout << "List of collections:\n";
    for (std::string col: cols) {
        std::cout << col << std::endl;
    }
    status = db.dropCollection("users01");
    status = db.dropCollection("users02");
    status = db.dropCollection("users03");
    status = db.dropCollection("users04");
    status = db.dropCollection("users05");
    // Close the database
    db.close();

    return 0;
}

