#include "ret26/named.hpp"

#include <iostream>

int main() {

    auto x = ret26::make_named<"hello","world">(12, 34.56);

    std::cout<<"Hello:"<<x.hello << "   World:" << x.world<<"\n";
    // prints:  Hello:12   World:34.56

    std::cout<<"Info on named type:"<<std::meta::display_string_of(^^decltype(x))<<"\n";

}