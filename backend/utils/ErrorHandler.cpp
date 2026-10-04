#include <iostream>
#include "httplib.h"
#include "utils/Utils.h"

AppError badRequest(const std::string& message) { return AppError(400, message); }
AppError notFound(const std::string& message) { return AppError(404, message); }

void sendSuccess(httplib::Response& res, const json& data, int status, const std::string& message) {
    json body = {{"success", true}, {"data", data}};
    if (!message.empty()) body["message"] = message;
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

void sendError(httplib::Response& res, int status, const std::string& message) {
    json body = {{"success", false}, {"message", message}};
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

RouteHandler safe(RouteHandler fn) {
    return [fn](const httplib::Request& req, httplib::Response& res) {
        try {
            fn(req, res);
        } catch (const AppError& e) {
            sendError(res, e.status, e.what());  // our own 400 / 404 errors
        } catch (const std::exception& e) {
            std::cerr << "ERROR: " << e.what() << std::endl;  // details only in the server log
            sendError(res, 500, "Something went wrong on the server");
        } catch (...) {
            std::cerr << "ERROR: unknown error" << std::endl;
            sendError(res, 500, "Something went wrong on the server");
        }
    };
}

json parseBody(const httplib::Request& req) {
    json body = json::parse(req.body, nullptr, false);  // false = do not throw
    if (body.is_discarded() || !body.is_object()) throw badRequest("Request body must be a valid JSON object");
    return body;
}

std::string pathId(const httplib::Request& req) { return requireId(req.path_params.at("id")); }
