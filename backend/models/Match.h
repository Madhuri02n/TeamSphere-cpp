#pragma once
#include <string>
#include "utils/Utils.h"

// date is "YYYY-MM-DD" and times are "HH:MM" (24-hour), stored as plain text.
// teamA / teamB hold the ids of the two teams.
struct Match {
    std::string id, teamA, teamB, date, startTime, endTime, venue, city;
    std::string status = "Scheduled";

    // Overwrite only the fields that are present in the request.
    // Used for create (on a new Match) and for update (on the saved Match).
    void mergeRequest(const json& body) {
        auto set = [&body](const char* key, std::string& target) {
            if (body.contains(key)) target = asText(body.at(key));
        };
        set("teamA", teamA);
        set("teamB", teamB);
        set("date", date);
        set("startTime", startTime);
        set("endTime", endTime);
        set("venue", venue);
        set("city", city);
        set("status", status);
    }
    static Match fromDoc(const json& d) {
        Match m;
        m.id = d.value("_id", "");
        m.teamA = d.value("teamA", "");
        m.teamB = d.value("teamB", "");
        m.date = d.value("date", "");
        m.startTime = d.value("startTime", "");
        m.endTime = d.value("endTime", "");
        m.venue = d.value("venue", "");
        m.city = d.value("city", "");
        m.status = d.value("status", "Scheduled");
        return m;
    }
    json toDoc() const {
        return json{{"teamA", teamA}, {"teamB", teamB}, {"date", date}, {"startTime", startTime},
                    {"endTime", endTime}, {"venue", venue}, {"city", city}, {"status", status}};
    }
    json toJson() const {
        json j = toDoc();
        j["_id"] = id;
        return j;
    }
};
