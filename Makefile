CXX = clang++
CXXFLAGS = -std=c++17 -Wall

my-sum: my-sum.cpp
	$(CXX) $(CXXFLAGS) my-sum.cpp -o my-sum && ./my-sum