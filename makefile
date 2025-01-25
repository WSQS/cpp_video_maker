main: main.cpp
	g++ -s main.cpp -Wall -Wextra --std=c++23 -o main

test: main
	./main
