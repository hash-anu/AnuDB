#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "json.hpp"
#include <string>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

namespace anudb {
    // Document class representing JSON documents stored in the database
    class Document {
    public:
        Document() {}
        Document(const std::string& id, const json& data) : id_(id), data_(data) {}
        Document(const std::string& id, json&& data) : id_(id), data_(std::move(data)) {}

        const std::string& id() const;
        void setId(const std::string& id);

        json& data();
        const json& data() const;
        void setData(const json& data);
        void setData(json&& data);
        void applyUpdate(const nlohmann::json& update);

        bool hasField(const std::string& field) const;

        template<typename T>
        T getValue(const std::string& field) const;

        void setValue(const std::string& field, const json& value);

        std::string toJson() const;

        static Document fromJson(const std::string& id, const std::string& jsonStr);

        // Convert Document to MessagePack format
        std::vector<uint8_t> to_msgpack() const;

        // Deserialize from MessagePack format
        static Document from_msgpack(const std::vector<uint8_t>& msgpack_data);

    private:
        std::string id_;
        json data_;
    };
}

#endif // DOCUMENT_H