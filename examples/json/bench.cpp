#include <iostream>
#include <sstream>
#include <fstream>
#include <print>

#include "json26.hpp"

struct metadata { int count; };
struct twittersearch { metadata search_metadata; };


int main(void)
{
    std::ifstream twit_file("twitter.json");
    std::stringstream sstr;
    sstr << twit_file.rdbuf();
    std::string data = sstr.str();

    for(int i=10000;i>=0;i--) {
        twittersearch search;
        if (!json26::parse<twittersearch>( search, data)) {
            std::cerr<<"FAILED TWIT PARSE!\n";
            return -1;
        }
        if (i==0)
            std::cout << search.search_metadata.count << " results." << std::endl;
 
        if (i==0)
            std::cout << search.search_metadata.[: ^^metadata::count :] << " results." << std::endl;

    }
    return 0;
}						