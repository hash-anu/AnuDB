#include "Document.h"

using namespace anudb;

const std::string& Document::id() const { return id_; }
void Document::setId(const std::string& id) { id_ = id; }

json& Document::data() { return data_; }
const json& Document::data() const { return data_; }
void Document::setData(const json& data) { data_ = data; }
void Document::setData(json&& data) { data_ = std::move(data); }

bool Document::hasField(const std::string& field) const {
    return data_.find(field) != data_.end();
}

void Document::applyUpdate(const json& update) {
    for (auto it = update.begin(); it != update.end(); it++) {
        const std::string& op = it.key();
        if (op == "$set") {
            const json& setOps = it.value();
            for (auto it = setOps.begin(); it != setOps.end(); it++) {
                std::string key = it.key();
                if (key.find('.') != std::string::npos) {
                    std::stringstream ss(key);
                    std::string token;
                    json* current = &data_;
                    std::vector<std::string> tokens;
                    while (std::getline(ss, token, '.')) {
                        tokens.push_back(token);
                    }
                    int i = 0;
                    for (i = 0; i < tokens.size() - 1; i++) {
                        token = tokens[i];
                        if (std::all_of(token.begin(), token.end(), ::isdigit) && current->is_array()) {
                            int index = std::stoi(token);
                            if (index >= 0 && index < current->size()) {
                                current = &(*current)[index];
                            }
                            else {
                                continue;
                            }
                        }
                        // Handle object property access
                        else if (current->is_object() && current->contains(token)) {
                            current = &(*current)[token];
                        }
                        // Key not found
                        else {
                            continue;
                        }
                    }
                    if (current->find(tokens[i]) != current->end())
                        (*current)[tokens[i]] = it.value();
                }
                else
                {
                    data_[key] = it.value();
                }
            }
        }
        else if (op == "$unset") {
            const json& unsetOps = it.value();
            for (auto it = unsetOps.begin(); it != unsetOps.end(); it++) {
                std::string key = it.key();
                if (key.find('.') != std::string::npos) {
                    std::stringstream ss(key);
                    std::string token;                    
                    std::vector<std::string> tokens;
                    while (std::getline(ss, token, '.')) {
                        tokens.push_back(token);
                    }
                    json* current = &data_;
                    int i = 0;
                    for (i = 0; i < tokens.size() - 1; i++) {
                        token = tokens[i];
                        if (std::all_of(token.begin(), token.end(), ::isdigit) && current->is_array()) {
                            int index = std::stoi(token);
                            if (index >= 0 && index < current->size()) {
                                current = &(*current)[index];
                            }
                            else {
                                break;
                            }
                        }
                        // Handle object property access
                        else if (current->is_object() && current->contains(token)) {
                            current = &(*current)[token];
                        }
                        // Key not found
                        else {
                            break;
                        }
                    }
                    if (i == tokens.size() - 1 && (current->find(tokens[i]) != current->end())) {
                        current->erase(tokens[i]);
                    }
                }
                else {
                    data_.erase(it.key());
                }
            }
        }
        else if (op == "$push") {
            // $push: Appends a value to an array
            const json& pushOps = it.value();
            for (auto it = pushOps.begin(); it != pushOps.end(); it++) {
                if (data_.find(it.key()) == data_.end()) {
                    data_[it.key()] =  it.value();
                }
                else if (data_[it.key()].is_array()) {
                    data_[it.key()].push_back(it.value());
                }
                else {
                    // Convert to array with original and new value
                    json original = data_[it.key()];
                    data_[it.key()] = json::array({ original, it.value()});
                }
            }
        }
        else if (op == "$pull") {
            // $pull: Removes all instances of a value from an array
            const json& pullOps = it.value();
            for (auto it = pullOps.begin(); it != pullOps.end(); it++) {
                if (data_.find(it.key()) != data_.end() && data_[it.key()].is_array()) {
                    json newArray = json::array();
                    for (const auto& item : data_[it.key()]) {
                        if (item != it.value()) {
                            newArray.push_back(item);
                        }
                    }
                    data_[it.key()] = newArray;
                }
                else if (data_.find(it.key()) != data_.end()) {
                    if (data_[it.key()] == it.value()) {
                        data_.erase(it.key());
                    }
                    else {
                        data_[it.key()] = it.value();
                    }
                }
            }
        }
    }
}

template<typename T>
T Document::getValue(const std::string& field) const {
    if (!hasField(field)) {
        throw std::runtime_error("Field not found: " + field);
    }
    return data_[field].get<T>();
}

void Document::setValue(const std::string& field, const json& value) {
    data_[field] = value;
}

std::string Document::toJson() const {
    return data_.dump();
}

Document Document::fromJson(const std::string& id, const std::string& jsonStr) {
    return Document(id, json::parse(jsonStr));
}

// Convert Document to MessagePack format
std::vector<uint8_t> Document::to_msgpack() const {
    return json::to_msgpack(json{ {"id", id_},  {"data", data_} });
}

// Deserialize from MessagePack format
Document Document::from_msgpack(const std::vector<uint8_t>& msgpack_data) {
    json j = json::from_msgpack(msgpack_data);
    return Document{ j["id"], j["data"] };
}