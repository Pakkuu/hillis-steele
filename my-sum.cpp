#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

bool read_and_validate_input(std::istream& in, int expected_count, std::vector<int>& out) {
    std::string value;
    out.clear();

    while (in >> value) {
        std::istringstream iss(value);
        int n;
        if (iss >> n && iss.eof()){
            out.push_back(std::stoi(value));
        }
        else {
            std::cout << value << " is not a valid int" << std::endl;
            return false;
        }
    }

    //check # of elements
    if (static_cast<int>(out.size()) != expected_count) {
        std::cout << "Invalid number of elements" << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {

    std::ifstream in(argv[3]);
    if (!in) {return 1;} //check file exists and can be opened

    int expected_count = std::stoi(argv[1]);
    std::vector<int> data;

    if (!read_and_validate_input(in, expected_count, data)) {
        return 1;
    }

    for(int x: data) { 
        std::cout << x << std::endl;
    }

    return 0;
}