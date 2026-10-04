// main.cpp - starts the TeamSphere API server.
//
// Environment variables (or a .env file next to the program):
//   MONGO_URI   MongoDB connection string, or the word "memory" for a throw-away in-memory database
//   PORT        port to listen on (default 5000)
//   CLIENT_URL  optional: the one website allowed to call this API (default: any website)

#include <cstdlib>
#include <fstream>
#include <iostream>
#include "controllers/Controllers.h"
#include "database/Database.h"
#include "httplib.h"
#include "utils/Utils.h"

static void setEnv(const std::string& key, const std::string& value) {
#ifdef _WIN32
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 0);  // 0 = do not overwrite a variable that is already set
#endif
}

// Reads lines like KEY=value from ".env" (if the file exists).
static void loadEnvFile(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        setEnv(trim(line.substr(0, eq)), trim(line.substr(eq + 1)));
    }
}

static std::string getEnv(const char* name, const std::string& fallback = "") {
    const char* value = std::getenv(name);
    return (value && *value) ? std::string(value) : fallback;
}

int main() {
    loadEnvFile(".env");
    std::string mongoUri = getEnv("MONGO_URI", "memory");
    int port = std::stoi(getEnv("PORT", "5000"));
    std::string clientUrl = getEnv("CLIENT_URL");

    // 1. Database
    std::unique_ptr<Database> db;
    try {
        db = createDatabase(mongoUri);
    } catch (const std::exception& e) {
        std::cerr << "Could not connect to MongoDB: " << e.what() << std::endl;
        return 1;
    }
    std::cout << (mongoUri == "memory" ? "Using the in-memory database (data is lost when the server stops)"
                                       : "MongoDB connected")
              << std::endl;

    // 2. Web server
    httplib::Server svr;
    svr.set_tcp_nodelay(true);  // answer immediately instead of waiting ~40 ms for more data

    // CORS: lets the React site (on another address) call this API.
    svr.set_post_routing_handler([clientUrl](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", clientUrl.empty() ? "*" : clientUrl);
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });
    // The browser sends an OPTIONS "preflight" request before POST / PUT / DELETE.
    svr.Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) { res.status = 204; });

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        sendSuccess(res, json::object(), 200, "TeamSphere API is running");
    });

    registerTeamRoutes(svr, *db);
    registerPlayerRoutes(svr, *db);
    registerMatchRoutes(svr, *db);
    registerFlightRoutes(svr, *db);

    // Unknown URL -> JSON 404 (only when a handler has not already written an answer)
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.body.empty()) sendError(res, res.status, res.status == 404 ? "Route not found" : "Request failed");
    });

    std::cout << "Server running on port " << port << std::endl;
    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "Could not start the server on port " << port << " (is it already in use?)" << std::endl;
        return 1;
    }
    return 0;
}
