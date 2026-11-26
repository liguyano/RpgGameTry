#pragma once
#include "game_logic.h"
#include <algorithm>
#include <cmath>

namespace combat {
constexpr float PI=3.14159265358979323846f;
constexpr float ATTACK_REACH=60.f;
constexpr float ATTACK_HALF_ANGLE=PI*35.f/180.f; // 70-degree total forward sword arc
constexpr float SWING_DURATION=.34f;
constexpr float ENEMY_NOTICE_DISTANCE=315.f;
constexpr float ENEMY_ATTACK_RANGE=37.f;
constexpr float ENEMY_WINDUP=.46f;
constexpr float ENEMY_RECOVERY=.83f;
constexpr float ENEMY_STAGGER=.27f;
constexpr float HERO_INVULNERABILITY=.55f;
inline game::Vec2 toward(game::Vec2 a,game::Vec2 b){
    return game::normalized({b.x-a.x,b.y-a.y});
}
inline bool swordHits(game::Vec2 origin,game::Vec2 facing,game::Vec2 enemy){
    const game::Vec2 delta{enemy.x-origin.x,enemy.y-origin.y};
    const float squared=game::distanceSquared(origin,enemy);
    if(squared>ATTACK_REACH*ATTACK_REACH)return false;
    if(squared<1.f)return false; // zero distance has no meaningful forward direction
    const game::Vec2 unit=game::normalized(delta);
    return unit.x*facing.x+unit.y*facing.y>=std::cos(ATTACK_HALF_ANGLE);
}
inline int slashFrame(float remaining,int count){
    const float elapsed=std::max(0.f,SWING_DURATION-remaining);
    return std::min(count-1,int(elapsed/SWING_DURATION*float(count)));
}
inline float knockbackFactor(float dt){return std::max(0.f,std::min(dt,.04f));}
} // namespace combat
