#include <algorithm>
#include "services/Services.h"

const Flight* TravelService::cheapest(const std::vector<Flight>& flights) {
    if (flights.empty()) return nullptr;
    const Flight* best = &flights[0];  // assume the first is the cheapest...
    for (size_t i = 1; i < flights.size(); i++) {
        if (flights[i].price < best->price) best = &flights[i];  // ...and replace it when we find better
    }
    return best;
}

const Flight* TravelService::fastest(const std::vector<Flight>& flights) {
    if (flights.empty()) return nullptr;
    const Flight* best = &flights[0];
    for (size_t i = 1; i < flights.size(); i++) {
        if (flights[i].durationMinutes < best->durationMinutes) best = &flights[i];
    }
    return best;
}

std::vector<Flight> TravelService::sortedByPrice(const std::vector<Flight>& flights) {
    std::vector<Flight> copy = flights;
    std::sort(copy.begin(), copy.end(), [](const Flight& a, const Flight& b) { return a.price < b.price; });
    return copy;
}
