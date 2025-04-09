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

    struct work {
        State state;
        nng_aio* aio;
        nng_msg* msg;
        nng_ctx ctx;
        AnuDBMqttClient* client;  // Pointer to client instance
        bool exit;
    };

    static void client_cb(void* arg) {
        struct work* w = static_cast<work*>(arg);
        if (w->exit) {
            return;
        }
        nng_msg* msg = nullptr;
        int rv;

        if (w->state == INIT) {
            w->state = RECV;
            nng_ctx_recv(w->ctx, w->aio);
        }
        else if (w->state == RECV) {
            if (w->exit) {
                return;
            }
            if ((rv = nng_aio_result(w->aio)) != 0) {
                w->state = RECV;
                nng_ctx_recv(w->ctx, w->aio);
                return;
            }
            w->msg = nng_aio_get_msg(w->aio);
            w->state = WAIT;
            nng_sleep_aio(0, w->aio);
        }
        else if (w->state == WAIT) {
            msg = w->msg;
            if (msg == nullptr) {
                w->state = RECV;
                nng_ctx_recv(w->ctx, w->aio);
                return;
            }

            uint32_t payload_len;
            uint8_t* payload = nng_mqtt_msg_get_publish_payload(msg, &payload_len);
            uint32_t topic_len;
            const char* recv_topic = nng_mqtt_msg_get_publish_topic(msg, &topic_len);

            std::string topic(recv_topic, topic_len);
            std::string payload_str(reinterpret_cast<char*>(payload), payload_len);

            if (payload_len == 0 || payload_str.empty()) {
                std::cout << "Empty payload received on topic '" << topic << "', skipping...\n";
                nng_msg_free(msg);
                w->msg = nullptr;
                w->state = RECV;
                nng_ctx_recv(w->ctx, w->aio);
                return;
			}

			std::cout << "RECV: '" << payload_str << "' FROM: '" << topic << "'\n";

			if (topic == ANUDB_REQUEST_TOPIC && w->client) {
				w->client->handle_request(w, topic, payload_str);
			}
			else {
				std::string response_str = "{\"error\": \"Unsupported topic\"}";

				// Create a new message for each response
				nng_msg* new_msg;
				nng_mqtt_msg_alloc(&new_msg, 0);

				std::string response_topic = ANUDB_RESPONSE_TOPIC;

				// Modify payload if needed
				std::string new_payload = response_str;

				nng_mqtt_msg_set_packet_type(new_msg, NNG_MQTT_PUBLISH);
				nng_mqtt_msg_set_publish_topic(new_msg, response_topic.c_str());
				nng_mqtt_msg_set_publish_payload(new_msg, (uint8_t*)new_payload.c_str(), new_payload.size());

				std::cout << "SEND: " << new_payload << " to topic: " << response_topic << std::endl;

				// Allocate a new AIO for this send
				nng_aio* send_aio;
				nng_aio_alloc(&send_aio, nullptr, nullptr);
				nng_aio_set_msg(send_aio, new_msg);
				nng_ctx_send(w->ctx, send_aio);
				nng_msg_free(new_msg);
				nng_aio_free(send_aio);
			}
            // Free original received message
            nng_msg_free(msg);
            w->msg = nullptr;
            w->state = SEND;
        }
        else if (w->state == SEND) {
            if (w->exit) return;
            if ((rv = nng_aio_result(w->aio)) != 0) {
                std::cerr << "nng_send_aio: " << nng_strerror(rv) << std::endl;
            }
            w->state = RECV;
            nng_ctx_recv(w->ctx, w->aio);
        }
    }

    struct work* alloc_work() {
        struct work* w = nullptr;
        int rv;

        if ((w = (work*)nng_alloc(sizeof(*w))) == nullptr) {
            std::cerr << "nng_alloc: " << nng_strerror(NNG_ENOMEM) << std::endl;
            return nullptr;
        }
        if ((rv = nng_aio_alloc(&w->aio, client_cb, w)) != 0) {
            std::cerr << "nng_aio_alloc: " << nng_strerror(rv) << std::endl;
            nng_free(w, sizeof(*w));
            return nullptr;
        }
        if ((rv = nng_ctx_open(&w->ctx, client_)) != 0) {
            std::cerr << "nng_ctx_open: " << nng_strerror(rv) << std::endl;
            nng_aio_free(w->aio);
            nng_free(w, sizeof(*w));
            return nullptr;
        }
        w->exit = false;
        w->state = INIT;
        w->client = this;
        workers_.push_back(w);
        return w;
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
        struct work* works[CONCURRENT_THREADS];

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
        for (auto w : workers_) {
            w->exit = true;
        }
        collMap_.clear();
        Status status = db_->close();
        if (!status.ok()) {
            std::cerr << "Failed to close database: " << status.message() << std::endl;
        }
        std::cout << "Closing in progress..\n";
        // Now stop aio and give callbacks time to bail out
        for (auto w : workers_) {
            nng_aio_stop(w->aio);
        }

        // Wait a bit to ensure all callbacks exited
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));

        for (auto w : workers_) {
            nng_aio_free(w->aio);
            int rv = nng_ctx_close(w->ctx);
            if (rv != 0) {
                std::cerr << "Error closing ctx[" << w->ctx.id << "]: " << nng_strerror(rv) << std::endl;
            }
            nng_free(w, sizeof(*w));
        }

        workers_.clear();
        nng_close(client_);
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

    void send_response(const std::string reply, struct work* wrk, const std::string& response_topic) {
        // Create a new message for each response
        nng_msg* new_msg;
        nng_mqtt_msg_alloc(&new_msg, 0);
        nng_mqtt_msg_set_packet_type(new_msg, NNG_MQTT_PUBLISH);
        nng_mqtt_msg_set_publish_topic(new_msg, response_topic.c_str());
        nng_mqtt_msg_set_publish_payload(new_msg, (uint8_t*)reply.c_str(), reply.size());

        std::cout << "SEND: " << reply << " to topic: " << response_topic << std::endl;

        // Allocate a new AIO for this send
        nng_aio* send_aio;
        nng_aio_alloc(&send_aio, nullptr, nullptr);
        nng_aio_set_msg(send_aio, new_msg);
        nng_ctx_send(wrk->ctx, send_aio);
        nng_msg_free(new_msg);
        nng_aio_free(send_aio);
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
                    resp["status"] = "error";
                    resp["message"] = "Collection :" + collectionName + " is not found";
                    return;
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
    void handle_read_document(json& req, json& resp, struct work* wrk, std::string& response_topic) {
        try {
            std::string collectionName = req["collection_name"];
            if (db_) {
                if (collMap_.count(collectionName) == 0) {
                    resp["status"] = "error";
                    resp["message"] = "Collection :" + collectionName + " is not found";
                    return;
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
					send_response(doc.data().dump(), wrk, response_topic);
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
							send_response(doc.data().dump(), wrk, response_topic);
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
                    resp["status"] = "error";
                    resp["message"] = "Collection :" + collectionName + " is not found";
                    return;
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



    void handle_request(struct work* wrk,const std::string& topic, const std::string& payload) {
        std::lock_guard<std::mutex> lock(mtx);
        json req, resp;
        struct work* w = wrk;
        std::string response_topic = ANUDB_RESPONSE_TOPIC;
        try {
            req = json::parse(payload);
            std::string cmd = req["command"];
            std::string req_id = req["request_id"];
            response_topic += req_id;
            if (cmd == "create_collection") {
                handle_create_collection(req, resp);
                send_response(resp.dump(), w, response_topic);
            }
            else if (cmd == "delete_collection") {
                handle_delete_collection(req, resp);
                send_response(resp.dump(), w, response_topic);
            }
            else if (cmd == "create_document") {
                handle_create_document(req, resp);
                send_response(resp.dump(), w, response_topic);
            }
            else if (cmd == "read_document") {
                handle_read_document(req, resp, w, response_topic);
            }
            else if (cmd == "delete_document") {
                handle_delete_document(req, resp);
                send_response(resp.dump(), w, response_topic);
            }
#if 0
            else if (cmd == "create_index") {
                handle_create_index(req, resp);
            }
            else if (cmd == "delete_index") {
                handle_delete_index(req, resp);
            }
#endif
            else {
                resp["status"] = "error";
                resp["message"] = "Unknown command: " + cmd;
                send_response(resp.dump(), w, response_topic);
            }
        }
        catch (const std::exception& e) {
            resp["status"] = "error";
            resp["message"] = std::string("Exception: ") + e.what();
            send_response(resp.dump(), w, response_topic);
        }
        return;
    }

    std::string broker_url_;
    std::string client_id_;
    nng_socket client_;
    Database* db_;
    bool running_;
    std::vector<work*> workers_;
    std::mutex mtx;
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
