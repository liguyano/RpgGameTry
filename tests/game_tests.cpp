#include "game_logic.h"
#include "village_map.h"
#include "shop_map.h"
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
    // Main water silhouette must be fixed; only particle motion moves downward.
    for(int tick=0;tick<40;tick++)
        assert(village::fountainWaterFrame(float(tick)/6.f)==4);
    assert(village::blocked(village::pos(750,540))); // original lower footprint preserved
    assert(village::blocked(village::pos(750,465))); // expanded upward edge
    assert(!village::blocked(village::pos(750,445))); // beyond the expanded edge and player radius
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
    assert(!village::blocked(village::pos(1250,375))); // upper bridge center
    // Continuous footpath across the east bridge, including both landings.
    for(int x=1175;x<=1375;x+=10)
        assert(!village::blocked(village::pos(float(x),375.f),8.f));
    auto crossing=village::pos(1175.f,375.f);
    village::move(crossing,{220.f*village::SCALE,0.f},1.f,8.f);
    assert(crossing.x>village::pos(1360.f,375.f).x);
    // The lower east/mill bridge must also have a usable corridor.
    for(int x=1200;x<=1375;x+=10)
        assert(!village::blocked(village::pos(float(x),750.f),8.f));

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
    // General store entrance, walkable aisle, walls, and furniture collisions.
    assert(!shop::collides(shop::ARRIVAL));
    assert(shop::nearDoor(shop::ENTRANCE));
    assert(!shop::nearDoor(shop::ARRIVAL));
    assert(shop::nearMerchant({480.f,369.f}));
    assert(shop::collides({480.f,315.f})); // sales counter
    assert(shop::collides({140.f,281.f})); // shelving
    assert(shop::collides({30.f,320.f})); // room perimeter
    // Formerly the top two thirds of barrels and cabinets were walk-through.
    assert(shop::collides({129.f,337.f})); // left barrel: upper body
    assert(shop::collides({843.f,343.f})); // right barrel: upper body
    assert(shop::collides({798.f,488.f})); // crate above the ground line
    assert(shop::collides({280.f,167.f})); // tall bookcase front
    assert(!shop::collides({480.f,480.f})); // clear walkway below counter
    assert(!shop::collides({480.f,535.f})); // entry spawn stays clear
    assert(shop::furniture[7].solid==false); // rug has no air wall
    const auto barrelFootprint=shop::footprint(8);
    assert(barrelFootprint.y<350.f);
    assert(shop::sortY(8)>barrelFootprint.y);
    assert(!shop::collides({480.f,410.f})); // central aisle
    Vec2 shopWalker{480.f,410.f};
    shop::move(shopWalker,{0.f,-150.f},1.f/5.f);
    assert(!shop::collides(shopWalker));
    std::cout << "All game, village and shop collision tests passed!\n";
}
