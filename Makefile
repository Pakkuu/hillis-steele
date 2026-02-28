CXX = clang++
CXXFLAGS = -std=c++17 -Wall

my-sum: my-sum.cpp
	@$(CXX) $(CXXFLAGS) my-sum.cpp -o my-sum

run: my-sum
	@./my-sum 8 4 A.txt B.txt


.PHONY: run