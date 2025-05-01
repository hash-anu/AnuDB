#include "Collection.h"

using namespace anudb;

// Create a document in the collection
Status  Collection::createDocument(Document& doc) {
	// Check if document has an ID, if not generate one
	if (doc.id().empty()) {
		doc.setId(generateId());
	}

	// Add _id field to the JSON data if it doesn't exist
	if (!doc.hasField("_id")) {
		doc.setValue("_id", doc.id());
	}
	for (std::string index : engine_->getIndexNames(name_)) {
		if (hasIndexField(doc.data(), index)) {
			Status status = insertIfIndexFieldExists(doc, index);
			if (!status.ok()) {
				return status;
			}
		}
	}
	// Serialize the document
	auto serialized = doc.to_msgpack();
	// Store in the database
	return engine_->put(name_, doc.id(), serialized);
}

Status Collection::deleteDocument(const std::string& id) {
	Document doc;
	Status status = readDocument(id, doc);
	if (!status.ok()) {
		std::cerr << "Unable to read document for id : " << id << " " << status.message() << std::endl;
		return status;
	}
	for (std::string index : engine_->getIndexNames(name_)) {
		if (hasIndexField(doc.data(), index)) {
			Status status = deleteIfIndexFieldExists(doc, index);
			if (!status.ok()) {
				return status;
			}
		}
	}
	return engine_->remove(name_, id);
}

std::string Collection::getIndexCfName(const std::string& index) {
	return (name() + "__index__" + index);
}

Status Collection::getIndex(std::vector<std::string>& indexList) const {
	std::set<std::string> indexes = engine_->getIndexNames(name_);
	for (std::string index : indexes) {
		indexList.push_back(index);
	}
	return Status::OK();
}

Status Collection::createIndex(const std::string& index) {
	Status status = engine_->createCollection(getIndexCfName(index));
	try {
		auto cursor = createCursor();
		while (cursor->isValid()) {
			Document doc;
			Status status = cursor->current(&doc);

			if (status.ok()) {
				if (hasIndexField(doc.data(), index)) {
					Status status = insertIfIndexFieldExists(doc, index);
					if (!status.ok()) {
						return status;
					}
				}
			}
			else {
				std::cerr << "Error reading document: " << status.message() << std::endl;
				return status;
			}

			cursor->next();
		}
	}
	catch (const std::exception& e) {
		return Status::Corruption("Failed to deserialize document: " + std::string(e.what()));
	}
	std::cout << "Index created successfully!!!\n";
	return Status::OK();
}

// Remove an index
Status Collection::deleteIndex(const std::string& index) {
	return engine_->dropCollection(getIndexCfName(index));
}

// Read a document from the collection
Status Collection::readDocument(const std::string& id, Document& doc) {
	std::vector<uint8_t> serialized;
	Status status = engine_->get(name_, id, &serialized);

	if (!status.ok()) {
		return status;
	}

	try {
		doc = Document::from_msgpack(serialized);
		return Status::OK();
	}
	catch (const std::exception& e) {
		return Status::Corruption("Failed to deserialize document: " + std::string(e.what()));
	}
}

std::unique_ptr<Cursor> Collection::createCursor() {
	return std::make_unique<Cursor>(name_, engine_);
}

// Read all documents from the collection
Status Collection::readAllDocuments(std::vector<Document>& docIds, uint64_t limit) {
	auto cursor = createCursor();
	uint64_t cnt = 0;
	try {
		while (cursor->isValid() && cnt < limit) {
			Document doc;
			Status status = cursor->current(&doc);

			if (status.ok()) {
				docIds.push_back(doc);
			}
			else {
				std::cerr << "Error reading document: " << status.message() << std::endl;
			}
			cnt++;
			cursor->next();
		}
	}
	catch (const std::exception& e) {
		return Status::Corruption("Failed to deserialize document: " + std::string(e.what()));
	}
	return Status::OK();
}

