# Build commands (Linux / macOS / WSL / Docker):
#   make teamsphere          real build, talks to MongoDB (needs: libmongoc-dev libbson-dev pkg-config)
#   make teamsphere-memory   build WITHOUT MongoDB (uses MONGO_URI=memory), nothing to install
#   make test                build and run the tests of the core logic
#   make clean
CXX      ?= g++
CXXFLAGS := -std=c++17 -O1 -Wall -I. -Ithird_party -pthread

SRCS := main.cpp $(wildcard controllers/*.cpp services/*.cpp database/*.cpp utils/*.cpp)
COMMON_OBJS := $(patsubst %.cpp,build/%.o,$(filter-out database/MongoDB.cpp,$(SRCS)))

MONGO_CFLAGS := $(shell pkg-config --cflags libmongoc-1.0 2>/dev/null)
MONGO_LIBS   := $(shell pkg-config --libs libmongoc-1.0 2>/dev/null)

all: teamsphere

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# MongoDB.cpp is compiled twice: once with MongoDB support, once without.
build/MongoDB_real.o: database/MongoDB.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -DUSE_MONGO $(MONGO_CFLAGS) -c $< -o $@

build/MongoDB_stub.o: database/MongoDB.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

teamsphere: $(COMMON_OBJS) build/MongoDB_real.o
	$(CXX) $^ -o $@ $(MONGO_LIBS) -pthread

teamsphere-memory: $(COMMON_OBJS) build/MongoDB_stub.o
	$(CXX) $^ -o $@ -pthread

test: tests/ServicesTest.cpp services/ScheduleService.cpp services/TravelService.cpp utils/Validation.cpp utils/ErrorHandler.cpp
	$(CXX) $(CXXFLAGS) $^ -o build/servicestest
	./build/servicestest

clean:
	rm -rf build teamsphere teamsphere-memory
