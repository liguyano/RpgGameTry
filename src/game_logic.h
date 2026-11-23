#pragma once
#include <algorithm>
#include <cmath>

namespace game {
constexpr int TILE = 32;
constexpr int COLS = 30;
constexpr int ROWS = 20;
constexpr int WIDTH = COLS * TILE;
constexpr int HEIGHT = ROWS * TILE;

struct Vec2 { float x = 0, y = 0; };
inline float distanceSquared(Vec2 a, Vec2 b) {
    float dx=a.x-b.x, dy=a.y-b.y;
    return dx*dx+dy*dy;
}
inline float clamp(float value,float lo,float hi) {
    return std::max(lo,std::min(value,hi));
}
inline bool withinRange(Vec2 a,Vec2 b,float radius) {
    return distanceSquared(a,b) <= radius*radius;
}
inline bool overlaps(float ax,float ay,float aw,float ah,float bx,float by,float bw,float bh) {
    return ax < bx+bw && ax+aw > bx && ay < by+bh && ay+ah > by;
}
inline int damage(int hp,int amount) { return std::max(0,hp-std::max(0,amount)); }
inline Vec2 normalized(Vec2 v) {
    float length=std::sqrt(v.x*v.x+v.y*v.y);
    if(length<0.0001f) return {};
    return {v.x/length,v.y/length};
}
} // namespace game
