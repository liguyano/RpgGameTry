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
    assert(village::blocked(village::pos(403,304))); // lower half of cottage
    assert(village::blocked(village::pos(1260,620))); // river
    assert(!village::blocked(village::pos(110,850))); // old oversized cliff air wall removed
    // All obsolete market/fountain/wall rectangles must now be walkable.
    assert(village::blocked(village::pos(770,490))); // new fountain basin
    assert(village::blocked(village::fountainCenter())); // center of pool
    assert(village::blocked(village::pos(750,540))); // original lower footprint preserved
    assert(village::blocked(village::pos(750,465))); // expanded upward edge
    assert(!village::blocked(village::pos(750,455))); // beyond the expanded edge
    assert(!village::blocked(village::pos(750,450))); // formerly blocked position is now free
    assert(!village::blocked(village::pos(700,540))); // plaza still accessible
    assert(!village::blocked(village::pos(620,490))); // old west market stand
    assert(!village::blocked(village::pos(1080,470))); // old east market stand
    assert(!village::blocked(village::pos(900,640))); // old street obstruction
    assert(!village::blocked(village::pos(120,560))); // old decorative wall
    assert(!village::blocked(village::pos(100,150))); // old northwest forest polygon
    assert(!village::blocked(village::pos(1490,150))); // old northeast forest polygon
    assert(village::blocked(village::pos(872,440))); // lower half of oak
    assert(!village::blocked(village::pos(405,210))); // roof is passable / occluded visually
    assert(!village::blocked(village::pos(850,360))); // upper crown is passable
    assert(village::blocked(village::pos(400,260))); // house's lower half is solid
    assert(!village::blocked(village::pos(400,190))); // upper roof remains passable
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
