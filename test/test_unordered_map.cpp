#include <iostream>
#include "unordered_map.hpp"

using namespace std;

int main() {
    xxw::unordered_map<std::string, int> mp;
    mp["january"] = 31;
    mp["february"] = 28;
    mp["march"] = 31;
    mp["april"] = 30;
    mp["may"] = 31;
    mp["june"] = 30;
    mp["july"] = 31;
    mp["august"] = 31;
    mp["september"] = 30;
    mp["october"] = 31;
    mp["november"] = 30;
    mp["december"] = 31;
    cout << mp.size() << endl;
    cout << "september -> " << mp["september"] << endl;
    cout << "april     -> " << mp["april"] << endl;
    cout << "june      -> " << mp["june"] << endl;
    cout << "november  -> " << mp["november"] << endl;
    cout << "february  -> " << mp["february"] << endl;
    
    mp.remove("february");
    cout << mp.size() << endl;
    cout << "february  -> " << mp["february"] << endl;
    return 0;
}