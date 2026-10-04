// Utils.h - small helpers used everywhere: errors, JSON responses, validation.
#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include "json.hpp"

using json = nlohmann::json;

// Forward declarations: only the .cpp files that really use httplib include the big header.
namespace httplib {
struct Request;
struct Response;
}

// ------------------------------------------------------------------
// ErrorHandler.cpp
// ------------------------------------------------------------------

// An error that carries an HTTP status code (400, 404 ...).
class AppError : public std::runtime_error {
public:
    int status;
    AppError(int status, const std::string& message) : std::runtime_error(message), status(status) {}
};

AppError badRequest(const std::string& message);  // 400
AppError notFound(const std::string& message);    // 404

using RouteHandler = std::function<void(const httplib::Request&, httplib::Response&)>;

// Wraps a controller function: any error thrown inside becomes a JSON error response.
// (This is the C++ version of asyncHandler + errorHandler.)
RouteHandler safe(RouteHandler fn);

void sendSuccess(httplib::Response& res, const json& data, int status = 200, const std::string& message = "");
void sendError(httplib::Response& res, int status, const std::string& message);

json parseBody(const httplib::Request& req);          // request body -> JSON object (or 400)
std::string pathId(const httplib::Request& req);      // the :id in the URL, checked for format

// ------------------------------------------------------------------
// Validation.cpp
// ------------------------------------------------------------------
std::string trim(const std::string& s);
std::string asText(const json& value);       // null -> "", text -> trimmed, number -> "22"
inline json field(const json& obj, const char* key) { return obj.contains(key) ? obj.at(key) : json(); }

bool isBlank(const json& value);             // null, "" or "   "
bool isNumber(const json& value);            // a number, or text that is fully a number ("22")
double toNumber(const json& value);
json numberJson(double x);                   // 22.0 -> 22 (so it is saved as an integer)

bool isValidId(const std::string& id);       // 24 hex characters, like a MongoDB ObjectId
std::string requireId(const std::string& id);  // returns id or throws 400 "Invalid ID format"
bool parseTime(const std::string& text, int& minutesOut);  // "18:00" -> 1080, false if invalid
bool isValidDate(const std::string& text);   // YYYY-MM-DD

void validateTeam(const json& body);
void validatePlayer(const json& body);
void validateMatch(const json& match);       // match has plain text fields (ids, date, times, status)
void validateFlight(const json& body);
