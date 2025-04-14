#define _CRT_DECLARE_NONSTDC_NAMES 1
#include <nng/mqtt/mqtt_client.h>
#include <nng/nng.h>
#include <Database.h>
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#ifdef _WIN32
#include <winsock2.h>
#include <process.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <mutex>
#include <condition_variable>
#include <json.hpp>
#include <signal.h>
#include <thread>
#include <chrono>
#define CONCURRENT_THREADS 32
#define ANUDB_REQUEST_TOPIC "anudb/request"
#define ANUDB_RESPONSE_TOPIC "anudb/response/"

// Forward declarations for AnuDB types
using namespace anudb;
using json = nlohmann::json;

class AnuDBMqttClient {
public:
    AnuDBMqttClient(const std::string& broker_url, const std::string& client_id, Database* db)
        : broker_url_(broker_url), client_id_(client_id), db_(db), running_(false) {}

    ~AnuDBMqttClient() {
        stop();
    }

    enum State { INIT, RECV, WAIT, SEND };

    struct Work {
        enum class State { INIT, RECV, WAIT, SEND };
        Work() {
            reply = NULL;
        }

        State state = State::INIT;
        nng_aio* aio = nullptr;
        nng_msg* msg = nullptr;
        nng_ctx ctx;
        AnuDBMqttClient* client;  // Pointer to client instance
        std::string* requestid;
        std::string* reply;
        ~Work() {
        }
    };

    static void client_cb(void* arg) {
        struct Work* work = static_cast<Work*>(arg);
        nng_msg* msg = nullptr;
        int rv;

        switch (work->state) {

        case Work::State::INIT:
            work->state = Work::State::RECV;
            nng_ctx_recv(work->ctx, work->aio);
            break;

        case Work::State::RECV:
            if ((rv = nng_aio_result(work->aio)) != 0) {
                std::cerr << "nng_recv_aio failed: " << nng_strerror(rv) << std::endl;
                work->state = Work::State::RECV;
                nng_ctx_recv(work->ctx, work->aio);
                break;
            }
            work->msg = nng_aio_get_msg(work->aio);
            work->state = Work::State::WAIT;
            nng_sleep_aio(0, work->aio);
            break;

        case Work::State::WAIT: {
            msg = work->msg;

            uint32_t topic_len = 0;
            uint32_t payload_len = 0;
            uint8_t* payload = nng_mqtt_msg_get_publish_payload(msg, &payload_len);
            const char* recv_topic = nng_mqtt_msg_get_publish_topic(msg, &topic_len);
            std::string topic(recv_topic, topic_len);
            std::string payload_str(reinterpret_cast<char*>(payload), payload_len);
            
            if (payload_len == 0 || payload_str.empty()) {
                std::cout << "Empty payload received on topic '" << topic << "', skipping...\n";
                nng_msg_free(msg);
                work->msg = nullptr;
                work->state = Work::State::RECV;
                nng_ctx_recv(work->ctx, work->aio);
                return;
            }
            std::cout << "RECV: '" << std::string(reinterpret_cast<char*>(payload), payload_len)
                << "' FROM: '" << std::string(recv_topic, topic_len) << "'" << std::endl; 
            
            std::string ret = work->client->handle_request(work, topic, payload_str);
            if (ret != "") {
                work->reply = new std::string(ret);
            }
            else {
                nng_msg_free(msg);
                work->msg = nullptr;
                work->state = Work::State::RECV;
                nng_ctx_recv(work->ctx, work->aio);
                return;
            }

            nng_msg_free(work->msg);
            work->msg = nullptr;

            work->state = Work::State::SEND;
            nng_sleep_aio(0, work->aio);  // schedule next step
            break;
        }

        case Work::State::SEND:
            if ((rv = nng_aio_result(work->aio)) != 0) {
                std::cerr << "nng_send_aio failed: " << nng_strerror(rv) << std::endl;
                work->state = Work::State::RECV;
                nng_ctx_recv(work->ctx, work->aio);
                break;
            }
            
            if (work->reply != NULL) {
                nng_msg* out_msg = nullptr;
                if ((rv = nng_mqtt_msg_alloc(&out_msg, 0)) != 0) {
                    std::cerr << "nng_msg_alloc failed: " << nng_strerror(rv) << std::endl;
                    work->state = Work::State::RECV;
                    nng_ctx_recv(work->ctx, work->aio);
                    break;
                }
                nng_mqtt_msg_set_packet_type(out_msg, NNG_MQTT_PUBLISH);
                nng_mqtt_msg_set_publish_topic(out_msg, (*(work->requestid)).c_str());
                nng_mqtt_msg_set_publish_payload(out_msg, (uint8_t*)work->reply->c_str(), work->reply->length());
                nng_aio_set_msg(work->aio, out_msg);
                std::cout << "Sending response to this topic:" << *(work->requestid) << std::endl;
                delete work->reply;
                work->reply = NULL;
                delete work->requestid;
                work->requestid = NULL;
                nng_ctx_send(work->ctx, work->aio);
            }
            else {
                if (work->reply != NULL) {
                    delete work->reply;
                    work->reply = NULL;
                }

                work->state = Work::State::RECV;
                nng_ctx_recv(work->ctx, work->aio);
            }
            break;

        default:
            std::cerr << "Fatal: Invalid state." << std::endl;
            std::exit(1);
        }
    }

