# AnuDB

AnuDB is a lightweight, embedded document database designed for C++ applications, offering efficient storage of JSON documents through MessagePack serialization. It provides a serverless, schema-less solution for applications requiring flexible data management with robust query capabilities.

As part of this repository, we have included a **stress test** that ingests **100,000 documents across 8 concurrent threads**. We encourage you to run this stress test on your platform and evaluate the ingestion/read/query performance. Your feedback on execution time and system behavior would be invaluable in further optimizing AnuDB’s performance. 

In addition to that, you can adjust memory/CPU usage of AnuDB based on RocksDB options mentioned in [StorageEngine.cpp](https://github.com/hash-anu/AnuDB/blob/master/src/storage_engine/StorageEngine.cpp)  and [StorageEngine.h](https://github.com/hash-anu/AnuDB/blob/master/src/storage_engine/StorageEngine.h). Based on these configurations, you can get your desired performance results tailored to your specific platform requirements.

## Features

- **Embedded & Serverless**: Run directly within your application with no separate server process required
- **Embedded Platform Support**: Specifically designed for both standard Linux systems and embedded platforms
-  **Faster Writes** – RocksDB (LSM-tree) backend is optimized for high-speed inserts/updates.
- **JSON Document Storage**: Store and query complex JSON documents
- **RocksDB Backend**: Built on RocksDB for high performance and durability
- **Transactional Properties**: Inherits atomic operations and configurable durability from RocksDB
- **Compression Options**: Support for ZSTD compression to reduce storage footprint
- **Flexible Querying**: Support for equality, comparison, logical operators and sorting
- **Indexing**: Create indexes to speed up queries on frequently accessed fields
- **Update Operations**: Rich set of document modification operations
- **Import/Export**: Easy JSON import and export for data migration.JSON file can be also used to migrate data to Postgresql/SQL sever/MySQL/Oracle DB
- **C++11 Compatible**: Designed to support a wide range of embedded devices efficiently.
- **Windows/Linux Support**: Designed for Windows/Linux environments and embedded Linux platforms

## Prerequisites

- C++11 >= compatible compiler
- CMake 3.10 or higher
- ZSTD development libraries (optional, for compression support)

## Building from Source
There is no need to install any other third party libraries.
Below commands will generate AnuDB.exe bin which executes all operations supported by AnuDB also it will generate liblibanu.a static library.

```bash

# Clone the repository
git clone https://github.com/hash-anu/AnuDB.git
cd AnuDB

# Create build directory
mkdir build && cd build

# Configure with CMake (basic)
cmake ..

# Configure with ZSTD compression support
cmake -DZSTD_INCLUDE_DIR=/path/to/zstd/include -DZSTD_LIB_DIR=/path/to/zstd/lib ..

# Build
make
```

### Embedded Platform Build

For embedded platforms, you may need to use a cross-compiler toolchain:

```bash
# Configure with cross-compiler
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/your/toolchain.cmake \
      -DZSTD_INCLUDE_DIR=/path/to/zstd/include \
      -DZSTD_LIB_DIR=/path/to/zstd/lib ..
```

## Examples
Examples of AnuDB database have been added to the examples folder. Currently, there are four examples available, with many more to be added in the near future. Please check back for additional resources as they become available. But based on APIs listed in table you can use AnuDB for your service.

## Quick Start

```cpp

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
```
[Sample for normal write -> read operation](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWriteReadDocument.cpp)

[Sample for get list of collections](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBCreateGetDropCollections.cpp)

[Sample for Export then Import operation](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBExportThenImport.cpp)

## API Overview

### Database Operations

| Operation | Description |
|-----------|-------------|
| `Database(const std::string& path)` | Constructor that sets the database path |
| `Status open()` | Opens the database |
| `Status close()` | Closes the database |
| `Status createCollection(const std::string& name)` | Creates a new collection |
| `Collection* getCollection(const std::string& name)` | Gets a pointer to a collection |
| `std::vector<std::string> getCollectionNames()` | Lists all collection names |
| `Status dropCollection(const std::string& name)` | Deletes a collection |
| `Status exportAllToJsonAsync(const std::string& collection, const std::string& outputDir)` | Exports a collection to JSON |
| `Status importFromJsonFile(const std::string& collection, const std::string& jsonFile)` | Imports JSON data into a collection |


### Collection Operations

| Operation | Description |
|-----------|-------------|
| `Status createDocument(Document& doc)` | Creates a new document |
| `Status readDocument(const std::string& id, Document& doc)` | Reads a document by ID |
| `Status updateDocument(const std::string& id, const json& updateDoc, bool upsert = false)` | Updates a document |
| `Status deleteDocument(const std::string& id)` | Deletes a document |
| `Status createIndex(const std::string& field)` | Creates an index on a field |
| `Status deleteIndex(const std::string& field)` | Deletes an index |
| `std::vector<std::string> findDocument(const json& query)` | Finds documents matching a query, to make find operation efficient indexing is **enforced** on the field |

### Document Class

| Operation | Description |
|-----------|-------------|
| `Document()` | Default constructor |
| `Document(const std::string& id, const json& data)` | Constructor with ID and data |
| `std::string id() const` | Gets the document ID |
| `void setId(const std::string& id)` | Sets the document ID |
| `json& data()` | Gets a reference to the document data |
| `void setData(const json& data)` | Sets the document data |

### Transactional Properties

AnuDB leverages RocksDB's storage engine and inherits the following transactional characteristics:

- **Atomic Operations**: Individual write operations (create, update, delete) are atomic
- **Write-Ahead Logging**: Configurable WAL for crash recovery
- **Point-in-time Snapshots**: Read consistency during queries

### Query Operations

AnuDB supports various query operations using a JSON-based query language:

| Operator | Description | Example |
|----------|-------------|---------|
| `$eq` | Equality match | `{"$eq": {"field": value}}`  [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWriteEqOperator.cpp) |
| `$gt` | Greater than | `{"$gt": {"field": value}}` [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWriteGtOperator.cpp)|
| `$lt` | Less than | `{"$lt": {"field": value}}` [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWriteLtOperator.cpp) |
| `$and` | Logical AND | `{"$and": [query1, query2, ...]}` [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWriteAndOperator.cpp) |
| `$or` | Logical OR | `{"$or": [query1, query2, ...]}` [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWriteOrOperator.cpp) |
| `$orderBy` | Sort results | `{"$orderBy": {"field": "asc"}}` [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWriteOrderByOperator.cpp) |

### Update Operations

| Operator | Description | Example |
|----------|-------------|---------|
| `$set` | Sets field values and supports nested fields using '.' | `{"$set": {"field": value}}` [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWriteSetUpdateReadDocument.cpp) |
| `$unset` | Removes fields and supports nested fields using '.' | `{"$unset": {"field": ""}}` [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWriteUnSetUpdateReadDocument.cpp)|
| `$push` | Adds to arrays | `{"$push": {"array": value}}` [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWritePushUpdateReadDocument.cpp)|
| `$pull` | Removes from arrays | `{"$pull": {"array": value}}` [example](https://github.com/hash-anu/AnuDB/blob/main/examples/AnuDBWritePopUpdateReadDocument.cpp)|

## Example Usage

### Creating and Populating a Collection

```cpp
#include "Database.h"
#include <iostream>

anudb::Database db("./product_db");
db.open();

// Create a collection
db.createCollection("products");
anudb::Collection* products = db.getCollection("products");

// Create a document
nlohmann::json productData = {
    {"name", "Laptop"},
    {"price", 1299.99},
    {"category", "Electronics"},
    {"specs", {
        {"processor", "i9"},
        {"ram", "32GB"}
    }}
};

anudb::Document doc("prod001", productData);
products->createDocument(doc);
```

### Query Examples

```cpp
// Find all electronics
nlohmann::json query = {
    {"$eq", {
        {"category", "Electronics"}
    }}
};

std::vector<std::string> results = products->findDocument(query);

// Price range query
nlohmann::json priceRangeQuery = {
    {"$and", {
        {{"$gt", {{"price", 1000.0}}}},
        {{"$lt", {{"price", 2000.0}}}}
    }}
};

results = products->findDocument(priceRangeQuery);
```

### Update Examples

```cpp
// Update document fields
nlohmann::json updateOp = {
    {"$set", {
        {"price", 1399.99},
        {"specs.ram", "64GB"}
    }}
};

products->updateDocument("prod001", updateOp);

// Add tags to a document
nlohmann::json pushOp = {
    {"$push", {
        {"tags", "sale"}
    }}
};

products->updateDocument("prod001", pushOp);
```

## Performance Considerations

- Create indexes on fields on which findDoc operations are needed
- Use specific queries rather than broad ones for better performance
- Export/import operations for large collections can be intensive; plan accordingly
- On embedded platforms, consider using the ZSTD compression to reduce storage requirements
- Adjust memory budget and cache size based on your device capabilities

## Embedded Platform Optimizations

- Configurable memory limits to work on constrained devices
- ZSTD compression reduces storage footprint and I/O operations
- Low CPU overhead suitable for embedded processors
- Minimal dependencies for smaller deployment size
- Supports cross-compilation for various architectures

## Limitations

- Embedded-only; no client-server architecture
- No built-in replication or sharding

## Acknowledgments

- Built on [RocksDB](https://rocksdb.org/)
- Uses [nlohmann/json](https://github.com/nlohmann/json) for JSON parsing
- Uses [MessagePack](https://msgpack.org/) for serialization
- Uses [ZSTD](https://github.com/facebook/zstd) for compression (optional)
