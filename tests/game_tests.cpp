#include "game_logic.h"
#include "village_map.h"
#include "shop_map.h"
#include "equipment.h"
#include "hero_animation.h"
#include "combat.h"
#include "enemy_tactics.h"
#include "east_battle_map.h"
#include "footstep_particles.h"
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
    // 9 authentic poses per direction, without an artificial 10th pose.
    assert(hero_anim::DIRECTIONS==8 && hero_anim::FRAMES==9);
    assert(hero_anim::direction({0.f,1.f})==0);
    assert(hero_anim::direction({-0.707f,0.707f})==1);
    assert(hero_anim::direction({-1.f,0.f})==2);
    assert(hero_anim::direction({-0.707f,-0.707f})==3);
    assert(hero_anim::direction({0.f,-1.f})==4);
    assert(hero_anim::direction({0.707f,-0.707f})==5);
    assert(hero_anim::direction({1.f,0.f})==6);
    assert(hero_anim::direction({0.707f,0.707f})==7);
    assert(hero_anim::frame(0.f,true,0)==0);
    assert(hero_anim::frame(1.f,false,6)==0);
    assert(hero_anim::frame(float(hero_anim::FRAMES)/12.f,true,0)==0);
    assert(hero_anim::frame(float(hero_anim::FRAMES)/12.f,true,6)==0);
    assert(hero_anim::frame(.5f,true,0)==hero_anim::frame(.5f,true,6));
    assert(hero_anim::frame(.1f,true,0)==1);
    assert(hero_anim::frame(.1f,true,6)==1);
    assert(hero_anim::GRID_W==1152 && hero_anim::GRID_H==1024);
    assert(hero_anim::SHEET_W==1402 && hero_anim::SHEET_H==1122);
    assert(hero_anim::ROWS[0].last==158);
    assert(hero_anim::ROWS[7].last==1086);
    assert(hero_anim::COLUMNS[0].first==40);
    assert(hero_anim::COLUMNS[8].last==1378);
    for(int direction=0;direction<hero_anim::DIRECTIONS;direction++)
        for(int pose=0;pose<hero_anim::FRAMES;pose++){
            const auto crop=hero_anim::crop(direction,pose);
            assert(crop.x>=0.f && crop.y>=0.f);
            assert(crop.w>0.f && crop.h>0.f);
            assert(crop.x+crop.w<=float(hero_anim::SHEET_W));
            assert(crop.y+crop.h<=float(hero_anim::SHEET_H));
            assert(crop.w<=hero_anim::CANVAS_W);
            assert(crop.h<=hero_anim::CANVAS_H);
            const auto cell=hero_anim::gridCrop(direction,pose);
            assert(cell.x==float(pose*128));
            assert(cell.y==float(direction*128));
            assert(cell.w==128.f && cell.h==128.f);
            assert(cell.x+cell.w<=float(hero_anim::GRID_W));
            assert(cell.y+cell.h<=float(hero_anim::GRID_H));
            assert(hero_anim::visualScale(direction)==1.f);
        }
    // Sword hits only within a shorter 70-degree cone in front of the hero.
    assert(combat::swordHits({0.f,0.f},{1.f,0.f},{40.f,0.f}));
    assert(combat::swordHits({0.f,0.f},{1.f,0.f},{45.f,20.f}));
    assert(!combat::swordHits({0.f,0.f},{1.f,0.f},{35.f,30.f}));
    assert(combat::swordHits({0.f,0.f},{1.f,0.f},{60.f,0.f}));
    assert(!combat::swordHits({0.f,0.f},{1.f,0.f},{61.f,0.f}));
    assert(!combat::swordHits({0.f,0.f},{1.f,0.f},{0.f,0.f}));
    assert(!combat::swordHits({0.f,0.f},{1.f,0.f},{-40.f,0.f}));
    assert(!combat::swordHits({0.f,0.f},{1.f,0.f},{0.f,45.f}));
    assert(!combat::swordHits({0.f,0.f},{1.f,0.f},{90.f,0.f}));
    assert(combat::slashFrame(combat::SWING_DURATION,4)==0);
    assert(combat::slashFrame(0.f,4)==3);
    assert(combat::ENEMY_WINDUP>.3f);
    assert(combat::ENEMY_RECOVERY>combat::ENEMY_WINDUP);
    assert(combat::ENEMY_STAGGER>0.f);
    // Eastern battle map: the left wooden crossing is actually traversable.
    assert(!east_map::blocked(east_map::ENTRY));
    assert(!east_map::blocked(east_map::CAMP_TRIGGER));
    assert(east_map::blocked({120.f,275.f})); // river outside bridge
    assert(east_map::blocked({442.f,392.f})); // central boulder
    assert(east_map::blocked({610.f,510.f})); // bottom rocks
    assert(!east_map::blocked({610.f,290.f})); // combat clearing
    for(int x=35;x<=195;x+=5)
        assert(!east_map::blocked({float(x),381.f}));
    game::Vec2 crossingBattle{194.f,381.f};
    east_map::move(crossingBattle,{-155.f,0.f},1.f,10.f);
    assert(east_map::atVillageExit(crossingBattle));
    for(const game::Vec2 spawn:{
        game::Vec2{430.f,450.f},game::Vec2{740.f,319.f},
        game::Vec2{600.f,291.f},game::Vec2{750.f,450.f},
        game::Vec2{705.f,500.f},game::Vec2{645.f,420.f},
        game::Vec2{860.f,390.f},game::Vec2{867.f,475.f},
        game::Vec2{550.f,510.f}})
        assert(!east_map::blocked(spawn));
    // Colliding with the boulder blocks the movement without tunnelling.
    game::Vec2 battleWalker{350.f,390.f};
    east_map::move(battleWalker,{160.f,0.f},1.f);
    assert(!east_map::blocked(battleWalker));
    assert(battleWalker.x<400.f);

    // Walking particles follow the feet, have a bounded pool, and decay.
    footsteps::Emitter emitter{};
    assert(footsteps::activeCount(emitter)==0);
    footsteps::update(emitter,1,{200.f,300.f},{200.f,300.f},.03f);
    assert(footsteps::activeCount(emitter)==0);
    footsteps::update(emitter,1,{200.f,300.f},{225.f,300.f},.03f);
    assert(footsteps::activeCount(emitter)==2);
    assert(emitter.particles[0].position.x>190.f);
    // Same scene + no displacement: no new emission, but existing dust fades.
    const int previous=footsteps::activeCount(emitter);
    footsteps::update(emitter,1,{225.f,300.f},{225.f,300.f},.04f);
    assert(footsteps::activeCount(emitter)==previous);
    for(int i=0;i<40;i++)
        footsteps::update(emitter,1,{225.f,300.f},{225.f,300.f},.03f);
    assert(footsteps::activeCount(emitter)==0);
    footsteps::update(emitter,0,{200.f,300.f},{240.f,300.f},.03f);
    assert(footsteps::activeCount(emitter)==0); // indoor floor is clean
    for(int i=0;i<90;i++)
        footsteps::update(emitter,2,{250.f,300.f},
                          {275.f,300.f},.03f);
    assert(footsteps::activeCount(emitter)<=footsteps::CAPACITY);
    // Large teleports, such as changing scenes, must not create smoke clouds.
    footsteps::reset(emitter,2);
    footsteps::update(emitter,2,{0.f,0.f},{400.f,400.f},.03f);
    assert(footsteps::activeCount(emitter)==0);
    // Tactical goblins must take distinct roles and surround instead of pile.
    assert(enemy_ai::role(0)==enemy_ai::Role::Vanguard);
    assert(enemy_ai::role(1)==enemy_ai::Role::FlankLeft);
    assert(enemy_ai::role(2)==enemy_ai::Role::FlankRight);
    assert(enemy_ai::role(3)==enemy_ai::Role::Harasser);
    const Vec2 fighter{480.f,370.f};
    const Vec2 look{1.f,0.f};
    const Vec2 front=enemy_ai::slot(fighter,look,0);
    const Vec2 left=enemy_ai::slot(fighter,look,1);
    const Vec2 right=enemy_ai::slot(fighter,look,2);
    const Vec2 rear=enemy_ai::slot(fighter,look,3);
    assert(front.x>fighter.x);
    assert(left.y<fighter.y && right.y>fighter.y);
    assert(rear.x<fighter.x);
    for(size_t index=0;index<8;index++)
        for(size_t other=index+1;other<8;other++)
            assert(game::distanceSquared(
                enemy_ai::slot(fighter,look,index),
                enemy_ai::slot(fighter,look,other))>20.f*20.f);
    struct DummyEnemy{Vec2 pos;int hp;};
    const std::array<DummyEnemy,3> opponents{{{{100.f,100.f},3},
                                               {{110.f,100.f},3},
                                               {{200.f,100.f},3}}};
    const Vec2 separation=enemy_ai::separation(opponents,0);
    assert(separation.x<0.f);
    assert(enemy_ai::length(separation)>0.f);
    const Vec2 detour=enemy_ai::steer(opponents,0,{180.f,100.f},look);
    assert(enemy_ai::length(detour)>.1f);
    assert(enemy_ai::canWindup(30.f,0.f,false,0));
    assert(!enemy_ai::canWindup(30.f,0.f,false,2));
    assert(!enemy_ai::canWindup(45.f,0.f,false,0));
    assert(!enemy_ai::canWindup(30.f,.5f,false,0));
    // Starter gear should be weak and ordinary, and swapping gear must
    // immediately affect both incoming and outgoing combat damage.
    std::array<int,6> equipped{{0,0,0,18,0,0}};
    std::array<int,24> bag{};
    bag[0]=17; // worn sword from father
    assert(equipment::attack(equipped)==2);
    assert(equipment::defense(equipped)==1);
    assert(equipment::hitDamage(equipment::ENEMY_ATTACK,equipment::defense(equipped))==13);
    assert(equipment::equip(bag,equipped,0));
    assert(equipped[0]==17 && bag[0]==0);
    assert(equipment::attack(equipped)==5);
    assert(equipment::hitDamage(equipment::attack(equipped),equipment::ENEMY_DEFENSE)==4);
    bag[1]=1; // iron sword replaces the worn sword
    assert(equipment::equip(bag,equipped,1));
    assert(equipped[0]==1 && bag[1]==17);
    assert(equipment::attack(equipped)==10);
    bag[2]=7; // chain armor replaces the tunic
    assert(equipment::equip(bag,equipped,2));
    assert(equipped[3]==7 && bag[2]==18);
    assert(equipment::defense(equipped)==6);
    assert(equipment::hitDamage(equipment::ENEMY_ATTACK,equipment::defense(equipped))==8);
    assert(equipment::hitDamage(2,100)==1); // minimum 1 damage
    assert(!equipment::equip(bag,equipped,23)); // no equipping empty slots
    assert(equipment::addToBag(bag,12));
    assert(!equipment::addToBag(bag,0));
    assert(equipment::item(17).attack==3 && equipment::item(18).defense==1);
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
