#pragma once
#include "game_logic.h"
#include <algorithm>
#include <cmath>

namespace hero_anim {
// Rows in assets/hero.png: down, south-west, south-east, west, east, north.
// The sheet deliberately has no north-west or north-east row.
constexpr int DIRECTIONS=6;
constexpr int FRAMES=12;
constexpr float FPS=12.f;
constexpr float SHEET_W=1536.f;
constexpr float SHEET_H=1024.f;
constexpr float LABEL_W=156.f;
constexpr float HEADER_H=46.f;
constexpr float GRID_BOTTOM=986.f;
struct Crop {float x,y,w,h;};
inline int direction(game::Vec2 facing){
    if(facing.y>0.25f){
        if(facing.x<-.25f)return 1;
        if(facing.x>.25f)return 2;
        return 0;
    }
    if(facing.y<-.25f){
        // Upper diagonals share the north row, since the sheet has six rows.
        if(std::abs(facing.x)<.85f)return 5;
    }
    if(facing.x<-.25f)return 3;
    if(facing.x>.25f)return 4;
    return facing.y<0.f?5:0;
}
inline int frame(float animationSeconds,bool moving){
    if(!moving)return 0;
    return static_cast<int>(std::floor(std::max(0.f,animationSeconds)*FPS))%FRAMES;
}
inline Crop crop(int row,int pose){
    const float cellW=(SHEET_W-LABEL_W)/FRAMES;
    const float cellH=(GRID_BOTTOM-HEADER_H)/DIRECTIONS;
    // Use a slightly inset crop to exclude dark divider lines from the atlas.
    constexpr float insetX=3.f,insetY=3.f;
    return {LABEL_W+pose*cellW+insetX,
            HEADER_H+row*cellH+insetY,
            cellW-2.f*insetX,cellH-2.f*insetY};
}
} // namespace hero_anim
