#include <iostream>
#include <sstream>
#include <fstream>
#include <print>
#include <map>

#include <stdlib.h>

#include "json26.hpp"

struct nonmov {
    int z=123;

    nonmov() = default;
    ~nonmov() = default;
    nonmov(const nonmov& other) = default;
    nonmov& operator=(const nonmov& other) = default;

    nonmov(nonmov&& other) = delete;
    nonmov& operator=(nonmov&& other) = delete;
};

struct testobjtype { 
    int x{};
    [[=json26::alias{"Aliased_y"}]]   
    int y{}; 
    [[=json26::ignore{}]]
    std::string password{};
    [[=json26::extra_props{}]]
    std::map<std::string,json26::value> extras;

    int count{}; 
    std::string some{};
    std::vector<nonmov> dx;
};

int main(void)
{
    std::string data = R"!!(
      {"x"  
      :  -123,"Alias\u0065d_y":33  , "count":44, "some":"world", "password":"should not be read", "extras":"not directly stored", "otherextra":123
      }
    )!!";

    testobjtype md { 101,102,"orig value",{},103, "dummy", {}};
    if(json26::parse(md, data)) {
        std::cout << "Parse done?!:" << md.count << " : " << md.x << " : " << md.y << " :" << md.some << ":" << md.password << "\n";
        for (auto&[k,v] : md.extras) {

            if (auto* sv = std::get_if<std::string>(&v)) {
                std::cout << "Extra str found:"<<k<<"="<<*sv<<"\n";
            } else if (auto* dv = std::get_if<double>(&v)) {
                std::cout << "Extra double found:"<<k<<"="<<*dv<<"\n";
            } else {
                std::cout << "Extra found of other type:"<<k<<"\n";
            }

        }
    }
    else
        std::cout << "Parse failed\n";
    
    
}
