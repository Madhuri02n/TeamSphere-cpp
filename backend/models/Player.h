#pragma once
#include <string>
#include "utils/Utils.h"

struct Player {
    std::string id, name, position, teamId;
    double age = 0;

    static Player fromRequest(const json& body) {
        Player p;
        p.name = asText(field(body, "name"));
        p.age = toNumber(field(body, "age"));  // validatePlayer() already checked it is a number
        p.position = asText(field(body, "position"));
        p.teamId = asText(field(body, "teamId"));
        return p;
    }
    static Player fromDoc(const json& d) {
        Player p;
        p.id = d.value("_id", "");
        p.name = d.value("name", "");
        p.age = d.value("age", 0.0);
        p.position = d.value("position", "");
        p.teamId = d.value("teamId", "");
        return p;
    }
    json toDoc() const {
        return json{{"name", name}, {"age", numberJson(age)}, {"position", position}, {"teamId", teamId}};
    }
    json toJson() const {
        json j = toDoc();
        j["_id"] = id;
        return j;
    }
};
