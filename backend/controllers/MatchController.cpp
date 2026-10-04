#include <algorithm>
#include <ctime>
#include <mutex>
#include <unordered_map>
#include "controllers/Controllers.h"
#include "httplib.h"
#include "models/Match.h"
#include "models/Team.h"
#include "services/Services.h"
#include "utils/Utils.h"

// Only ONE match is checked-and-saved at a time. Without this lock, two requests arriving together
// could both pass the conflict check before either one is saved (a "race condition").
// Limit: it protects a single server process, not several servers sharing one database.
static std::mutex scheduleMutex;

static std::string todayString() {
    std::time_t now = std::time(nullptr);
    std::tm local;
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buf[11];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", &local);
    return buf;
}

// Same job as populate("teamA teamB") in Mongoose: replace team ids with the team objects.
static std::unordered_map<std::string, Team> loadTeams(Database& db) {
    std::unordered_map<std::string, Team> teams;  // id -> team (one query instead of one per match)
    for (const json& doc : db.find("teams")) {
        Team t = Team::fromDoc(doc);
        teams[t.id] = t;
    }
    return teams;
}

static json populate(const Match& m, const std::unordered_map<std::string, Team>& teams) {
    json j = m.toJson();
    auto a = teams.find(m.teamA);
    auto b = teams.find(m.teamB);
    j["teamA"] = (a == teams.end()) ? json(nullptr) : a->second.toJson();
    j["teamB"] = (b == teams.end()) ? json(nullptr) : b->second.toJson();
    return j;
}

static void requireTeamsExist(Database& db, const Match& m) {
    requireId(m.teamA);
    requireId(m.teamB);
    if (!db.findById("teams", m.teamA) || !db.findById("teams", m.teamB)) throw notFound("Team not found");
}

// Throws a 400 error if either team already has an overlapping match.
// ignoreId is the match being edited, so it does not conflict with itself ("" when creating).
static void checkConflict(Database& db, const Match& m, const std::string& ignoreId) {
    int newStart = 0, newEnd = 0;
    parseTime(m.startTime, newStart);  // already validated
    parseTime(m.endTime, newEnd);

    // Step 1 (database): Scheduled matches on the SAME DATE that involve EITHER team.
    // Cancelled matches are ignored - they no longer block a team.
    json filter = json::object();
    filter["date"] = m.date;
    filter["status"] = "Scheduled";
    std::vector<Match> sameDay;
    for (const json& doc : db.find("matches", filter)) {
        Match e = Match::fromDoc(doc);
        if (e.id == ignoreId) continue;
        bool involvesOurTeam = e.teamA == m.teamA || e.teamA == m.teamB || e.teamB == m.teamA || e.teamB == m.teamB;
        if (involvesOurTeam) sameDay.push_back(e);
    }

    // Step 2 (service): which of them overlap the new time range?
    std::vector<MatchSlot> slots;
    for (const Match& e : sameDay) {
        int s = 0, en = 0;
        if (parseTime(e.startTime, s) && parseTime(e.endTime, en)) slots.push_back(MatchSlot(e.id, s, en));
    }
    std::vector<std::string> conflictIds = ScheduleService::findConflicts(newStart, newEnd, slots);
    if (conflictIds.empty()) return;

    // Build a clear message with the name of the busy team.
    for (const Match& e : sameDay) {
        if (e.id != conflictIds[0]) continue;
        std::string busyTeamId = (e.teamA == m.teamA || e.teamA == m.teamB) ? e.teamA : e.teamB;
        auto team = db.findById("teams", busyTeamId);
        std::string name = team ? Team::fromDoc(*team).teamName : "A team";
        throw badRequest("Scheduling conflict: " + name + " already has a match during this time.");
    }
}

void registerMatchRoutes(httplib::Server& svr, Database& db) {
    // GET /api/matches  or  /api/matches?upcoming=true
    svr.Get("/api/matches", safe([&db](const httplib::Request& req, httplib::Response& res) {
        bool upcomingOnly = req.get_param_value("upcoming") == "true";
        std::string today = todayString();

        json filter = json::object();
        if (upcomingOnly) filter["status"] = "Scheduled";

        std::vector<Match> matches;
        for (const json& doc : db.find("matches", filter)) {
            Match m = Match::fromDoc(doc);
            if (upcomingOnly && m.date < today) continue;  // "YYYY-MM-DD" text compares correctly
            matches.push_back(m);
        }
        std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
            if (a.date != b.date) return a.date < b.date;
            return a.startTime < b.startTime;
        });

        auto teams = loadTeams(db);
        json list = json::array();
        for (const Match& m : matches) list.push_back(populate(m, teams));
        sendSuccess(res, list);
    }));

    // GET /api/matches/conflicts - used by the dashboard
    svr.Get("/api/matches/conflicts", safe([&db](const httplib::Request&, httplib::Response& res) {
        json filter = json::object();
        filter["status"] = "Scheduled";
        std::vector<MatchSlot> slots;
        for (const json& doc : db.find("matches", filter)) {
            Match m = Match::fromDoc(doc);
            int s = 0, e = 0;
            if (parseTime(m.startTime, s) && parseTime(m.endTime, e)) slots.push_back(MatchSlot(m.id, s, e, m.date, m.teamA, m.teamB));
        }
        auto pairs = ScheduleService::findAllConflicts(slots);

        json list = json::array();
        for (const auto& p : pairs) list.push_back(json{{"matchA", p.first}, {"matchB", p.second}});
        sendSuccess(res, json{{"count", pairs.size()}, {"pairs", list}});
    }));

    // POST /api/matches - schedule a match
    svr.Post("/api/matches", safe([&db](const httplib::Request& req, httplib::Response& res) {
        json body = parseBody(req);
        Match m;  // status starts as "Scheduled"
        m.mergeRequest(body);
        validateMatch(m.toDoc());
        requireTeamsExist(db, m);

        {
            std::lock_guard<std::mutex> lock(scheduleMutex);  // check + save as one step
            if (m.status == "Scheduled") checkConflict(db, m, "");
            m.id = db.insertOne("matches", m.toDoc());
        }
        sendSuccess(res, populate(m, loadTeams(db)), 201, "Match scheduled successfully.");
    }));

    // PUT /api/matches/:id - reschedule, cancel, edit venue ...
    svr.Put("/api/matches/:id", safe([&db](const httplib::Request& req, httplib::Response& res) {
        std::string id = pathId(req);
        auto doc = db.findById("matches", id);
        if (!doc) throw notFound("Match not found");

        Match m = Match::fromDoc(*doc);  // start from the saved values...
        m.mergeRequest(parseBody(req));  // ...and overwrite what the client sent
        validateMatch(m.toDoc());
        requireTeamsExist(db, m);

        {
            std::lock_guard<std::mutex> lock(scheduleMutex);
            if (m.status == "Scheduled") checkConflict(db, m, id);  // re-check on reschedule
            db.updateById("matches", id, m.toDoc());
        }
        sendSuccess(res, populate(m, loadTeams(db)), 200, "Match updated successfully.");
    }));

    // DELETE /api/matches/:id
    svr.Delete("/api/matches/:id", safe([&db](const httplib::Request& req, httplib::Response& res) {
        std::string id = pathId(req);
        if (!db.findById("matches", id)) throw notFound("Match not found");
        json flights = json::object();
        flights["matchId"] = id;
        db.deleteMany("flights", flights);  // its travel options go too
        db.deleteById("matches", id);
        sendSuccess(res, json{{"message", "Match deleted"}});
    }));
}
