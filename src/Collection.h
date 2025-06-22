#ifndef COLLECTION_H
#define COLLECTION_H

#include "StorageEngine.h"
#include "Document.h"
#include "Cursor.h"
#ifdef _WIN32
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
extern "C" uintptr_t __cdecl _beginthreadex(void*, unsigned int,
	unsigned int(__stdcall*)(void*), void*, unsigned int, unsigned int*);
#endif

namespace anudb {
	// Collection class representing a MongoDB-like collection
	class Collection {
	public:
		Collection(const std::string& name, StorageEngine* engine)
			: name_(name), engine_(engine) {}

		const std::string& name() const { return name_; }

		// Create a document in the collection
		Status createDocument(Document& doc);

		// Delete a document from the collection
		Status deleteDocument(const std::string& id);

		// Get indexes
		Status getIndex(std::vector<std::string>& indexes) const;

		// Create an index
		Status createIndex(const std::string& index);

		// Remove an index
		Status deleteIndex(const std::string& index);

		// Create cursor for collection
		std::unique_ptr<Cursor> createCursor();

		// Read a document from the collection`-
		Status readDocument(const std::string& id, Document& doc);

		// Read all documents from the collection
		Status readAllDocuments(std::vector<Document>& docIds, uint64_t limit = 10);

		// Read all documents from the collection
		Status exportAllToJsonAsync(const std::string& exportPath);

		// Read all documents from the collection
		Status importFromJsonFile(const std::string& filePath);

		// Update document from the collection whose Id is matching
		Status updateDocument(const std::string& id, const json& update, bool upsert = false);

		// find document from the collection whose filter option is matchin
		std::vector<std::string> findDocument(const json& filterOption);

		void waitForExportOperation();

		~Collection();
	private:
		std::string name_;
		StorageEngine* engine_;
		std::thread export_thread_;
		std::string getIndexCfName(const std::string& index);
		// Simple ID generation
		std::string generateId() const;
		// Check index field exist
		bool hasIndexField(const json& doc, const std::string& field);
		// Insert doc id from index table
		Status insertIfIndexFieldExists(const Document& doc, const std::string& index);
		// Delete doc id from index table
		Status deleteIfIndexFieldExists(const Document& doc, const std::string& index);
		// parse value
		std::string parseValue(const json& val);

		Status findDocumentsUsingEq(const json& eqOps, std::set<std::string>& indexes, std::vector<std::string>& docIds);
		Status findDocumentsUsingGt(const json& gtOps, std::set<std::string>& indexes, std::vector<std::string>& docIds);
		Status findDocumentsUsingLt(const json& ltOps, std::set<std::string>& indexes, std::vector<std::string>& docIds);

		std::string encodeIntKey(int value);
		int64_t decodeIntKey(const std::string& encoded);
		std::string encodeDoubleKey(double value);
		double decodeDoubleKey(const std::string& encoded);
		std::mutex collection_mutex_;
	};

	// For threaded implementation
	class ExportTask {
	public:
		ExportTask(StorageEngine* engine, const std::string& collection_name,
			const std::string& output_path) :
			engine_(engine), collection_name_(collection_name), output_path_(output_path) {}

		void operator()() {
			Status s = engine_->exportAllToJson(collection_name_, output_path_);
			if (!s.ok()) {
				std::cerr << "Failed to export collection with : " << s.message() << std::endl;
				return;
			}
			std::cout << "Export complete for collection : " << collection_name_ << std::endl;
		}

	private:
		StorageEngine* engine_;
		std::string collection_name_;
		std::string output_path_;
	};

	class MemoryEfficientJsonParser {
	private:
		std::ifstream& file;
		std::vector<char> buffer;
		size_t bufferPos;
		size_t validSize;
		static const size_t BUFFER_SIZE = 8192; // 8KB chunks
		bool arrayStarted;
		bool arrayEnded;

	public:
		MemoryEfficientJsonParser(std::ifstream& f)
			: file(f), bufferPos(0), validSize(0), arrayStarted(false), arrayEnded(false) {
			buffer.resize(BUFFER_SIZE * 2); // Fixed 16KB buffer
		}

		~MemoryEfficientJsonParser() {
			// Explicit cleanup
			buffer.clear();
			buffer.shrink_to_fit();
		}

