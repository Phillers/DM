all: DM
DM:  main.cpp DM.cpp DM.h
	g++ -std=c++11 main.cpp DM.cpp -o DM -lzmq -pthread

