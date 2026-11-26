#pragma once
#include "game_logic.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace hero_anim {
constexpr int DIRECTIONS=8;
constexpr int FRAMES=9;
constexpr float FPS=10.f;

// Original, unresized 9-column x 8-row image. Pixel positions were
// measured from the source image (1402 x 1122), not guessed via
// imageWidth/9 or imageHeight/8. The latter cuts off several boots.
constexpr int SHEET_W=1402;
constexpr int SHEET_H=1122;
constexpr int CANVAS_W=138;
constexpr int CANVAS_H=158;
struct Crop {float x,y,w,h;};
struct Span {int first,last;}; // [first,last), source pixel bounds
inline constexpr std::array<Span,FRAMES> COLUMNS{{
    {40,166},{195,319},{345,471},{497,626},{647,777},
    {798,924},{948,1076},{1099,1227},{1249,1378}
}};
inline constexpr std::array<Span,DIRECTIONS> ROWS{{
    {0,158},{158,296},{296,428},{428,554},
    {554,686},{686,814},{814,946},{946,1086}
}};

// Future equipment art may use a standard regular 128px grid with 9 columns.
constexpr int GRID_CELL=128;
constexpr int GRID_W=GRID_CELL*FRAMES;
constexpr int GRID_H=GRID_CELL*DIRECTIONS;
inline constexpr Crop gridCrop(int row,int pose){
    return {float(pose*GRID_CELL),float(row*GRID_CELL),
            float(GRID_CELL),float(GRID_CELL)};
}
inline Crop crop(int row,int pose){
    const Span x=COLUMNS[static_cast<size_t>(pose)];
    const Span y=ROWS[static_cast<size_t>(row)];
    return {float(x.first),float(y.first),
            float(x.last-x.first),float(y.last-y.first)};
}
inline int direction(game::Vec2 facing){
    constexpr float diagonal=.30f;
    const float x=facing.x,y=facing.y;
    if(y>diagonal){
        if(x< -diagonal)return 1;
        if(x> diagonal)return 7;
        return 0;
    }
    if(y< -diagonal){
        if(x< -diagonal)return 3;
        if(x> diagonal)return 5;
        return 4;
    }
    if(x<0.f)return 2;
    if(x>0.f)return 6;
    return y<0.f?4:0;
}
inline constexpr float visualScale(int){return 1.f;}
inline int frame(float seconds,bool moving,int direction){
    if(!moving)return 0;
    // Keep the slower gait only when travelling straight up or down.
    (void)direction;
    const float fps=FPS;
    return static_cast<int>(std::floor(std::max(0.f,seconds)*fps))%FRAMES;
}
} // namespace hero_anim
