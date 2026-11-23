#include "game_logic.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace game;
    assert(distanceSquared({0,0},{3,4}) == 25);
    assert(withinRange({0,0},{3,4},5));
    assert(!withinRange({0,0},{3,4},4));
    assert(clamp(8.f,0.f,5.f)==5.f);
    assert(overlaps(0,0,10,10,9,9,5,5));
    assert(!overlaps(0,0,10,10,10,10,2,2));
    assert(damage(10,4)==6 && damage(2,5)==0 && damage(10,-5)==10);
    auto n=normalized({3,4});
    assert(std::fabs(n.x-0.6f)<0.0001f && std::fabs(n.y-0.8f)<0.0001f);
    auto z=normalized({0,0});
    assert(z.x==0 && z.y==0);
    std::cout << "All game logic tests passed!\n";
}
