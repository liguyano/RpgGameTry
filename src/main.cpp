#include "game_logic.h"
#include "village_map.h"
#include "shop_map.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#if RPG_HAS_SDL3_TTF
#include <SDL3_ttf/SDL_ttf.h>
#endif
#if RPG_HAS_SDL3_IMAGE
#include <SDL3_image/SDL_image.h>
#endif
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace game;
namespace {
constexpr int SCALE=1;
constexpr float PLAYER_SPEED=155.f;
constexpr float ENEMY_SPEED=56.f;
constexpr int MAX_HEALTH=100;

struct Enemy {
    Vec2 pos;
    int hp=3;
    float attackTimer=0;
    float wobble=0;
};
struct Player {
    Vec2 pos{WIDTH/2.f,HEIGHT/2.f};
    int hp=MAX_HEALTH;
    Vec2 facing{0,1};
    float slashTimer=0;
    float hurtTimer=0;
    int kills=0;
    bool moving=false;
    float animationTime=0.f;
};
enum class Scene { HOME, VILLAGE, SHOP, OUTSKIRTS };

// Each relative follows a small safe route within the cottage, with pauses and
// direction changes. The location is authoritative for rendering and E interaction.
struct FamilyMotion {
    Vec2 position{};
    Vec2 facing{0.f,1.f};
    int nextWaypoint=0;
    float wait=0.f;
    float walkingTime=0.f;
    bool moving=false;
};
constexpr std::array<std::array<Vec2,3>,3> FAMILY_ROUTES{{
    {{{300,260},{310,290},{255,282}}},  // Mother: between hearth and kitchen
    {{{615,270},{655,299},{610,320}}},  // Father: bookshelf / work area
    {{{460,405},{508,455},{408,470}}}   // Lily: plays around the south rug
}};
inline FamilyMotion initialFamily(int i){
    FamilyMotion f;
    f.position=FAMILY_ROUTES[size_t(i)][0];
    f.nextWaypoint=1;
    f.wait=0.8f+float(i)*0.7f;
    return f;
}
void updateFamilyMotion(std::array<FamilyMotion,3>& family,Vec2 hero,float dt){
    for(int i=0;i<3;i++){
        auto& npc=family[size_t(i)];
        npc.moving=false;
        if(npc.wait>0.f){npc.wait=std::max(0.f,npc.wait-dt);continue;}
        const Vec2 target=FAMILY_ROUTES[size_t(i)][size_t(npc.nextWaypoint)];
        const Vec2 delta{target.x-npc.position.x,target.y-npc.position.y};
        const float distance=std::sqrt(delta.x*delta.x+delta.y*delta.y);
        if(distance<2.5f){
            npc.position=target;
            npc.nextWaypoint=(npc.nextWaypoint+1)%3;
            npc.wait=1.3f+float((i+npc.nextWaypoint)%3)*0.7f;
            continue;
        }
        const Vec2 direction=normalized(delta);
        npc.facing=direction;
        // Never crowd the player, and do not slide into their feet.
        Vec2 step{npc.position.x+direction.x*52.f*dt,
                  npc.position.y+direction.y*52.f*dt};
        if(withinRange(step,hero,29.f)){npc.wait=0.5f;continue;}
        npc.position=step;
        npc.moving=true;
        npc.walkingTime+=dt;
    }
}

// Eight additional citizens follow small deterministic routes near their spawn.
// World-space anchors are in the 1536x1024 village image.
constexpr int VILLAGER_COUNT=8;
struct VillagerMotion {
    Vec2 position{};
    Vec2 home{};
    Vec2 facing{0.f,1.f};
    Vec2 destination{};
    float pause=0.f;
    float walkingTime=0.f;
    int step=0;
    bool moving=false;
};
constexpr std::array<Vec2,VILLAGER_COUNT> EXTRA_VILLAGER_START{{
    {540.f,608.f},{322.f,418.f},{980.f,561.f},{514.f,755.f},
    {850.f,784.f},{690.f,425.f},{918.f,728.f},{400.f,722.f}
}};
inline std::array<VillagerMotion,VILLAGER_COUNT> initialVillagers(){
    std::array<VillagerMotion,VILLAGER_COUNT> result{};
    for(int i=0;i<VILLAGER_COUNT;i++){
        const Vec2 home=village::pos(EXTRA_VILLAGER_START[size_t(i)].x,
                                     EXTRA_VILLAGER_START[size_t(i)].y);
        result[size_t(i)].home=home;
        result[size_t(i)].position=home;
        result[size_t(i)].destination=home;
        result[size_t(i)].pause=0.5f+float(i%4)*0.6f;
    }
    return result;
}
void updateVillagerMotion(std::array<VillagerMotion,VILLAGER_COUNT>& npcs,Vec2 hero,float dt){
    for(int i=0;i<VILLAGER_COUNT;i++){
        auto& npc=npcs[size_t(i)];
        npc.moving=false;
        if(npc.pause>0.f){npc.pause=std::max(0.f,npc.pause-dt);continue;}
        if(withinRange(npc.position,npc.destination,3.f)){
            npc.step++;
            const float angle=float((npc.step*3+i*5)%16)*0.39269908f;
            const float distance=float(22+(npc.step+i)%3*13)*village::SCALE;
            const Vec2 wanted{npc.home.x+std::cos(angle)*distance,
                              npc.home.y+std::sin(angle)*distance};
            npc.destination=village::blocked(wanted,11.f)?npc.home:wanted;
            npc.pause=0.6f+float((npc.step+i)%4)*0.45f;
            continue;
        }
        const Vec2 delta{npc.destination.x-npc.position.x,
                         npc.destination.y-npc.position.y};
        const Vec2 direction=normalized(delta);
        const float speed=39.f;
        const Vec2 candidate{npc.position.x+direction.x*speed*dt,
                             npc.position.y+direction.y*speed*dt};
        if(village::blocked(candidate,11.f)||withinRange(candidate,hero,28.f)){
            npc.pause=0.9f;
            npc.destination=npc.home;
            continue;
        }
        npc.position=candidate;
        npc.facing=direction;
        npc.walkingTime+=dt;
        npc.moving=true;
    }
}

struct World {
    Scene scene=Scene::HOME;
    int story=0;
    std::string dialogue;
    std::string speaker;
    float dialogueAge=0.f;
    bool justArrived=true;
    bool largeVillage=false;
    bool showCollision=false;
    float noticeAge=0.f; // real-time fade clock for chapter and control tips
    bool inventoryOpen=false;
    bool shopMenu=false;
    int shopSelection=0;
    std::string shopFeedback;
    int inventorySelection=0;
    std::array<int,24> bag{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,0,0,0,0,0,0,0,0}};
    std::array<int,6> equipped{{0,0,0,0,0,0}};
    std::array<int,8> quickSlots{{1,7,8,9,0,0,0,0}};
    int quickSelected=0;
    int gold=75;
    Player hero;
    std::array<FamilyMotion,3> family{initialFamily(0),initialFamily(1),initialFamily(2)};
    std::array<VillagerMotion,VILLAGER_COUNT> villagers=initialVillagers();
    std::vector<Enemy> enemies;
    bool won=false, lost=false;
    float time=0;
    float spawnTimer=0;
    int wave=0;
};

void showDialogue(World& w,const char* speaker,const char* words);

struct ArtAssets {
    SDL_Texture* home=nullptr;
    SDL_Texture* village=nullptr;
    bool expandedVillage=false;
    SDL_Texture* outskirts=nullptr;
    SDL_Texture* hero=nullptr;
    SDL_Texture* father=nullptr;
    SDL_Texture* mother=nullptr;
    SDL_Texture* lily=nullptr;
    std::array<SDL_Texture*,54> familyFrames{};
    std::array<SDL_Texture*,16> heroFrames{};
    std::array<SDL_Texture*,12> villageProps{};
    std::array<SDL_Texture*,12> fountainFrames{}; // legacy spring.png fallback (first frame only)
    SDL_Texture* fountainBase=nullptr;
    SDL_Texture* fountainWater=nullptr;
    SDL_Texture* fountainAnimation=nullptr;
    SDL_Texture* uiAtlas=nullptr;
    SDL_Texture* itemsAtlas=nullptr;
    SDL_Texture* shopBackground=nullptr;
    std::array<SDL_Texture*,shop::furniture.size()> shopFurniture{};
    std::array<SDL_Texture*,8*4*4> villagerFrames{};
};
SDL_Texture* loadArt(SDL_Renderer* renderer,const char* filename){
    namespace fs=std::filesystem;
    const fs::path paths[]={
        fs::path("assets")/filename,
        fs::path("../assets")/filename,
        fs::path("../../assets")/filename
    };
    for(const auto& path:paths){
        if(!fs::exists(path)) continue;
        SDL_Surface* surface=nullptr;
#if RPG_HAS_SDL3_IMAGE
        surface=IMG_Load(path.string().c_str());
#else
        surface=SDL_LoadBMP(path.string().c_str());
#endif
        if(!surface){
            std::cerr<<"Cannot load image "<<path.string()<<": "<<SDL_GetError()<<"\n";
            continue;
        }
        SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,surface);
        SDL_DestroySurface(surface);
        if(texture){
            SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
            return texture;
        }
    }
    return nullptr;
}

// Accept the newly generated sheet under its original name (including a
// Japanese filename) so users do not need to rename it to fountain_anim.png.
// Ignore old fountain_base / fountain_water assets and other artwork.
SDL_Texture* loadNewFountainAnimation(SDL_Renderer* renderer){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    const fs::path roots[]={"assets","../assets","../../assets"};
    const std::string exact="水しぶきの噴水アニメーション_spritesheet.png";
    for(const auto& root:roots){
        if(!fs::exists(root) || !fs::is_directory(root))continue;
        fs::path chosen=root/exact;
        if(!fs::exists(chosen)){
            std::vector<fs::path> candidates;
            for(const auto& entry:fs::directory_iterator(root)){
                if(!entry.is_regular_file())continue;
                std::string name=entry.path().filename().string();
                std::string lower=name;
                for(char& ch:lower)ch=char(std::tolower(static_cast<unsigned char>(ch)));
                if(entry.path().extension()!=".png")continue;
                if(lower.find("ani")!=std::string::npos ||
                   name.find("アニメ")!=std::string::npos)
                    candidates.push_back(entry.path());
            }
            std::sort(candidates.begin(),candidates.end());
            if(candidates.empty())continue;
            chosen=candidates.front();
        }
        SDL_Texture* texture=loadArt(renderer,chosen.filename().string().c_str());
        if(!texture)continue;
        float w=0.f,h=0.f;
        if(!SDL_GetTextureSize(texture,&w,&h) || w<4.f || h<3.f ||
           int(w)%4!=0 || int(h)%3!=0){
            std::cerr<<"Ignoring invalid fountain animation sheet: "<<chosen.string()<<"\n";
            SDL_DestroyTexture(texture);
            continue;
        }
        std::cerr<<"Fountain 12-frame sheet: "<<chosen.string()<<"\n";
        return texture;
    }
#else
    (void)renderer;
#endif
    return nullptr;
}

// Extract the 3 relatives' four facing directions (3 frames each) from family.png.
// The supplied sheet is a labeled concept sheet rather than a transparent atlas,
// so the dark sheet background is removed during one-time loading.

// The generated hero.png is a 1536x1024 labeled concept sheet, not a conventional
// transparent spritesheet. Each row has 4 walking poses facing down/left/right/up.
// Extract only each character silhouette and flood-fill the connected dark backdrop
// from the frame border so that dark outlines INSIDE the hero are not erased.
void loadHeroFrames(SDL_Renderer* r,ArtAssets& art){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path sheetPath;
    for(const fs::path& path:{fs::path("assets/hero.png"),
                              fs::path("../assets/hero.png"),
                              fs::path("../../assets/hero.png")}){
        if(fs::exists(path)){sheetPath=path;break;}
    }
    if(sheetPath.empty()){std::cerr<<"hero.png not found; using procedural player\n";return;}
    SDL_Surface* sheet=IMG_Load(sheetPath.string().c_str());
    if(!sheet){std::cerr<<"Cannot load hero.png: "<<SDL_GetError()<<"\n";return;}
    // Positions measured on the original art sheet (1536x1024).
    constexpr int cx[4]={321,523,728,930};
    constexpr int top[4]={19,266,512,759};
    constexpr int baseWidth=152,baseHeight=218;
    const float sx=float(sheet->w)/1536.f,sy=float(sheet->h)/1024.f;
    for(int dir=0;dir<4;dir++)for(int pose=0;pose<4;pose++){
        const int frameW=std::max(1,int(baseWidth*sx));
        const int frameH=std::max(1,int(baseHeight*sy));
        const int left=int(cx[pose]*sx)-frameW/2;
        const int upper=int(top[dir]*sy);
        if(left<0||upper<0||left+frameW>sheet->w||upper+frameH>sheet->h)continue;
        SDL_Surface* cropped=SDL_CreateSurface(frameW,frameH,SDL_PIXELFORMAT_RGBA32);
        if(!cropped)continue;
        SDL_Rect region{left,upper,frameW,frameH};
        if(SDL_BlitSurface(sheet,&region,cropped,nullptr)){
            const size_t count=size_t(frameW)*size_t(frameH);
            std::vector<Uint8> seen(count,0);
            std::vector<int> queue;
            queue.reserve(count);
            // The sheet's actual matte is almost uniform charcoal
            // (approximately RGB 20,21,27). Never use a broad brightness
            // threshold: the hero's blue clothes and dark hair would disappear.
            Uint8 matteR=20,matteG=21,matteB=27,matteA=255;
            SDL_ReadSurfacePixel(cropped,3,3,&matteR,&matteG,&matteB,&matteA);
            auto candidate=[&](int x,int y){
                Uint8 red=0,green=0,blue=0,alpha=255;
                if(!SDL_ReadSurfacePixel(cropped,x,y,&red,&green,&blue,&alpha))return false;
                constexpr int tolerance=18;
                return alpha<20 ||
                    (std::abs(int(red)-int(matteR))<tolerance &&
                     std::abs(int(green)-int(matteG))<tolerance &&
                     std::abs(int(blue)-int(matteB))<tolerance);
            };
            auto enqueue=[&](int x,int y){
                if(x<0||y<0||x>=frameW||y>=frameH)return;
                size_t index=size_t(y)*size_t(frameW)+size_t(x);
                if(seen[index] || !candidate(x,y))return;
                seen[index]=1;
                queue.push_back(int(index));
            };
            for(int x=0;x<frameW;x++){enqueue(x,0);enqueue(x,frameH-1);}
            for(int y=0;y<frameH;y++){enqueue(0,y);enqueue(frameW-1,y);}
            for(size_t head=0;head<queue.size();head++){
                int k=queue[head],x=k%frameW,y=k/frameW;
                SDL_WriteSurfacePixel(cropped,x,y,0,0,0,0);
                enqueue(x-1,y);enqueue(x+1,y);enqueue(x,y-1);enqueue(x,y+1);
            }
            SDL_Texture* tex=SDL_CreateTextureFromSurface(r,cropped);
            if(tex){
                SDL_SetTextureScaleMode(tex,SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);
                art.heroFrames[size_t(dir)*4+size_t(pose)]=tex;
            }
        }
        SDL_DestroySurface(cropped);
    }
    SDL_DestroySurface(sheet);
    std::cerr<<"Loaded hero.png directional animation frames\n";
#else
    (void)r;(void)art;
#endif
}
int facingSpriteDirection(Vec2 facing){
    if(std::abs(facing.x)>std::abs(facing.y))return facing.x<0.f?1:2;
    return facing.y<0.f?3:0;
}
bool drawAnimatedHero(SDL_Renderer* r,const ArtAssets& art,
                      Vec2 position,Vec2 facing,bool moving,float animTime){
    const int direction=facingSpriteDirection(facing);
    const int frame=moving?int(animTime*7.5f)%4:0;
    SDL_Texture* tex=art.heroFrames[size_t(direction)*4+size_t(frame)];
    if(!tex)tex=art.heroFrames[size_t(direction)*4];
    if(!tex)return false;
    // Align bottom of sprite to character feet; preserve all four facing poses.
    SDL_FRect dst{position.x-26.f,position.y-63.f,52.f,78.f};
    SDL_RenderTexture(r,tex,nullptr,&dst);
    return true;
}


