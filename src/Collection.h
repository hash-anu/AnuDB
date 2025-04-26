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
#ifdef MAKE_UNIQUE
#include <memory>

	namespace std {
		template <typename T, typename... Args>
		std::unique_ptr<T> make_unique(Args&&... args) {
			return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
		}
	}
#endif
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
		Status exportAllToJsonAsync(const std::string &exportPath);

		// Read all documents from the collection
		Status importFromJsonFile(const std::string& filePath);

		// Update document from the collection whose Id is matching
		Status updateDocument(const std::string& id, const json &update, bool upsert = false);

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
}
#endif // COLLECTION_H
