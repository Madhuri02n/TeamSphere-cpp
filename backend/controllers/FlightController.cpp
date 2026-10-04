#include <algorithm>
#include "controllers/Controllers.h"
#include "httplib.h"
#include "models/Flight.h"
#include "services/Services.h"
#include "utils/Utils.h"

// Shared by /cheapest and /fastest: load the match's flights and let the service pick one.
static void sendBestFlight(Database& db, const httplib::Request& req, httplib::Response& res, bool wantCheapest) {
    std::string matchId = trim(req.get_param_value("matchId"));
    if (matchId.empty()) throw badRequest("matchId is required");
    json filter = json::object();
    filter["matchId"] = requireId(matchId);

    std::vector<Flight> flights;
    for (const json& doc : db.find("flights", filter)) flights.push_back(Flight::fromDoc(doc));
    if (flights.empty()) throw notFound("No flights found for this match");

    const Flight* best = wantCheapest ? TravelService::cheapest(flights) : TravelService::fastest(flights);
    sendSuccess(res, best->toJson());
}

void registerFlightRoutes(httplib::Server& svr, Database& db) {
    // GET /api/flights  or  /api/flights?matchId=...  (cheapest first)
    svr.Get("/api/flights", safe([&db](const httplib::Request& req, httplib::Response& res) {
        json filter = json::object();
        if (req.has_param("matchId")) filter["matchId"] = requireId(req.get_param_value("matchId"));

        std::vector<Flight> flights;
        for (const json& doc : db.find("flights", filter)) flights.push_back(Flight::fromDoc(doc));
        json list = json::array();
        for (const Flight& f : TravelService::sortedByPrice(flights)) list.push_back(f.toJson());
        sendSuccess(res, list);
    }));

    svr.Get("/api/flights/cheapest", safe([&db](const httplib::Request& req, httplib::Response& res) {
        sendBestFlight(db, req, res, true);
    }));

    svr.Get("/api/flights/fastest", safe([&db](const httplib::Request& req, httplib::Response& res) {
        sendBestFlight(db, req, res, false);
    }));

    svr.Post("/api/flights", safe([&db](const httplib::Request& req, httplib::Response& res) {
        json body = parseBody(req);
        validateFlight(body);
        std::string matchId = requireId(asText(field(body, "matchId")));
        if (!db.findById("matches", matchId)) throw notFound("Match not found");

        Flight flight = Flight::fromRequest(body);
        flight.id = db.insertOne("flights", flight.toDoc());
        sendSuccess(res, flight.toJson(), 201);
    }));

    svr.Delete("/api/flights/:id", safe([&db](const httplib::Request& req, httplib::Response& res) {
        if (!db.deleteById("flights", pathId(req))) throw notFound("Flight not found");
        sendSuccess(res, json{{"message", "Flight deleted"}});
    }));
}
