#include <cctype>
#include <cmath>
#include "utils/Utils.h"

std::string trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string asText(const json& value) {
    if (value.is_null()) return "";
    if (value.is_string()) return trim(value.get<std::string>());
    return trim(value.dump());
}

bool isBlank(const json& value) { return asText(value).empty(); }

bool isNumber(const json& value) {
    if (value.is_number()) return true;
    if (!value.is_string()) return false;
    std::string s = trim(value.get<std::string>());
    if (s.empty()) return false;
    try {
        size_t used = 0;
        double d = std::stod(s, &used);
        return used == s.size() && std::isfinite(d);
    } catch (...) {
        return false;
    }
}

double toNumber(const json& value) {
    if (value.is_number()) return value.get<double>();
    return std::stod(trim(value.get<std::string>()));
}

json numberJson(double x) {
    if (x == std::floor(x) && std::fabs(x) < 9e15) return json(static_cast<long long>(x));
    return json(x);
}

bool isValidId(const std::string& id) {
    if (id.size() != 24) return false;
    for (char c : id) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

std::string requireId(const std::string& id) {
    if (!isValidId(id)) throw badRequest("Invalid ID format");
    return id;
}

// "18:00" -> 1080. Unlike stoi(), this never crashes: it checks the format first.
bool parseTime(const std::string& t, int& minutesOut) {
    if (t.size() != 5 || t[2] != ':') return false;
    const int digitPositions[4] = {0, 1, 3, 4};
    for (int p : digitPositions) {
        if (!std::isdigit(static_cast<unsigned char>(t[p]))) return false;
    }
    int hours = (t[0] - '0') * 10 + (t[1] - '0');
    int minutes = (t[3] - '0') * 10 + (t[4] - '0');
    if (hours > 23 || minutes > 59) return false;
    minutesOut = hours * 60 + minutes;
    return true;
}

bool isValidDate(const std::string& d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
    for (size_t i = 0; i < d.size(); i++) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(d[i]))) return false;
    }
    return true;
}

void validateTeam(const json& body) {
    if (isBlank(field(body, "teamName"))) throw badRequest("Team name is required");
    if (isBlank(field(body, "city"))) throw badRequest("City is required");
}

void validatePlayer(const json& body) {
    if (isBlank(field(body, "name"))) throw badRequest("Player name is required");
    json age = field(body, "age");
    if (!isNumber(age) || toNumber(age) <= 0) throw badRequest("Age must be greater than 0");
    if (isBlank(field(body, "position"))) throw badRequest("Position is required");
    if (isBlank(field(body, "teamId"))) throw badRequest("teamId is required");
}

void validateMatch(const json& m) {
    if (isBlank(field(m, "teamA"))) throw badRequest("Team A is required");
    if (isBlank(field(m, "teamB"))) throw badRequest("Team B is required");
    if (asText(field(m, "teamA")) == asText(field(m, "teamB"))) {
        throw badRequest("Team A and Team B cannot be the same team");
    }
    if (isBlank(field(m, "date"))) throw badRequest("Date is required");
    if (!isValidDate(asText(field(m, "date")))) throw badRequest("Date must be in YYYY-MM-DD format");
    if (isBlank(field(m, "startTime"))) throw badRequest("Start time is required");
    if (isBlank(field(m, "endTime"))) throw badRequest("End time is required");

    int start = 0, end = 0;
    if (!parseTime(asText(field(m, "startTime")), start) || !parseTime(asText(field(m, "endTime")), end)) {
        throw badRequest("Times must be in HH:MM (24-hour) format");
    }
    if (end <= start) throw badRequest("End time must be after start time");

    std::string status = asText(field(m, "status"));
    if (status != "Scheduled" && status != "Cancelled" && status != "Completed") {
        throw badRequest("Status must be Scheduled, Cancelled or Completed");
    }
}

void validateFlight(const json& body) {
    if (isBlank(field(body, "flightNumber"))) throw badRequest("Flight number is required");
    if (isBlank(field(body, "from"))) throw badRequest("From city is required");
    if (isBlank(field(body, "to"))) throw badRequest("To city is required");
    json price = field(body, "price");
    if (!isNumber(price) || toNumber(price) < 0) throw badRequest("Price must be 0 or more");
    json duration = field(body, "durationMinutes");
    if (!isNumber(duration) || toNumber(duration) <= 0) throw badRequest("Duration must be greater than 0");
    json seats = field(body, "availableSeats");
    if (!isNumber(seats) || toNumber(seats) < 0) throw badRequest("Seats must be 0 or more");
    if (isBlank(field(body, "matchId"))) throw badRequest("matchId is required");
}
