#pragma once
#include "game_logic.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace east_map {
// Screen-space collision map for the wilderness painting in
// assets/gablingather.png. The image is scaled to the 960x640 game viewport.
// No enemies are baked into the image; they move over this environment.
struct Rect {float x,y,w,h;};
inline constexpr std::array<Rect,7> SOLIDS{{
    {0.f,0.f,960.f,112.f},     // tree line and cliffs along the north
    {0.f,551.f,960.f,89.f},    // dense lower forest / cliffs
    {394.f,354.f,98.f,79.f},  // large boulder in central clearing
    {595.f,477.f,78.f,65.f},  // lower rocky formation
    {751.f,481.f,95.f,76.f},  // ruins and right side rocks
    {337.f,188.f,96.f,89.f},  // collapsed columns in the upper west
    {821.f,117.f,139.f,59.f}  // cliff around the cave entry
}};
inline constexpr game::Vec2 ENTRY{194.f,381.f};
inline constexpr game::Vec2 CAMP_TRIGGER{762.f,292.f};
inline constexpr float EXIT_X=47.f;
inline constexpr float EXIT_Y_MIN=324.f, EXIT_Y_MAX=438.f;
// Left of the crossing is river, except the bridge connecting to the road.
inline bool isBridge(game::Vec2 p){
    return p.x>=18.f && p.x<174.f && p.y>=EXIT_Y_MIN && p.y<=EXIT_Y_MAX;
}
inline bool blocked(game::Vec2 p,float radius=10.f){
    if(p.x-radius<18.f||p.x+radius>942.f||
       p.y-radius<105.f||p.y+radius>558.f)return true;
    if(p.x<174.f && !isBridge(p))return true;
    for(const auto& r:SOLIDS){
        const float nearestX=game::clamp(p.x,r.x,r.x+r.w);
        const float nearestY=game::clamp(p.y,r.y,r.y+r.h);
        if(game::distanceSquared(p,{nearestX,nearestY})<radius*radius)
            return true;
    }
    return false;
}
inline void move(game::Vec2& position,game::Vec2 velocity,float dt,
                 float radius=10.f){
    // Several small moves avoid walking straight through a thin rock at speed.
    const int steps=std::max(1,int(std::ceil(
        std::max(std::abs(velocity.x),std::abs(velocity.y))*dt/5.f)));
    const float duration=dt/float(steps);
    for(int i=0;i<steps;i++){
        game::Vec2 xPos{position.x+velocity.x*duration,position.y};
        if(!blocked(xPos,radius))position.x=xPos.x;
        game::Vec2 yPos{position.x,position.y+velocity.y*duration};
        if(!blocked(yPos,radius))position.y=yPos.y;
    }
}
inline bool atVillageExit(game::Vec2 p){
    return p.x<EXIT_X && p.y>=EXIT_Y_MIN && p.y<=EXIT_Y_MAX;
}
} // namespace east_map