    Work* alloc_work()
    {
        Work* work = nullptr;

        if ((work = (Work*)nng_alloc(sizeof(*work))) == nullptr) {
            std::cerr << "nng_alloc: " << nng_strerror(NNG_ENOMEM) << std::endl;
            return nullptr;
        }

        if (nng_aio_alloc(&work->aio, client_cb, work) != 0) {
            std::cerr << "nng_aio_alloc failed\n";
            delete work;
            return nullptr;
        }

        if (nng_ctx_open(&work->ctx, client_) != 0) {
            std::cerr << "nng_ctx_open failed\n";
            nng_aio_free(work->aio);
            delete work;
            return nullptr;
        }
        work->reply = NULL;
        work->client = this;
        work->state = Work::State::INIT;
        workers_.push_back(work);
        return work;
    }

    bool start() {
        if (running_) return true;

        Status status = db_->open();
        if (!status.ok()) {
            std::cerr << "Failed to open database: " << status.message() << std::endl;
            return 1;
        }
        nng_dialer dialer;
        int rv;
        struct Work* works[CONCURRENT_THREADS];

        if ((rv = nng_mqtt_client_open(&client_)) != 0) {
            std::cerr << "nng_socket: " << nng_strerror(rv) << std::endl;
            return false;
        }

        for (int i = 0; i < CONCURRENT_THREADS; i++) {
            works[i] = alloc_work();
        }

        nng_msg* msg;
        nng_mqtt_msg_alloc(&msg, 0);
        nng_mqtt_msg_set_packet_type(msg, NNG_MQTT_CONNECT);
        nng_mqtt_msg_set_connect_keep_alive(msg, 60);
        nng_mqtt_msg_set_connect_clean_session(msg, false);
        nng_mqtt_msg_set_connect_proto_version(msg, MQTT_PROTOCOL_VERSION_v311);
        nng_mqtt_set_connect_cb(client_, connect_cb, &client_);

        if ((rv = nng_dialer_create(&dialer, client_, broker_url_.c_str())) != 0) {
            std::cerr << "Failed to create dialer: " << nng_strerror(rv) << std::endl;
            return false;
        }

        nng_dialer_set_ptr(dialer, NNG_OPT_MQTT_CONNMSG, msg);
        if ((rv = nng_dialer_start(dialer, NNG_FLAG_ALLOC)) != 0) {
            std::cerr << "Failed to start dialer: " << nng_strerror(rv) << std::endl;
            return false;
        }

        for (int i = 0; i < CONCURRENT_THREADS; i++) {
            client_cb(works[i]);
        }
        running_ = true;
        return true;
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        // Signal workers to stop gracefully
        collMap_.clear();
        Status status = db_->close();
        if (!status.ok()) {
            std::cerr << "Failed to close database: " << status.message() << std::endl;
        }
        std::cout << "Closing in progress..\n";
        // Stop receiving/sending new work
        for (auto* work : workers_) {
            if (work) {
                if (work->aio) {
                    nng_aio_stop(work->aio);  // request cancel, non-blocking
                }
            }
        }
        // Let in-flight callbacks safely return
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        for (auto* work : workers_) {
            if (work) {
                if (work->aio) {
                    nng_aio_free(work->aio);
                    work->aio = nullptr;
                }
                nng_ctx_close(work->ctx);  // now safe
            }
        }
        workers_.clear();

        std::cout << "Client shutdown complete.\n";
        std::cout << "MQTT client stopped" << std::endl;
    }

private:
    static void connect_cb(nng_pipe pipe, nng_pipe_ev ev, void* arg) {
        std::string request_topic = ANUDB_REQUEST_TOPIC;
        if (ev == NNG_PIPE_EV_ADD_POST) {
            nng_socket* sock = (nng_socket*)arg;
            nng_mqtt_topic_qos subscriptions[1];
            subscriptions[0].qos = 1;
            subscriptions[0].topic.buf = (uint8_t*)request_topic.c_str();
            subscriptions[0].topic.length = request_topic.length();

            nng_mqtt_topic_qos* topics_qos = nng_mqtt_topic_qos_array_create(1);
            nng_mqtt_topic_qos_array_set(topics_qos, 0, (char*)subscriptions[0].topic.buf, subscriptions[0].qos, 1, 0, 0);

            nng_mqtt_subscribe(*sock, topics_qos, 1, NULL);
            nng_mqtt_topic_qos_array_free(topics_qos, 1);

            std::cout << "Subscribed to topic: " << request_topic << std::endl;
        }
    }

