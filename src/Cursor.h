#ifndef CURSOR_H
#define CURSOR_H

#include "StorageEngine.h"
#include "Document.h"
#include <iostream>

namespace anudb {

    class Cursor {
    public:
        Cursor(const std::string& collectionName, StorageEngine* engine);

        // Check if the cursor points to a valid document
        bool isValid() const;

        // Move to the next document
        void next();

        // Get the current document
        Status current(Document* doc);

        // Get the ID of the current document
        std::string currentId();

        // Seek to a specific document by ID
        void seek(const std::string& id);

        // Reset to the beginning
        void reset();
    private:
        std::string collectionName_;
        StorageEngine* engine_;
        std::unique_ptr<rocksdb::Iterator> iterator_;
        bool valid_;
    };
};

#endif //CURSOR_H
