#pragma once
#include "game_logic.h"
#include <algorithm>
#include <cmath>

namespace hero_anim {
// Exact layout of the current hero.png: 8 direction rows, 10 poses per row,
// no title/label margins. Row order is clockwise from South toward South-East.
constexpr int DIRECTIONS=8;
constexpr int FRAMES=10;
constexpr float FPS=14.f;
// Coordinates normalized to the complete image, so cropping works at any PNG size.
struct Crop {float x,y,w,h;};
inline int direction(game::Vec2 facing){
    constexpr float diagonal=0.30f;
    const float x=facing.x,y=facing.y;
    if(y>diagonal){
        if(x< -diagonal)return 1;  // SW
        if(x> diagonal)return 7;   // SE
        return 0;                   // S
    }
    if(y< -diagonal){
        if(x< -diagonal)return 3;  // NW
        if(x> diagonal)return 5;   // NE
        return 4;                   // N
    }
    if(x<0.f)return 2;             // W
    if(x>0.f)return 6;             // E
    return y<0.f?4:0;
}
inline int frame(float seconds,bool moving){
    if(!moving)return 0;
    return static_cast<int>(std::floor(std::max(0.f,seconds)*FPS))%FRAMES;
}
inline Crop crop(int row,int pose){
    const float cellW=1.f/float(FRAMES),cellH=1.f/float(DIRECTIONS);
    // Slightly inset the edges to avoid including any inter-cell grid seam.
    constexpr float marginX=0.001f,marginY=0.001f;
    return {float(pose)*cellW+marginX,float(row)*cellH+marginY,
            cellW-2.f*marginX,cellH-2.f*marginY};
}
} // namespace hero_anim
