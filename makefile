main: main.cpp
	g++ main.cpp -Wall -Wextra -pg -g -O3 --std=c++23 -o main

test: main
	./main