    void handle_create_collection(json& req, json& resp) {
        try {
            std::string collectionName = req["collection_name"];
            if (db_) {
                Status status = db_->createCollection(collectionName);
                if (!status.ok()) {
                    resp["status"] = "error while creating collection";
                    resp["message"] = status.message();
                }
                else {
                    collMap_[collectionName] = db_->getCollection(collectionName);
                    resp["status"] = "success";
                    resp["message"] = collectionName + " collection created successfully in AnuDB.";
                }
            }
        }
        catch (const std::exception & e) {
            resp["status"] = "error";
            resp["message"] = std::string("Exception: ") + e.what();
        }
    }
    void handle_delete_collection(json& req, json& resp) {
        try {
            std::string collectionName = req["collection_name"];
            if (db_) {
                Status status = db_->dropCollection(collectionName);
                if (!status.ok()) {
                    resp["status"] = "error while deleting collection";
                    resp["message"] = status.message();
                }
                else {
                    collMap_.erase(collectionName);
                    resp["status"] = "success";
                    resp["message"] = collectionName + " collection deleted successfully in AnuDB.";
                }
            }
        }
        catch (const std::exception& e) {
            resp["status"] = "error";
            resp["message"] = std::string("Exception: ") + e.what();
        }
    }

    void handle_create_document(json& req, json& resp) {
        try {
            std::string collectionName = req["collection_name"];
            if (db_) {
                if (collMap_.count(collectionName) == 0) {
                    Collection* coll = db_->getCollection(collectionName);
                    if (coll != NULL) {
                        collMap_[collectionName] = coll;
                    }
                    else {
                        resp["status"] = "error";
                        resp["message"] = "Collection :" + collectionName + " is not found";
                        return;
                    }
                }
                Collection* coll = collMap_[collectionName];
                std::string docId = "";
                if (req.contains("document_id")) {
                    docId = req["document_id"];
                }
                json data = req["content"];
                Document doc(docId, data);
                Status status = coll->createDocument(doc);
                if (!status.ok()) {
                    resp["status"] = "error while adding document in collection " + collectionName;
                    resp["message"] = status.message();
                    return;
                }
                resp["status"] = "success";
                resp["docId"] = doc.id();
                resp["message"] = "Document added in collection " + collectionName;
            }
        }
        catch (const std::exception& e) {
            resp["status"] = "error";
            resp["message"] = std::string("Exception: ") + e.what();
        }
    }

