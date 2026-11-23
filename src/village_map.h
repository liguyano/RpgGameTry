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
inline const std::array<Polygon,13>& obstacles(){
    static const std::array<Polygon,13> zones{{
        // Buildings: the house footprint, not the decorative overhanging trees.
        polygon({{294,143},{462,140},{470,291},{285,290}}), // northwest house
        polygon({{592,129},{745,124},{748,235},{589,239}}), // northern house
        polygon({{882,209},{1031,207},{1042,332},{881,332}}), // east house
        polygon({{354,326},{566,319},{566,454},{351,462}}), // blue roof at west
        polygon({{611,282},{758,277},{758,388},{613,390}}), // plaza house
        polygon({{306,514},{502,510},{504,701},{303,704}}), // southwest inn
        polygon({{988,530},{1173,529},{1173,710},{981,711}}), // east blue-roof home
        polygon({{617,698},{806,691},{808,842},{618,840}}), // southern cottage
        polygon({{308,783},{480,786},{487,893},{308,892}}), // southern blue-roof house
        polygon({{1301,579},{1492,577},{1498,806},{1311,813}}), // watermill building
        polygon({{1180,0},{1381,0},{1388,154},{1188,159}}), // church
        polygon({{0,264},{157,264},{153,432},{0,432}}), // windmill
        polygon({{839,376},{972,380},{974,503},{837,501}}) // large central tree
    }};
    return zones;
}
inline const std::array<Polygon,8>& terrain(){
    static const std::array<Polygon,8> zones{{
        // Upper stream, southern stream and lake; bridge cutouts handled below.
        polygon({{910,0},{1050,0},{1074,160},{1041,250},{1080,328},{1165,377},
                 {1280,368},{1365,409},{1364,469},{1230,437},{1108,420},
                 {1018,353},{972,273},{978,170}}),
        polygon({{1202,386},{1290,380},{1323,484},{1310,610},{1290,725},
                 {1266,740},{1231,704},{1230,608},{1261,520}}),
        polygon({{1250,698},{1354,677},{1381,782},{1519,829},{1536,854},
                 {1536,1024},{1108,1024},{1106,950},{1184,886}}),
        polygon({{1021,929},{1130,914},{1164,1024},{1014,1024}}), // dockside coast
        polygon({{0,0},{225,0},{242,121},{131,195},{0,212}}), // northwest forest/cliffs
        polygon({{0,765},{250,755},{329,851},{317,959},{205,1024},{0,1024}}), // southwest cliffs
        polygon({{1445,0},{1536,0},{1536,266},{1436,246}}), // northeast trees
        polygon({{0,1020},{1536,1020},{1536,1024},{0,1024}}) // bottom edge
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
    for(const auto& terrainZone:terrain()){
        bool crossing=false;
        for(auto bridge:bridgePassages())if(inRect(world,bridge,0.f)){crossing=true;break;}
        if(!crossing && inPolygon(world,terrainZone,radius))return true;
    }
    // fountain, market booths, walls and other decorative fixtures
    for(auto r:std::array<Rect,5>{{
        {734*SCALE,448*SCALE,88*SCALE,84*SCALE},
        {571*SCALE,455*SCALE,93*SCALE,92*SCALE},
        {1032*SCALE,433*SCALE,90*SCALE,81*SCALE},
        {853*SCALE,596*SCALE,105*SCALE,95*SCALE},
        {57*SCALE,502*SCALE,127*SCALE,130*SCALE}
    }})if(inRect(world,r,radius))return true;
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
