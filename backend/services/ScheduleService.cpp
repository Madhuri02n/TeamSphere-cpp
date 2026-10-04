#include <unordered_map>
#include "services/Services.h"

bool ScheduleService::hasConflict(int newStart, int newEnd, int existingStart, int existingEnd) {
    return newStart < existingEnd && newEnd > existingStart;
}

std::vector<std::string> ScheduleService::findConflicts(int newStart, int newEnd, const std::vector<MatchSlot>& existing) {
    std::vector<std::string> conflictIds;
    for (const MatchSlot& m : existing) {
        if (hasConflict(newStart, newEnd, m.start, m.end)) conflictIds.push_back(m.id);
    }
    return conflictIds;
}

std::vector<std::pair<std::string, std::string>> ScheduleService::findAllConflicts(const std::vector<MatchSlot>& matches) {
    // Hash map: date -> positions of the matches on that date.
    std::unordered_map<std::string, std::vector<size_t>> byDate;
    for (size_t i = 0; i < matches.size(); i++) byDate[matches[i].date].push_back(i);

    std::vector<std::pair<std::string, std::string>> pairsFound;
    for (const auto& group : byDate) {
        const std::vector<size_t>& idx = group.second;
        for (size_t x = 0; x < idx.size(); x++) {
            for (size_t y = x + 1; y < idx.size(); y++) {  // y starts after x: each pair once
                const MatchSlot& a = matches[idx[x]];
                const MatchSlot& b = matches[idx[y]];
                bool shareTeam = a.teamA == b.teamA || a.teamA == b.teamB || a.teamB == b.teamA || a.teamB == b.teamB;
                if (shareTeam && hasConflict(a.start, a.end, b.start, b.end)) {
                    pairsFound.push_back(std::make_pair(a.id, b.id));
                }
            }
        }
    }
    return pairsFound;
}
