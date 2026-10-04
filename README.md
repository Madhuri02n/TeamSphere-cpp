# TeamSphere - Team & Match Scheduling System (C++ backend)

A manager creates teams and players, schedules matches (with **automatic conflict detection**) and stores
flight options for each match. **The whole backend is written in C++.** The frontend is React.

**Stack:** React (Vite) - C++17 backend - MongoDB (official C driver) - Postman

The REST API is exactly the same as in the earlier Node version, so the React app and the Postman
collection did not change.

LIVE: https://frontend-five-opal-96.vercel.app/

---

## 1. Project structure

```
teamsphere/
├── frontend/                  React app (unchanged)
├── postman/                   API tests (34 requests, 79 checks)
└── backend/
    ├── main.cpp               starts the server, connects the database, registers all URLs
    ├── models/                Team.h  Player.h  Match.h  Flight.h   (data + JSON conversion)
    ├── controllers/           one file per resource: URL -> function
    │   ├── TeamController.cpp  PlayerController.cpp  MatchController.cpp  FlightController.cpp
    │   └── Controllers.h
    ├── services/              THE CORE LOGIC (no HTTP, no database)
    │   ├── ScheduleService.cpp   conflict detection
    │   ├── TravelService.cpp     cheapest / fastest flight
    │   └── Services.h
    ├── database/
    │   ├── MongoDB.cpp        real MongoDB (official C driver)
    │   ├── MemoryDB.cpp       tiny in-RAM database for tests
    │   └── Database.h         the interface both implement
    ├── utils/
    │   ├── Validation.cpp     input checks + time parsing
    │   ├── ErrorHandler.cpp   turns errors into { success:false, message }
    │   └── Utils.h
    ├── tests/ServicesTest.cpp 25 checks of the core logic
    ├── third_party/           httplib.h (web server) and json.hpp (JSON) - single files, nothing to install
    ├── Makefile   Dockerfile   .env.example
```

`.h` files hold the declarations (what exists); `.cpp` files hold the code. Models are header-only because
they are small.

**Request flow:** React -> `api.js` -> httplib server -> controller -> (validation) -> service decides -> database -> JSON back.

---

## 2. Run it

### Windows: use WSL or Docker (the C driver is not easy to build natively on Windows)

**Option A - Docker Desktop (also what you deploy with)**
```bash
cd backend
docker build -t teamsphere-backend .
docker run -p 5000:5000 -e MONGO_URI="your-atlas-string" teamsphere-backend
```

**Option B - WSL (Ubuntu)**
```bash
sudo apt update && sudo apt install -y build-essential pkg-config libmongoc-dev libbson-dev
cd backend
cp .env.example .env        # then edit MONGO_URI
make teamsphere
./teamsphere
```
Linux and macOS work the same way (on macOS install `mongo-c-driver` with Homebrew).

### Try it with NO database at all
```bash
make teamsphere-memory
MONGO_URI=memory ./teamsphere-memory     # data disappears when you stop it
```

### Frontend (any OS)
```bash
cd frontend
npm install
npm run dev          # http://localhost:5173  (VITE_API_URL=http://localhost:5000/api in frontend/.env)
```

You should see `MongoDB connected` and `Server running on port 5000`. Opening http://localhost:5000 shows `TeamSphere API is running`.

### Tests
```bash
cd backend && make test      # 25 checks of the scheduling and travel logic
```
Postman: import `postman/TeamSphere.postman_collection.json`, then Run collection (all 34 requests pass).

---

## 3. How the C++ logic works

### Conflict detection (`services/ScheduleService.cpp`)
Two time ranges overlap when **each starts before the other ends**:
```cpp
return newStart < existingEnd && newEnd > existingStart;
```
Times are first changed to minutes after midnight (18:00 -> 1080) by `parseTime()` in `utils/Validation.cpp`.
Existing match 18:00-20:00:

| New match | Case | Result |
|---|---|---|
| 18:00-20:00 | exact same time | conflict |
| 19:00-21:00 | partial overlap | conflict |
| 19:00-22:00 | starts during | conflict |
| 17:00-19:00 | ends during | conflict |
| 17:00-21:00 | existing inside new | conflict |
| 20:00-22:00 | back-to-back | allowed (`20:00 < 20:00` is false) |
| 10:00-12:00 | separate | allowed |

`checkConflict()` in `MatchController.cpp`:
1. **Database:** load Scheduled matches on the same date, keep the ones where either team plays.
2. **Service:** `ScheduleService::findConflicts()` loops over them with the rule above (O(n)).
3. If any overlap -> 400 `Scheduling conflict: <team> already has a match during this time.`

