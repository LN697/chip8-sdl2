CXX = g++

SRC = main.cc

OUT = main

LDFLAGS = `sdl2-config --cflags --libs`

CXXFLAGS = -std=c++23 -Wall -Werror -Wextra

compile:
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

run:	compile
	./$(OUT) > run.log

runtest:	compile
	./$(OUT) IBM\ Logo.ch8 > run.log

clear:
	rm -rf $(OUT)
	rm -rf run.log
