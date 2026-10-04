// Services.h - the CORE LOGIC of TeamSphere (scheduling and travel algorithms).
// These classes know nothing about HTTP or MongoDB, so they are easy to test.
#pragma once
#include <string>
#include <utility>
#include <vector>
#include "models/Flight.h"

// The part of a match that scheduling needs: when it is, who plays, and the time range in
// minutes after midnight (18:00 -> 1080).
class MatchSlot {
public:
    std::string id;
    std::string date;  // "2026-10-18"
    std::string teamA;
    std::string teamB;
    int start;
    int end;

    MatchSlot(std::string id, int start, int end, std::string date = "", std::string teamA = "",
              std::string teamB = "")
        : id(id), date(date), teamA(teamA), teamB(teamB), start(start), end(end) {}
};

class ScheduleService {
public:
    // Two time ranges overlap when EACH one starts before the OTHER ends.
    // Back-to-back (end == start) is NOT a conflict because of the strict < and >.
    // Time: O(1)  Space: O(1)
    static bool hasConflict(int newStart, int newEnd, int existingStart, int existingEnd);

    // One new time range against a list of existing matches. Returns the ids that overlap.
    // Time: O(n)  Space: O(k), k = number of conflicts
    static std::vector<std::string> findConflicts(int newStart, int newEnd, const std::vector<MatchSlot>& existing);

    // Every pair of matches that clash (same date + a shared team + overlapping time).
    // Matches are grouped by date first, so only matches on the same day are compared.
    // Time: O(n + sum of g^2 over the date groups; g = group size)  Space: O(n)
    static std::vector<std::pair<std::string, std::string>> findAllConflicts(const std::vector<MatchSlot>& matches);
};

class TravelService {
public:
    // Linear scan for the lowest price. Returns nullptr for an empty list.
    // Time: O(n)  Space: O(1)
    static const Flight* cheapest(const std::vector<Flight>& flights);

    // Linear scan for the shortest duration. Time: O(n)  Space: O(1)
    static const Flight* fastest(const std::vector<Flight>& flights);

    // All flights from cheapest to most expensive (sorts a copy). Time: O(n log n)  Space: O(n)
    static std::vector<Flight> sortedByPrice(const std::vector<Flight>& flights);
};
