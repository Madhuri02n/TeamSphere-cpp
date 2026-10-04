#pragma once
#include <string>
#include "utils/Utils.h"

struct Flight {
    std::string id, flightNumber, from, to, matchId;
    double price = 0, durationMinutes = 0, availableSeats = 0;

    static Flight fromRequest(const json& body) {
        Flight f;
        f.flightNumber = asText(field(body, "flightNumber"));
        f.from = asText(field(body, "from"));
        f.to = asText(field(body, "to"));
        f.price = toNumber(field(body, "price"));  // validateFlight() already checked these
        f.durationMinutes = toNumber(field(body, "durationMinutes"));
        f.availableSeats = toNumber(field(body, "availableSeats"));
        f.matchId = asText(field(body, "matchId"));
        return f;
    }
    static Flight fromDoc(const json& d) {
        Flight f;
        f.id = d.value("_id", "");
        f.flightNumber = d.value("flightNumber", "");
        f.from = d.value("from", "");
        f.to = d.value("to", "");
        f.price = d.value("price", 0.0);
        f.durationMinutes = d.value("durationMinutes", 0.0);
        f.availableSeats = d.value("availableSeats", 0.0);
        f.matchId = d.value("matchId", "");
        return f;
    }
    json toDoc() const {
        return json{{"flightNumber", flightNumber}, {"from", from}, {"to", to},
                    {"price", numberJson(price)}, {"durationMinutes", numberJson(durationMinutes)},
                    {"availableSeats", numberJson(availableSeats)}, {"matchId", matchId}};
    }
    json toJson() const {
        json j = toDoc();
        j["_id"] = id;
        return j;
    }
};