    void send_response(std::string& payload, Work* work, std::string& response_topic) {
        std::lock_guard<std::mutex> lock(mtx_);
        nng_msg* out_msg;
        nng_mqtt_msg_alloc(&out_msg, 0);

        nng_mqtt_msg_set_packet_type(out_msg, NNG_MQTT_PUBLISH);
        nng_mqtt_msg_set_publish_topic(out_msg, response_topic.c_str());
        nng_mqtt_msg_set_publish_payload(out_msg, (uint8_t*)payload.data(), payload.size());

        nng_aio* aio;
        nng_aio_alloc(&aio, nullptr, nullptr);  // callback owns and frees it
        nng_aio_set_msg(aio, out_msg);
        nng_ctx_send(work->ctx, aio);  // send message

        //wait for document to send
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        nng_msg_free(out_msg);
        nng_aio_free(aio);
    }
    void handle_get_collections(json& req, json& resp) {
        if (db_) {
            std::string collections = "";
            for (std::string& name : db_->getCollectionNames()) {
                collections += (name + ",");
            }
            resp["Collections"] = collections;
        }
	}
	void handle_get_indexes(json& req, json& resp) {
		try {
			std::string collectionName = req["collection_name"];
			if (db_) {
				if (collMap_.count(collectionName) == 0) {

					Collection* coll = db_->getCollection(collectionName);
					if (coll != NULL) {
						collMap_[collectionName] = coll;
					}
					else {
						resp["status"] = "error";
						resp["message"] = "Collection :" + collectionName + " is not found";
						return;
					}
				}
				Collection* coll = collMap_[collectionName];
				std::vector<std::string> indexes;
				Status status = coll->getIndex(indexes);
				std::string indexList = "";
				for (std::string& name : indexes) {
					indexList += (name + ",");
				}
				resp["collection"] = collectionName;
				resp["indexList"] = indexList;
			}
		}
		catch (const std::exception& e) {
			resp["status"] = "error";
			resp["message"] = std::string("Exception: ") + e.what();
		}
	}
    void handle_read_document(json& req, json& resp, Work* work, std::string& response_topic) {
        try {
            std::string collectionName = req["collection_name"];
            if (db_) {
                if (collMap_.count(collectionName) == 0) {
                    
                    Collection* coll = db_->getCollection(collectionName);
                    if (coll != NULL) {
                        collMap_[collectionName] = coll;
                    }
                    else {
                        resp["status"] = "error";
                        resp["message"] = "Collection :" + collectionName + " is not found";
                        return;
                    }
                }
                Collection* coll = collMap_[collectionName];
                std::string docId = "";
                if (req.contains("document_id")) {
                    docId = req["document_id"];
                    Document doc;
                    Status status = coll->readDocument(docId, doc);
					if (!status.ok()) {
						resp["status"] = "failed to read document";
						resp["message"] = status.message();
						return;
					}
                    resp = doc.data();
					return;
				}
				else {
                    uint32_t limit = UINT32_MAX;
                    if (req.contains("limit")) {
                        limit = req["limit"];
                    }
                    auto cursor = coll->createCursor();
                    uint64_t cnt = 0;
                    while (cursor->isValid() && cnt < limit) {
                        Document doc;
                        Status status = cursor->current(&doc);

                        if (status.ok()) {
                            std::string tmp = (std::string)doc.data().dump();
                            send_response(tmp, work, response_topic);
                        }
                        else {
                            std::cerr << "Error reading document: " << status.message() << std::endl;
                        }
                        cnt++;
                        cursor->next();
                    }
				}
			}
        }
        catch (const std::exception& e) {
            resp["status"] = "error";
            resp["message"] = std::string("Exception: ") + e.what();
        }
    }
    void handle_delete_document(json& req, json& resp) {
        try {
            std::string collectionName = req["collection_name"];
            if (db_) {
                if (collMap_.count(collectionName) == 0) {
                    Collection* coll = db_->getCollection(collectionName);
                    if (coll != NULL) {
                        collMap_[collectionName] = coll;
                    }
                    else {
                        resp["status"] = "error";
                        resp["message"] = "Collection :" + collectionName + " is not found";
                        return;
                    }
                }
                Collection* coll = collMap_[collectionName];
                if (req.contains("document_id")) {
                    std::string docId = req["document_id"];
                    Status status = coll->deleteDocument(docId);
                    if (!status.ok()) {
                        resp["status"] = "error while deleting document in collection " + collectionName;
                        resp["message"] = status.message();
                        return;
                    }
                    resp["status"] = "success";
                    resp["docId"] = docId;
                    resp["message"] = "Document deleted from collection " + collectionName;
				}
			}
		}
		catch (const std::exception& e) {
			resp["status"] = "error";
			resp["message"] = std::string("Exception: ") + e.what();
		}
	}