// Decorations are separate from the ground layer. "houseAndTree.png" is a
// presentation sheet, so these are individual approximated source rectangles.
// Coordinates below use 1408x1056 reference art dimensions and automatically
// scale to the decoded actual image dimensions.
struct VillageProp {
    int atlasX,atlasY,atlasW,atlasH;
    float mapX,mapY,mapW,mapH; // village image coordinates (1536x1024)
    float feetY;               // sorting boundary in image coordinates
};
constexpr std::array<VillageProp,12> VILLAGE_PROPS{{
    {18,28,220,265, 310,122,190,203, 312},     // cottage
    {246,24,298,275, 558,115,225,214, 329},    // thatched farmhouse
    {556,23,299,276, 865,187,200,190, 377},    // tavern
    {889,28,243,271, 1007,509,194,190, 699},   // shop
    {54,321,290,270, 321,483,212,193, 676},    // stone house
    {360,315,278,270, 611,678,212,179, 857},   // blue-roof house
    {30,581,226,236, 800,314,150,163, 477},    // oak
    {265,595,180,213, 504,389,112,140, 529},   // green deciduous tree
    {482,587,171,229, 1138,447,120,163, 610}, // pine
    {646,587,256,228, 1257,171,153,152, 323}, // blossom tree
    {906,588,190,222, 232,681,125,148, 829},  // apple tree
    {1116,587,274,233, 132,177,152,154, 331}  // spreading tree
}};

// spring.png is a 4-column x 3-row sheet with twelve phases of the water jet.
// Load the frames once, removing only the dark matte connected to their edges.
void loadFountainFrames(SDL_Renderer* renderer,ArtAssets& art){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path path;
    for(const auto& candidate:{fs::path("assets/spring.png"),
                               fs::path("../assets/spring.png"),
                               fs::path("../../assets/spring.png")}){
        if(fs::exists(candidate)){path=candidate;break;}
    }
    if(path.empty()){std::cerr<<"spring.png missing; fountain disabled\n";return;}
    SDL_Surface* sheet=IMG_Load(path.string().c_str());
    if(!sheet){std::cerr<<"spring.png: "<<SDL_GetError()<<"\n";return;}
    const int cellW=sheet->w/4,cellH=sheet->h/3;
    for(int frame=0;frame<12;frame++){
        SDL_Rect source{(frame%4)*cellW,(frame/4)*cellH,cellW,cellH};
        SDL_Surface* crop=SDL_CreateSurface(cellW,cellH,SDL_PIXELFORMAT_RGBA32);
        if(!crop)continue;
        if(SDL_BlitSurface(sheet,&source,crop,nullptr)){
            std::vector<Uint8> visited(size_t(cellW)*size_t(cellH),0);
            std::vector<int> pending;
            pending.reserve(visited.size());
            auto isBackdrop=[&](int x,int y){
                Uint8 r=0,g=0,b=0,a=255;
                if(!SDL_ReadSurfacePixel(crop,x,y,&r,&g,&b,&a))return false;
                const int maxComponent=std::max({int(r),int(g),int(b)});
                const int minComponent=std::min({int(r),int(g),int(b)});
                return a<20 || (maxComponent<40 && maxComponent-minComponent<19);
            };
            auto addPixel=[&](int x,int y){
                if(x<0||y<0||x>=cellW||y>=cellH)return;
                const size_t idx=size_t(y)*size_t(cellW)+size_t(x);
                if(visited[idx]||!isBackdrop(x,y))return;
                visited[idx]=1;
                pending.push_back(int(idx));
            };
            for(int x=0;x<cellW;x++){addPixel(x,0);addPixel(x,cellH-1);}
            for(int y=0;y<cellH;y++){addPixel(0,y);addPixel(cellW-1,y);}
            for(size_t i=0;i<pending.size();i++){
                int idx=pending[i],x=idx%cellW,y=idx/cellW;
                SDL_WriteSurfacePixel(crop,x,y,0,0,0,0);
                addPixel(x-1,y);addPixel(x+1,y);addPixel(x,y-1);addPixel(x,y+1);
            }
            SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,crop);
            if(texture){
                SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
                art.fountainFrames[size_t(frame)]=texture;
            }
        }
        SDL_DestroySurface(crop);
    }
    SDL_DestroySurface(sheet);
#else
    (void)renderer;(void)art;
#endif
}
// Render the fixed stone basin and the water sheet in the SAME rectangle.
// Both exported sprites use a 362x362 cell and a common anchor; never animate
// the base. A single source-rect changes for the water (4 columns x 3 rows).
// Animate small highlights flowing DOWN through the already-visible water.
// The stone basin and the actual jet silhouette are never translated or resized.
// Coordinates are authored in the 362x362 source-frame space; all water layers
// share the same destination, so there is no drifting across frames.
// Play the new twelve-frame atlas directly, without synthetic water droplets,
// height changes, or the old animated fountain-water layer.
// The generated water sprites include a painted pool and must NOT be drawn
// as a complete second fountain over the stone base: that hides the rim.
// Crop them to the actual falling-water region, then draw the lower stone rim
// on top. Both textures use the same 362x362 source-frame coordinates.
void drawAnimatedFountain(SDL_Renderer* renderer,const ArtAssets& art,
                          Vec2 camera,float heroY,float seconds,bool foreground){
    const float fountainY=village::fountainCenter().y;
    if((fountainY>heroY)!=foreground)return;
    const Vec2 center=village::fountainCenter();
    const float s=village::SCALE;
    const SDL_FRect dst{center.x-camera.x-96.f*s,
                       center.y-camera.y-152.f*s,192.f*s,192.f*s};
    if(art.fountainBase){
        SDL_RenderTexture(renderer,art.fountainBase,nullptr,&dst);
        SDL_Texture* water=art.fountainAnimation?art.fountainAnimation:art.fountainWater;
        float width=0.f,height=0.f;
        if(water && SDL_GetTextureSize(water,&width,&height)){
            constexpr int columns=4,rows=3,count=12;
            const int frame=int(std::max(0.f,seconds)*8.f)%count;
            const float cellW=width/columns,cellH=height/rows;
            // Keep the upper basin and the three falling streams but exclude
            // the oversized water-pool art below the stone rim.
            constexpr float x0=32.f/362.f,x1=330.f/362.f;
            constexpr float yEnd=244.f/362.f;
            const SDL_FRect src{
                float(frame%columns)*cellW+x0*cellW,
                float(frame/columns)*cellH,
                (x1-x0)*cellW,yEnd*cellH
            };
            const SDL_FRect overlay{
                dst.x+dst.w*x0,dst.y,
                dst.w*(x1-x0),dst.h*yEnd
            };
            SDL_RenderTexture(renderer,water,&src,&overlay);
            // Fixed stone wall in front of the pool, drawn last. This prevents
            // the water art from covering the stone and spilling beyond the basin.
            const SDL_FRect rimSrc{0.f,220.f,362.f,142.f};
            const SDL_FRect rimDst{dst.x,dst.y+dst.h*(220.f/362.f),
                                   dst.w,dst.h*(142.f/362.f)};
            SDL_RenderTexture(renderer,art.fountainBase,&rimSrc,&rimDst);
        }
    }else if(art.fountainAnimation){
        // Base not installed yet: avoid showing an oversized all-blue fountain.
        // Use the legacy static basin until the matching base PNG is present.
        if(art.fountainFrames[0])
            SDL_RenderTexture(renderer,art.fountainFrames[0],nullptr,&dst);
    }else if(art.fountainFrames[0]){
        SDL_RenderTexture(renderer,art.fountainFrames[0],nullptr,&dst);
    }
}