std::string Collection::encodeIntKey(int value) {
	// Add offset to make all values positive
	uint64_t shifted = value + INT64_MAX + 1;

	// Convert to big-endian byte order (network byte order)
	char bytes[8];
	bytes[0] = (shifted >> 56) & 0xFF;
	bytes[1] = (shifted >> 48) & 0xFF;
	bytes[2] = (shifted >> 40) & 0xFF;
	bytes[3] = (shifted >> 32) & 0xFF;
	bytes[4] = (shifted >> 24) & 0xFF;
	bytes[5] = (shifted >> 16) & 0xFF;
	bytes[6] = (shifted >> 8) & 0xFF;
	bytes[7] = shifted & 0xFF;

	return std::string(bytes, 8);
}

std::string Collection::encodeDoubleKey(double value) {
	union {
		double d;
		uint64_t i;
	} converter;

	converter.d = value;
	uint64_t bits = converter.i;

	// Handle sign bit: flip all bits if it's negative
	if (bits & 0x8000000000000000) {
		// For negative numbers, flip all bits
		bits = ~bits;
	}
	else {
		// For positive numbers, flip the sign bit
		bits |= 0x8000000000000000;
	}

	// Convert to big-endian byte order
	unsigned char bytes[8];
	for (int i = 0; i < 8; i++) {
		bytes[i] = (bits >> (56 - i * 8)) & 0xFF;
	}

	return std::string(reinterpret_cast<char*>(bytes), 8);
}

double Collection::decodeDoubleKey(const std::string& encoded) {
	assert(encoded.size() == 8);

	// Convert from big-endian bytes to uint64_t
	uint64_t value = 0;
	for (int i = 0; i < 8; i++) {
		value = (value << 8) | (unsigned char)(encoded[i]);
	}

	// Check the sign bit and reverse the transformation
	if ((value & (1ULL << 63)) == 0) {
		// Was negative, flip bits back
		value = ~value;
	}
	else {
		// Was positive, clear the sign bit
		value = value & ~(1ULL << 63);
	}

	// Convert back to double
	double result;
	memcpy(&result, &value, sizeof(double));
	return result;
}

int64_t Collection::decodeIntKey(const std::string& encoded) {
	assert(encoded.size() == 8);

	// Convert from big-endian bytes to integer
	uint64_t value = 0;
	for (int i = 0; i < 8; i++) {
		value = (value << 8) | (unsigned char)(encoded[i]);
	}

	// Flip the sign bit back
	int64_t result = value ^ (1ULL << 63);
	return result;
}

std::string Collection::parseValue(const json& value) {
	// Handle different value types
	if (value.is_string()) {
		return value.get<std::string>();
	}
	else if (value.is_number_integer()) {
		return encodeIntKey(value.get<int>());
	}
	else if (value.is_number_float()) {
		return encodeDoubleKey(value.get<double>());
	}
	else if (value.is_boolean()) {
		return (value.get<bool>() ? "true" : "false");
	}
	else if (value.is_null()) {
		return "";
	}
	else if (value.is_object() || value.is_array()) {
		return value;
	}
	return "";
}

Status Collection::findDocumentsUsingEq(const json& eqOps, std::set<std::string>& indexes, std::vector<std::string>& docIds) {
	std::string key = eqOps.begin().key();
	std::string value = parseValue(eqOps.begin().value());

	if (indexes.count(key) == 0) {
		return Status::InvalidArgument("Specified key is not indexed, please create index for " + key);
	}
	value += "#";
	return engine_->fetchDocIdsForEqual(getIndexCfName(key), value, docIds);
}

Status Collection::findDocumentsUsingLt(const json& ltOps, std::set<std::string>& indexes, std::vector<std::string>& docIds) {
	std::string key = ltOps.begin().key();
	std::string value = parseValue(ltOps.begin().value());
	if (value == "") {
		return Status::InvalidArgument("Unable to parse value of operator..");
	}
	value += "#";
	return engine_->fetchDocIdsForLesser(getIndexCfName(key), value, docIds);
}

Status Collection::findDocumentsUsingGt(const json& gtOps, std::set<std::string>& indexes, std::vector<std::string>& docIds) {
	std::string key = gtOps.begin().key();
	std::string value = parseValue(gtOps.begin().value());
	if (value == "") {
		return Status::InvalidArgument("Unable to parse value of operator..");
	}
	value += "#";
	return engine_->fetchDocIdsForGreater(getIndexCfName(key), value, docIds);
}

