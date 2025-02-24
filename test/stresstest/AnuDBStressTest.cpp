#include <gtest/gtest.h>
#include "Database.h"
#include "json.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

using namespace anudb;
using json = nlohmann::json;

// Inherit from ::testing::Test explicitly
class AnuDBStressTest : public ::testing::Test {
protected:
	static int NUM_DOCUMENTS;
	static int NUM_THREADS;
	static std::string dbPath;
	static Database* db;
	static Collection* products;
	
	static bool removeDirectoryRecursive(const std::string& path) {
#ifdef _WIN32
		std::string command = "rmdir /S /Q \"" + path + "\"";
#else
		std::string command = "rm -rf \"" + path + "\"";
#endif
		return system(command.c_str()) == 0;
	}

	// Use SetUpTestCase and TearDownTestCase for static setup and teardown
	static void SetUpTestCase() {
		//removeDirectoryRecursive(dbPath);
		// Initialize database
		db = new Database(dbPath);
		Status status = db->open();
		ASSERT_TRUE(status.ok()) << "Failed to open database: " << status.message();

		// Create products collection
		status = db->createCollection("products");
		ASSERT_TRUE(status.ok() ||
			status.message().find("already exists") != std::string::npos)
			<< "Failed to create collection: " << status.message();

		products = db->getCollection("products");
		ASSERT_NE(products, nullptr) << "Failed to get products collection";

		std::vector<std::string> fieldsToIndex = { "price" , "category" , "stock" , "rating"};

		for (const auto& field : fieldsToIndex) {
			Status status = products->createIndex(field);
			EXPECT_TRUE(status.ok());
		}
	}

	static void TearDownTestCase() {
		// Cleanup
		if (db) {
			Status status = db->dropCollection("products");
			status = db->close();
			delete db;
			db = nullptr;
			products = nullptr;
		}
	}

	// Rest of the implementation remains the same as previous artifact...
	// (Keep all the previous methods: generateRandomProduct, insertDocumentsMultiThreaded, etc.)

	// Generate a random product JSON
	static json generateRandomProduct(int index) {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<> priceDist(10.0, 1000.0);
		std::uniform_int_distribution<> stockDist(1, 500);
		std::uniform_real_distribution<> ratingDist(1.0, 5.0);

		// Categories to cycle through
		std::vector<std::string> categories = { "Electronics", "Books", "Food", "Clothing" };
		std::string category = categories[index % categories.size()];

		json product = {
			{"name", "Product " + std::to_string(index)},
			{"price", priceDist(gen)},
			{"stock", stockDist(gen)},
			{"category", category},
			{"rating", ratingDist(gen)},
			{"available", index % 2 == 0},
			{"metadata", {
				{"created_at", std::to_string(std::chrono::system_clock::now().time_since_epoch().count())},
				{"updated_at", ""},
				{"unique_id", "unique_" + std::to_string(index)}
			}}
		};

		// Add some nested complexity
		if (category == "Electronics") {
			product["specs"] = {
				{"processor", "CPU_" + std::to_string(index)},
				{"ram", std::to_string(index % 32) + "GB"}
			};
		}
		else if (category == "Books") {
			product["author"] = "Author " + std::to_string(index % 100);
		}

		return product;
	}

	// Threaded document insertion test
	void insertDocumentsMultiThreaded() {
		std::vector<std::thread> threads;
		std::atomic<int> successfulInserts{ 0 };
		std::atomic<int> failedInserts{ 0 };

		auto insertWorker = [this, &successfulInserts, &failedInserts](int startIndex, int endIndex) {
			for (int i = startIndex; i < endIndex; ++i) {
				json productData = generateRandomProduct(i);
				Document doc("prod_" + std::to_string(i), productData);

				Status status = products->createDocument(doc);
				if (status.ok()) {
					successfulInserts++;
				}
				else {
					failedInserts++;
				}
			}
			};

		// Divide work among threads
		int docsPerThread = NUM_DOCUMENTS / NUM_THREADS;
		for (int t = 0; t < NUM_THREADS; ++t) {
			int start = t * docsPerThread;
			int end = (t == NUM_THREADS - 1) ? NUM_DOCUMENTS : (t + 1) * docsPerThread;
			threads.emplace_back(insertWorker, start, end);
		}

		// Wait for all threads to complete
		for (auto& thread : threads) {
			thread.join();
		}

		// Verify insertions
		EXPECT_EQ(successfulInserts, NUM_DOCUMENTS)
			<< "Not all documents were inserted successfully";
		EXPECT_EQ(failedInserts, 0)
			<< "Some document insertions failed";
	}