void loadVillageProps(SDL_Renderer* renderer,ArtAssets& art){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path source;
    for(const fs::path& path:{fs::path("assets/houseAndTree.png"),
                             fs::path("../assets/houseAndTree.png"),
                             fs::path("../../assets/houseAndTree.png")}){
        if(fs::exists(path)){source=path;break;}
    }
    if(source.empty())return;
    SDL_Surface* sheet=IMG_Load(source.string().c_str());
    if(!sheet){std::cerr<<"Cannot load houseAndTree.png: "<<SDL_GetError()<<"\n";return;}
    const float sx=float(sheet->w)/1408.f,sy=float(sheet->h)/1056.f;
    for(size_t i=0;i<VILLAGE_PROPS.size();i++){
        const auto& prop=VILLAGE_PROPS[i];
        SDL_Rect src{int(prop.atlasX*sx),int(prop.atlasY*sy),
                     int(prop.atlasW*sx),int(prop.atlasH*sy)};
        if(src.x<0||src.y<0||src.w<=0||src.h<=0||
           src.x+src.w>sheet->w||src.y+src.h>sheet->h)continue;
        SDL_Surface* crop=SDL_CreateSurface(src.w,src.h,SDL_PIXELFORMAT_RGBA32);
        if(!crop)continue;
        if(SDL_BlitSurface(sheet,&src,crop,nullptr)){
            // Remove only the black matte touching the crop perimeter,
            // without hollowing out windows, dark roof tiles or shadows.
            std::vector<Uint8> seen(size_t(src.w)*size_t(src.h),0);
            std::vector<int> queue;
            queue.reserve(seen.size());
            // The previous broad RGB<58 key removed shaded leaves and wood.
            // Find the near-black matte color at an outer corner, and erase
            // ONLY matching pixels connected to the crop's outer perimeter.
            Uint8 matteR=0,matteG=0,matteB=0,matteA=255;
            SDL_ReadSurfacePixel(crop,2,2,&matteR,&matteG,&matteB,&matteA);
            auto dark=[&](int x,int y){
                Uint8 r=0,g=0,b=0,a=255;
                if(!SDL_ReadSurfacePixel(crop,x,y,&r,&g,&b,&a))return false;
                if(a<12)return true;
                constexpr int tolerance=12;
                // Only dark, nearly neutral sheet backgrounds may be removed.
                return matteR<50 && matteG<50 && matteB<50 &&
                    std::abs(int(r)-int(matteR))<=tolerance &&
                    std::abs(int(g)-int(matteG))<=tolerance &&
                    std::abs(int(b)-int(matteB))<=tolerance;
            };
            auto enqueue=[&](int x,int y){
                if(x<0||y<0||x>=src.w||y>=src.h)return;
                size_t idx=size_t(y)*size_t(src.w)+size_t(x);
                if(seen[idx]||!dark(x,y))return;
                seen[idx]=1;queue.push_back(int(idx));
            };
            for(int x=0;x<src.w;x++){enqueue(x,0);enqueue(x,src.h-1);}
            for(int y=0;y<src.h;y++){enqueue(0,y);enqueue(src.w-1,y);}
            for(size_t head=0;head<queue.size();++head){
                int k=queue[head],x=k%src.w,y=k/src.w;
                SDL_WriteSurfacePixel(crop,x,y,0,0,0,0);
                enqueue(x-1,y);enqueue(x+1,y);enqueue(x,y-1);enqueue(x,y+1);
            }
            art.villageProps[i]=SDL_CreateTextureFromSurface(renderer,crop);
            if(art.villageProps[i]){
                SDL_SetTextureScaleMode(art.villageProps[i],SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(art.villageProps[i],SDL_BLENDMODE_BLEND);
            }
        }
        SDL_DestroySurface(crop);
    }
    SDL_DestroySurface(sheet);
#else
    (void)renderer;(void)art;
#endif
}
void drawVillageProps(SDL_Renderer* renderer,const ArtAssets& art,
                      Vec2 camera,float heroWorldY,bool foreground){
    for(size_t i=0;i<VILLAGE_PROPS.size();i++){
        const auto& p=VILLAGE_PROPS[i];
        if((p.feetY*village::SCALE>heroWorldY)!=foreground)continue;
        SDL_Texture* tex=art.villageProps[i];
        if(!tex)continue;
        SDL_FRect dst{p.mapX*village::SCALE-camera.x,
                      p.mapY*village::SCALE-camera.y,
                      p.mapW*village::SCALE,p.mapH*village::SCALE};
        if(dst.x>WIDTH || dst.y>HEIGHT || dst.x+dst.w<0 || dst.y+dst.h<0)continue;
        SDL_RenderTexture(renderer,tex,nullptr,&dst);
    }
}

// Exact sheet layout verified against family.png (reference: 1448 x 1086).
// Each row contains idle 3, down 3, left 3, right 3, up 3, talk 3.
// Keep all 18 poses for each relative: 0-3 walking, 4 idle, 5 talking.
constexpr int FAMILY_STATES=6;
constexpr int FAMILY_POSES=3;
constexpr int FAMILY_FRAME_W=62;
constexpr int FAMILY_FRAME_H=112;
constexpr int FAMILY_CENTERS[FAMILY_STATES][FAMILY_POSES]={
    {222,289,356},     // idle
    {442,506,571},     // walking down
    {648,712,777},     // walking left
    {853,918,983},     // walking right
    {1068,1132,1195}, // walking up
    {1267,1332,1396}  // talking
};
constexpr int FAMILY_TOPS[3]={269,522,782}; // father, mother, Lily

void loadFamilyFrames(SDL_Renderer* r,ArtAssets& art){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path sheetPath;
    for(const auto& candidate:{fs::path("assets/family.png"),
                              fs::path("../assets/family.png"),
                              fs::path("../../assets/family.png")}){
        if(fs::exists(candidate)){sheetPath=candidate;break;}
    }
    if(sheetPath.empty())return;
    SDL_Surface* sheet=IMG_Load(sheetPath.string().c_str());
    if(!sheet){std::cerr<<"Family sheet load failed: "<<SDL_GetError()<<"\n";return;}
    const float sx=float(sheet->w)/1448.f, sy=float(sheet->h)/1086.f;
    for(int who=0;who<3;who++)for(int state=0;state<FAMILY_STATES;state++)
    for(int pose=0;pose<FAMILY_POSES;pose++){
        const int cw=std::max(1,int(std::round(FAMILY_FRAME_W*sx)));
        const int ch=std::max(1,int(std::round(FAMILY_FRAME_H*sy)));
        const int cx=int(std::round(FAMILY_CENTERS[state][pose]*sx));
        const int top=int(std::round(FAMILY_TOPS[who]*sy));
        SDL_Rect src{cx-cw/2,top,cw,ch};
        if(src.x<0||src.y<0||src.x+src.w>sheet->w||src.y+src.h>sheet->h)
            continue;
        SDL_Surface* cropped=SDL_CreateSurface(cw,ch,SDL_PIXELFORMAT_RGBA32);
        if(!cropped)continue;
        if(SDL_BlitSurface(sheet,&src,cropped,nullptr)){
            // Conservative edge flood fill: never remove arbitrary dark pixels
            // inside hair, shoes, beards or clothing.
            Uint8 mr=25,mg=25,mb=30,ma=255;
            SDL_ReadSurfacePixel(cropped,2,2,&mr,&mg,&mb,&ma);
            std::vector<Uint8> seen(size_t(cw)*ch,0);
            std::vector<int> queue;
            queue.reserve(seen.size());
            auto isMatte=[&](int x,int y){
                Uint8 red=0,green=0,blue=0,alpha=255;
                if(!SDL_ReadSurfacePixel(cropped,x,y,&red,&green,&blue,&alpha))return false;
                constexpr int tolerance=15;
                return alpha<20 || (std::abs(int(red)-int(mr))<=tolerance &&
                       std::abs(int(green)-int(mg))<=tolerance &&
                       std::abs(int(blue)-int(mb))<=tolerance);
            };
            auto enqueue=[&](int x,int y){
                if(x<0||y<0||x>=cw||y>=ch)return;
                const size_t idx=size_t(y)*cw+size_t(x);
                if(seen[idx]||!isMatte(x,y))return;
                seen[idx]=1;queue.push_back(int(idx));
            };
            for(int x=0;x<cw;x++){enqueue(x,0);enqueue(x,ch-1);}
            for(int y=0;y<ch;y++){enqueue(0,y);enqueue(cw-1,y);}
            for(size_t head=0;head<queue.size();head++){
                int idx=queue[head],x=idx%cw,y=idx/cw;
                SDL_WriteSurfacePixel(cropped,x,y,0,0,0,0);
                enqueue(x-1,y);enqueue(x+1,y);enqueue(x,y-1);enqueue(x,y+1);
            }
            SDL_Texture* texture=SDL_CreateTextureFromSurface(r,cropped);
            if(texture){
                SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
                // Sheet state order differs from the in-game direction order:
                // game states 0/1/2/3 = down/left/right/up; 4 idle; 5 talk.
                const int gameState=state==0?4:(state==5?5:state-1);
                art.familyFrames[(who*FAMILY_STATES+gameState)*FAMILY_POSES+pose]=texture;
            }
        }
        SDL_DestroySurface(cropped);
    }
    SDL_DestroySurface(sheet);
#else
    (void)r;(void)art;
#endif
}
void drawFamilySprite(SDL_Renderer* r,const ArtAssets& art,int person,
                      int x,int y,int state,int pose){
    const int index=(person*FAMILY_STATES+state)*FAMILY_POSES+pose;
    if(index<0||index>=int(art.familyFrames.size()))return;
    SDL_Texture* texture=art.familyFrames[size_t(index)];
    if(!texture)return;
    SDL_FRect destination{float(x-21),float(y-56),42.f,67.f};
    SDL_RenderTexture(r,texture,nullptr,&destination);
}
// villager.png top panel: eight character columns, four walking directions,
// four poses per direction. Ignore labelled portrait/icon panels below y=410.
// Each crop is converted to RGBA and only its edge-connected dark matte is removed.
void loadVillagerFrames(SDL_Renderer* renderer,ArtAssets& art){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path file;
    for(const auto& candidate:{fs::path("assets/villager.png"),
                               fs::path("../assets/villager.png"),
                               fs::path("../../assets/villager.png")}){
        if(fs::exists(candidate)){file=candidate;break;}
    }
    if(file.empty()){std::cerr<<"villager.png missing; using simple NPCs\n";return;}
    SDL_Surface* sheet=IMG_Load(file.string().c_str());
    if(!sheet){std::cerr<<"Cannot load villager.png: "<<SDL_GetError()<<"\n";return;}
    // Measured against the actual 1536x1024 villager sheet, not a uniform grid.
    constexpr std::array<int,8> startX{{122,326,511,698,871,1040,1207,1390}};
        constexpr std::array<int,8> spacing{{45,42,42,39,40,39,42,41}};
        constexpr std::array<int,4> topY{{80,161,242,323}};
    const float sx=float(sheet->w)/1536.f,sy=float(sheet->h)/1024.f;
    int loaded=0;
    for(int who=0;who<8;who++)for(int direction=0;direction<4;direction++)
    for(int pose=0;pose<4;pose++){
        const int cx=startX[size_t(who)]+spacing[size_t(who)]*pose;
        const int left=int((cx-17)*sx),top=int(topY[size_t(direction)]*sy);
        const int w=std::max(1,int(34*sx)),h=std::max(1,int(65*sy));
        if(left<0||top<0||left+w>sheet->w||top+h>sheet->h)continue;
        SDL_Surface* crop=SDL_CreateSurface(w,h,SDL_PIXELFORMAT_RGBA32);
        if(!crop)continue;
        const SDL_Rect region{left,top,w,h};
        if(SDL_BlitSurface(sheet,&region,crop,nullptr)){
            std::vector<Uint8> visited(size_t(w)*size_t(h),0);
            std::vector<int> q;
            q.reserve(visited.size());
            auto background=[&](int x,int y){
                Uint8 red=0,green=0,blue=0,alpha=255;
                if(!SDL_ReadSurfacePixel(crop,x,y,&red,&green,&blue,&alpha))return false;
                // Only near-neutral matte matching the artwork backdrop.
                // Do not flood through dark hair, shoes, clothing or outlines.
                const int r=int(red),g=int(green),b=int(blue);
                return alpha<16 || (r>=17 && r<=41 &&
                    g>=16 && g<=41 && b>=19 && b<=46 &&
                    std::abs(r-g)<=10 && std::abs(b-g)<=13);
            };
            auto add=[&](int x,int y){
                if(x<0||y<0||x>=w||y>=h)return;
                const size_t idx=size_t(y)*size_t(w)+size_t(x);
                if(visited[idx]||!background(x,y))return;
                visited[idx]=1;q.push_back(int(idx));
            };
            for(int x=0;x<w;x++){add(x,0);add(x,h-1);}
            for(int y=0;y<h;y++){add(0,y);add(w-1,y);}
            for(size_t qi=0;qi<q.size();qi++){
                const int index=q[qi],x=index%w,y=index/w;
                SDL_WriteSurfacePixel(crop,x,y,0,0,0,0);
                add(x-1,y);add(x+1,y);add(x,y-1);add(x,y+1);
            }
            SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,crop);
            if(texture){
                SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
                art.villagerFrames[size_t((who*4+direction)*4+pose)]=texture;
                loaded++;
            }
        }
        SDL_DestroySurface(crop);
    }
    SDL_DestroySurface(sheet);
    std::cerr<<"Loaded villager.png walking poses: "<<loaded<<"/128\n";
#else
    (void)renderer;(void)art;
#endif
}
// Extract individually positioned furniture from the labelled-free shopitems.png.
// Edge-connected near-black matte is transparent; the wooden furniture stays
// opaque, including dark recesses not connected to the cell border.
void loadShopFurniture(SDL_Renderer* renderer,ArtAssets& art){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path imageFile;
    for(const auto& candidate:{fs::path("assets/shopitems.png"),
                               fs::path("../assets/shopitems.png"),
                               fs::path("../../assets/shopitems.png")}){
        if(fs::exists(candidate)){imageFile=candidate;break;}
    }
    if(imageFile.empty())return;
    SDL_Surface* image=IMG_Load(imageFile.string().c_str());
    if(!image){std::cerr<<"shopitems.png: "<<SDL_GetError()<<"\n";return;}
    const float sx=float(image->w)/1536.f,sy=float(image->h)/1024.f;
    for(size_t i=0;i<shop::furniture.size();i++){
        const auto& prop=shop::furniture[i];
        SDL_Rect from{int(prop.srcX*sx),int(prop.srcY*sy),
                      std::max(1,int(prop.srcW*sx)),std::max(1,int(prop.srcH*sy))};
        if(from.x<0||from.y<0||from.x+from.w>image->w||
           from.y+from.h>image->h)continue;
        SDL_Surface* crop=SDL_CreateSurface(from.w,from.h,SDL_PIXELFORMAT_RGBA32);
        if(!crop)continue;
        if(SDL_BlitSurface(image,&from,crop,nullptr)){
            std::vector<Uint8> marked(size_t(from.w)*size_t(from.h),0);
            std::vector<int> q;
            auto isMatte=[&](int x,int y){
                Uint8 r=0,g=0,b=0,a=255;
                if(!SDL_ReadSurfacePixel(crop,x,y,&r,&g,&b,&a))return false;
                return a<16 || (r<48 && g<38 && b<40);
            };
            auto enqueue=[&](int x,int y){
                if(x<0||y<0||x>=from.w||y>=from.h)return;
                size_t k=size_t(y)*size_t(from.w)+size_t(x);
                if(marked[k]||!isMatte(x,y))return;
                marked[k]=1;q.push_back(int(k));
            };
            for(int x=0;x<from.w;x++){enqueue(x,0);enqueue(x,from.h-1);}
            for(int y=0;y<from.h;y++){enqueue(0,y);enqueue(from.w-1,y);}
            for(size_t head=0;head<q.size();head++){
                const int index=q[head],x=index%from.w,y=index/from.w;
                SDL_WriteSurfacePixel(crop,x,y,0,0,0,0);
                enqueue(x-1,y);enqueue(x+1,y);
                enqueue(x,y-1);enqueue(x,y+1);
            }
            art.shopFurniture[i]=SDL_CreateTextureFromSurface(renderer,crop);
            if(art.shopFurniture[i]){
                SDL_SetTextureScaleMode(art.shopFurniture[i],SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(art.shopFurniture[i],SDL_BLENDMODE_BLEND);
            }
        }
        SDL_DestroySurface(crop);
    }
    SDL_DestroySurface(image);
#else
    (void)renderer;(void)art;
#endif
}
void drawShopFurniture(SDL_Renderer* r,const ArtAssets& art,
                       float playerY,bool foreground){
    for(size_t i=0;i<shop::furniture.size();i++){
        const auto& p=shop::furniture[i];
        if((shop::sortY(i)>playerY)!=foreground)continue;
        SDL_Texture* t=art.shopFurniture[i];
        if(!t)continue;
        const SDL_FRect dst{p.x,p.y,p.w,p.h};
        SDL_RenderTexture(r,t,nullptr,&dst);
    }
}

ArtAssets loadArtAssets(SDL_Renderer* renderer){
    ArtAssets result;
#if RPG_HAS_SDL3_IMAGE
    result.home=loadArt(renderer,"home.png");
#endif
    if(!result.home)result.home=loadArt(renderer,"home.bmp");
#if RPG_HAS_SDL3_IMAGE
    result.village=loadArt(renderer,"village.png");
    result.expandedVillage=(result.village!=nullptr);
#endif
    if(!result.village)result.village=loadArt(renderer,"village.bmp");
    result.outskirts=loadArt(renderer,"outskirts.bmp");
    result.hero=loadArt(renderer,"hero.bmp");
    result.father=loadArt(renderer,"father.bmp");
    result.mother=loadArt(renderer,"mother.bmp");
    result.lily=loadArt(renderer,"lily.bmp");
    loadFamilyFrames(renderer,result);
    loadHeroFrames(renderer,result);
    loadVillageProps(renderer,result);
    loadVillagerFrames(renderer,result);
    result.uiAtlas=loadArt(renderer,"ui.png");
    result.itemsAtlas=loadArt(renderer,"items.png");
    result.shopBackground=loadArt(renderer,"shopback.png");
    loadShopFurniture(renderer,result);
    result.fountainBase=loadArt(renderer,"fountain_base.png");
    result.fountainAnimation=loadNewFountainAnimation(renderer);
    if(!result.fountainAnimation)
        result.fountainWater=loadArt(renderer,"fountain_water_12frames.png");
    // A missing base must not turn the new water sheet into an all-blue fountain.
    // The existing spring.png remains a last-resort fixed fallback.
    if(!result.fountainBase)
        loadFountainFrames(renderer,result);
    std::cerr<<"Fountain renderer: "<<(result.fountainBase?"fixed stone base":"legacy base")
             <<", water: "<<(result.fountainAnimation?"new 12-frame sheet":
                              result.fountainWater?"split 12-frame sheet":"none")<<"\n";
    if(!result.home||!result.village||!result.outskirts)
        std::cerr<<"AI art missing: extract art pack so assets/*.bmp exists. Using fallback visuals.\n";
    return result;
}
void drawBackground(SDL_Renderer* r,SDL_Texture* art){
    SDL_FRect dst{0.f,0.f,float(WIDTH),float(HEIGHT)};
    SDL_RenderTexture(r,art,nullptr,&dst);
}
void destroyArtAssets(ArtAssets& art){
    for(SDL_Texture* t:{art.home,art.village,art.outskirts,
                       art.hero,art.father,art.mother,art.lily})
        if(t)SDL_DestroyTexture(t);
    for(SDL_Texture* t:art.familyFrames)
        if(t)SDL_DestroyTexture(t);
    for(SDL_Texture* t:art.heroFrames)
        if(t)SDL_DestroyTexture(t);
    for(SDL_Texture* t:art.villageProps)
        if(t)SDL_DestroyTexture(t);
    for(SDL_Texture* t:art.fountainFrames)
        if(t)SDL_DestroyTexture(t);
    if(art.fountainBase)SDL_DestroyTexture(art.fountainBase);
    if(art.fountainWater)SDL_DestroyTexture(art.fountainWater);
    if(art.fountainAnimation)SDL_DestroyTexture(art.fountainAnimation);
    if(art.uiAtlas)SDL_DestroyTexture(art.uiAtlas);
    if(art.itemsAtlas)SDL_DestroyTexture(art.itemsAtlas);
    if(art.shopBackground)SDL_DestroyTexture(art.shopBackground);
    for(SDL_Texture* t:art.shopFurniture)if(t)SDL_DestroyTexture(t);
    for(SDL_Texture* t:art.villagerFrames)if(t)SDL_DestroyTexture(t);
}
void color(SDL_Renderer* r,Uint8 red,Uint8 green,Uint8 blue,Uint8 alpha=255){
    SDL_SetRenderDrawColor(r,red,green,blue,alpha);
}
void rect(SDL_Renderer* r,int x,int y,int w,int h,Uint8 red,Uint8 green,Uint8 blue){
    color(r,red,green,blue);
    SDL_FRect q{float(x),float(y),float(w),float(h)}; SDL_RenderFillRect(r,&q);
}
void outline(SDL_Renderer* r,int x,int y,int w,int h,Uint8 red,Uint8 green,Uint8 blue){
    color(r,red,green,blue);
    SDL_FRect q{float(x),float(y),float(w),float(h)}; SDL_RenderRect(r,&q);
}
void line(SDL_Renderer* r,int x1,int y1,int x2,int y2,Uint8 red,Uint8 green,Uint8 blue){
    color(r,red,green,blue); SDL_RenderLine(r,float(x1),float(y1),float(x2),float(y2));
}
uint32_t noise(int x,int y,int salt=0){
    uint32_t n=static_cast<uint32_t>(x*9781+y*6271+salt*3109);
    n^=n>>16; n*=0x7feb352dU; n^=n>>15; n*=0x846ca68bU; return n^(n>>16);
}
bool inHouse(int tx,int ty){
    return (tx>=3 && tx<=8 && ty>=3 && ty<=7) ||
           (tx>=21 && tx<=26 && ty>=3 && ty<=7) ||
           (tx>=4 && tx<=9 && ty>=14 && ty<=17);
}
bool inTree(int tx,int ty){
    if(tx<1 || ty<1 || tx>=COLS-1 || ty>=ROWS-1) return false;
    if(inHouse(tx,ty)) return false;
    if(tx>=12 && tx<=18) return false;
    if(ty>=9 && ty<=11) return false;
    return noise(tx,ty,13)%17==0;
}
bool pathTile(int tx,int ty){
    return (ty>=9 && ty<=11) || (tx>=14 && tx<=16) ||
           (tx>=5 && tx<=7 && ty>=7 && ty<=10) ||
           (tx>=23 && tx<=24 && ty>=7 && ty<=10);
}
bool solidTile(int tx,int ty){
    if(tx<0||ty<0||tx>=COLS||ty>=ROWS) return true;
    return inHouse(tx,ty) || inTree(tx,ty);
}
bool blocked(Vec2 p,float radius){
    for(int y=static_cast<int>(std::floor((p.y-radius)/TILE));
        y<=static_cast<int>(std::floor((p.y+radius)/TILE));++y)
        for(int x=static_cast<int>(std::floor((p.x-radius)/TILE));
            x<=static_cast<int>(std::floor((p.x+radius)/TILE));++x)
            if(solidTile(x,y) && overlaps(p.x-radius,p.y-radius,2*radius,2*radius,
                                         x*TILE,y*TILE,TILE,TILE)) return true;
    return false;
}
void moveBody(Vec2& p,Vec2 velocity,float dt,float radius){
    Vec2 next{p.x+velocity.x*dt,p.y};
    if(!blocked(next,radius)) p.x=next.x;
    next={p.x,p.y+velocity.y*dt};
    if(!blocked(next,radius)) p.y=next.y;
}
void fillText(SDL_Renderer* r,const std::string& text,int x,int y,int scale,
              Uint8 red,Uint8 green,Uint8 blue){
    // Compact 3x5 bitmap alphabet. No SDL_ttf or asset files required.
    static const std::array<std::pair<char,const char*>,72> glyphs{{
        {'A',"010101111101101"},{'B',"110101110101110"},{'C',"011100100100011"},
        {'D',"110101101101110"},{'E',"111100110100111"},{'F',"111100110100100"},
        {'G',"011100101101011"},{'H',"101101111101101"},{'I',"111010010010111"},
        {'J',"001001001101010"},{'K',"101101110101101"},{'L',"100100100100111"},
        {'M',"101111111101101"},{'N',"101111111111101"},{'O',"010101101101010"},
        {'P',"110101110100100"},{'Q',"010101101111011"},{'R',"110101110101101"},
        {'S',"011100010001110"},{'T',"111010010010010"},{'U',"101101101101111"},
        {'V',"101101101101010"},{'W',"101101111111101"},{'X',"101101010101101"},
        {'Y',"101101010010010"},{'Z',"111001010100111"},
        {'0',"111101101101111"},{'1',"010110010010111"},{'2',"110001111100111"},
        {'3',"110001111001110"},{'4',"101101111001001"},{'5',"111100110001110"},
        {'6',"011100110101010"},{'7',"111001010010010"},{'8',"010101010101010"},
        {'9',"010101011001110"},{':',"000010000010000"},{'/',"001001010100100"},
        {'-',"000000111000000"},{'.',"000000000000010"},{'!',"010010010000010"},
        {' ',"000000000000000"},
        {'a',"000011101111101"},
        {'b',"100110101101110"},
        {'c',"000011100100011"},
        {'d',"001011101101011"},
        {'e',"000010101110011"},
        {'f',"011100110100100"},
        {'g',"000011101011110"},
        {'h',"100110101101101"},
        {'i',"010000010010010"},
        {'j',"001000001101010"},
        {'k',"100101110110101"},
        {'l',"110010010010011"},
        {'m',"000110111111101"},
        {'n',"000110101101101"},
        {'o',"000010101101010"},
        {'p',"000110101110100"},
        {'q',"000011101011001"},
        {'r',"000101110100100"},
        {'s',"000011110011110"},
        {'t',"010111010010011"},
        {'u',"000101101101011"},
        {'v',"000101101101010"},
        {'w',"000101111111010"},
        {'x',"000101010010101"},
        {'y',"000101101011110"},
        {'z',"000111001010111"},
        {'?', "111001010000010"},
        {',', "000000000010100"},
        {'\'', "010010000000000"}
    }};
    int start=x;
    for(char ch:text){
        if(ch=='\n'){y+=scale*7;x=start;continue;}
        const char* bits=nullptr;
        for(const auto& g:glyphs) if(g.first==ch){bits=g.second;break;}
        if(bits) for(int iy=0;iy<5;iy++) for(int ix=0;ix<3;ix++)
            if(bits[iy*3+ix]=='1') rect(r,x+ix*scale,y+iy*scale,scale,scale,red,green,blue);
        x+=scale*4;
    }
}
void drawTree(SDL_Renderer* r,int x,int y,uint32_t seed){
    rect(r,x+13,y+17,7,14,77,51,30);
    rect(r,x+4,y+8,24,16,29,73,48);
    rect(r,x+8,y+2,17,24,43,96,54);
    rect(r,x+3,y+12,13,10,36,105,53);
    rect(r,x+16,y+9,12,12,46,110,62);
    if(seed%3==0)rect(r,x+10,y+6,4,4,76,138,66);
}
void drawHouse(SDL_Renderer* r,int x,int y,int w,int h,bool tavern){
    rect(r,x+4,y+17,w-8,h-17,165,131,84);
    rect(r,x,y+13,w,h-25,91,64,44);
    rect(r,x+7,y+20,w-14,h-35,168,138,92);
    rect(r,x-7,y+4,w+14,19,62,65,74);
    rect(r,x-2,y-2,w+4,13,82,85,95);
    for(int i=0;i<w/22;i++) line(r,x+i*22,y+2,x+i*22-10,y+16,112,109,107);
    rect(r,x+w/2-12,y+h-40,24,35,71,47,33);
    rect(r,x+w/2-9,y+h-36,18,29,91,58,39);
    rect(r,x+16,y+37,20,22,52,43,34);
    rect(r,x+19,y+40,14,16,213,168,86);
    rect(r,x+w-36,y+37,20,22,52,43,34);
    rect(r,x+w-33,y+40,14,16,213,168,86);
    if(tavern){
        rect(r,x+w/2-17,y+24,34,13,96,57,40);
        fillText(r,"INN",x+w/2-12,y+27,2,242,212,132);
    }
}
void drawMap(SDL_Renderer* r,float time){
    rect(r,0,0,WIDTH,HEIGHT,55,102,67);
    for(int y=0;y<ROWS;y++)for(int x=0;x<COLS;x++){
        uint32_t n=noise(x,y);
        int px=x*TILE,py=y*TILE;
        if(pathTile(x,y)){
            int variation=static_cast<int>(n%18);
            rect(r,px,py,TILE,TILE,133+variation,112+variation,78+variation/2);
            if(n%3==0) rect(r,px+5+(n%17),py+8,5,3,105,89,69);
        }else{
            int variation=static_cast<int>(n%20);
            rect(r,px,py,TILE,TILE,48+variation,100+variation,62+variation/3);
            if(!solidTile(x,y)){
                if(n%4==0)rect(r,px+6+(n%18),py+11,2,5,100,146,80);
                if(n%11==0){
                    rect(r,px+15,py+18,3,3,231,199,104);
                    rect(r,px+19,py+17,3,3,230,228,175);
                }
            }
        }
    }
    // Stream, bridges, stone well and village decorations.
    for(int y=0;y<HEIGHT;y+=12){
        int xx=WIDTH-34+static_cast<int>(8*std::sin(y*0.03));
        rect(r,xx,y,34,12,45,96,126);
        line(r,xx+5,y+5,xx+19,y+5,77,146,173);
    }
    for(int y=0;y<ROWS;y++)for(int x=0;x<COLS;x++)
        if(inTree(x,y)) drawTree(r,x*TILE,y*TILE,noise(x,y));
    drawHouse(r,3*TILE,3*TILE,6*TILE,5*TILE,true);
    drawHouse(r,21*TILE,3*TILE,6*TILE,5*TILE,false);
    drawHouse(r,4*TILE,14*TILE,6*TILE,4*TILE,false);
    rect(r,19*TILE,13*TILE,64,9,92,77,56);
    rect(r,19*TILE+8,13*TILE-21,48,24,127,119,104);
    rect(r,19*TILE+16,13*TILE-16,32,17,48,65,74);
    rect(r,19*TILE+26,13*TILE-13,12,10,57,121,150);
    for(int i=0;i<7;i++){
        int x=11*TILE+i*32;
        rect(r,x,2*TILE,4,22,95,69,45);
        rect(r,x,2*TILE,31,4,134,108,71);
    }
    (void)time;
}
void drawKnight(SDL_Renderer* r,Vec2 p,Vec2 facing,float slash,bool hurt){
    int x=static_cast<int>(p.x), y=static_cast<int>(p.y);
    rect(r,x-13,y+12,27,6,39,66,48);
    rect(r,x-9,y+3,7,13,48,47,57);
    rect(r,x+3,y+3,7,13,48,47,57);
    rect(r,x-11,y-14,22,24,hurt?207:49,hurt?83:88,hurt?73:135);
    rect(r,x-7,y-13,14,9,133,144,163);
    rect(r,x-9,y-27,18,16,147,154,165);
    rect(r,x-6,y-24,12,5,64,74,88);
    rect(r,x-10,y-28,20,4,204,199,154);
    rect(r,x-15,y-11,5,17,135,145,160);
    rect(r,x+11,y-11,5,17,135,145,160);
    // Red cape and crest.
    rect(r,x-3,y-13,7,17,153,49,48);
    rect(r,x-1,y-13,3,12,220,177,78);
    // Sword and attack trail.
    int sx=x+static_cast<int>(facing.x*25)+12;
    int sy=y+static_cast<int>(facing.y*25)-9;
    line(r,x+12,y-5,sx,sy,201,206,216);
    rect(r,sx-3,sy-3,7,4,224,210,160);
    if(slash>0){
        color(r,243,216,126);
        for(int i=-2;i<=2;i++){
            int ax=x+static_cast<int>(facing.x*42-facing.y*i*9);
            int ay=y+static_cast<int>(facing.y*42+facing.x*i*9);
            SDL_RenderPoint(r,float(ax),float(ay));
            rect(r,ax-2,ay-2,4,4,244,213,132);
        }
    }
}
void drawEnemy(SDL_Renderer* r,const Enemy& e){
    int x=static_cast<int>(e.pos.x),y=static_cast<int>(e.pos.y+std::sin(e.wobble)*2);
    rect(r,x-12,y+12,24,5,36,66,47);
    rect(r,x-10,y-12,20,25,94,65,45);
    rect(r,x-13,y-23,26,16,94,112,65);
    rect(r,x-8,y-20,5,4,239,93,65);
    rect(r,x+3,y-20,5,4,239,93,65);
    rect(r,x-13,y-27,9,10,71,89,52);
    rect(r,x+5,y-27,9,10,71,89,52);
    rect(r,x-15,y-3,5,18,74,66,56);
    rect(r,x+11,y-3,5,18,74,66,56);
    rect(r,x-12,y-33,24,3,62,42,35);
    rect(r,x-12,y-33,static_cast<int>(24*e.hp/3.f),3,188,66,57);
}
void drawHud(SDL_Renderer* r,const World& w){
    // Persistent health bar is drawn separately in all scenes.
    fillText(r,"KNIGHT OF ASHVALE",23,14,2,231,202,135);
    rect(r,WIDTH-225,10,213,54,22,32,38);
    outline(r,WIDTH-225,10,213,54,189,153,88);
    fillText(r,"GOBLINS: "+std::to_string(w.hero.kills)+"/8",WIDTH-213,21,3,233,199,133);
    rect(r,9,HEIGHT-35,WIDTH-18,25,22,32,38);
    outline(r,9,HEIGHT-35,WIDTH-18,25,189,153,88);
    fillText(r,"WASD MOVE   SPACE ATTACK   R RESTART   ESC QUIT",23,HEIGHT-28,2,231,209,168);
}
void reset(World& w){
    w=World{};
    w.hero.pos={480,470};
    showDialogue(w,"NARRATOR","ASHVALE. A QUIET MORNING.\nTHE FIRE IS WARM AND HOME IS SAFE.");
    /* The old encounter is saved until the story reaches the road. */
    w.enemies={
        {{345,271},3,0,0},{{625,277},3,0,1},{{717,427},3,0,2},
        {{305,430},3,0,3},{{787,365},3,0,4},{{140,345},3,0,5},
        {{580,560},3,0,6},{{806,545},3,0,7}
    };
    // Move any accidentally obstructed spawn to the central clearing.
    w.enemies.clear();
    for(size_t i=0;i<w.enemies.size();++i)
        if(blocked(w.enemies[i].pos,12))
            w.enemies[i].pos={345.f+static_cast<float>(i%4)*72.f,
                               335.f+static_cast<float>(i/4)*62.f};
}
void goVillage(World& w);
void goOutskirts(World& w);
void exitShop(World& w);
void enterHome(World& w);

void attack(World& w){
    if(w.story<9 || !w.dialogue.empty() || w.won || w.lost || w.hero.slashTimer>0) return;
    w.hero.slashTimer=0.32f;
    for(auto& e:w.enemies){
        Vec2 delta{e.pos.x-w.hero.pos.x,e.pos.y-w.hero.pos.y};
        if(!withinRange(w.hero.pos,e.pos,66)) continue;
        Vec2 d=normalized(delta);
        if(d.x*w.hero.facing.x+d.y*w.hero.facing.y>=0.20f){
            --e.hp;
            if(e.hp<=0)++w.hero.kills;
        }
    }
    if(w.hero.kills>=8)w.won=true;
}
void update(World& w,const bool* keys,float dt){
    if(w.won||w.lost) return;
    w.time+=dt;
    if(!w.dialogue.empty()){
        w.dialogueAge+=dt;
        return;
    }
    if(w.scene==Scene::HOME)updateFamilyMotion(w.family,w.hero.pos,dt);
    if(w.scene==Scene::VILLAGE && w.largeVillage)
        updateVillagerMotion(w.villagers,w.hero.pos,dt);
    w.hero.slashTimer=std::max(0.f,w.hero.slashTimer-dt);
    w.hero.moving=false;
    w.hero.hurtTimer=std::max(0.f,w.hero.hurtTimer-dt);
    Vec2 input{static_cast<float>(int(keys[SDL_SCANCODE_D])-int(keys[SDL_SCANCODE_A])),
               static_cast<float>(int(keys[SDL_SCANCODE_S])-int(keys[SDL_SCANCODE_W]))};
    auto dir=normalized(input);
    if(dir.x!=0 || dir.y!=0){
        w.hero.facing=dir;
        w.hero.moving=true;
        w.hero.animationTime+=dt;
    }else{
        w.hero.animationTime=0.f;
    }
    if(dir.x!=0 || dir.y!=0){
        if(w.scene==Scene::HOME){
            w.hero.pos.x=clamp(w.hero.pos.x+dir.x*PLAYER_SPEED*dt,88.f,float(WIDTH-90));
            w.hero.pos.y=clamp(w.hero.pos.y+dir.y*PLAYER_SPEED*dt,112.f,float(HEIGHT-86));
        }else if(w.scene==Scene::SHOP){
            shop::move(w.hero.pos,{dir.x*PLAYER_SPEED,dir.y*PLAYER_SPEED},dt);
        }else if(w.scene==Scene::OUTSKIRTS){
            w.hero.pos.x=clamp(w.hero.pos.x+dir.x*PLAYER_SPEED*dt,20.f,float(WIDTH-20));
            w.hero.pos.y=clamp(w.hero.pos.y+dir.y*PLAYER_SPEED*dt,195.f,float(HEIGHT-20));
        }else{
            if(w.largeVillage)village::move(w.hero.pos,{dir.x*PLAYER_SPEED,dir.y*PLAYER_SPEED},dt,8);
            else moveBody(w.hero.pos,{dir.x*PLAYER_SPEED,dir.y*PLAYER_SPEED},dt,11);
        }
    }
    // Walk through doorways without pressing E or finishing an NPC dialogue.
    // Directional triggers prevent bouncing straight back on scene entry.
    if(dir.x!=0.f || dir.y!=0.f){
        if(w.scene==Scene::HOME && dir.y>0.25f &&
           w.hero.pos.y>=HEIGHT-97.f &&
           w.hero.pos.x>418.f && w.hero.pos.x<550.f){
            w.story=std::max(w.story,4);
            goVillage(w);
            return;
        }
        if(w.scene==Scene::VILLAGE){
            const Vec2 door=w.largeVillage?village::pos(405.f,362.f):Vec2{400.f,275.f};
            if(dir.y<-.30f && withinRange(w.hero.pos,door,44.f)){
                enterHome(w);
                return;
            }
            // Cross the east / mill bridge before the outskirts transition.
            const Vec2 exit=w.largeVillage?village::pos(1395.f,751.f):Vec2{832.f,345.f};
            if(dir.x>0.3f && withinRange(w.hero.pos,exit,51.f)){
                w.story=std::max(w.story,8);
                goOutskirts(w);
                return;
            }
        }
        if(w.scene==Scene::SHOP && dir.y>0.30f && shop::nearDoor(w.hero.pos)){
            exitShop(w);
            return;
        }
        if(w.scene==Scene::OUTSKIRTS && w.story<9 && dir.x<-.30f &&
           w.hero.pos.x<=35.f){
            w.scene=Scene::VILLAGE;
            w.hero.pos=w.largeVillage?village::pos(1376.f,751.f):Vec2{780.f,345.f};
            w.hero.facing={-1.f,0.f};
            w.hero.moving=false;
            w.dialogue.clear();
            return;
        }
    }
    if(w.story<9)return;
    for(auto& e:w.enemies){
        if(e.hp<=0)continue;
        e.wobble+=dt*5;
        e.attackTimer=std::max(0.f,e.attackTimer-dt);
        Vec2 delta{w.hero.pos.x-e.pos.x,w.hero.pos.y-e.pos.y};
        if(withinRange(w.hero.pos,e.pos,360) && !withinRange(w.hero.pos,e.pos,28)){
            auto chase=normalized(delta);
            e.pos.x=clamp(e.pos.x+chase.x*ENEMY_SPEED*dt,20.f,float(WIDTH-20));
            e.pos.y=clamp(e.pos.y+chase.y*ENEMY_SPEED*dt,195.f,float(HEIGHT-20));
        }
        if(withinRange(w.hero.pos,e.pos,32) && e.attackTimer<=0){
            e.attackTimer=1.15f;
            if(w.hero.hurtTimer<=0){
                w.hero.hp=damage(w.hero.hp,12);
                w.hero.hurtTimer=0.55f;
                if(w.hero.hp<=0)w.lost=true;
            }
        }
    }
}

struct Character {
    Vec2 p;
    const char* name;
    Uint8 r,g,b;
    bool older=false;
    bool child=false;
};
const std::array<Character,3> FAMILY{{
    {{300,260},"MOTHER",157,65,85,false,false},
    {{615,270},"FATHER",91,104,129,true,false},
    {{460,405},"LILY",192,113,72,false,true}
}};
const std::array<Vec2,3> LARGE_VILLAGE_NPCS{{
    village::pos(774,565),
    village::pos(670,564),
    village::pos(971,767)
}};
const Vec2 LARGE_VILLAGE_EXIT=village::pos(1395.f,751.f);
Vec2 npcPosition(const World& w,int index){
    if(w.largeVillage)return LARGE_VILLAGE_NPCS[size_t(index)];
    constexpr std::array<Vec2,3> fallback{{{480,255},{345,325},{670,325}}};
    return fallback[size_t(index)];
}
const std::array<Character,3> VILLAGERS{{
    {{480,255},"ELDER",125,83,72,true,false},
    {{345,325},"MERCHANT",96,137,106,false,false},
    {{670,325},"GUARD",123,133,158,false,false}
}};
void drawPerson(SDL_Renderer* r,const Character& c) {
    int x=int(c.p.x),y=int(c.p.y),height=c.child?15:20;
    rect(r,x-13,y+14,26,5,37,59,49);
    rect(r,x-8,y,6,14,63,60,66);
    rect(r,x+3,y,6,14,63,60,66);
    rect(r,x-11,y-height,22,height+2,c.r,c.g,c.b);
    rect(r,x-15,y-height+4,5,14,207,159,118);
    rect(r,x+11,y-height+4,5,14,207,159,118);
    rect(r,x-9,y-height-16,18,16,224,176,127);
    rect(r,x-10,y-height-19,20,7,c.older?109:95,c.older?91:59,c.older?71:44);
    rect(r,x-11,y-height-15,4,10,c.older?132:99,c.older?115:68,c.older?93:48);
    rect(r,x+7,y-height-15,4,10,c.older?132:99,c.older?115:68,c.older?93:48);
    rect(r,x-5,y-height-9,3,3,55,50,49);
    rect(r,x+3,y-height-9,3,3,55,50,49);
    if(c.older)rect(r,x-6,y-height-3,12,7,166,147,119);
    if(c.child)rect(r,x-14,y-height-15,6,8,159,82,56);
}
void drawVillagerSprite(SDL_Renderer* renderer,const ArtAssets& art,
                        int who,Vec2 feet,Vec2 facing,bool moving,float clock,
                        const Character& fallback){
    const int direction=std::abs(facing.x)>std::abs(facing.y)?
        (facing.x<0.f?1:2):(facing.y<0.f?3:0);
    const int pose=moving?int(clock*7.f)%4:0;
    const int idx=(who*4+direction)*4+pose;
    SDL_Texture* texture=art.villagerFrames[size_t(idx)];
    if(!texture)texture=art.villagerFrames[size_t((who*4+direction)*4)];
    if(texture){
        SDL_FRect destination{feet.x-20.f,feet.y-61.f,40.f,66.f};
        SDL_RenderTexture(renderer,texture,nullptr,&destination);
    }else{
        Character npc=fallback;
        npc.p=feet;
        drawPerson(renderer,npc);
    }
}
// Story NPCs retain fixed plot coordinates and interactions; additional NPCs
// stroll near their respective homes. Draw in two passes around the hero.
void drawVillagePopulation(SDL_Renderer* renderer,const ArtAssets& art,
                           const World& world,Vec2 camera,bool foreground){
    const int questType[3]={0,2,6}; // Elder / Merchant / Guard
    for(int i=0;i<3;i++){
        const Vec2 location=npcPosition(world,i);
        if((location.y>world.hero.pos.y)!=foreground)continue;
        Character npc=VILLAGERS[size_t(i)];
        npc.p={location.x-camera.x,location.y-camera.y};
        drawVillagerSprite(renderer,art,questType[i],npc.p,{0.f,1.f},false,
                           world.time,npc);
    }
    constexpr std::array<int,VILLAGER_COUNT> types{{1,3,4,5,6,7,1,3}};
    for(int i=0;i<VILLAGER_COUNT;i++){
        const VillagerMotion& motion=world.villagers[size_t(i)];
        if((motion.position.y>world.hero.pos.y)!=foreground)continue;
        const Vec2 screen{motion.position.x-camera.x,motion.position.y-camera.y};
        if(screen.x<-80.f||screen.y<-95.f||screen.x>WIDTH+80.f||screen.y>HEIGHT+75.f)continue;
        Character fallback{{0,0},"VILLAGER",106,132,85,false,false};
        drawVillagerSprite(renderer,art,types[size_t(i)],screen,motion.facing,
                           motion.moving,motion.walkingTime,fallback);
    }
}

void drawYoungHero(SDL_Renderer* r,Vec2 p) {
    int x=int(p.x),y=int(p.y);
    rect(r,x-13,y+12,27,5,37,56,47);
    rect(r,x-8,y+1,7,12,60,58,69);
    rect(r,x+2,y+1,7,12,60,58,69);
    rect(r,x-10,y-14,20,17,57,95,160);
    rect(r,x-13,y-12,4,14,191,143,107);
    rect(r,x+10,y-12,4,14,191,143,107);
    rect(r,x-9,y-29,18,17,223,172,121);
    rect(r,x-11,y-31,22,11,93,52,34);
    rect(r,x-4,y-18,3,3,57,43,44);
    rect(r,x+4,y-18,3,3,57,43,44);
    rect(r,x-3,y-12,6,11,183,55,63);
}
void drawCottage(SDL_Renderer* r,float time) {
    rect(r,0,0,WIDTH,HEIGHT,29,24,29);
    rect(r,40,30,WIDTH-80,HEIGHT-70,96,65,46);
    for(int y=50;y<HEIGHT-40;y+=23){
        rect(r,50,y,WIDTH-100,2,73,48,38);
        for(int x=70+(y%4)*20;x<WIDTH-70;x+=95)
            rect(r,x,y+2,2,20,84,55,40);
    }
    rect(r,45,32,WIDTH-90,29,54,41,40);
    rect(r,48,62,22,HEIGHT-117,58,39,35);
    rect(r,WIDTH-70,62,22,HEIGHT-117,58,39,35);
    rect(r,45,HEIGHT-68,WIDTH-90,21,52,40,38);
    // central hearth with animated fire
    rect(r,105,74,165,152,73,77,82);
    rect(r,123,86,129,131,139,140,135);
    rect(r,145,130,84,73,49,38,35);
    rect(r,152,149,71,49,22,18,19);
    int flicker=int(7*std::sin(time*11));
    rect(r,175,151-flicker,30,44+flicker,242,139,43);
    rect(r,183,129+flicker,15,57-flicker,255,211,88);
    rect(r,174,187,42,8,115,56,30);
    // Window with distant green hills
    rect(r,350,73,209,113,52,43,39);
    rect(r,358,80,193,100,138,195,207);
    rect(r,359,133,192,46,92,137,83);
    rect(r,358,97,193,25,139,185,216);
    rect(r,450,81,11,101,101,65,46);
    rect(r,359,123,192,9,101,65,46);
    // Cupboards, hanging pans, plants
    rect(r,640,73,201,139,94,61,44);
    rect(r,652,85,176,18,65,43,37);
    rect(r,658,113,23,38,147,94,63);
    rect(r,700,116,26,36,65,84,73);
    rect(r,750,118,23,35,142,111,67);
    rect(r,790,114,23,38,128,79,50);
    rect(r,652,162,176,15,65,43,37);
    for(int i=0;i<5;i++)rect(r,663+i*31,181,23,26,119,92,66);
    // Family table
    rect(r,350,330,262,92,56,37,32);
    rect(r,360,319,242,86,134,87,51);
    rect(r,370,329,222,67,164,116,65);
    rect(r,401,344,36,17,218,185,111);
    rect(r,496,347,21,17,213,212,166);
    rect(r,555,347,17,20,134,158,129);
    // Bed, carpet, lanterns
    rect(r,712,385,159,155,63,48,56);
    rect(r,722,392,140,140,126,113,147);
    rect(r,728,402,129,38,207,195,178);
    rect(r,728,449,129,75,97,103,142);
    rect(r,125,434,202,94,116,52,47);
    rect(r,132,440,188,83,146,76,59);
    rect(r,152,450,150,62,178,116,66);
    // Exit threshold
    rect(r,427,HEIGHT-68,106,27,112,75,46);
    rect(r,434,HEIGHT-68,92,22,159,112,64);
    rect(r,446,HEIGHT-64,69,6,193,153,89);
    fillText(r,"EXIT",453,HEIGHT-56,3,244,221,166);
    for(int x: {80,870}){
        rect(r,x,240,10,32,90,61,43);
        rect(r,x-8,228,26,14,245,188,88);
        rect(r,x-4,223,17,7,255,220,142);
    }
}
void drawOutskirts(SDL_Renderer* r,float time){
    rect(r,0,0,WIDTH,HEIGHT,76,130,95);
    rect(r,0,0,WIDTH,170,113,164,183);
    rect(r,0,118,WIDTH,97,76,125,119);
    for(int x=0;x<WIDTH;x+=35) {
        int ridge=165+int(27*std::sin(x*.014f));
        rect(r,x,ridge,36,85,56,105,87);
    }
    for(int y=208;y<HEIGHT;y+=32)for(int x=0;x<WIDTH;x+=32){
        auto n=noise(x/32,y/32,21);
        rect(r,x,y,32,32,52+int(n%27),113+int(n%30),69+int(n%19));
        if(n%3==0)rect(r,x+7,y+13,3,5,159,191,88);
        if(n%17==0)rect(r,x+19,y+11,4,4,211,174,83);
    }
    // Long winding dirt road
    for(int y=190;y<HEIGHT;y+=12){
        int cx=470+int(95*std::sin(y*.007f));
        rect(r,cx-110,y,220,13,147,120,78);
        rect(r,cx-86,y+6,9,3,172,149,108);
    }
    // Fences on both sides of the road
    for(int i=0;i<15;i++){
        int x=i*70;
        if(x>720 && x<920)continue;
        int y=440+int(30*std::sin(i));
        rect(r,x,y,7,42,103,72,43);
        rect(r,x+5,y+12,55,6,139,99,58);
    }
    for(int i=0;i<14;i++){
        int x=int(noise(i,8)%WIDTH),y=int(noise(i,9)%300)+240;
        if(x>340&&x<650)continue;
        drawTree(r,x,y,noise(i,18));
    }
    // Ominous smoke from a distant ruined watchtower
    rect(r,765,153,96,96,102,93,86);
    rect(r,754,143,118,18,69,66,70);
    rect(r,767,124,21,28,78,77,80);
    rect(r,839,124,22,27,78,77,80);
    rect(r,798,184,30,63,35,32,38);
    for(int i=0;i<6;i++){
        int off=int(10*std::sin(time*.8f+i));
        rect(r,802+off-i*5,100-i*18,42-i*3,15,82+i*9,82+i*9,86+i*8);
    }
    rect(r,783,247,50,9,213,111,51);
    rect(r,792,240,33,8,241,154,64);
}
void storyPanel(SDL_Renderer* r,const World& w) {
    rect(r,12,12,WIDTH-24,68,27,33,41);
    outline(r,12,12,WIDTH-24,68,207,171,99);
    std::string objective;
    switch(w.story){
        case 0:objective="SPEAK TO MOTHER";break;
        case 1:objective="SPEAK TO FATHER";break;
        case 2:objective="SPEAK TO LILY";break;
        case 3:objective="LEAVE THE COTTAGE";break;
        case 4:objective="MEET THE VILLAGE ELDER";break;
        case 5:objective="ASK THE MERCHANT";break;
        case 6:objective="TALK TO THE GUARD";break;
        case 7:objective="GO TO THE EAST ROAD";break;
        case 8:objective="EXAMINE THE SMOKE";break;
        default:objective="DEFEND ASHVALE";break;
    }
    fillText(r,"CHAPTER 1 - SMOKE ON THE HORIZON",25,22,3,232,206,145);
    fillText(r,"QUEST: "+objective,25,52,2,224,224,208);
    rect(r,13,HEIGHT-42,WIDTH-26,31,26,33,40);
    outline(r,13,HEIGHT-42,WIDTH-26,31,207,171,99);
    fillText(r,"WASD MOVE  E TALK / INTERACT  SPACE CONTINUE",27,HEIGHT-32,2,230,204,162);
}
// The top quest banner and bottom controls are pre-rendered into one
// transparent texture, so their text, borders and backgrounds all fade together.
void redrawStoryNotice(SDL_Renderer* renderer,SDL_Texture* notice,const World& w){
    if(!notice)return;
    SDL_SetRenderTarget(renderer,notice);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer,0,0,0,0);
    SDL_RenderClear(renderer);
    storyPanel(renderer,w);
    SDL_SetRenderTarget(renderer,nullptr);
}
Uint8 storyNoticeAlpha(float age){
    // Fade in 0.4s, hold 2.8s, fade out 1.0s.
    constexpr float fadeIn=0.4f,holdUntil=3.2f,fadeEnd=4.2f;
    float opacity=0.f;
    if(age<fadeIn)opacity=age/fadeIn;
    else if(age<holdUntil)opacity=1.f;
    else if(age<fadeEnd)opacity=(fadeEnd-age)/(fadeEnd-holdUntil);
    return Uint8(game::clamp(opacity,0.f,1.f)*255.f);
}
void drawStoryNotice(SDL_Renderer* renderer,SDL_Texture* notice,float age){
    if(!notice)return;
    const Uint8 alpha=storyNoticeAlpha(age);
    if(!alpha)return;
    SDL_SetTextureAlphaMod(notice,alpha);
    SDL_RenderTexture(renderer,notice,nullptr,nullptr);
}
// Compact persistent HP bar, kept separate from temporary quest banners.
void drawHealthBar(SDL_Renderer* r,const World& w){
    constexpr float x=19.f,y=96.f,width=232.f,height=31.f;
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,18,26,36,220);
    SDL_FRect box{x,y,width,height};
    SDL_RenderFillRect(r,&box);
    SDL_SetRenderDrawColor(r,209,174,103,255);
    SDL_RenderRect(r,&box);
    SDL_SetRenderDrawColor(r,64,37,44,255);
    SDL_FRect track{x+37.f,y+8.f,143.f,14.f};
    SDL_RenderFillRect(r,&track);
    const float fraction=game::clamp(float(w.hero.hp)/float(MAX_HEALTH),0.f,1.f);
    SDL_SetRenderDrawColor(r,176,50,58,255);
    SDL_FRect fill{x+39.f,y+10.f,139.f*fraction,10.f};
    if(fill.w>0.f)SDL_RenderFillRect(r,&fill);
    SDL_SetRenderDrawColor(r,222,181,112,255);
    SDL_RenderRect(r,&track);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    fillText(r,"HP",int(x+8),int(y+10),2,240,218,186);
    fillText(r,std::to_string(w.hero.hp)+"/"+std::to_string(MAX_HEALTH),
             int(x+185),int(y+10),1,243,218,187);
}

