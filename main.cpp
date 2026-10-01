#include <iostream>
#include <vector>

#include "unit.hpp"
#include "move.hpp"


std::vector<Unit> get_units(){
    Unit unit1(4, 2, {0,0});
    Unit unit2(5, 6, {0,1});
    Unit unit3(1, 2, {1,1});
    Unit unit4(3, 1, {5,5});

    return {unit1, unit2, unit3, unit4};
}

int main(){
    std::vector<Unit> units = get_units();

    for (int i = 0; i < units.size(); i++){
        units[i].print();
    }

    std::expected<MoveRequest, std::string> result =  units[0].make_movement({5,5});
    if (!result.has_value()){
        std::cout << result.error() << "\n";
    }
}