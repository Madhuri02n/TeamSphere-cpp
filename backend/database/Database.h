// Database.h - the small interface the rest of the program uses to talk to MongoDB.
// Filters are simple JSON objects that mean "every field must be equal", e.g. {"teamId":"abc"}.
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "json.hpp"
using json = nlohmann::json;

class Database {
public:
    virtual ~Database() {}

    // Saves a new document and returns its new id (stored in the "_id" field).
    virtual std::string insertOne(const std::string& collection, json doc) = 0;
    virtual std::vector<json> find(const std::string& collection, const json& filter = json::object()) = 0;
    virtual std::optional<json> findById(const std::string& collection, const std::string& id) = 0;
    // Sets the given fields on the document. Returns false if no document has that id.
    virtual bool updateById(const std::string& collection, const std::string& id, const json& fields) = 0;
    virtual bool deleteById(const std::string& collection, const std::string& id) = 0;
    virtual long deleteMany(const std::string& collection, const json& filter) = 0;
    virtual long count(const std::string& collection, const json& filter = json::object()) = 0;
};

// MONGO_URI = "memory"  -> in-memory database (for quick tests, data is lost on exit)
// anything else         -> real MongoDB. Throws std::runtime_error if it cannot connect.
std::unique_ptr<Database> createDatabase(const std::string& uri);
std::unique_ptr<Database> makeMemoryDatabase();  // defined in MemoryDB.cpp