std::string lowercaseAscii(std::string value){
    for(char& ch:value){
        const unsigned char c=static_cast<unsigned char>(ch);
        ch=static_cast<char>(std::tolower(c));
    }
    return value;
}

// All text rendering resources are owned by the game and released on shutdown.
// Avoid creating a new SDL_Texture every frame; cache by visible text.
struct DialogueFonts {
#if RPG_HAS_SDL3_TTF
    TTF_Font* body=nullptr;
    TTF_Font* name=nullptr;
    TTF_Font* hint=nullptr;
    SDL_Texture* bodyTexture=nullptr;
    SDL_Texture* nameTexture=nullptr;
    SDL_Texture* hintTexture=nullptr;
    std::string lastBody,lastName;
    int bodyW=0,bodyH=0,nameW=0,nameH=0,hintW=0,hintH=0;
#endif
    bool enabled=false;
};
#if RPG_HAS_SDL3_TTF
SDL_Texture* makeText(SDL_Renderer* renderer,TTF_Font* font,const std::string& utf8,
                      SDL_Color fg,int maxWidth,int& w,int& h){
    if(!font || utf8.empty())return nullptr;
    SDL_Surface* surface=TTF_RenderText_Blended_Wrapped(font,utf8.c_str(),0,fg,maxWidth);
    if(!surface)return nullptr;
    w=surface->w;h=surface->h;
    SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_DestroySurface(surface);
    if(texture)SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
    return texture;
}
void cachedText(SDL_Renderer* renderer,TTF_Font* font,
                SDL_Texture*& texture,std::string& last,
                const std::string& value,SDL_Color fg,int wrap,
                int& w,int& h,float x,float y){
    if(last!=value){
        if(texture)SDL_DestroyTexture(texture);
        texture=makeText(renderer,font,value,fg,wrap,w,h);
        last=value;
    }
    if(texture){
        SDL_FRect dst{x,y,float(w),float(h)};
        SDL_RenderTexture(renderer,texture,nullptr,&dst);
    }
}
#endif
DialogueFonts openDialogueFonts(SDL_Renderer* renderer){
    DialogueFonts fonts;
#if RPG_HAS_SDL3_TTF
    if(!TTF_Init()){
        std::cerr<<"SDL3_ttf init failed: "<<SDL_GetError()<<"\n";
        return fonts;
    }
    namespace fs=std::filesystem;
    // Custom font first; Windows system font as a no-install fallback.
    const fs::path candidates[]={
        "assets/fonts/dialogue.ttf",
        "assets/fonts/dialogue.otf",
        "../assets/fonts/dialogue.ttf",
        "../assets/fonts/dialogue.otf",
        "../../assets/fonts/dialogue.ttf",
        "C:/Windows/Fonts/georgia.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
    };
    for(const auto& path:candidates){
        if(!fs::exists(path))continue;
        fonts.body=TTF_OpenFont(path.string().c_str(),23.f);
        fonts.name=TTF_OpenFont(path.string().c_str(),23.f);
        fonts.hint=TTF_OpenFont(path.string().c_str(),17.f);
        if(fonts.body&&fonts.name&&fonts.hint){
            fonts.enabled=true;
            std::cerr<<"Using dialogue font: "<<path.string()<<"\n";
            break;
        }
        if(fonts.body)TTF_CloseFont(fonts.body);
        if(fonts.name)TTF_CloseFont(fonts.name);
        if(fonts.hint)TTF_CloseFont(fonts.hint);
        fonts.body=fonts.name=fonts.hint=nullptr;
    }
    if(!fonts.enabled)std::cerr<<"No usable TTF font found; pixel text fallback\n";
    (void)renderer;
#else
    (void)renderer;
#endif
    return fonts;
}
void closeDialogueFonts(DialogueFonts& fonts){
#if RPG_HAS_SDL3_TTF
    for(SDL_Texture* t:{fonts.bodyTexture,fonts.nameTexture,fonts.hintTexture})
        if(t)SDL_DestroyTexture(t);
    for(TTF_Font* f:{fonts.body,fonts.name,fonts.hint})
        if(f)TTF_CloseFont(f);
    TTF_Quit();
#else
    (void)fonts;
#endif
}

