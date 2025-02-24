#include "Cursor.h"

using namespace anudb;

Cursor::Cursor(const std::string& collectionName, StorageEngine* engine)
    : collectionName_(collectionName), engine_(engine), valid_(false) {

    // Get the column family handle for the collection
    std::unordered_map<std::string, rocksdb::ColumnFamilyHandle*> it = engine_->getColumnFamilies();
    if (it.find(collectionName_) == it.end()) {
        return;
    }

    // Create iterator
    rocksdb::ReadOptions readOptions;
    iterator_.reset(engine_->getDB()->NewIterator(RocksDBOptimizer::getReadOptions(), it[collectionName_]));

    // Position at first key
    iterator_->SeekToFirst();
    valid_ = iterator_->Valid();
}

bool Cursor::isValid() const {
    return valid_;
}

void Cursor::next() {
    if (!valid_) return;

    iterator_->Next();
    valid_ = iterator_->Valid();
}

Status Cursor::current(Document* doc) {
    if (!valid_) {
        return Status::InvalidArgument("Invalid cursor position");
    }

    // Get key and value
    rocksdb::Slice keySlice = iterator_->key();
    rocksdb::Slice valueSlice = iterator_->value();

    // Convert to document
    std::vector<uint8_t> value(valueSlice.data(), valueSlice.data() + valueSlice.size());
    *doc = Document::from_msgpack(value);

    return Status::OK();
}

std::string Cursor::currentId() {
    if (!valid_) return "";

    // Get key
    rocksdb::Slice keySlice = iterator_->key();
    return std::string(keySlice.data(), keySlice.size());
}

void Cursor::seek(const std::string& id) {
    if (!iterator_) return;

    iterator_->Seek(rocksdb::Slice(id));
    valid_ = iterator_->Valid();
}

// Reset to the beginning
void Cursor::reset() {
    if (!iterator_) return;

    iterator_->SeekToFirst();
    valid_ = iterator_->Valid();
}