Cancelled matches never block a team. On reschedule the match is excluded from its own check.

### Two bookings at the same moment
The check and the save run inside one `std::mutex` lock, so they happen as one step. Test: 20 simultaneous
requests for the same team and time gave **1 created and 19 rejected**. Limit: the lock protects one server
process. If you ran several copies of the server on one database you would need a database-level rule.

### Cheapest / fastest flight (`services/TravelService.cpp`)
A linear scan: remember the first flight, replace it whenever a better one appears. O(n) time, O(1) space.
`sortedByPrice()` uses `std::sort` with a lambda (O(n log n)) and sorts the flight list shown on screen.

### STL used and why
| Tool | Where | Why |
|---|---|---|
| `vector` | lists of matches / flights | grows by itself, fast to loop |
| `unordered_map` | `findAllConflicts` groups matches by date; `loadTeams` finds a team by id | O(1) average lookup instead of searching |
| `sort` + lambda | sorting teams, matches, flights | needs a custom "which comes first" rule |
| `optional` | `Database::findById` | "found or not found" without special values |
| `mutex` | `MatchController` | one booking at a time |

### Libraries
- **cpp-httplib** - the web server, one header file, no installation.
- **nlohmann/json** - JSON in C++, one header file.
- **MongoDB C driver** (`libmongoc`) - installed with `apt`. Documents travel as JSON text, which keeps `MongoDB.cpp` short.

---

## 4. API (same as before)

| Method | URL | Purpose |
|---|---|---|
| GET / POST | /api/teams | list / create |
| GET / PUT / DELETE | /api/teams/:id | view / edit / delete (blocked while the team has matches) |
| GET / POST | /api/players (?teamId=) | list / add |
| PUT / DELETE | /api/players/:id | edit / delete |
| GET / POST | /api/matches (?upcoming=true) | list / schedule (conflict check) |
| PUT / DELETE | /api/matches/:id | reschedule or cancel (conflict check again) / delete |
| GET | /api/matches/conflicts | number of clashing matches (dashboard) |
| GET / POST | /api/flights (?matchId=) | list / add |
| DELETE | /api/flights/:id | delete |
| GET | /api/flights/cheapest, /fastest (?matchId=) | best flight |

Responses: `{ "success": true, "data": ... }` or `{ "success": false, "message": "..." }`.
Codes: 200, 201, 400 (bad input / conflict / bad id), 404, 500 (generic message, details only in the server log).

---

## 5. Deploy

**1. Database - MongoDB Atlas:** create a free cluster, a database user (letters and numbers in the password), allow `0.0.0.0/0` in Network Access, copy the connection string and put `/teamsphere` before the `?`.

**2. Backend - Render (Docker):**
1. New -> Web Service -> your GitHub repo.
2. **Root Directory:** `backend` - **Runtime / Language:** `Docker` (Render finds `backend/Dockerfile`).
3. Environment variable: `MONGO_URI` = your Atlas string. (No build or start command is needed; the Dockerfile does it.)
4. Deploy. The first build compiles the C++ code and takes a few minutes. Then open `https://YOUR-NAME.onrender.com/api/teams`: you should see `{"data":[],"success":true}`.

**3. Frontend - Vercel (unchanged):** Root Directory `frontend`, Framework Vite, variable `VITE_API_URL` = `https://YOUR-NAME.onrender.com/api`, then deploy (redeploy after changing the variable).

Leave `CLIENT_URL` empty while learning (any site may call the API); set it to your Vercel address later if you want to lock it down.

---

## 6. Honest limits (say these before you are asked)
- No login: anyone with the link can change data.
- Matches cannot run past midnight (end must be after start on the same date).
- The booking lock covers one server process only.
- Flights are typed in by hand (no real flight API).
- Windows needs WSL or Docker to run the real MongoDB build.
- The free Render plan sleeps when idle, so the first request can take about a minute.

## 7. Questions interviewers ask about this version
- **Why a C++ backend?** To show systems-level skills: classes, STL, memory-safe patterns, and a clean split into models, controllers, services and database.
- **Why the C driver and not mongocxx?** `libmongoc` installs with one `apt` command; mongocxx has no Ubuntu package and must be built from source.
- **Why an interface (`Database.h`) with two implementations?** The controllers do not care where data lives, so the same code runs on MongoDB and on the in-memory test database.
- **How are errors handled?** Code throws `AppError(status, message)`; `safe()` catches it and sends JSON. Anything unexpected is logged and becomes a generic 500.
- **What does `std::stoi` risk, and what did you do instead?** It throws on bad text. `parseTime()` checks the format first, so bad times become a 400 and never crash the server.
