#include <numeric>
#include <vector>
#include <iostream>

int main() {

    std::vector<int> nums = {1, 2, 3, 4, 5};
    std::vector<int> result(5);

    std::inclusive_scan(nums.begin(), nums.end(), result.begin());

    for(int i = 0; i < result.size(); i++){
        std::cout << result[i] << std::endl;
    }

    return 0;
}