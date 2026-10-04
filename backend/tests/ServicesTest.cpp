// Tests for the core logic. Build and run with:  make test
#include <iostream>
#include "services/Services.h"
#include "utils/Utils.h"

int passed = 0, failed = 0;

void check(const std::string& name, bool ok) {
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << name << std::endl;
    ok ? passed++ : failed++;
}

static Flight flight(const std::string& id, double price, double minutes) {
    Flight f;
    f.id = id;
    f.price = price;
    f.durationMinutes = minutes;
    return f;
}

int main() {
    int s = 0, e = 0;
    // Existing match 18:00-20:00 = 1080-1200
    auto conflict = [](int ns, int ne) { return ScheduleService::hasConflict(ns, ne, 1080, 1200); };

    std::cout << "Match conflict rule (existing match 18:00-20:00)" << std::endl;
    check("exact same time        18:00-20:00", conflict(1080, 1200));
    check("partial overlap        19:00-21:00", conflict(1140, 1260));
    check("new starts during      19:00-22:00", conflict(1140, 1320));
    check("new ends during        17:00-19:00", conflict(1020, 1140));
    check("existing inside new    17:00-21:00", conflict(1020, 1260));
    check("back-to-back after     20:00-22:00 is allowed", !conflict(1200, 1320));
    check("back-to-back before    16:00-18:00 is allowed", !conflict(960, 1080));
    check("completely separate    10:00-12:00 is allowed", !conflict(600, 720));

    std::cout << std::endl << "Lists and pairs" << std::endl;
    std::vector<MatchSlot> day = {MatchSlot("m1", 1080, 1200, "2026-10-18", "A", "B"),
                                  MatchSlot("m2", 1200, 1320, "2026-10-18", "A", "C")};
    check("19:00-21:00 clashes with m1 and m2", ScheduleService::findConflicts(1140, 1260, day).size() == 2);
    check("22:00-23:00 clashes with nothing", ScheduleService::findConflicts(1320, 1380, day).empty());
    check("back-to-back m1/m2 is not a clashing pair", ScheduleService::findAllConflicts(day).empty());
    day.push_back(MatchSlot("m3", 1140, 1260, "2026-10-18", "C", "D"));  // shares team C with m2
    check("m3 clashes with m2 (team C, overlapping time)", ScheduleService::findAllConflicts(day).size() == 1);
    day.push_back(MatchSlot("m4", 1140, 1260, "2026-10-19", "C", "D"));  // same teams but another day
    check("same teams on another date do not clash", ScheduleService::findAllConflicts(day).size() == 1);

    std::cout << std::endl << "Flights" << std::endl;
    std::vector<Flight> flights = {flight("a", 4500, 130), flight("b", 3800, 155), flight("c", 5200, 115)};
    check("cheapest is b (3800)", TravelService::cheapest(flights)->id == "b");
    check("fastest is c (115 min)", TravelService::fastest(flights)->id == "c");
    check("empty list gives nullptr", TravelService::cheapest({}) == nullptr && TravelService::fastest({}) == nullptr);
    check("sortedByPrice order is b, a, c", [&] {
        auto sorted = TravelService::sortedByPrice(flights);
        return sorted[0].id == "b" && sorted[1].id == "a" && sorted[2].id == "c";
    }());

    std::cout << std::endl << "Time and input checks" << std::endl;
    check("parseTime 18:00 = 1080", parseTime("18:00", s) && s == 1080);
    check("parseTime rejects 25:70", !parseTime("25:70", s));
    check("parseTime rejects abc", !parseTime("abc", s));
    check("parseTime rejects 9:00 (needs 09:00)", !parseTime("9:00", e));
    check("isNumber accepts '22' and 22", isNumber(json("22")) && isNumber(json(22)));
    check("isNumber rejects 'abc' and ''", !isNumber(json("abc")) && !isNumber(json("")));
    check("isBlank treats spaces as blank", isBlank(json("   ")) && !isBlank(json("x")));
    check("isValidId needs 24 hex characters", isValidId("0123456789abcdef01234567") && !isValidId("abc"));

    std::cout << std::endl << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