void drawDialogue(SDL_Renderer* r,const World& w,const ArtAssets& art,DialogueFonts& fonts){
    if(w.dialogue.empty())return;
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    color(r,18,18,26,230);
    SDL_FRect bg{45,float(HEIGHT-204),float(WIDTH-90),160};
    SDL_RenderFillRect(r,&bg);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    outline(r,45,HEIGHT-204,WIDTH-90,160,204,163,94);
    outline(r,50,HEIGHT-199,WIDTH-100,150,110,88,66);
    rect(r,72,HEIGHT-223,210,34,83,55,44);
    outline(r,72,HEIGHT-223,210,34,216,174,95);
    const std::string speaker=lowercaseAscii(w.speaker);
#if RPG_HAS_SDL3_TTF
    if(fonts.enabled)
        cachedText(r,fonts.name,fonts.nameTexture,fonts.lastName,speaker,
                   {243,217,156,255},200,fonts.nameW,fonts.nameH,91.f,float(HEIGHT-221));
    else
#endif
        fillText(r,speaker,91,HEIGHT-214,3,243,217,156);
    SDL_Texture* portrait=art.hero;
    if(w.speaker=="MOTHER")portrait=art.mother;
    else if(w.speaker=="FATHER" || w.speaker=="ELDER ROWAN")portrait=art.father;
    else if(w.speaker=="LILY")portrait=art.lily;
    if(portrait){
        SDL_FRect dst{72.f,float(HEIGHT-169),103.f,104.f};
        SDL_RenderTexture(r,portrait,nullptr,&dst);
        outline(r,71,HEIGHT-170,105,106,218,177,109);
    }

    const std::string body=lowercaseAscii(w.dialogue);
    const size_t revealed=std::min(body.size(),size_t(w.dialogueAge*34.f));
    const std::string visible=body.substr(0,revealed);
#if RPG_HAS_SDL3_TTF
    if(fonts.enabled){
        const float x=portrait?193.f:75.f;
        cachedText(r,fonts.body,fonts.bodyTexture,fonts.lastBody,visible,
                   {242,232,213,255},int(WIDTH-x-95),fonts.bodyW,fonts.bodyH,
                   x,float(HEIGHT-172));
        // Hint texture is constant; render only once.
        if(!fonts.hintTexture)
            fonts.hintTexture=makeText(r,fonts.hint,"press space or e",
                                       {232,194,112,255},0,fonts.hintW,fonts.hintH);
        if(fonts.hintTexture){
            SDL_FRect dst{float(WIDTH-70-fonts.hintW),float(HEIGHT-82),
                          float(fonts.hintW),float(fonts.hintH)};
            SDL_RenderTexture(r,fonts.hintTexture,nullptr,&dst);
        }
    }else
#endif
    {
        fillText(r,visible,portrait?193:75,HEIGHT-169,3,242,232,213);
        fillText(r,"press space or e",WIDTH-279,HEIGHT-75,2,232,194,112);
    }
}
void showDialogue(World& w,const char* speaker,const char* words) {
    w.speaker=speaker;w.dialogue=words;w.dialogueAge=0.f;
}
void goVillage(World& w){
    w.scene=Scene::VILLAGE;
    w.hero.pos=w.largeVillage?village::pos(405.f,385.f):Vec2{400.f,315.f};
    w.hero.facing={0.f,1.f};
    w.hero.moving=false;
    w.dialogue.clear();
}
void enterHome(World& w){
    w.scene=Scene::HOME;
    w.hero.pos={480.f,HEIGHT-155.f};
    w.hero.facing={0.f,-1.f};
    w.hero.moving=false;
    w.dialogue.clear();
}
// The shop is an optional side activity; leave story quest progress unchanged.
inline const Vec2 SHOP_VILLAGE_ENTRY=village::pos(1104.f,731.f);
void enterShop(World& w){
    w.scene=Scene::SHOP;
    w.shopMenu=false;
    w.shopFeedback.clear();
    w.hero.pos=shop::ARRIVAL;
    w.hero.facing={0.f,-1.f};
    w.hero.moving=false;
    w.dialogue.clear();
}
void exitShop(World& w){
    w.scene=Scene::VILLAGE;
    w.shopMenu=false;
    w.shopFeedback.clear();
    w.hero.pos=w.largeVillage?village::pos(1104.f,745.f):Vec2{670.f,425.f};
    w.hero.facing={0.f,1.f};
    w.hero.moving=false;
    w.dialogue.clear();
}
inline constexpr std::array<int,6> SHOP_GOODS{{12,14,15,16,5,8}};
inline constexpr std::array<int,6> SHOP_PRICES{{22,8,6,5,54,36}};
enum class PurchaseResult {BOUGHT,NO_GOLD,BAG_FULL,INVALID};
PurchaseResult buyShopItem(World& w,int slot){
    if(slot<0||slot>=int(SHOP_GOODS.size()))return PurchaseResult::INVALID;
    if(w.gold<SHOP_PRICES[size_t(slot)])return PurchaseResult::NO_GOLD;
    for(int& bagItem:w.bag){
        if(bagItem==0){
            bagItem=SHOP_GOODS[size_t(slot)];
            w.gold-=SHOP_PRICES[size_t(slot)];
            return PurchaseResult::BOUGHT;
        }
    }
    return PurchaseResult::BAG_FULL;
}

