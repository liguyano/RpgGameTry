#pragma once
#include "game_logic.h"
#include <array>
#include <cmath>
#include <cstddef>

// Gameplay coordinates for the 1536x1024 village illustration.
// The scene is scaled by 1.5 so it is wider than a 960x640 viewport.
// Obstacles are approximate hand-authored footprints matching the painting;
// F3 in the game shows them for easy tuning.
namespace village {
constexpr float SCALE=1.5f;
constexpr float IMAGE_W=1536.f, IMAGE_H=1024.f;
constexpr float WORLD_W=IMAGE_W*SCALE, WORLD_H=IMAGE_H*SCALE;
inline game::Vec2 pos(float imageX,float imageY){return {imageX*SCALE,imageY*SCALE};}
inline game::Vec2 fountainCenter(){return pos(750.f,515.f);} // moved down 40 image pixels
// Maintain a fully flowing jet: water sheet frames 0-2 are start-up,
// 6-11 are shutdown/retraction. Loop only strong spray frames 3-5,
// reversing the sequence to avoid a hard jump at the loop boundary.
inline int fountainWaterFrame(float seconds){
    constexpr std::array<int,4> sustained{{3,4,5,4}};
    const int tick=std::max(0,int(seconds*6.f));
    return sustained[size_t(tick)%sustained.size()];
}

// Collision only: extend the basin 12 image pixels upward while leaving
// its bottom edge unchanged. Visual placement stays at fountainCenter().
constexpr float FOUNTAIN_RADIUS=49.f*SCALE;
inline game::Vec2 fountainCollisionCenter(){
    auto center=fountainCenter();
    center.y-=6.f*SCALE;
    return center;
}
inline bool fountainBlocked(game::Vec2 world,float radius=0.f){
    const game::Vec2 c=fountainCollisionCenter();
    const float dx=world.x-c.x,dy=world.y-c.y;
    const float limit=FOUNTAIN_RADIUS+radius;
    return dx*dx+dy*dy<=limit*limit;
}
struct Rect { float x,y,w,h; };
inline bool inRect(game::Vec2 p,Rect r,float radius){
    float nearestX=game::clamp(p.x,r.x,r.x+r.w);
    float nearestY=game::clamp(p.y,r.y,r.y+r.h);
    float dx=p.x-nearestX,dy=p.y-nearestY;
    return dx*dx+dy*dy <= radius*radius;
}
struct Polygon { std::array<game::Vec2,12> points{}; std::size_t count=0; };
inline bool pointInside(game::Vec2 p,const Polygon& polygon){
    bool inside=false;
    for(std::size_t i=0,j=polygon.count-1;i<polygon.count;j=i++){
        auto a=polygon.points[i],b=polygon.points[j];
        if((a.y>p.y)!=(b.y>p.y)){
            float x=(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x;
            if(p.x<x)inside=!inside;
        }
    }
    return inside;
}
inline float distanceSegmentSquared(game::Vec2 p,game::Vec2 a,game::Vec2 b){
    float dx=b.x-a.x,dy=b.y-a.y,den=dx*dx+dy*dy;
    float t=den>0?game::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/den,0.f,1.f):0.f;
    return game::distanceSquared(p,{a.x+t*dx,a.y+t*dy});
}
inline bool inPolygon(game::Vec2 p,const Polygon& poly,float radius){
    if(pointInside(p,poly))return true;
    for(std::size_t i=0;i<poly.count;i++){
        if(distanceSegmentSquared(p,poly.points[i],poly.points[(i+1)%poly.count])<radius*radius)
            return true;
    }
    return false;
}
inline Polygon polygon(std::initializer_list<game::Vec2> imagePoints){
    Polygon p;
    for(auto v:imagePoints)if(p.count<p.points.size())p.points[p.count++]=pos(v.x,v.y);
    return p;
}
// Collision is based on the LOWER HALF of each placed visual sprite, rather
// than a tiny foundation or tree trunk. Upper roofs/canopies remain foreground
// occluders. Positions match VILLAGE_PROPS in src/main.cpp (image coordinates).
struct ObjectBounds {float x,y,w,h;bool tree;};
inline const std::array<ObjectBounds,12>& objectBounds(){
    static const std::array<ObjectBounds,12> props{{
        {310,122,190,203,false},   // cottage
        {558,115,225,214,false},   // thatched farmhouse
        {865,187,200,190,false},   // tavern
        {1007,509,194,190,false},  // shop
        {321,483,212,193,false},   // stone house
        {611,678,212,179,false},   // blue-roof house
        {800,314,150,163,true},    // oak
        {504,389,112,140,true},    // deciduous tree
        {1138,447,120,163,true},   // pine
        {1257,171,153,152,true},   // blossom tree
        {232,681,125,148,true},    // apple tree
        {132,177,152,154,true}     // spreading tree
    }};
    return props;
}
inline const std::array<Polygon,12>& obstacles(){
    static const std::array<Polygon,12> zones=[]{
        std::array<Polygon,12> result{};
        for(size_t i=0;i<result.size();++i){
            const auto& b=objectBounds()[i];
            // Buildings: 85% of sprite width. Trees: 65% of canopy width,
            // preserving some ability to walk past the outer branches.
            const float widthFraction=b.tree?0.65f:0.85f;
            const float inset=b.w*(1.f-widthFraction)*0.5f;
            const float x0=b.x+inset,x1=b.x+b.w-inset;
            const float y0=b.y+b.h*0.5f,y1=b.y+b.h;
            result[i]=polygon({{x0,y0},{x1,y0},{x1,y1},{x0,y1}});
        }
        return result;
    }();
    return zones;
}
// River/water zones only. The previous map's forest, cliff and edge polygons
// covered clear grass/paths in the new ground artwork and caused invisible walls.
// Cliffs can be restored later with a mask matched to the current map.
inline const std::array<Polygon,4>& terrain(){
    static const std::array<Polygon,4> zones{{
        polygon({{910,0},{1050,0},{1074,160},{1041,250},{1080,328},{1165,377},
                 {1280,368},{1365,409},{1364,469},{1230,437},{1108,420},
                 {1018,353},{972,273},{978,170}}),
        polygon({{1202,386},{1290,380},{1323,484},{1310,610},{1290,725},
                 {1266,740},{1231,704},{1230,608},{1261,520}}),
        polygon({{1250,698},{1354,677},{1381,782},{1519,829},{1536,854},
                 {1536,1024},{1108,1024},{1106,950},{1184,886}}),
        polygon({{1021,929},{1130,914},{1164,1024},{1014,1024}})
    }};
    return zones;
}
inline const std::array<Rect,3>& bridgePassages(){
    // Bridges are the only legal crossing through the river collision.
    static const std::array<Rect,3> bridges{{
        {1186*SCALE,346*SCALE,144*SCALE,55*SCALE}, // upper bridge
        {1195*SCALE,729*SCALE,158*SCALE,48*SCALE}, // mill bridge
        {1022*SCALE,940*SCALE,107*SCALE,41*SCALE}  // dock planks
    }};
    return bridges;
}
inline bool blocked(game::Vec2 world,float radius=11.f){
    if(world.x<radius || world.y<radius ||
       world.x>WORLD_W-radius || world.y>WORLD_H-radius)return true;
    for(const auto& obstacle:obstacles())
        if(inPolygon(world,obstacle,radius))return true;
    if(fountainBlocked(world,radius))return true;
    for(const auto& terrainZone:terrain()){
        bool crossing=false;
        for(auto bridge:bridgePassages())if(inRect(world,bridge,0.f)){crossing=true;break;}
        if(!crossing && inPolygon(world,terrainZone,radius))return true;
    }
    // Do not collide with legacy fountain, stalls or wall rectangles:
    // those objects no longer exist in the new village ground map.
    return false;
}
inline void move(game::Vec2& p,game::Vec2 velocity,float dt,float radius=11.f){
    // Axis-separated motion slides along obstacle walls and avoids tunnelling.
    int steps=std::max(1,int(std::ceil(std::max(std::abs(velocity.x),std::abs(velocity.y))*dt/7.f)));
    for(int i=0;i<steps;i++){
        game::Vec2 candidate{p.x+velocity.x*dt/steps,p.y};
        if(!blocked(candidate,radius))p.x=candidate.x;
        candidate={p.x,p.y+velocity.y*dt/steps};
        if(!blocked(candidate,radius))p.y=candidate.y;
    }
}
inline game::Vec2 camera(game::Vec2 focus){
    return {game::clamp(focus.x-game::WIDTH/2.f,0.f,WORLD_W-game::WIDTH),
            game::clamp(focus.y-game::HEIGHT/2.f,0.f,WORLD_H-game::HEIGHT)};
}
} // namespace village
