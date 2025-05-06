CXX = g++
CXXFLAGS = -Wall -std=c++17 -pthread
LDFLAGS = -lws2_32
TARGETS = chat_server.exe chat_client.exe

all: $(TARGETS)

chat_server.exe: chat_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

chat_client.exe: chat_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	del /F $(TARGETS)