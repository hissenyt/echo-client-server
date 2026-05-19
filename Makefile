CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

all: echo-client echo-server

echo-client: echo-client.cpp
	$(CXX) $(CXXFLAGS) -o echo-client echo-client.cpp

echo-server: echo-server.cpp
	$(CXX) $(CXXFLAGS) -o echo-server echo-server.cpp

clean:
	rm -f echo-client echo-server