	// Threaded document reading test
	void readDocumentsMultiThreaded() {
		std::vector<std::thread> threads;
		std::atomic<int> successfulReads{ 0 };
		std::atomic<int> failedReads{ 0 };

		auto readWorker = [this, &successfulReads, &failedReads](int startIndex, int endIndex) {
			for (int i = startIndex; i < endIndex; ++i) {
				Document doc;
				Status status = products->readDocument("prod_" + std::to_string(i), doc);

				if (status.ok()) {
					successfulReads++;

					// Optionally verify some basic document properties
					EXPECT_EQ(doc.id(), "prod_" + std::to_string(i));
				}
				else {
					failedReads++;
				}
			}
			};

		// Divide work among threads
		int docsPerThread = NUM_DOCUMENTS / NUM_THREADS;
		for (int t = 0; t < NUM_THREADS; ++t) {
			int start = t * docsPerThread;
			int end = (t == NUM_THREADS - 1) ? NUM_DOCUMENTS : (t + 1) * docsPerThread;
			threads.emplace_back(readWorker, start, end);
		}

		// Wait for all threads to complete
		for (auto& thread : threads) {
			thread.join();
		}

		// Verify reads
		EXPECT_EQ(successfulReads, NUM_DOCUMENTS)
			<< "Not all documents were read successfully";
		EXPECT_EQ(failedReads, 0)
			<< "Some document reads failed";
	}

	// Test querying with multiple threads
	void queryDocumentsMultiThreaded() {
		std::vector<std::thread> threads;
		std::atomic<int> successfulQueries{ 0 };
		std::atomic<int> failedQueries{ 0 };

		auto queryWorker = [this, &successfulQueries, &failedQueries]() {
			// Various query types to test
			std::vector<json> queries = {
				{{"$orderBy", {{"rating", "desc"}}}},
				{{"$gt", {{"price", 500.0}}}},
				{{"$eq", {{"category", "Electronics"}}}},
				{{"$lt", {{"stock", 100}}}},
				{{"$and", {{{"$gt", {{"price", 100.0}}}},{{"$lt", {{"price", 800.0}}}}}}},
				{{"$or", {{{"$eq", {{"category", "Books"}}}}, {{"$eq", {{"category", "Food"}}}}}}},
			};
			
			for (const auto& query : queries) {
				std::vector<std::string> docIds = products->findDocument(query);

				if (docIds.size()) {
					successfulQueries++;
				}
				else {
					failedQueries++;
				}
			}
		};

		// Create multiple query threads
		for (int t = 0; t < NUM_THREADS; ++t) {
			threads.emplace_back(queryWorker);
		}

		// Wait for all threads to complete
		for (auto& thread : threads) {
			thread.join();
		}

		// Verify queries
		EXPECT_GT(successfulQueries, 0)
			<< "No successful queries executed";
		EXPECT_EQ(failedQueries, 0)
			<< "Some queries failed";
	}

