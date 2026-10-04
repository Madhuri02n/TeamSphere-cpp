// MemoryDB.cpp - a tiny fake database that keeps everything in RAM.
// Used for tests and for running the app without MongoDB.
#include <cstdio>
#include <mutex>
#include <random>
#include <unordered_map>
#include "database/Database.h"

namespace {

class MemoryDatabase : public Database {
    std::mutex mutex;  // requests run on several threads, so protect the data
    std::unordered_map<std::string, std::vector<json>> collections;
    std::mt19937_64 rng{std::random_device{}()};

    std::string newId() {  // 24 hex characters, like a MongoDB ObjectId
        char buf[25];
        std::snprintf(buf, sizeof buf, "%08llx%016llx", (unsigned long long)(rng() & 0xffffffffULL),
                      (unsigned long long)rng());
        return buf;
    }
    static bool matches(const json& doc, const json& filter) {
        for (auto it = filter.begin(); it != filter.end(); ++it) {
            if (!doc.contains(it.key()) || doc.at(it.key()) != it.value()) return false;
        }
        return true;
    }

public:
    std::string insertOne(const std::string& collection, json doc) override {
        std::lock_guard<std::mutex> lock(mutex);
        std::string id = newId();
        doc["_id"] = id;
        collections[collection].push_back(doc);
        return id;
    }
    std::vector<json> find(const std::string& collection, const json& filter) override {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<json> out;
        for (const json& doc : collections[collection]) {
            if (matches(doc, filter)) out.push_back(doc);
        }
        return out;
    }
    std::optional<json> findById(const std::string& collection, const std::string& id) override {
        json filter = json::object();
        filter["_id"] = id;
        std::vector<json> found = find(collection, filter);
        if (found.empty()) return std::nullopt;
        return found[0];
    }
    bool updateById(const std::string& collection, const std::string& id, const json& fields) override {
        std::lock_guard<std::mutex> lock(mutex);
        for (json& doc : collections[collection]) {
            if (doc.value("_id", "") == id) {
                for (auto it = fields.begin(); it != fields.end(); ++it) doc[it.key()] = it.value();
                return true;
            }
        }
        return false;
    }
    bool deleteById(const std::string& collection, const std::string& id) override {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<json>& docs = collections[collection];
        for (size_t i = 0; i < docs.size(); i++) {
            if (docs[i].value("_id", "") == id) {
                docs.erase(docs.begin() + i);
                return true;
            }
        }
        return false;
    }
    long deleteMany(const std::string& collection, const json& filter) override {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<json>& docs = collections[collection];
        long removed = 0;
        for (size_t i = 0; i < docs.size();) {
            if (matches(docs[i], filter)) {
                docs.erase(docs.begin() + i);
                removed++;
            } else {
                i++;
            }
        }
        return removed;
    }
    long count(const std::string& collection, const json& filter) override {
        return static_cast<long>(find(collection, filter).size());
    }
};

}  // namespace

std::unique_ptr<Database> makeMemoryDatabase() { return std::make_unique<MemoryDatabase>(); }
