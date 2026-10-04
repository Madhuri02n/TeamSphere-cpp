// Controllers.h - each function adds the URLs of one resource to the web server.
#pragma once
#include "database/Database.h"

namespace httplib {
class Server;
}

void registerTeamRoutes(httplib::Server& svr, Database& db);
void registerPlayerRoutes(httplib::Server& svr, Database& db);
void registerMatchRoutes(httplib::Server& svr, Database& db);
void registerFlightRoutes(httplib::Server& svr, Database& db);
