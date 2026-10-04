#include <algorithm>
#include "controllers/Controllers.h"
#include "httplib.h"
#include "models/Player.h"
#include "utils/Utils.h"

// Field checks + the team must exist.
static void checkPlayer(Database& db, const json& body) {
    validatePlayer(body);
    std::string teamId = requireId(asText(field(body, "teamId")));
    if (!db.findById("teams", teamId)) throw notFound("Team not found");
}

void registerPlayerRoutes(httplib::Server& svr, Database& db) {
    // GET /api/players  or  /api/players?teamId=...
    svr.Get("/api/players", safe([&db](const httplib::Request& req, httplib::Response& res) {
        json filter = json::object();
        if (req.has_param("teamId")) filter["teamId"] = requireId(req.get_param_value("teamId"));

        std::vector<Player> players;
        for (const json& doc : db.find("players", filter)) players.push_back(Player::fromDoc(doc));
        std::sort(players.begin(), players.end(), [](const Player& a, const Player& b) { return a.name < b.name; });

        json list = json::array();
        for (const Player& p : players) list.push_back(p.toJson());
        sendSuccess(res, list);
    }));

    svr.Post("/api/players", safe([&db](const httplib::Request& req, httplib::Response& res) {
        json body = parseBody(req);
        checkPlayer(db, body);
        Player player = Player::fromRequest(body);
        player.id = db.insertOne("players", player.toDoc());
        sendSuccess(res, player.toJson(), 201);
    }));

    svr.Put("/api/players/:id", safe([&db](const httplib::Request& req, httplib::Response& res) {
        std::string id = pathId(req);
        json body = parseBody(req);
        checkPlayer(db, body);
        Player player = Player::fromRequest(body);
        if (!db.updateById("players", id, player.toDoc())) throw notFound("Player not found");
        player.id = id;
        sendSuccess(res, player.toJson());
    }));

    svr.Delete("/api/players/:id", safe([&db](const httplib::Request& req, httplib::Response& res) {
        if (!db.deleteById("players", pathId(req))) throw notFound("Player not found");
        sendSuccess(res, json{{"message", "Player deleted"}});
    }));
}