void goOutskirts(World& w){
    w.scene=Scene::OUTSKIRTS;
    w.hero.pos={138.f,355.f};
    w.hero.facing={1.f,0.f};
    w.hero.moving=false;
    w.dialogue.clear();
}
void beginBattle(World& w){
    w.story=9;
    w.hero.hp=100;
    w.hero.pos={430,450};
    w.enemies={
        {{740,319},3,0,0},{{600,291},3,0,1},
        {{750,450},3,0,2},{{715,550},3,0,3},
        {{645,420},3,0,4},{{820,390},3,0,5},
        {{810,550},3,0,6},{{550,525},3,0,7}
    };
    showDialogue(w,"HERO","GOBLINS! I HAVE TO PROTECT\nTHE ROAD TO ASHVALE!");
}
void interact(World& w){
    if(!w.dialogue.empty()){w.dialogue.clear();return;}
    if(w.scene==Scene::SHOP){
        if(shop::nearDoor(w.hero.pos)){exitShop(w);return;}
        if(shop::nearMerchant(w.hero.pos)){w.shopMenu=true;return;}
        return;
    }
    if(w.scene==Scene::VILLAGE &&
       withinRange(w.hero.pos,w.largeVillage?SHOP_VILLAGE_ENTRY:Vec2{670.f,425.f},75.f)){
        enterShop(w);return;
    }
    if(w.story==0 && withinRange(w.hero.pos,w.family[0].position,72)){
        w.story=1;showDialogue(w,"MOTHER","GOOD MORNING, ELIAN!\nYOUR FATHER HAS SOMETHING TO SAY.");return;
    }
    if(w.story==1 && withinRange(w.hero.pos,w.family[1].position,72)){
        w.story=2;showDialogue(w,"FATHER","TAKE THIS OLD SWORD WHEN YOU GO.\nBUT SEE LILY BEFORE LEAVING.");return;
    }
    if(w.story==2 && withinRange(w.hero.pos,w.family[2].position,72)){
        w.story=3;showDialogue(w,"LILY","COME HOME BEFORE SUNSET!\nI MADE YOU A LUCKY CHARM.");return;
    }
    if(w.scene==Scene::HOME && w.hero.pos.y>HEIGHT-130 && w.hero.pos.x>395 && w.hero.pos.x<570){
        w.story=4;goVillage(w);return;
    }
    if(w.story==4 && withinRange(w.hero.pos,npcPosition(w,0),75)){
        w.story=5;showDialogue(w,"ELDER ROWAN","A STRANGE SMOKE RISES IN THE EAST.\nASK THE MERCHANT WHAT SHE SAW.");return;
    }
    if(w.story==5 && withinRange(w.hero.pos,npcPosition(w,1),75)){
        w.story=6;showDialogue(w,"MERCHANT","MY CART HORSES FLED THE EAST ROAD.\nTHE GUARD MAY KNOW MORE.");return;
    }
    if(w.story==6 && withinRange(w.hero.pos,npcPosition(w,2),75)){
        w.story=7;showDialogue(w,"GUARD","I HEARD SCREAMS BEYOND THE FIELD.\nTAKE CARE IF YOU INVESTIGATE.");return;
    }
    if(w.scene==Scene::VILLAGE &&
       withinRange(w.hero.pos,w.largeVillage?LARGE_VILLAGE_EXIT:Vec2{825,340},80)){
        w.story=std::max(w.story,8);
        goOutskirts(w);return;
    }
    if(w.story==8 && withinRange(w.hero.pos,{782,285},90)){
        beginBattle(w);return;
    }
    if(w.scene==Scene::HOME) {
        for(int i=0;i<3;i++)if(withinRange(w.hero.pos,w.family[size_t(i)].position,72)){
            showDialogue(w,FAMILY[size_t(i)].name,"BE SAFE OUT THERE, ELIAN.");return;
        }
    }
    if(w.scene==Scene::VILLAGE){
        if(w.largeVillage){
            constexpr std::array<const char*,VILLAGER_COUNT> names{{
                "FARMER","VILLAGER","YOUNG TRAVELER","VILLAGE GIRL",
                "BLACKSMITH","SISTER","FARMER","VILLAGER"
            }};
            constexpr std::array<const char*,VILLAGER_COUNT> lines{{
                "THE FIELDS NEED RAIN. I HOPE THE HARVEST WILL BE GOOD.",
                "ASHVALE IS A PEACEFUL VILLAGE. STAY FOR A WHILE!",
                "ONE DAY I WILL TRAVEL BEYOND THE MOUNTAINS.",
                "THE FLOWERS NEAR THE SQUARE ARE BEAUTIFUL TODAY!",
                "GOOD STEEL TAKES PATIENCE, FIRE, AND A STEADY HAND.",
                "MAY YOUR JOURNEY BE SAFE, DEAR TRAVELER.",
                "THE MARKET IS BUSY. I SHOULD FINISH MY WORK.",
                "DID YOU HEAR THE FOUNTAIN? IT NEVER STOPS FLOWING."
            }};
            for(int i=0;i<VILLAGER_COUNT;i++)
                if(withinRange(w.hero.pos,w.villagers[size_t(i)].position,65.f)){
                    showDialogue(w,names[size_t(i)],lines[size_t(i)]);
                    return;
                }
        }
        for(int i=0;i<3;i++)if(withinRange(w.hero.pos,npcPosition(w,i),75)){
            showDialogue(w,VILLAGERS[size_t(i)].name,"MAY THE ROAD BE KIND TO YOU.");return;
        }
    }
}
void drawInteractPrompt(SDL_Renderer* r,const World& w){
    if(!w.dialogue.empty() || w.story>=9)return;
    if(w.scene==Scene::SHOP){
        const bool atDoor=shop::nearDoor(w.hero.pos);
        const bool atCounter=shop::nearMerchant(w.hero.pos);
        if(atDoor||atCounter){
            const Vec2 target=atDoor?shop::ENTRANCE:shop::SHOPKEEPER;
            rect(r,int(target.x)-20,int(target.y)-72,40,27,34,35,43);
            outline(r,int(target.x)-20,int(target.y)-72,40,27,215,182,114);
            fillText(r,"E",int(target.x)-6,int(target.y)-66,3,250,226,154);
        }
        return;
    }
    Vec2 target{};
    switch(w.story){
        case 0:target=w.family[0].position;break;
        case 1:target=w.family[1].position;break;
        case 2:target=w.family[2].position;break;
        case 3:target={477,float(HEIGHT-89)};break;
        case 4:target=npcPosition(w,0);break;
        case 5:target=npcPosition(w,1);break;
        case 6:target=npcPosition(w,2);break;
        case 7:target=w.largeVillage?LARGE_VILLAGE_EXIT:Vec2{825,340};break;
        case 8:target={782,285};break;
    }
    if(w.scene==Scene::VILLAGE && w.largeVillage){
        Vec2 camera=village::camera(w.hero.pos);
        target.x-=camera.x;target.y-=camera.y;
    }
    rect(r,int(target.x)-20,int(target.y)-77,40,29,34,35,43);
    outline(r,int(target.x)-20,int(target.y)-77,40,29,215,182,114);
    fillText(r,"E",int(target.x)-5,int(target.y)-70,3,250,226,154);
    if(w.scene==Scene::VILLAGE){
        Vec2 door=w.largeVillage?SHOP_VILLAGE_ENTRY:Vec2{670.f,425.f};
        if(withinRange(w.hero.pos,door,125.f)){
            if(w.largeVillage){
                const Vec2 camera=village::camera(w.hero.pos);
                door.x-=camera.x;door.y-=camera.y;
            }
            rect(r,int(door.x)-48,int(door.y)-80,96,36,34,35,43);
            outline(r,int(door.x)-48,int(door.y)-80,96,36,215,182,114);
            fillText(r,"E SHOP",int(door.x)-35,int(door.y)-69,2,250,226,154);
        }
    }
}

