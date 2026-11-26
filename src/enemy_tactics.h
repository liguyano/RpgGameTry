#pragma once
#include "game_logic.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace enemy_ai {
constexpr float PI=3.14159265358979323846f;
constexpr float TAU=2.f*PI;
constexpr float DESIRED_SPACING=43.f;
constexpr float SENSE_RADIUS=145.f;
constexpr float PREFERRED_RANGE=60.f;
constexpr int MAX_ATTACKERS=2;

// Stable roles prevent an entire pack from simply chasing the player's
// current position. Flankers select different points on a ring surrounding
// the player; harassers remain further out until a good opening appears.
enum class Role { Vanguard, FlankLeft, FlankRight, Harasser };
inline Role role(std::size_t i){
    switch(i%4u){
        case 0:return Role::Vanguard;
        case 1:return Role::FlankLeft;
        case 2:return Role::FlankRight;
        default:return Role::Harasser;
    }
}
inline float length(game::Vec2 v){return std::sqrt(v.x*v.x+v.y*v.y);}
inline game::Vec2 sub(game::Vec2 a,game::Vec2 b){return {a.x-b.x,a.y-b.y};}
inline game::Vec2 add(game::Vec2 a,game::Vec2 b){return {a.x+b.x,a.y+b.y};}
inline game::Vec2 mul(game::Vec2 a,float f){return {a.x*f,a.y*f};}
inline game::Vec2 perpendicular(game::Vec2 a){return {-a.y,a.x};}
inline game::Vec2 normalizedOr(game::Vec2 a,game::Vec2 fallback={1.f,0.f}){
    const float l=length(a);
    return l>.0001f?mul(a,1.f/l):fallback;
}
inline float dot(game::Vec2 a,game::Vec2 b){return a.x*b.x+a.y*b.y;}
inline game::Vec2 slot(game::Vec2 hero,game::Vec2 heroFacing,std::size_t index){
    const auto forward=normalizedOr(heroFacing);
    const auto side=perpendicular(forward);
    // Two members per role receive slightly different positions, not
    // identical waypoints. The extra depth keeps a staggered battle line.
    const float rank=index>=4?1.f:0.f;
    switch(role(index)){
        case Role::Vanguard:
            return add(hero,add(mul(forward,59.f+rank*15.f),
                                mul(side,rank?21.f:-16.f)));
        case Role::FlankLeft:
            return add(hero,add(mul(side,-79.f-rank*12.f),
                                mul(forward,-7.f-rank*17.f)));
        case Role::FlankRight:
            return add(hero,add(mul(side,79.f+rank*12.f),
                                mul(forward,10.f+rank*13.f)));
        case Role::Harasser:
            return add(hero,add(mul(forward,-65.f-rank*15.f),
                                mul(side,rank?43.f:-43.f)));
    }
    return hero;
}
// Neighbour separation: enemies no longer occupy the same point.
template<class EnemyList>
inline game::Vec2 separation(const EnemyList& enemies,std::size_t index){
    const auto& current=enemies[index];
    game::Vec2 force{0.f,0.f};
    for(std::size_t j=0;j<enemies.size();j++){
        if(j==index || enemies[j].hp<=0)continue;
        const game::Vec2 apart=sub(current.pos,enemies[j].pos);
        const float distance=length(apart);
        if(distance<.001f || distance>DESIRED_SPACING)continue;
        force=add(force,mul(apart,(DESIRED_SPACING-distance)/
                           (DESIRED_SPACING*distance)));
    }
    return force;
}
template<class EnemyList>
inline game::Vec2 steer(const EnemyList& enemies,std::size_t index,
                        game::Vec2 hero,game::Vec2 facing){
    const auto& e=enemies[index];
    const auto offset=sub(e.pos,hero);
    const float distance=length(offset);
    const game::Vec2 target=slot(hero,facing,index);
    const game::Vec2 toSlot=sub(target,e.pos);
    // Stay out of the player's center even when the target slot is beyond
    // the player. Back up instead of making everyone rush to the same pixel.
    game::Vec2 avoidCenter{0.f,0.f};
    if(distance<49.f)
        avoidCenter=mul(normalizedOr(offset),(.8f+(49.f-distance)*.07f));
    const game::Vec2 avoidCrowd=mul(separation(enemies,index),3.1f);
    const game::Vec2 heading=add(add(toSlot,avoidCrowd),mul(avoidCenter,28.f));
    if(length(toSlot)<9.f && length(avoidCrowd)<.04f &&
       distance>=49.f)return {0.f,0.f};
    return normalizedOr(heading,{0.f,0.f});
}
inline float speedFactor(std::size_t i){
    switch(role(i)){
        case Role::Vanguard:return 1.07f;
        case Role::FlankLeft:return .90f;
        case Role::FlankRight:return .96f;
        case Role::Harasser:return .77f;
    }
    return 1.f;
}
inline bool canWindup(float distance,float cooldown,bool staggered,
                      int currentlyPreparing,std::size_t i){
    // Opponents only attack at melee distance, and only up to two at once.
    // A short role-dependent cooldown ensures attackers rotate naturally.
    return distance<37.f && cooldown<=0.f && !staggered &&
           currentlyPreparing<MAX_ATTACKERS && role(i)!=Role::Harasser;
}
} // namespace enemy_ai
