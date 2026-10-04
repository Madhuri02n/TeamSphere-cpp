// MongoDB.cpp - talks to a real MongoDB (Atlas or local) using the official MongoDB C driver.
// Every document goes in and out as JSON text, which keeps this file short.
#include <stdexcept>
#include "database/Database.h"

#ifdef USE_MONGO
#include <mongoc/mongoc.h>

namespace {

// Turns a JSON object into the BSON format MongoDB understands. The caller must bson_destroy() it.
bson_t* toBson(const json& j) {
    std::string text = j.dump();
    bson_error_t error;
    bson_t* b = bson_new_from_json(reinterpret_cast<const uint8_t*>(text.c_str()), -1, &error);
    if (!b) throw std::runtime_error(std::string("Could not convert JSON for MongoDB: ") + error.message);
    return b;
}

json idFilter(const std::string& id) {
    json f = json::object();
    f["_id"] = id;
    return f;
}

// Borrows a connection from the pool and gives it back when it goes out of scope (RAII).
class Connection {
    mongoc_client_pool_t* pool;
    mongoc_client_t* client;

public:
    mongoc_collection_t* coll;
    Connection(mongoc_client_pool_t* p, const std::string& db, const std::string& collection) : pool(p) {
        client = mongoc_client_pool_pop(pool);
        coll = mongoc_client_get_collection(client, db.c_str(), collection.c_str());
    }
    ~Connection() {
        mongoc_collection_destroy(coll);
        mongoc_client_pool_push(pool, client);
    }
};

// Reads one number (like "deletedCount") from a MongoDB reply document.
long replyNumber(const bson_t* reply, const char* key) {
    char* text = bson_as_relaxed_extended_json(reply, nullptr);
    json j = json::parse(text);
    bson_free(text);
    return j.value(key, 0L);
}

class MongoDatabase : public Database {
    mongoc_uri_t* uri = nullptr;
    mongoc_client_pool_t* pool = nullptr;
    std::string dbName;

public:
    explicit MongoDatabase(const std::string& uriText) {
        mongoc_init();
        bson_error_t error;
        uri = mongoc_uri_new_with_error(uriText.c_str(), &error);
        if (!uri) {
            std::string message = error.message;
            mongoc_cleanup();
            throw std::runtime_error("Invalid MONGO_URI: " + message);
        }
        const char* name = mongoc_uri_get_database(uri);  // the part after .net/ in the URI
        dbName = (name && *name) ? name : "teamsphere";
        pool = mongoc_client_pool_new(uri);

        // Ping the server once so a wrong password or blocked IP fails right at start-up.
        mongoc_client_t* client = mongoc_client_pool_pop(pool);
        bson_t ping = BSON_INITIALIZER;
        BSON_APPEND_INT32(&ping, "ping", 1);
        bson_t reply;
        bool ok = mongoc_client_command_simple(client, "admin", &ping, nullptr, &reply, &error);
        bson_destroy(&ping);
        bson_destroy(&reply);
        mongoc_client_pool_push(pool, client);
        if (!ok) {
            std::string message = error.message;
            mongoc_client_pool_destroy(pool);
            mongoc_uri_destroy(uri);
            mongoc_cleanup();
            throw std::runtime_error(message);
        }
    }
    ~MongoDatabase() override {
        mongoc_client_pool_destroy(pool);
        mongoc_uri_destroy(uri);
        mongoc_cleanup();
    }

    std::string insertOne(const std::string& collection, json doc) override {
        bson_oid_t oid;
        bson_oid_init(&oid, nullptr);
        char idText[25];
        bson_oid_to_string(&oid, idText);
        doc["_id"] = std::string(idText);

        Connection c(pool, dbName, collection);
        bson_t* b = toBson(doc);
        bson_error_t error;
        bool ok = mongoc_collection_insert_one(c.coll, b, nullptr, nullptr, &error);
        bson_destroy(b);
        if (!ok) throw std::runtime_error(error.message);
        return idText;
    }

    std::vector<json> find(const std::string& collection, const json& filter) override {
        Connection c(pool, dbName, collection);
        bson_t* f = toBson(filter);
        mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(c.coll, f, nullptr, nullptr);
        std::vector<json> out;
        const bson_t* doc;
        while (mongoc_cursor_next(cursor, &doc)) {
            char* text = bson_as_relaxed_extended_json(doc, nullptr);
            out.push_back(json::parse(text));
            bson_free(text);
        }
        bson_error_t error;
        bool failed = mongoc_cursor_error(cursor, &error);
        mongoc_cursor_destroy(cursor);
        bson_destroy(f);
        if (failed) throw std::runtime_error(error.message);
        return out;
    }

    std::optional<json> findById(const std::string& collection, const std::string& id) override {
        std::vector<json> found = find(collection, idFilter(id));
        if (found.empty()) return std::nullopt;
        return found[0];
    }

    bool updateById(const std::string& collection, const std::string& id, const json& fields) override {
        json update = json::object();
        update["$set"] = fields;
        Connection c(pool, dbName, collection);
        bson_t* selector = toBson(idFilter(id));
        bson_t* change = toBson(update);
        bson_t reply;
        bson_error_t error;
        bool ok = mongoc_collection_update_one(c.coll, selector, change, nullptr, &reply, &error);
        long matched = ok ? replyNumber(&reply, "matchedCount") : 0;
        bson_destroy(&reply);
        bson_destroy(selector);
        bson_destroy(change);
        if (!ok) throw std::runtime_error(error.message);
        return matched > 0;
    }

    bool deleteById(const std::string& collection, const std::string& id) override {
        Connection c(pool, dbName, collection);
        bson_t* selector = toBson(idFilter(id));
        bson_t reply;
        bson_error_t error;
        bool ok = mongoc_collection_delete_one(c.coll, selector, nullptr, &reply, &error);
        long deleted = ok ? replyNumber(&reply, "deletedCount") : 0;
        bson_destroy(&reply);
        bson_destroy(selector);
        if (!ok) throw std::runtime_error(error.message);
        return deleted > 0;
    }

    long deleteMany(const std::string& collection, const json& filter) override {
        Connection c(pool, dbName, collection);
        bson_t* selector = toBson(filter);
        bson_t reply;
        bson_error_t error;
        bool ok = mongoc_collection_delete_many(c.coll, selector, nullptr, &reply, &error);
        long deleted = ok ? replyNumber(&reply, "deletedCount") : 0;
        bson_destroy(&reply);
        bson_destroy(selector);
        if (!ok) throw std::runtime_error(error.message);
        return deleted;
    }

    long count(const std::string& collection, const json& filter) override {
        Connection c(pool, dbName, collection);
        bson_t* f = toBson(filter);
        bson_error_t error;
        int64_t n = mongoc_collection_count_documents(c.coll, f, nullptr, nullptr, nullptr, &error);
        bson_destroy(f);
        if (n < 0) throw std::runtime_error(error.message);
        return static_cast<long>(n);
    }
};

}  // namespace
#endif  // USE_MONGO

std::unique_ptr<Database> createDatabase(const std::string& uri) {
    if (uri == "memory") return makeMemoryDatabase();
#ifdef USE_MONGO
    return std::make_unique<MongoDatabase>(uri);
#else
    throw std::runtime_error("This build has no MongoDB support. Use MONGO_URI=memory, or build with: make teamsphere");
#endif
}