std::vector<std::string> Collection::findDocument(const json& filterOption) {
	std::vector<std::string> docIds;
	Status status;
	std::set<std::string> indexes = engine_->getIndexNames(name_);
	for (auto it = filterOption.begin(); it != filterOption.end(); it++) {
		const std::string& op = it.key();
		if (op == "$eq") {
			const json& eqOps = it.value();
			status = findDocumentsUsingEq(eqOps, indexes, docIds);
			if (!status.ok()) {
				std::cerr << "Error while finding doc:" << status.message() << std::endl;
			}
		}
		else if (op == "$gt") {
			const json& gtOps = it.value();
			status = findDocumentsUsingGt(gtOps, indexes, docIds);
			if (!status.ok()) {
				std::cerr << "Error while finding doc:" << status.message() << std::endl;
			}
		}
		else if (op == "$lt") {
			const json& ltOps = it.value();
			status = findDocumentsUsingLt(ltOps, indexes, docIds);
			if (!status.ok()) {
				std::cerr << "Error while finding doc:" << status.message() << std::endl;
			}
		}
		else if (op == "$and") {
			const json& andOps = it.value();
			std::unordered_set<std::string> andDocIds;
			if (andOps.is_array()) {
				for (json::const_iterator it = andOps.begin(); it != andOps.end(); ++it) {
					const json& item = *it;
					for (auto element = item.begin(); element != item.end(); element++) {
						std::vector<std::string> docIds;
						std::string ops = element.key().data();
						if (ops == "$eq") {
							status = findDocumentsUsingEq(element.value(), indexes, docIds);
						}
						else if (ops == "$gt") {
							status = findDocumentsUsingGt(element.value(), indexes, docIds);
						}
						else if (ops == "$lt") {
							status = findDocumentsUsingLt(element.value(), indexes, docIds);
						}
						else {
							status = Status::InvalidArgument("Not supported operator is passed");
						}
						if (!status.ok()) {
							std::cerr << "Error while finding doc:" << status.message() << std::endl;
							return {};
						}

						if (andDocIds.empty()) {
							andDocIds.insert(docIds.begin(), docIds.end());
						}
						else {
							for (std::string docId : docIds) {
								if (andDocIds.count(docId) == 0) {
									andDocIds.erase(docId);
								}
							}
							std::unordered_set<std::string> tmpDoc1 = andDocIds;
							std::unordered_set<std::string> tmpDoc2(docIds.begin(), docIds.end());
							for (auto it = tmpDoc1.begin(); it != tmpDoc1.end(); it++) {
								if (tmpDoc2.count(*it) == 0) {
									andDocIds.erase(*it);
								}
							}
						}
					}
				}
			}
			for (auto it = andDocIds.begin(); it != andDocIds.end(); it++) {
				docIds.push_back(*it);
			}
		}
		else if (op == "$or") {
			const json& orOps = it.value();
			std::unordered_set<std::string> orDocIds;
			if (orOps.is_array()) {
				for (json::const_iterator it = orOps.begin(); it != orOps.end(); ++it) {
					const json& item = *it;
					std::vector<std::string> docIds;
					for (auto element = item.begin(); element != item.end(); element++) {
						std::vector<std::string> docIds;
						std::string ops = element.key().data();
						if (ops == "$eq") {
							status = findDocumentsUsingEq(element.value(), indexes, docIds);
						}
						else if (ops == "$gt") {
							status = findDocumentsUsingGt(element.value(), indexes, docIds);
						}
						else if (ops == "$lt") {
							status = findDocumentsUsingLt(element.value(), indexes, docIds);
						}
						else {
							status = Status::InvalidArgument("Not supported operator is passed");
						}
						if (!status.ok()) {
							std::cerr << "Error while finding doc:" << status.message() << std::endl;
							return {};
						}
						if (orDocIds.empty()) {
							orDocIds.insert(docIds.begin(), docIds.end());
						}
						else {
							for (std::string docId : docIds) {
								orDocIds.insert(docId);
							}
						}
					}
				}
			}
			for (auto it = orDocIds.begin(); it != orDocIds.end(); it++) {
				docIds.push_back(*it);
			}
		}
		else if (op == "$orderBy") {
			const json& orderbyOps = it.value();
			std::string key = orderbyOps.begin().key();
			std::string value = orderbyOps.begin().value();
			if (value == "") {
				std::cerr << "Unable to parse value of operator..";
			}
			status = engine_->fetchDocIdsByOrder(getIndexCfName(key), value, docIds);
			if (!status.ok()) {
				std::cerr << "Error while finding doc:" << status.message() << std::endl;
			}
		}
	}
	return docIds;
}

