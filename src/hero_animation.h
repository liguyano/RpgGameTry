#pragma once
#include "game_logic.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace hero_anim {
constexpr int DIRECTIONS=8;
constexpr int FRAMES=10;
constexpr float FPS=14.f;

// Measured from the actual 1448 x 1086 hero.png, not from equal-sized
// eighths of its height. On this image the first row runs to y=147,
// whereas 1086/8=135.75 would cut off the feet.
// Coordinates are divided by the reference size for resolution independence.
constexpr float SHEET_W=1448.f;
constexpr float SHEET_H=1086.f;
constexpr int CANVAS_W=116;
constexpr int CANVAS_H=154;
struct Crop {float x,y,w,h;};
struct Span {int first,last;}; // [first,last)
inline constexpr std::array<Span,FRAMES> COLUMNS{{
    {39,145}, {180,290}, {324,430}, {465,575}, {604,710},
    {742,852}, {880,989}, {1017,1128}, {1161,1265}, {1306,1407}
}};
inline constexpr std::array<Span,DIRECTIONS> ROWS{{
    {4,154}, {154,295}, {295,429}, {429,559},
    {559,696}, {696,828}, {828,956}, {956,1086}
}};
inline int direction(game::Vec2 facing){
    constexpr float diagonal=0.30f;
    const float x=facing.x,y=facing.y;
    if(y>diagonal){
        if(x< -diagonal)return 1; // SW
        if(x> diagonal)return 7;  // SE
        return 0;                  // S
    }
    if(y< -diagonal){
        if(x< -diagonal)return 3; // NW
        if(x> diagonal)return 5;  // NE
        return 4;                  // N
    }
    if(x<0.f)return 2;            // W
    if(x>0.f)return 6;            // E
    return y<0.f?4:0;
}
inline int frame(float seconds,bool moving){
    if(!moving)return 0;
    return static_cast<int>(std::floor(std::max(0.f,seconds)*FPS))%FRAMES;
}
inline Crop crop(int row,int pose){
    const Span x=COLUMNS[static_cast<size_t>(pose)];
    const Span y=ROWS[static_cast<size_t>(row)];
    return {float(x.first)/SHEET_W,float(y.first)/SHEET_H,
            float(x.last-x.first)/SHEET_W,float(y.last-y.first)/SHEET_H};
}
} // namespace hero_anim