	void handle_create_index(json& req, json& resp) {
		try {
			std::string collectionName = req["collection_name"];
			if (db_) {
				if (collMap_.count(collectionName) == 0) {
					Collection* coll = db_->getCollection(collectionName);
					if (coll != NULL) {
						collMap_[collectionName] = coll;
					}
					else {
                        resp["status"] = "error";
                        resp["message"] = "Collection :" + collectionName + " is not found";
						return;
					}
				}
				Collection* coll = collMap_[collectionName];
				std::string field = req["field"];;
				Status status = coll->createIndex(field);
				if (!status.ok()) {
					resp["status"] = "error while creating index in collection " + collectionName;
					resp["message"] = status.message();
					return;
				}
				resp["status"] = "success";
				resp["message"] = "Index created on field name: " + field;
			}
		}
		catch (const std::exception& e) {
			resp["status"] = "error";
			resp["message"] = std::string("Exception: ") + e.what();
		}
	}
    void handle_delete_index(json& req, json& resp) {
        try {
            std::string collectionName = req["collection_name"];
            if (db_) {
                if (collMap_.count(collectionName) == 0) {
                    Collection* coll = db_->getCollection(collectionName);
                    if (coll != NULL) {
                        collMap_[collectionName] = coll;
                    }
                    else {
                        resp["status"] = "error";
                        resp["message"] = "Collection :" + collectionName + " is not found";
                        return;
                    }
                }
                Collection* coll = collMap_[collectionName];
                std::string field = req["field"];
                Status status = coll->createIndex(field);
                if (!status.ok()) {
                    resp["status"] = "error while deleting index in collection " + collectionName;
                    resp["message"] = status.message();
                    return;
                }
                resp["status"] = "success";
                resp["message"] = "Index deleted on field name: " + field;
            }
        }
        catch (const std::exception& e) {
            resp["status"] = "error";
            resp["message"] = std::string("Exception: ") + e.what();
        }
    }
    void handle_find_documents(json& req, json& resp, Work* work, std::string& response_topic) {
        try {
            std::string collectionName = req["collection_name"];
            if (db_) {
                if (collMap_.count(collectionName) == 0) {
                    Collection* coll = db_->getCollection(collectionName);
                    if (coll != NULL) {
                        collMap_[collectionName] = coll;
                    }
                    else {
                        resp["status"] = "error";
                        resp["message"] = "Collection :" + collectionName + " is not found";
                        return;
                    }
                }
                std::string requestId = req["request_id"];
                json query = req["query"];
                Collection* coll = collMap_[collectionName];
                std::vector<std::string> docIds;
                docIds = coll->findDocument(query);

                for (const std::string& docId : docIds) {
                    Document doc;
                    Status status = coll->readDocument(docId, doc);
                    if (status.ok()) {
                        std::string tmp = (std::string)doc.data().dump();
                        send_response(tmp, work, response_topic);
                        //std::cout << doc.data().dump() << std::endl;
                    }
                    else {
                        std::cerr << "Failed to read document " << docId << ": " << status.message() << std::endl;
                    }
                }
            }
        }
        catch (const std::exception& e) {
            resp["status"] = "error";
            resp["message"] = std::string("Exception: ") + e.what();
        }
    }

    std::string handle_request(struct Work* wrk,const std::string& topic, const std::string& payload) {
        json req, resp;
        Work* w = wrk;
        std::string response_topic = ANUDB_RESPONSE_TOPIC;
        wrk->requestid = new std::string(response_topic);
        try {
            req = json::parse(payload);
            std::string cmd = req["command"];
            std::string req_id = req["request_id"];
            response_topic += req_id;
            delete wrk->requestid;
            wrk->requestid = new std::string(response_topic);
            if (cmd == "create_collection") {
                handle_create_collection(req, resp);
            }
            else if (cmd == "delete_collection") {
                handle_delete_collection(req, resp);
            }
            else if (cmd == "get_collections") {
                handle_get_collections(req, resp);
            }
            else if (cmd == "create_document") {
                handle_create_document(req, resp);
            }
            else if (cmd == "delete_document") {
                handle_delete_document(req, resp);
            }
            else if (cmd == "read_document") {
                handle_read_document(req, resp, wrk, response_topic);
                resp["status"] = "success";
            }
            else if (cmd == "create_index") {
                handle_create_index(req, resp);
            }
            else if (cmd == "delete_index") {
                handle_delete_index(req, resp);
            }
            else if (cmd == "get_indexes") {
                handle_get_indexes(req, resp);
            }
            else if (cmd == "find_documents") {
                handle_find_documents(req, resp, wrk, response_topic);
                resp["status"] = "success";
            }
            else {
                resp["status"] = "error";
                resp["message"] = "Unknown command: " + cmd;
            }
        }
        catch (const std::exception& e) {
            resp["status"] = "error";
            resp["message"] = std::string("Exception :") + e.what();
        }
        return resp.dump();
    }

    std::string broker_url_;
    std::string client_id_;
    nng_socket client_;
    Database* db_;
    bool running_;
    std::vector<Work*> workers_;
    std::mutex mtx_;
    std::unordered_map<std::string, Collection*> collMap_;
};

volatile sig_atomic_t running = 1;
void signal_handler(int sig) {
    running = 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <broker_url> <database_name>" << std::endl;
        return 1;
    }

    std::string broker_url = argv[1];
    std::string db_name = argv[2];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        std::unique_ptr<Database> db = std::make_unique<Database>(db_name);
        std::string client_id = "anudb_mqtt_server_" + std::to_string(time(nullptr));

        AnuDBMqttClient mqtt_client(broker_url, client_id, db.get());
        if (!mqtt_client.start()) {
            std::cerr << "Failed to start MQTT client" << std::endl;
            return 1;
        }

        std::cout << "AnuDB MQTT Server started. Press Ctrl+C to exit." << std::endl;
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        mqtt_client.stop();
        std::cout << "AnuDB MQTT Server stopped." << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
