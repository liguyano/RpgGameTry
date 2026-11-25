#pragma once
#include "game_logic.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Short-lived, allocation-free world-space particles drawn under the hero.
// No particles are emitted when the feet cannot move (e.g. blocked by a wall).
namespace footsteps {
constexpr int CAPACITY=64;
constexpr float STEP_DISTANCE=18.f;
constexpr float MAX_SPAWN_DISTANCE=48.f;
struct Particle {
    game::Vec2 position{};
    game::Vec2 velocity{};
    float life=0.f;
    float lifetime=.45f;
    float size=2.f;
    bool grass=false;
};
struct Emitter {
    std::array<Particle,CAPACITY> particles{};
    int next=0;
    float travelled=0.f;
    std::uint32_t seed=0x7d91c423u;
    int scene=-1; // 1 village, 2 wilderness; indoor scenes emit nothing
};
inline float random01(Emitter& e){
    e.seed=e.seed*1664525u+1013904223u;
    return float((e.seed>>8)&0xffffu)/65535.f;
}
inline int activeCount(const Emitter& e){
    int n=0;
    for(const auto& p:e.particles)if(p.life>0.f)++n;
    return n;
}
inline void reset(Emitter& e,int scene){
    e.particles={};
    e.next=0;
    e.travelled=0.f;
    e.scene=scene;
}
inline void emit(Emitter& e,game::Vec2 foot,game::Vec2 direction,int scene){
    const game::Vec2 tangent{-direction.y,direction.x};
    for(int i=0;i<2;i++){
        const float offset=(i==0?-1.f:1.f)*(4.f+random01(e)*3.f);
        const bool grass=scene==2 && random01(e)<.65f;
        Particle& p=e.particles[static_cast<size_t>(e.next)];
        e.next=(e.next+1)%CAPACITY;
        p.position={foot.x+tangent.x*offset-direction.x*7.f,
                    foot.y+tangent.y*offset-direction.y*7.f+7.f};
        p.velocity={tangent.x*(random01(e)*24.f-12.f)-direction.x*9.f,
                    -9.f-random01(e)*16.f};
        p.lifetime=.30f+random01(e)*.26f;
        p.life=p.lifetime;
        p.size=grass?2.f+random01(e)*1.5f:2.5f+random01(e)*2.f;
        p.grass=grass;
    }
}
// scene 1 or 2 is outdoors, 0 indoors; positions use original world coords.
inline void update(Emitter& e,int scene,game::Vec2 before,game::Vec2 after,
                   float dt,bool allowed=true){
    if(e.scene!=scene)reset(e,scene);
    dt=std::clamp(dt,0.f,.05f);
    for(auto& p:e.particles){
        if(p.life<=0.f)continue;
        p.life=std::max(0.f,p.life-dt);
        p.position.x+=p.velocity.x*dt;
        p.position.y+=p.velocity.y*dt;
        p.velocity.y+=29.f*dt;
    }
    const float dx=after.x-before.x,dy=after.y-before.y;
    const float distance=std::sqrt(dx*dx+dy*dy);
    if(!allowed || (scene!=1 && scene!=2) || distance<.2f)return;
    // Frame spikes or teleports must not fill the entire particle pool.
    if(distance>MAX_SPAWN_DISTANCE){e.travelled=0.f;return;}
    const game::Vec2 direction{dx/distance,dy/distance};
    const float old=e.travelled;
    e.travelled+=distance;
    while(e.travelled>=STEP_DISTANCE){
        const float ratio=std::clamp((STEP_DISTANCE-old)/distance,0.f,1.f);
        const game::Vec2 position{before.x+dx*ratio,before.y+dy*ratio};
        emit(e,position,direction,scene);
        e.travelled-=STEP_DISTANCE;
    }
}
} // namespace footsteps
