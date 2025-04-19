# AnuDB

AnuDB is a lightweight, serverless document database designed for C++ applications, offering efficient storage of JSON documents through MessagePack serialization. It provides a serverless, schema-less solution for applications requiring flexible data management with robust query capabilities.

 Since AnuDB written on top of RocksDB, it ensures atomicity, durability and consistency of your documents. You can adjust memory/CPU usage of AnuDB based on RocksDB options mentioned in [StorageEngine.cpp](https://github.com/hash-anu/AnuDB/blob/master/src/storage_engine/StorageEngine.cpp)  and [StorageEngine.h](https://github.com/hash-anu/AnuDB/blob/master/src/storage_engine/StorageEngine.h).Based on these configurations, you can get your desired performance results tailored to your specific platform requirements.

## Features

- **Embedded & Serverless**: Run directly within your application with no separate server process required
- **Embedded Platform Support**: Specifically designed for both standard Linux/Windows systems and embedded platforms
-  **Faster Writes** – RocksDB (LSM-tree) backend is optimized for high-speed inserts/updates.
- **JSON Document Storage**: Store and query complex JSON documents
- **RocksDB Backend**: Built on RocksDB for high performance and durability
- **Transactional Properties**: Inherits atomic operations and configurable durability from RocksDB
- **Compression Options**: Support for ZSTD compression to reduce storage footprint
- **Flexible Querying**: Support for equality, comparison, logical operators and sorting
- **Indexing**: Create indexes to speed up queries on frequently accessed fields
- **Update Operations**: Rich set of document modification operations
- **Import/Export**: Easy JSON import and export for data migration. Exported JSON file can be also used to migrate data to Postgresql/SQL sever/MySQL/Oracle DB
- **C++11 Compatible**: Designed to support a wide range of embedded devices efficiently
- **Windows/Linux Support**: Designed for Windows/Linux environments and embedded Linux platforms
- **MQTT Interface**: Connect and operate via MQTT protocol from various platforms
- **High Concurrency**: Supports 32 concurrent nng worker threads for handling MQTT requests
- **TLS Security**: Secure communications using mbedTLS for encrypted MQTT connections
- **Cloud MQTT Support**: Compatible with major cloud MQTT brokers including AWS IoT, Azure IoT, GCP IoT, etc

## MQTT Interface

AnuDB now supports interaction via MQTT protocol, allowing you to connect and operate the database from various platforms without direct C++ integration. The implementation uses nlohmann::json for JSON handling.

In below demo, showing AnuDBMqttBridge and mosquitto broker server started, then using client.bash script, ran all supported MQTT commands

![AnuDBMqttDemo](demo.gif)

## Prerequisites

- C++11 >= compatible compiler
- CMake 3.10 or higher
- ZSTD development libraries (optional, for compression support)
- Mosquitto MQTT broker (for MQTT interface)
- mbedTLS libraries (for TLS support)

## Building from Source
There is no need to install any other third party libraries.
Below commands will generate AnuDB.exe bin which executes all operations supported by AnuDB, also it will generate liblibanu.a static library, you can use it in your application with required header files.

```bash

# Clone the repository
git clone https://github.com/hash-anu/AnuDB.git
cd AnuDB/third_party/nanomq/
git submodule update --init --recursive
cd ../..
mkdir build
cd build
cmake ..
make

or
# Configure with ZSTD compression support
cmake -DZSTD_INCLUDE_DIR=/path/to/zstd/include -DZSTD_LIB_DIR=/path/to/zstd/lib ..
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
Examples of AnuDB database have been added to the examples folder. 

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

### High-Performance MQTT Worker Architecture

AnuDB implements a high-performance concurrent worker architecture to handle MQTT requests efficiently:

- **32 Concurrent Worker Threads**: The MQTT bridge spawns 32 nng (nanomsg-next-generation) worker threads to process incoming requests in parallel
- **Load Balancing**: Requests are automatically distributed across all worker threads for optimal performance
- **Thread Safety**: All database operations are thread-safe, enabling true concurrent processing
- **Asynchronous Processing**: Each worker operates independently, preventing slow operations from blocking the entire system
- **Scalable Design**: The worker architecture scales efficiently on multi-core systems

#### Worker Thread Architecture Diagram

```
                           ┌─────────────────┐
                           │  MQTT Broker    │
                           │  (Mosquitto)    │
                           └────────┬────────┘
                                    │
                                    ▼
                           ┌─────────────────┐
                           │  MQTT Bridge    │
                           │   Connector     │
                           └────────┬────────┘
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │     Message Dispatcher        │
                    └───┬───────┬───────┬───────┬───┘
          ┌─────────────┘       │       │       └─────────────┐
          │                     │       │                     │
┌─────────▼───────┐   ┌─────────▼───┐   ▲           ┌─────────▼───────┐
│  nng Workers    │   │  nng Workers │   │           │  nng Workers    │
│ (Threads 1-8)   │   │ (Threads 9-16)│   │           │ (Threads 25-32) │
└─────────────────┘   └─────────────┘    │           └─────────────────┘
                                     ┌───┴───────┐
                                     │nng Workers│
                                     │(17-24)    │
                                     └───────────┘
                                          │
                                          ▼
                                  ┌─────────────────┐
                                  │    AnuDB        │
                                  │   Core Engine   │
                                  └─────────────────┘
```

### Setting Up MQTT Interface

1. Install and start Mosquitto MQTT broker:
   ```bash
   # On Ubuntu/Debian
   sudo apt-get install mosquitto mosquitto-clients
   sudo systemctl start mosquitto
   
   # On Windows
   # Download and install from https://mosquitto.org/download/
   # Start the service via Windows Services
   ```

2. Run the AnuDB MQTT service (after building the project):
   ```bash
   # Start the AnuDB MQTT service using AnuDBMqttBridge (TLS example)
   .\AnuDBMqttBridge.exe --broker_url tls+mqtt-tcp://<mqtt broker URL>:8883 --database_name <Your DB name> --tls_cacert <ca-cert path>

   # Start the AnuDB MQTT service using AnuDBMqttBridge (with out TLS example)
   .\AnuDBMqttBridge.exe --broker_url mqtt-tcp://<mqtt broker URL>:1883 --database_name <Your DB name>
   
   ```

   Usage of AnuDBMqttBridge:
   ```
   Usage: AnuDBMqttBridge.exe --broker_url <url> --database_name <name> [--username <user>] [--password <pass>] [--tls_cacert <path>] [--tls_cert <path>] [--tls_key <path>] [--tls_pass <pass>]
   ```

### Using the MQTT Client Scripts

We provide client scripts for both Linux (client.bash) and Windows (client.ps1) environments to interact with AnuDB via MQTT:

```bash
# Basic usage
./client.bash

# Run load test
./client.bash --load-test --threads 16 --operations 1000

```

### Supported MQTT Commands

The following commands are supported through the MQTT interface:

#### Collection Management

| Command | Description | Example Payload |
|---------|-------------|----------------|
| `create_collection` | Creates a new collection | `{"command":"create_collection","collection_name":"users","request_id":"req123"}` |
| `delete_collection` | Deletes an existing collection | `{"command":"delete_collection","collection_name":"users","request_id":"req123"}` |
| `get_collections` | Lists all collections | `{"command":"get_collections","request_id":"req123"}` |
| `export_collection` | Exports a collection to JSON files in specified directory | `{"command":"export_collection","collection_name":"users","dest_dir":"./product_mqtt_export/","request_id":"req123"}` |


#### Document Operations

| Command | Description | Example Payload |
|---------|-------------|----------------|
| `create_document` | Creates a new document (also used for updates) | `{"command":"create_document","collection_name":"users","document_id":"user001","content":{"name":"John","age":30},"request_id":"req123"}` |
| `read_document` | Reads a document by ID | `{"command":"read_document","collection_name":"users","document_id":"user001","request_id":"req123"}` |
| `delete_document` | Deletes a document | `{"command":"delete_document","collection_name":"users","document_id":"user001","request_id":"req123"}` |

Note: To update a document, use the `create_document` command with an existing document ID. This will overwrite the previous document with the new content.

#### Index Operations

| Command | Description | Example Payload |
|---------|-------------|----------------|
| `create_index` | Creates an index on a field | `{"command":"create_index","collection_name":"users","field":"age","request_id":"req123"}` |
| `delete_index` | Deletes an index | `{"command":"delete_index","collection_name":"users","field":"age","request_id":"req123"}` |
| `get_indexes` | Lists all indexes for a collection | `{"command":"get_indexes","collection_name":"users","request_id":"req123"}` |

#### Query Operations

| Command | Description | Example Payload |
|---------|-------------|----------------|
| `find_documents` | Finds documents matching a query | `{"command":"find_documents","collection_name":"users","query":{"$eq":{"age":30}},"request_id":"req123"}` |

#### Query Operators

The `find_documents` command supports the following query operators:

| Operator | Description | Example |
|----------|-------------|---------|
| `$eq` | Equality match | `{"$eq":{"field":"value"}}` |
| `$gt` | Greater than | `{"$gt":{"field":value}}` |
| `$lt` | Less than | `{"$lt":{"field":value}}` |
| `$and` | Logical AND | `{"$and":[{"$eq":{"field":"value"}},{"$gt":{"field":value}}]}` |
| `$or` | Logical OR | `{"$or":[{"$eq":{"field":"value"}},{"$gt":{"field":value}}]}` |
| `$orderBy` | Sort results | `{"$orderBy":{"field":"asc"}}` |

### Client Script Features

The provided client scripts offer several features:

- Interactive demo mode showcasing all MQTT operations
- Arbitrary command execution for testing specific commands
- Load testing with configurable threads and operations
- Detailed performance reporting
- Comprehensive help with command-line options

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
- For MQTT operations, leverage the 32 concurrent worker threads for optimal throughput
- Consider adjusting worker thread count for your specific hardware capabilities
- For high-volume MQTT usage, ensure your broker is properly configured for the expected load

## Embedded Platform Optimizations

- Configurable memory limits to work on constrained devices
- ZSTD compression reduces storage footprint and I/O operations
- Low CPU overhead suitable for embedded processors
- Minimal dependencies for smaller deployment size
- Supports cross-compilation for various architectures
- MQTT interface enables lightweight clients on resource-constrained devices
- TLS support can be conditionally compiled to reduce binary size when not needed

## Limitations

- Embedded-only; no client-server architecture (except via MQTT interface)
- No built-in replication or sharding

## Acknowledgments

- Built on [RocksDB](https://rocksdb.org/)
- Uses [nlohmann/json](https://github.com/nlohmann/json) for JSON parsing
- Uses [MessagePack](https://msgpack.org/) for serialization
- Uses [ZSTD](https://github.com/facebook/zstd) for compression (optional)
- Uses [NanoMQ](https://github.com/nanomq/nanomq) for MQTT communication
- Uses [nng (nanomsg-next-generation)](https://github.com/nanomsg/nng) for worker thread architecture
- Uses [mbedTLS](https://tls.mbed.org/) for TLS security
