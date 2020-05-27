
all:
	g++ src/tbdmud_server.cpp -pthread -std=c++17 -o tbdmud_server

debug:
	g++ src/tbdmud_server.cpp -pthread -std=c++17 -o tbdmud_server -g

ddebug:
	g++ src/tbdmud_server.cpp -pthread -std=c++17 -o tbdmud_server -g -DDEBUG
