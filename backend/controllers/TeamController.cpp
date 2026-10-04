#include <algorithm>
#include "controllers/Controllers.h"
#include "httplib.h"
#include "models/Team.h"
#include "utils/Utils.h"

void registerTeamRoutes(httplib::Server& svr, Database& db) {
    // GET /api/teams - list all teams, sorted by name
    svr.Get("/api/teams", safe([&db](const httplib::Request&, httplib::Response& res) {
        std::vector<Team> teams;
        for (const json& doc : db.find("teams")) teams.push_back(Team::fromDoc(doc));
        std::sort(teams.begin(), teams.end(), [](const Team& a, const Team& b) { return a.teamName < b.teamName; });

        json list = json::array();
        for (const Team& t : teams) list.push_back(t.toJson());
        sendSuccess(res, list);
    }));

    // GET /api/teams/:id - one team
    svr.Get("/api/teams/:id", safe([&db](const httplib::Request& req, httplib::Response& res) {
        auto doc = db.findById("teams", pathId(req));
        if (!doc) throw notFound("Team not found");
        sendSuccess(res, Team::fromDoc(*doc).toJson());
    }));

    // POST /api/teams - create a team
    svr.Post("/api/teams", safe([&db](const httplib::Request& req, httplib::Response& res) {
        json body = parseBody(req);
        validateTeam(body);
        Team team = Team::fromRequest(body);
        team.id = db.insertOne("teams", team.toDoc());
        sendSuccess(res, team.toJson(), 201);
    }));

    // PUT /api/teams/:id - edit a team
    svr.Put("/api/teams/:id", safe([&db](const httplib::Request& req, httplib::Response& res) {
        std::string id = pathId(req);
        json body = parseBody(req);
        validateTeam(body);
        Team team = Team::fromRequest(body);
        if (!db.updateById("teams", id, team.toDoc())) throw notFound("Team not found");
        team.id = id;
        sendSuccess(res, team.toJson());
    }));

    // DELETE /api/teams/:id
    svr.Delete("/api/teams/:id", safe([&db](const httplib::Request& req, httplib::Response& res) {
        std::string id = pathId(req);
        if (!db.findById("teams", id)) throw notFound("Team not found");

        // Rule: do not leave matches pointing to a deleted team.
        json asA = json::object(), asB = json::object();
        asA["teamA"] = id;
        asB["teamB"] = id;
        if (db.count("matches", asA) + db.count("matches", asB) > 0) {
            throw badRequest("This team has matches. Delete those matches first.");
        }

        json players = json::object();
        players["teamId"] = id;
        db.deleteMany("players", players);  // players belong to the team
        db.deleteById("teams", id);
        sendSuccess(res, json{{"message", "Team deleted"}});
    }));
}
