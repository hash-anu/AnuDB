#include "Cursor.h"

using namespace anudb;

Cursor::Cursor(const std::string& collectionName, StorageEngine* engine)
    : collectionName_(collectionName), engine_(engine), valid_(false) {

    std::unordered_map<std::string, rocksdb::ColumnFamilyHandle*> it = engine_->getColumnFamilies();
    if (it.find(collectionName_) == it.end()) {
        return;
    }

    rocksdb::ReadOptions readOptions;
    iterator_.reset(engine_->getDB()->NewIterator(RocksDBOptimizer::getReadOptions(), it[collectionName_]));
    iterator_->SeekToFirst();
    valid_ = iterator_->Valid();
}

bool Cursor::isValid() const {
    std::lock_guard<std::mutex> lock(cursor_mutex_);
    return valid_;
}

void Cursor::next() {
    std::lock_guard<std::mutex> lock(cursor_mutex_);
    if (!valid_) return;
    iterator_->Next();
    valid_ = iterator_->Valid();
}

Status Cursor::current(Document* doc) {
    std::lock_guard<std::mutex> lock(cursor_mutex_);
    if (!valid_) {
        return Status::InvalidArgument("Invalid cursor position");
    }

    rocksdb::Slice valueSlice = iterator_->value();
    std::vector<uint8_t> value(valueSlice.data(), valueSlice.data() + valueSlice.size());
    *doc = Document::from_msgpack(value);

    return Status::OK();
}

std::string Cursor::currentId() {
    std::lock_guard<std::mutex> lock(cursor_mutex_);
    if (!valid_) return "";

    rocksdb::Slice keySlice = iterator_->key();
    return std::string(keySlice.data(), keySlice.size());
}

void Cursor::seek(const std::string& id) {
    std::lock_guard<std::mutex> lock(cursor_mutex_);
    if (!iterator_) return;
    iterator_->Seek(rocksdb::Slice(id));
    valid_ = iterator_->Valid();
}

void Cursor::reset() {
    std::lock_guard<std::mutex> lock(cursor_mutex_);
    if (!iterator_) return;
    iterator_->SeekToFirst();
    valid_ = iterator_->Valid();
}
