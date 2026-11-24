#pragma once
#include "game_logic.h"
#include <array>

namespace shop {
// Pixel coordinates in the 960 x 640 shop interior.
// The image itself is a 1536x1024 painting scaled to the full viewport.
struct Prop {
    int srcX,srcY,srcW,srcH;
    float x,y,w,h; // destination in interior screen coordinates
    float footY;   // occlusion sorting: behind hero below this Y
    bool solid;
};
inline constexpr std::array<Prop,13> furniture{{
    // Counter, upper shelves, produce, tables and smaller supplies.
    {35,37,711,143, 310,250,340,102,315,true},  // counter, back of store
    {24,170,225,218, 89,149,137,161,299,true},  // jars
    {260,170,219,218, 251,147,123,155,296,true}, // books
    {486,170,218,218, 587,147,122,155,296,true}, // potions
    {724,170,205,218, 735,145,138,158,299,true}, // goods
    {36,405,318,158, 103,378,176,113,478,true}, // produce stand
    {376,415,246,145, 693,388,151,100,475,true}, // small desk
    {23,595,195,100, 112,468,112,69,528,false}, // decorative rug: walkable
    {759,579,108,124, 99,320,62,88,397,true}, // barrel
    {885,579,102,121, 813,328,62,87,405,true}, // barrel
    {1060,587,117,107, 764,475,79,73,545,true}, // crate
    {536,712,137,99, 657,481,76,66,535,true}, // table
    {1246,717,112,100, 235,494,75,66,552,true} // chest
}};
struct Rect {float x,y,w,h;};
inline constexpr float MIN_X=42.f,MAX_X=918.f,MIN_Y=140.f,MAX_Y=602.f;
inline constexpr game::Vec2 ENTRANCE{480.f,592.f};
inline constexpr game::Vec2 SHOPKEEPER{480.f,240.f};
inline constexpr game::Vec2 ARRIVAL{480.f,535.f};
inline bool collides(game::Vec2 position,float radius=12.f) {
    if(position.x-radius<MIN_X||position.x+radius>MAX_X||
       position.y-radius<MIN_Y||position.y+radius>MAX_Y)return true;
    for(const auto& p:furniture){
        if(!p.solid)continue;
        // Only the furniture footprint is solid (not decorative height).
        const float left=p.x+4.f,right=p.x+p.w-4.f;
        const float bottom=p.y+p.h-4.f;
        const float top=p.y+p.h*0.57f;
        if(position.x+radius>left && position.x-radius<right &&
           position.y+radius>top && position.y-radius<bottom)return true;
    }
    return false;
}
inline void move(game::Vec2& pos,game::Vec2 velocity,float dt,float radius=12.f){
    game::Vec2 next{pos.x+velocity.x*dt,pos.y};
    if(!collides(next,radius))pos.x=next.x;
    next={pos.x,pos.y+velocity.y*dt};
    if(!collides(next,radius))pos.y=next.y;
}
inline bool nearDoor(game::Vec2 p){return game::withinRange(p,ENTRANCE,52.f);}
inline bool nearMerchant(game::Vec2 p){return game::withinRange(p,{480.f,342.f},95.f);}
}