	void updateDocumentsMultiThreaded() {
		std::vector<std::thread> threads;
		std::atomic<int> successfulUpdates{ 0 };
		std::atomic<int> failedUpdates{ 0 };

		auto updateOperatorsWorker = [this, &successfulUpdates, &failedUpdates]() {
			// Comprehensive set of update operations covering supported operators
			std::vector<std::pair<std::string, json>> updateOperations = {
				// $set - Top-level fields
				{"$set_top_level", {
					{"$set", {
						{"price", 599.99},
						{"stock", 250}
					}}
				}},

				// $set - Nested fields using dot notation
				{"$set_nested", {
					{"$set", {
						{"metadata.updated_at", std::to_string(std::chrono::system_clock::now().time_since_epoch().count())},
					}}
				}}
			};

			// Perform updates on a subset of documents with upsert option
			for (int i = 0; i < NUM_DOCUMENTS; i += NUM_THREADS) {
				std::string docId = "prod_" + std::to_string(i);

				// Cycle through different update operations
				for (std::pair<std::string, json> updateOps : updateOperations) {
					std::string opName = updateOps.first;
					json updateOp = updateOps.second;
					// Alternate between normal update and upsert
					//bool upsert = (i % 2 == 0);
					Status status = products->updateDocument(docId, updateOp);

					if (status.ok()) {
						successfulUpdates++;
					}
					else {
						failedUpdates++;
						std::cerr << "Update failed for document " << docId
							<< " with operation " << opName
							<< ": " << status.message() << std::endl;
					}
				}
			}
			};

		// Create threads for concurrent updates
		for (int t = 0; t < NUM_THREADS; ++t) {
			threads.emplace_back(updateOperatorsWorker);
		}

		// Wait for all update threads to complete
		for (auto& thread : threads) {
			thread.join();
		}

		// Verification of update results
		EXPECT_GT(successfulUpdates, 0)
			<< "No successful updates were performed";

		EXPECT_EQ(failedUpdates, 0)
			<< "Some updates failed: " << failedUpdates << " failures";

		// In-depth verification of update operations
		for (int i = 0; i < NUM_DOCUMENTS; i += NUM_THREADS * 10) {
			std::string docId = "prod_" + std::to_string(i);
			Document doc;
			Status status = products->readDocument(docId, doc);

			ASSERT_TRUE(status.ok())
				<< "Failed to read document " << docId << " after updates";

			json docData = doc.data();

			// Verification checks for specific update scenarios
			//EXPECT_TRUE(docData.contains("metadata") || docData.contains("tags") || docData.contains("categories"))
				//<< "Document should have been modified by update operations";
			if (docData.contains("metadata")) {
				if (docData["metadata"].contains("updated_at")) {
					EXPECT_NE(docData["metadata"]["updated_at"].get<std::string>(), "");
				}
			}
			if (docData.contains("price")) {
				EXPECT_GE(docData["price"].get<double>(), 0.0);
				EXPECT_LE(docData["price"].get<double>(), 1000.0);
			}

			if (docData.contains("stock")) {
				EXPECT_GE(docData["stock"].get<int>(), 0);
				EXPECT_LE(docData["stock"].get<int>(), 500);
			}
		}

		// Performance measurement
		auto start = std::chrono::high_resolution_clock::now();
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

		std::cout << "Update Operators Stress Test Summary:" << std::endl;
		std::cout << "  Successful Updates: " << successfulUpdates << std::endl;
		std::cout << "  Failed Updates: " << failedUpdates << std::endl;
	}
};
// Actual test case
TEST_F(AnuDBStressTest, WriteReadFindUpdateStressTest) {
	// Assumes previous tests have already inserted documents
	auto startInsert = std::chrono::high_resolution_clock::now();
	insertDocumentsMultiThreaded();
	auto endInsert = std::chrono::high_resolution_clock::now();
	auto durationInsert = std::chrono::duration_cast<std::chrono::milliseconds>(endInsert - startInsert);
	std::cout << "Inserting documents took "
		<< durationInsert.count() << " ms" << std::endl;

	auto startRead = std::chrono::high_resolution_clock::now();
	readDocumentsMultiThreaded();
	auto endRead = std::chrono::high_resolution_clock::now();

	auto durationRead = std::chrono::duration_cast<std::chrono::milliseconds>(endRead - startRead);
	std::cout << "Reading documents took "
		<< durationRead.count() << " ms" << std::endl;

	auto startQuery = std::chrono::high_resolution_clock::now();
	queryDocumentsMultiThreaded();
	auto endQuery = std::chrono::high_resolution_clock::now();

	auto durationQuery = std::chrono::duration_cast<std::chrono::milliseconds>(endQuery - startQuery);
	std::cout << "Querying documents took "
		<< durationQuery.count() << " ms" << std::endl;

#if 0
	auto startUpdate = std::chrono::high_resolution_clock::now();
	updateDocumentsMultiThreaded();
	auto endUpdate = std::chrono::high_resolution_clock::now();
	auto durationUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(endUpdate - startUpdate);
	std::cout << "Updating documents took "
		<< durationUpdate.count() << " ms" << std::endl;
#endif
}

// Static member initialization
Database* AnuDBStressTest::db = nullptr;
Collection* AnuDBStressTest::products = nullptr;
std::string AnuDBStressTest::dbPath = "./stress_test_db";
int AnuDBStressTest::NUM_DOCUMENTS = 100000;
int AnuDBStressTest::NUM_THREADS = 8;
// Main function to run the tests
int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
