#pragma once
#include <string>
#include "utils/Utils.h"

// A team. Header-only: small struct with helpers to convert to/from JSON.
struct Team {
    std::string id, teamName, city, sport, coach;

    // Copy only the fields we allow from a request.
    static Team fromRequest(const json& body) {
        Team t;
        t.teamName = asText(field(body, "teamName"));
        t.city = asText(field(body, "city"));
        t.sport = asText(field(body, "sport"));
        t.coach = asText(field(body, "coach"));
        return t;
    }
    static Team fromDoc(const json& d) {  // from a database document
        Team t;
        t.id = d.value("_id", "");
        t.teamName = d.value("teamName", "");
        t.city = d.value("city", "");
        t.sport = d.value("sport", "");
        t.coach = d.value("coach", "");
        return t;
    }
    json toDoc() const {  // what we save (no _id)
        return json{{"teamName", teamName}, {"city", city}, {"sport", sport}, {"coach", coach}};
    }
    json toJson() const {  // what we send to the browser
        json j = toDoc();
        j["_id"] = id;
        return j;
    }
};