void drawOverlay(SDL_Renderer* r,const World& w){
    if(!w.won&&!w.lost)return;
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    color(r,10,18,24,210);
    SDL_FRect full{0,0,float(WIDTH),float(HEIGHT)};SDL_RenderFillRect(r,&full);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    rect(r,WIDTH/2-290,HEIGHT/2-108,580,215,30,39,44);
    outline(r,WIDTH/2-290,HEIGHT/2-108,580,215,197,165,105);
    fillText(r,w.won?"VICTORY!":"YOU HAVE FALLEN",WIDTH/2-175,HEIGHT/2-55,5,239,202,118);
    fillText(r,w.won?"ASHVALE IS SAFE":"THE VILLAGE NEEDS YOU",WIDTH/2-156,HEIGHT/2+12,3,221,211,188);
    fillText(r,"PRESS R TO RESTART",WIDTH/2-132,HEIGHT/2+65,3,213,176,113);
}

// First-playable inventory: item ids are fixed positions on items.png.
struct ItemView {const char* name;int column,row;int gearSlot;bool usable;};
constexpr std::array<ItemView,17> ITEM_VIEW{{
    {"EMPTY",0,0,-1,false},
    {"IRON SWORD",0,0,0,false},{"BATTLE AXE",1,0,0,false},
    {"HUNTING BOW",2,0,0,false},{"OAK STAFF",3,0,0,false},
    {"ROYAL SHIELD",4,0,1,false},{"STEEL HELM",5,0,2,false},
    {"CHAIN ARMOR",0,1,3,false},{"LEATHER BOOTS",1,1,4,false},
    {"WORK GLOVES",2,1,5,false},{"SILVER RING",3,1,5,false},
    {"RUBY AMULET",4,1,5,false},{"HEALTH POTION",5,1,-1,true},
    {"MANA POTION",0,2,-1,true},{"HERBS",1,2,-1,true},
    {"BREAD",2,2,-1,true},{"APPLE",3,2,-1,true}
}};
inline const ItemView& itemView(int id){
    return ITEM_VIEW[size_t(id>=0 && id<int(ITEM_VIEW.size())?id:0)];
}
void useInventoryItem(World& world,int index){
    if(index<0||index>=int(world.bag.size()))return;
    const int id=world.bag[size_t(index)];
    if(!id)return;
    const ItemView& def=itemView(id);
    if(def.usable){
        if(id==12||id==14||id==15||id==16){
            if(world.hero.hp>=MAX_HEALTH)return;
            world.hero.hp=std::min(MAX_HEALTH,world.hero.hp+(id==12?40:15));
        }
        world.bag[size_t(index)]=0;
    }else if(def.gearSlot>=0){
        const size_t slot=size_t(def.gearSlot);
        std::swap(world.bag[size_t(index)],world.equipped[slot]);
    }
}
bool useQuickSlot(World& world,int slot){
    if(slot<0||slot>=int(world.quickSlots.size()))return false;
    world.quickSelected=slot;
    const int id=world.quickSlots[size_t(slot)];
    if(!id)return false;
    for(int i=0;i<int(world.bag.size());i++){
        if(world.bag[size_t(i)]==id){
            if(itemView(id).usable){useInventoryItem(world,i);return true;}
            return false;
        }
    }
    return false;
}
void clickInventory(World& world,float x,float y){
    constexpr float bagX=80.f,bagY=230.f,bagStep=58.f;
    if(x>=bagX&&x<bagX+8*bagStep&&y>=bagY&&y<bagY+3*bagStep){
        const int col=int((x-bagX)/bagStep),row=int((y-bagY)/bagStep);
        world.inventorySelection=row*8+col;
        return;
    }
    // Click equipment slots to unequip into the first empty backpack cell.
    constexpr float gearX=639.f,gearY=230.f,gearStep=79.f;
    if(x>=gearX&&x<gearX+3*gearStep&&y>=gearY&&y<gearY+2*gearStep){
        const int slot=int((y-gearY)/gearStep)*3+int((x-gearX)/gearStep);
        if(world.equipped[size_t(slot)]){
            for(auto& item:world.bag)if(!item){
                item=world.equipped[size_t(slot)];
                world.equipped[size_t(slot)]=0;
                break;
            }
        }
        return;
    }
    // Click on quickbar assigns the currently selected backpack item.
    if(y>=538.f && y<598.f && x>=280.f && x<680.f){
        const int slot=int((x-280.f)/50.f);
        world.quickSlots[size_t(slot)]=world.bag[size_t(world.inventorySelection)];
        world.quickSelected=slot;
    }
}
void atlasImage(SDL_Renderer* renderer,SDL_Texture* texture,
                float sx,float sy,float sw,float sh,
                float x,float y,float w,float h,float refW,float refH){
    if(!texture)return;
    float tw=0.f,th=0.f;
    if(!SDL_GetTextureSize(texture,&tw,&th)||tw<=0||th<=0)return;
    const SDL_FRect source{sx*tw/refW,sy*th/refH,sw*tw/refW,sh*th/refH};
    const SDL_FRect destination{x,y,w,h};
    SDL_RenderTexture(renderer,texture,&source,&destination);
}
void itemIcon(SDL_Renderer* renderer,const ArtAssets& art,int id,float x,float y,float size){
    if(id<=0 || !art.itemsAtlas)return;
    const auto& info=itemView(id);
    // First three rows of items.png have six centered icons per row.
    // Crop well inside each tile to exclude neighbouring art.
    constexpr float centersX[6]={141.f,365.f,586.f,810.f,1037.f,1253.f};
    constexpr float centersY[3]={109.f,278.f,447.f};
    atlasImage(renderer,art.itemsAtlas,
               centersX[info.column]-77.f,centersY[info.row]-73.f,154.f,146.f,
               x,y,size,size,1408.f,1024.f);
}
void uiBox(SDL_Renderer* renderer,float x,float y,float w,float h){
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,19,25,36,242);
    SDL_FRect fill{x,y,w,h};
    SDL_RenderFillRect(renderer,&fill);
    SDL_SetRenderDrawColor(renderer,197,159,91,255);
    SDL_RenderRect(renderer,&fill);
    SDL_FRect inner{x+4.f,y+4.f,w-8.f,h-8.f};
    SDL_SetRenderDrawColor(renderer,104,87,67,245);
    SDL_RenderRect(renderer,&inner);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
}
void drawQuickbar(SDL_Renderer* renderer,const World& w,const ArtAssets& art){
    // Draw during play as well as in the inventory screen.
    uiBox(renderer,271.f,528.f,418.f,76.f);
    for(int i=0;i<8;i++){
        const float x=281.f+50.f*i,y=541.f;
        uiBox(renderer,x,y,46.f,51.f);
        itemIcon(renderer,art,w.quickSlots[size_t(i)],x+6.f,y+3.f,34.f);
        if(i==w.quickSelected){
            color(renderer,240,211,116);
            SDL_FRect selected{x-2.f,y-2.f,50.f,55.f};
            SDL_RenderRect(renderer,&selected);
        }
        fillText(renderer,std::to_string(i+1),int(x+18),int(y+40),1,240,207,147);
    }
}
// Both mouse hit testing and rendering share the same item row rectangles.
int shopItemAt(float x,float y){
    constexpr float left=204.f,right=730.f,top=192.f,stride=55.f;
    if(x<left||x>=right||y<top)return -1;
    const int row=int((y-top)/stride);
    if(row<0||row>=int(SHOP_GOODS.size()))return -1;
    return y<top+float(row)*stride+52.f?row:-1;
}
bool shopCloseAt(float x,float y){
    return x>=715.f&&x<=765.f&&y>=119.f&&y<=165.f;
}
void purchaseShopSelection(World& world){
    const PurchaseResult outcome=buyShopItem(world,world.shopSelection);
    world.shopFeedback=outcome==PurchaseResult::BOUGHT?"PURCHASE COMPLETE":
        outcome==PurchaseResult::NO_GOLD?"NOT ENOUGH GOLD":
        outcome==PurchaseResult::BAG_FULL?"BACKPACK IS FULL":"CANNOT BUY ITEM";
}
void handleShopClick(World& world,float x,float y){
    if(shopCloseAt(x,y)){world.shopMenu=false;return;}
    const int selected=shopItemAt(x,y);
    if(selected>=0){
        world.shopSelection=selected;
        purchaseShopSelection(world);
    }
}

void drawShopMenu(SDL_Renderer* r,const World& w,const ArtAssets& art){
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,4,9,16,205);
    SDL_FRect dim{0.f,0.f,float(WIDTH),float(HEIGHT)};
    SDL_RenderFillRect(r,&dim);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    uiBox(r,176.f,102.f,608.f,460.f);
    fillText(r,"GENERAL STORE",225,131,4,246,207,138);
    uiBox(r,717.f,120.f,46.f,43.f);
    fillText(r,"X",732,129,3,246,207,138);
    fillText(r,"GOLD: "+std::to_string(w.gold),565,143,2,244,211,143);
    for(int i=0;i<int(SHOP_GOODS.size());i++){
        const int id=SHOP_GOODS[size_t(i)];
        const int x=217,y=200+55*i;
        if(i==w.shopSelection){
            uiBox(r,float(x-13),float(y-8),526.f,52.f);
        }
        itemIcon(r,art,id,float(x),float(y-3),41.f);
        fillText(r,itemView(id).name,x+68,y+7,2,239,218,183);
        fillText(r,std::to_string(SHOP_PRICES[size_t(i)])+" G",x+425,y+7,2,243,198,107);
    }
    fillText(r,w.shopFeedback.empty()?"MOUSE: CLICK TO BUY   X: CLOSE   UP/DOWN + E: BUY":w.shopFeedback,
             218,528,2,241,204,151);
}