		// Fill buffer with new data, moving remaining data to beginning
		bool fillBuffer() {
			if (file.eof()) return false;

			// Move remaining data to beginning using efficient memmove
			if (bufferPos > 0 && bufferPos < validSize) {
				size_t remaining = validSize - bufferPos;
				std::memmove(&buffer[0], &buffer[bufferPos], remaining);
				validSize = remaining;
				bufferPos = 0;
			}
			else if (bufferPos >= validSize) {
				validSize = 0;
				bufferPos = 0;
			}

			// Read new data into available space
			size_t spaceAvailable = buffer.size() - validSize;
			if (spaceAvailable > 0) {
				file.read(&buffer[validSize], spaceAvailable);
				std::streamsize bytesRead = file.gcount();
				if (bytesRead > 0) {
					validSize += bytesRead;
					return true;
				}
			}
			return false;
		}

		// Skip whitespace characters
		void skipWhitespace() {
			while (bufferPos < validSize && std::isspace(buffer[bufferPos])) {
				bufferPos++;
			}

			// Refill buffer if we're near the end
			if (bufferPos >= validSize - 10 && !file.eof()) {
				fillBuffer();
				while (bufferPos < validSize && std::isspace(buffer[bufferPos])) {
					bufferPos++;
				}
			}
		}

		// Extract next JSON object from array
		std::string extractNextObject() {
			if (arrayEnded) return "";

			skipWhitespace();

			// Ensure we have data
			if (bufferPos >= validSize) {
				if (!fillBuffer()) return "";
				skipWhitespace();
			}

			if (bufferPos >= validSize) return "";

			char ch = buffer[bufferPos];

			// Handle array start
			if (!arrayStarted && ch == '[') {
				arrayStarted = true;
				bufferPos++;
				skipWhitespace();

				// Check for empty array
				if (bufferPos < validSize && buffer[bufferPos] == ']') {
					arrayEnded = true;
					return "";
				}
			}
			// Handle array end
			else if (ch == ']') {
				arrayEnded = true;
				return "";
			}
			// Handle comma separator
			else if (ch == ',') {
				bufferPos++;
				skipWhitespace();
			}

			// Verify we're in an array
			if (!arrayStarted) {
				return ""; // Not a valid JSON array
			}

			// Extract the JSON object
			return extractJsonObject();
		}

		bool isArrayStarted() const { return arrayStarted; }
		bool isArrayEnded() const { return arrayEnded; }

	private:
		// Extract a single JSON object using incremental building
		std::string extractJsonObject() {
			skipWhitespace();

			if (bufferPos >= validSize || buffer[bufferPos] != '{') {
				return ""; // Not a valid object start
			}

			// Use incremental result building to avoid large string copies
			std::string result;
			result.reserve(512); // Start with reasonable size

			size_t objectStart = bufferPos;
			int braceCount = 0;
			bool inString = false;
			bool escaped = false;

			while (true) {
				// Check if we need more data
				if (bufferPos >= validSize) {
					// Append current segment to result
					if (objectStart < validSize) {
						result.append(&buffer[objectStart], validSize - objectStart);
					}

					// Reset buffer and read more data
					bufferPos = 0;
					validSize = 0;
					objectStart = 0;

					if (!fillBuffer()) {
						break; // End of file
					}
				}

				char ch = buffer[bufferPos];

				// JSON parsing state machine
				if (!inString) {
					if (ch == '"') {
						inString = true;
					}
					else if (ch == '{') {
						braceCount++;
					}
					else if (ch == '}') {
						braceCount--;
						if (braceCount == 0) {
							// Complete object found
							bufferPos++; // Include closing brace

							// Append final segment
							result.append(&buffer[objectStart], bufferPos - objectStart);

							// Optimize memory usage
							result.shrink_to_fit();
							return result;
						}
					}
				}
				else {
					// Inside string
					if (escaped) {
						escaped = false;
					}
					else if (ch == '\\') {
						escaped = true;
					}
					else if (ch == '"') {
						inString = false;
					}
				}

				bufferPos++;
			}

			// Incomplete object (shouldn't happen with valid JSON)
			return "";
		}
	};
}
#endif // COLLECTION_H
