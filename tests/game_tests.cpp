#include "game_logic.h"
#include "village_map.h"
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
    // The village's collision overlay (F3) must match these world-space zones.
    assert(village::blocked(village::pos(400,200))); // house
    assert(village::blocked(village::pos(1260,620))); // river
    assert(village::blocked(village::pos(110,850))); // cliff
    assert(village::blocked(village::pos(770,490))); // fountain
    assert(!village::blocked(village::pos(1250,375))); // bridge
    assert(!village::blocked(village::pos(700,565))); // village street
    assert(!village::blocked(village::pos(545,755))); // spawn
    assert(!village::blocked(village::pos(774,565))); // elder interaction
    assert(!village::blocked(village::pos(670,564))); // merchant interaction
    assert(!village::blocked(village::pos(971,767))); // guard interaction
    auto from=village::pos(700,565);
    village::move(from,{2000,0},0.4f);
    assert(!village::blocked(from)); // no high-speed tunnelling
    auto camera=village::camera({village::WORLD_W,village::WORLD_H});
    assert(camera.x == village::WORLD_W-WIDTH);
    assert(camera.y == village::WORLD_H-HEIGHT);
    std::cout << "All game and village collision tests passed!\n";
}