void drawInventory(SDL_Renderer* renderer,const World& w,const ArtAssets& art){
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,5,9,17,202);
    SDL_FRect shade{0.f,0.f,float(WIDTH),float(HEIGHT)};
    SDL_RenderFillRect(renderer,&shade);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    uiBox(renderer,54.f,112.f,513.f,407.f);
    uiBox(renderer,599.f,112.f,305.f,407.f);
    // Decorative upper carved title bars cropped from ui.png. The rest of
    // the panels remain readable even if the atlas is missing.
    atlasImage(renderer,art.uiAtlas,95,0,370,142,114,120,390,91,1408,1024);
    atlasImage(renderer,art.uiAtlas,610,0,250,145,633,122,242,92,1408,1024);
    fillText(renderer,"BACKPACK",87,183,3,247,222,151);
    fillText(renderer,"EQUIPMENT",627,183,3,247,222,151);
    for(int i=0;i<24;i++){
        const int col=i%8,row=i/8;
        const float x=80.f+float(col)*58.f,y=230.f+float(row)*58.f;
        uiBox(renderer,x,y,52.f,52.f);
        itemIcon(renderer,art,w.bag[size_t(i)],x+5.f,y+5.f,42.f);
        if(i==w.inventorySelection){
            color(renderer,253,222,112);
            SDL_FRect chosen{x-2.f,y-2.f,56.f,56.f};
            SDL_RenderRect(renderer,&chosen);
        }
    }
    constexpr std::array<const char*,6> labels{{
        "WEAPON","SHIELD","HELM","ARMOR","BOOTS","OTHER"
    }};
    for(int i=0;i<6;i++){
        const float x=639.f+float(i%3)*79.f,y=230.f+float(i/3)*79.f;
        uiBox(renderer,x,y,68.f,68.f);
        itemIcon(renderer,art,w.equipped[size_t(i)],x+9.f,y+5.f,50.f);
        fillText(renderer,labels[size_t(i)],int(x+2),int(y+55),1,201,180,127);
    }
    uiBox(renderer,78.f,416.f,464.f,86.f);
    const int selected=w.bag[size_t(w.inventorySelection)];
    fillText(renderer,itemView(selected).name,94,434,3,243,216,151);
    fillText(renderer,selected?(itemView(selected).usable?"E: USE ITEM":"E: EQUIP ITEM"):
             "EMPTY SLOT",94,464,2,221,206,180);
    fillText(renderer,"GOLD: "+std::to_string(w.gold),645,399,2,241,203,114);
    fillText(renderer,"B / I: CLOSE     ARROWS: SELECT     E: USE / EQUIP",
             91,496,1,215,198,159);
    drawQuickbar(renderer,w,art);
}

} // namespace

int main(int argc,char** argv){
    (void)argc;(void)argv;
    if(!SDL_Init(SDL_INIT_VIDEO)){
        std::cerr<<"SDL_Init failed: "<<SDL_GetError()<<"\n";return 1;
    }
    SDL_Window* window=SDL_CreateWindow("Medieval RPG - The Ashvale Trial",WIDTH*SCALE,HEIGHT*SCALE,0);
    if(!window){std::cerr<<SDL_GetError()<<"\n";SDL_Quit();return 1;}
    SDL_Renderer* renderer=SDL_CreateRenderer(window,nullptr);
    if(!renderer)renderer=SDL_CreateRenderer(window,"software");
    if(!renderer){
        std::cerr<<SDL_GetError()<<"\n";SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer,WIDTH,HEIGHT,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer,1);
    ArtAssets art=loadArtAssets(renderer);
    DialogueFonts fonts=openDialogueFonts(renderer);
    World world;reset(world);
    world.largeVillage=art.expandedVillage;
    SDL_Texture* storyNotice=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,
                                              SDL_TEXTUREACCESS_TARGET,WIDTH,HEIGHT);
    if(storyNotice){
        SDL_SetTextureBlendMode(storyNotice,SDL_BLENDMODE_BLEND);
        redrawStoryNotice(renderer,storyNotice,world);
    }else{
        std::cerr<<"Story notice render target unavailable: "<<SDL_GetError()<<"\\n";
    }
    bool running=true;
    int noticeStory=world.story;
    Scene noticeScene=world.scene;
    Uint64 last=SDL_GetPerformanceCounter();
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_EVENT_QUIT)running=false;
            if(ev.type==SDL_EVENT_MOUSE_MOTION && world.shopMenu){
                SDL_ConvertEventToRenderCoordinates(renderer,&ev);
                const int hovered=shopItemAt(ev.motion.x,ev.motion.y);
                if(hovered>=0 && hovered!=world.shopSelection){
                    world.shopSelection=hovered;
                    world.shopFeedback.clear();
                }
            }
            if(ev.type==SDL_EVENT_MOUSE_BUTTON_DOWN &&
               ev.button.button==SDL_BUTTON_LEFT){
                SDL_ConvertEventToRenderCoordinates(renderer,&ev);
                if(world.shopMenu)handleShopClick(world,ev.button.x,ev.button.y);
                else if(world.inventoryOpen)clickInventory(world,ev.button.x,ev.button.y);
            }
            if(ev.type==SDL_EVENT_KEY_DOWN && ev.key.repeat==0){
                if(world.shopMenu){
                    if(ev.key.key==SDLK_ESCAPE || ev.key.key==SDLK_B){
                        world.shopMenu=false;
                    }else if(ev.key.key==SDLK_UP){
                        world.shopSelection=(world.shopSelection+int(SHOP_GOODS.size())-1)%int(SHOP_GOODS.size());
                        world.shopFeedback.clear();
                    }else if(ev.key.key==SDLK_DOWN){
                        world.shopSelection=(world.shopSelection+1)%int(SHOP_GOODS.size());
                        world.shopFeedback.clear();
                    }else if(ev.key.key==SDLK_RETURN || ev.key.key==SDLK_E){
                        purchaseShopSelection(world);
                    }
                    continue;
                }
                if(ev.key.key==SDLK_I || ev.key.key==SDLK_B){
                    world.inventoryOpen=!world.inventoryOpen;
                    continue;
                }
                if(ev.key.key==SDLK_ESCAPE){
                    if(world.inventoryOpen){world.inventoryOpen=false;continue;}
                    running=false;
                }
                if(world.inventoryOpen){
                    if(ev.key.key==SDLK_LEFT)
                        world.inventorySelection=(world.inventorySelection+23)%24;
                    else if(ev.key.key==SDLK_RIGHT)
                        world.inventorySelection=(world.inventorySelection+1)%24;
                    else if(ev.key.key==SDLK_UP)
                        world.inventorySelection=(world.inventorySelection+16)%24;
                    else if(ev.key.key==SDLK_DOWN)
                        world.inventorySelection=(world.inventorySelection+8)%24;
                    else if(ev.key.key==SDLK_E || ev.key.key==SDLK_RETURN)
                        useInventoryItem(world,world.inventorySelection);
                    else if(ev.key.key>=SDLK_1 && ev.key.key<=SDLK_8)
                        world.quickSlots[size_t(ev.key.key-SDLK_1)]=
                            world.bag[size_t(world.inventorySelection)];
                    continue;
                }
                if(ev.key.key>=SDLK_1 && ev.key.key<=SDLK_8)
                    useQuickSlot(world,int(ev.key.key-SDLK_1));
                if(ev.key.key==SDLK_R){
                    reset(world);world.largeVillage=art.expandedVillage;
                    noticeStory=-1; // reset also redraws if restarted at the same chapter
                }
                if(ev.key.key==SDLK_F3)world.showCollision=!world.showCollision;
                if(ev.key.key==SDLK_F1)world.noticeAge=0.f;
                if(ev.key.key==SDLK_E || (ev.key.key==SDLK_SPACE && !world.dialogue.empty())){
                    if(!world.dialogue.empty() &&
                       size_t(world.dialogueAge*34.f)<world.dialogue.size())
                        world.dialogueAge=float(world.dialogue.size())/34.f+1.f;
                    else interact(world);
                }
                else if(ev.key.key==SDLK_SPACE)attack(world);
            }
        }
        Uint64 now=SDL_GetPerformanceCounter();
        float dt=static_cast<float>(now-last)/static_cast<float>(SDL_GetPerformanceFrequency());
        last=now;
        dt=clamp(dt,0.f,0.035f);
        if(!world.inventoryOpen && !world.shopMenu)update(world,SDL_GetKeyboardState(nullptr),dt);
        if(world.story!=noticeStory || world.scene!=noticeScene){
            noticeStory=world.story;
            noticeScene=world.scene;
            world.noticeAge=0.f;
            redrawStoryNotice(renderer,storyNotice,world);
        }
        world.noticeAge+=dt;
        if(world.scene==Scene::HOME){
            if(art.home)drawBackground(renderer,art.home);
            else drawCottage(renderer,world.time);
            // Draw the three moving relatives in their current world positions.
            for(int i=0;i<int(FAMILY.size());++i){
                Character c=FAMILY[size_t(i)];
                const FamilyMotion& motion=world.family[size_t(i)];
                c.p=motion.position;
                // FAMILY order: mother, father, Lily; atlas order: father, mother, Lily.
                const int sheetPerson=i==0?1:(i==1?0:2);
                const bool talking=world.speaker==c.name && !world.dialogue.empty();
                const int facing=facingSpriteDirection(motion.facing);
                const int state=talking?5:(motion.moving?facing:4);
                const int frame=motion.moving?int(motion.walkingTime*6.f)%3:
                    (talking?int(world.time*3.f)%3:int(world.time*1.5f+float(i))%3);
                if(art.familyFrames[(sheetPerson*FAMILY_STATES+state)*FAMILY_POSES+frame])
                    drawFamilySprite(renderer,art,sheetPerson,int(c.p.x),int(c.p.y),state,frame);
                else drawPerson(renderer,c);
            }
        }else if(world.scene==Scene::SHOP){
            if(art.shopBackground)drawBackground(renderer,art.shopBackground);
            else drawCottage(renderer,world.time);
            Character keeper{{480.f,250.f},"SHOPKEEPER",143,89,58,false,false};
            drawVillagerSprite(renderer,art,2,keeper.p,{0.f,1.f},false,0.f,keeper);
            drawShopFurniture(renderer,art,world.hero.pos.y,false);
            if(world.showCollision){
                for(size_t i=0;i<shop::furniture.size();i++){
                    if(!shop::furniture[i].solid)continue;
                    color(renderer,241,124,64,255);
                    const auto b=shop::footprint(i);
                    SDL_FRect boundary{b.x,b.y,b.w,b.h};
                    SDL_RenderRect(renderer,&boundary);
                }
            }
        }else if(world.scene==Scene::VILLAGE){
            if(world.largeVillage && art.village){
                const Vec2 camera=village::camera(world.hero.pos);
                SDL_FRect bg{-camera.x,-camera.y,village::WORLD_W,village::WORLD_H};
                SDL_RenderTexture(renderer,art.village,nullptr,&bg);
                drawVillageProps(renderer,art,camera,world.hero.pos.y,false);
                drawAnimatedFountain(renderer,art,camera,world.hero.pos.y,world.time,false);
                drawVillagePopulation(renderer,art,world,camera,false);
                if(world.showCollision){
                    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
                    for(const auto& poly:village::obstacles()){
                        for(size_t i=0;i<poly.count;i++){
                            auto a=poly.points[i],b=poly.points[(i+1)%poly.count];
                            color(renderer,255,72,72,180);
                            SDL_RenderLine(renderer,a.x-camera.x,a.y-camera.y,b.x-camera.x,b.y-camera.y);
                        }
                    }
                    for(const auto& poly:village::terrain()){
                        for(size_t i=0;i<poly.count;i++){
                            auto a=poly.points[i],b=poly.points[(i+1)%poly.count];
                            color(renderer,60,190,255,210);
                            SDL_RenderLine(renderer,a.x-camera.x,a.y-camera.y,b.x-camera.x,b.y-camera.y);
                        }
                    }
                    {
                        const Vec2 center=village::fountainCollisionCenter();
                        const float radius=village::FOUNTAIN_RADIUS;
                        color(renderer,245,193,76,245);
                        for(int j=0;j<48;j++){
                            const float a=float(j)*6.2831853f/48.f;
                            const float b=float(j+1)*6.2831853f/48.f;
                            SDL_RenderLine(renderer,
                                center.x-camera.x+radius*std::cos(a),
                                center.y-camera.y+radius*std::sin(a),
                                center.x-camera.x+radius*std::cos(b),
                                center.y-camera.y+radius*std::sin(b));
                        }
                    }
                    for(const auto& bridge:village::bridgePassages()){
                        SDL_FRect r{bridge.x-camera.x,bridge.y-camera.y,bridge.w,bridge.h};
                        color(renderer,109,245,130,230);SDL_RenderRect(renderer,&r);
                    }
                    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
                }
            }else{
                if(art.village)drawBackground(renderer,art.village);
                else drawMap(renderer,world.time);
                for(const auto& c:VILLAGERS)drawPerson(renderer,c);
                rect(renderer,809,303,70,67,145,116,76);
                outline(renderer,809,303,70,67,218,175,91);
                fillText(renderer,"EAST",813,324,3,241,218,167);
            }
        }else{
            if(art.outskirts)drawBackground(renderer,art.outskirts);
            else drawOutskirts(renderer,world.time);
            for(const auto& e:world.enemies)if(e.hp>0)drawEnemy(renderer,e);
        }
        Vec2 heroScreen=world.hero.pos;
        if(world.scene==Scene::VILLAGE && world.largeVillage){
            Vec2 camera=village::camera(world.hero.pos);
            heroScreen.x-=camera.x;heroScreen.y-=camera.y;
        }
        if(!drawAnimatedHero(renderer,art,heroScreen,world.hero.facing,
                             world.hero.moving,world.hero.animationTime)){
            if(world.story>=9)
                drawKnight(renderer,heroScreen,world.hero.facing,world.hero.slashTimer,world.hero.hurtTimer>0);
            else drawYoungHero(renderer,heroScreen);
        }
        if(world.scene==Scene::SHOP)
            drawShopFurniture(renderer,art,world.hero.pos.y,true);
        if(world.scene==Scene::VILLAGE && world.largeVillage){
            const Vec2 camera=village::camera(world.hero.pos);
            drawVillageProps(renderer,art,camera,world.hero.pos.y,true);
            drawAnimatedFountain(renderer,art,camera,world.hero.pos.y,world.time,true);
            drawVillagePopulation(renderer,art,world,camera,true);
        }
        if(world.story<9)drawInteractPrompt(renderer,world);
        else drawHud(renderer,world);
        drawStoryNotice(renderer,storyNotice,world.noticeAge);
        drawHealthBar(renderer,world);
        drawOverlay(renderer,world);
        drawDialogue(renderer,world,art,fonts);
        if(world.inventoryOpen)drawInventory(renderer,world,art);
        else if(world.shopMenu)drawShopMenu(renderer,world,art);
        else if(world.dialogue.empty())drawQuickbar(renderer,world,art);
        SDL_RenderPresent(renderer);
        if(!(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS))SDL_Delay(16);
    }
    if(storyNotice)SDL_DestroyTexture(storyNotice);
    closeDialogueFonts(fonts);
    destroyArtAssets(art);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