Status Collection::updateDocument(const std::string& id, const nlohmann::json& update, bool upsert) {
	std::lock_guard<std::mutex> lock(collection_mutex_);
	Document doc;
	Status status = readDocument(id, doc);

	if (status.isNotFound()) {
		if (upsert) {
			doc = Document(id, json::object());
			doc.setValue("_id", id);
		}
		else {
			return status;
		}
	}
	else if (!status.ok()) {
		std::cerr << "Failed to read doc:: " << status.message() << std::endl;
		return status;
	}
	doc.applyUpdate(update);

	return createDocument(doc);
}

Status Collection::insertIfIndexFieldExists(const Document& doc, const std::string& index) {
	// if index column has any values then add its entry in table
	std::stringstream key;
	key << parseValue(doc.data()[index]) << "#" << doc.id();
	return engine_->putIndex(getIndexCfName(index), key.str(), doc.id());
}

Status Collection::deleteIfIndexFieldExists(const Document& doc, const std::string& index) {
	// if index column has any values then add its entry in table
	std::stringstream key;
	key << parseValue(doc.data()[index]) << "#" << doc.id();
	return engine_->remove(getIndexCfName(index), key.str());
}

Status Collection::importFromJsonFile(const std::string& filePath) {
	try
	{
		std::ifstream file(filePath);
		if (!file.is_open()) {
			return Status::IOError("Could not open file: " + filePath);
		}

		json jsonData;
		file >> jsonData;

		// Check if the loaded JSON is an array
		if (!jsonData.is_array()) {
			return Status::NotSupported("File must contain a JSON array of objects: " + filePath);
		}

		int successCount = 0;
		int failureCount = 0;

		// Iterate through array of objects
		for (const auto& item : jsonData) {
			// Ensure each item is an object
			if (!item.is_object()) {
				failureCount++;
				std::cerr << "Skipping non-object item in array" << std::endl;
				continue;
			}

			// Generate a unique document ID
			std::string docId;
			if (item.contains("_id")) {
				docId = item["_id"].get<std::string>();
			}
			else {
				// Generate a unique ID if not provided
				docId = "doc_" + std::to_string(successCount + failureCount);
			}
			Document doc(docId, item);
			Status status = createDocument(doc);

			if (status.ok()) {
				successCount++;
			}
			else {
				failureCount++;
				std::cerr << "Failed to import document " << docId << ": "
					<< status.message() << std::endl;
			}
		}

		std::cout << "JSON Array Import Summary:\n"
			<< "File: " << filePath << "\n"
			<< "Successfully imported: " << successCount << " documents\n"
			<< "Failed to import: " << failureCount << " documents" << std::endl;

		return Status::OK();
	}
	catch (const std::exception& e) {
		return Status::InternalError("JSON import error: " + std::string(e.what()));
	}
}

Status Collection::exportAllToJsonAsync(const std::string& exportPath) {
	ExportTask task(engine_, name_, exportPath);
	export_thread_ = std::thread(task);
	return Status::OK();
}

bool Collection::hasIndexField(const json& doc, const std::string& field) {
	return doc.contains(field);
}

// Simple ID generation
std::string Collection::generateId() const {
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	std::string id;
	id.reserve(12);

	// Add timestamp component (4 bytes)
	uint32_t timestamp = static_cast<uint32_t>(time(nullptr));
	for (int i = 0; i < 8; i++) {
		id += alphanum[timestamp % sizeof(alphanum)];
		timestamp /= sizeof(alphanum);
	}

	// Add random component (remaining bytes)
	for (int i = 8; i < 12; i++) {
		id += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return id;
}

void Collection::waitForExportOperation() {
	if (export_thread_.joinable()) {
		export_thread_.join();
	}
}

Collection::~Collection() {
	for (std::string index : engine_->getIndexNames(name_)) {
		deleteIndex(index);
	}
	waitForExportOperation();
}
