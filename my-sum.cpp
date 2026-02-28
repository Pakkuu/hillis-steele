#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <numeric>

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

void prefix_sum(std::vector<int>& nums) {
    std::vector<int> result(nums.size());
    std::inclusive_scan(nums.begin(), nums.end(), result.begin());
    nums = result;
    return;
}

void write_to_file(std::ostream& out, std::vector<int>& nums) {
    for (size_t i = 0; i < nums.size(); i++) {
        if (i > 0) {
            out << ' ';
        }
        out << nums[i];
    }
    return;
}

int main(int argc, char* argv[]) {

    //open input stream
    std::ifstream in(argv[3]);
    if (!in) {return 1;} //check file exists and can be opened

    int expected_count = std::stoi(argv[1]); //expected # of elements to process
    std::vector<int> nums;

    if (!read_and_validate_input(in, expected_count, nums)) {
        return 1;
    }

    prefix_sum(nums);

    //open output stream
    std::ofstream out(argv[4]);
    if (!out) {return 1;}

    write_to_file(out, nums);

    return 0;
}