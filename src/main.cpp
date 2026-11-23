#include "game_logic.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#if RPG_HAS_SDL3_IMAGE
#include <SDL3_image/SDL_image.h>
#endif
#include <array>
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
};
enum class Scene { HOME, VILLAGE, OUTSKIRTS };

struct World {
    Scene scene=Scene::HOME;
    int story=0;
    std::string dialogue;
    std::string speaker;
    bool justArrived=true;
    Player hero;
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
    SDL_Texture* outskirts=nullptr;
    SDL_Texture* hero=nullptr;
    SDL_Texture* father=nullptr;
    SDL_Texture* mother=nullptr;
    SDL_Texture* lily=nullptr;
    std::array<SDL_Texture*,36> familyFrames{};
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

// Extract the 3 relatives' four facing directions (3 frames each) from family.png.
// The supplied sheet is a labeled concept sheet rather than a transparent atlas,
// so the dark sheet background is removed during one-time loading.
void loadFamilyFrames(SDL_Renderer* r,ArtAssets& art){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path sheetPath;
    for(const fs::path& candidate:{
            fs::path("assets/family.png"),fs::path("../assets/family.png"),
            fs::path("../../assets/family.png")}){
        if(fs::exists(candidate)){sheetPath=candidate;break;}
    }
    if(sheetPath.empty())return;
    SDL_Surface* sheet=IMG_Load(sheetPath.string().c_str());
    if(!sheet){std::cerr<<"Family sprite sheet load failed: "<<SDL_GetError()<<"\n";return;}
    constexpr int FRAME_WIDTH=57, FRAME_HEIGHT=108;
    constexpr int centers[4][3]={
        {429,492,554},  // down
        {632,693,756},  // left
        {835,895,955},  // right
        {1034,1097,1158} // up
    };
    constexpr int tops[3]={268,515,765}; // father, mother, Lily
    for(int who=0;who<3;who++)for(int dir=0;dir<4;dir++)for(int frame=0;frame<3;frame++){
        SDL_Surface* cropped=SDL_CreateSurface(FRAME_WIDTH,FRAME_HEIGHT,SDL_PIXELFORMAT_RGBA32);
        if(!cropped)continue;
        SDL_Rect src{centers[dir][frame]-FRAME_WIDTH/2,tops[who],FRAME_WIDTH,FRAME_HEIGHT};
        if(SDL_BlitSurface(sheet,&src,cropped,nullptr)){
            for(int y=0;y<FRAME_HEIGHT;y++)for(int x=0;x<FRAME_WIDTH;x++){
                Uint8 red=0,green=0,blue=0,alpha=255;
                if(!SDL_ReadSurfacePixel(cropped,x,y,&red,&green,&blue,&alpha))continue;
                const int hi=std::max({int(red),int(green),int(blue)});
                const int lo=std::min({int(red),int(green),int(blue)});
                // Preserve colored outfits while keying out the near-charcoal sheet.
                if(hi<62 && hi-lo<24)
                    SDL_WriteSurfacePixel(cropped,x,y,0,0,0,0);
            }
            SDL_Texture* frameTexture=SDL_CreateTextureFromSurface(r,cropped);
            if(frameTexture){
                SDL_SetTextureScaleMode(frameTexture,SDL_SCALEMODE_NEAREST);
                SDL_SetTextureBlendMode(frameTexture,SDL_BLENDMODE_BLEND);
            }
            art.familyFrames[(who*4+dir)*3+frame]=frameTexture;
        }
        SDL_DestroySurface(cropped);
    }
    SDL_DestroySurface(sheet);
#else
    (void)r;(void)art;
#endif
}
void drawFamilySprite(SDL_Renderer* r,const ArtAssets& art,int person,
                      int x,int y,int direction,int frame){
    const int idx=(person*4+direction)*3+frame;
    SDL_Texture* texture=art.familyFrames[idx];
    if(!texture)return;
    // PNG sheet proportions: scale frames down for the 960x640 world.
    SDL_FRect destination{float(x-20),float(y-53),40.f,65.f};
    SDL_RenderTexture(r,texture,nullptr,&destination);
}
ArtAssets loadArtAssets(SDL_Renderer* renderer){
    ArtAssets result;
#if RPG_HAS_SDL3_IMAGE
    result.home=loadArt(renderer,"home.png");
#endif
    if(!result.home)result.home=loadArt(renderer,"home.bmp");
    result.village=loadArt(renderer,"village.bmp");
    result.outskirts=loadArt(renderer,"outskirts.bmp");
    result.hero=loadArt(renderer,"hero.bmp");
    result.father=loadArt(renderer,"father.bmp");
    result.mother=loadArt(renderer,"mother.bmp");
    result.lily=loadArt(renderer,"lily.bmp");
    loadFamilyFrames(renderer,result);
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
    static const std::array<std::pair<char,const char*>,43> glyphs{{
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
        {' ',"000000000000000"}
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
    rect(r,11,10,298,66,22,32,38);
    outline(r,11,10,298,66,189,153,88);
    fillText(r,"KNIGHT OF ASHVALE",23,20,3,231,202,135);
    rect(r,23,48,181,13,67,51,51);
    rect(r,25,50,static_cast<int>(177*w.hero.hp/100.f),9,183,59,59);
    outline(r,23,48,181,13,203,170,110);
    fillText(r,"HP",214,49,2,244,224,190);
    fillText(r,std::to_string(w.hero.hp),243,49,2,244,224,190);
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
    if(!w.dialogue.empty())return;
    w.hero.slashTimer=std::max(0.f,w.hero.slashTimer-dt);
    w.hero.hurtTimer=std::max(0.f,w.hero.hurtTimer-dt);
    Vec2 input{static_cast<float>(int(keys[SDL_SCANCODE_D])-int(keys[SDL_SCANCODE_A])),
               static_cast<float>(int(keys[SDL_SCANCODE_S])-int(keys[SDL_SCANCODE_W]))};
    auto dir=normalized(input);
    if(dir.x!=0 || dir.y!=0){
        w.hero.facing=dir;
        if(w.scene==Scene::HOME){
            w.hero.pos.x=clamp(w.hero.pos.x+dir.x*PLAYER_SPEED*dt,88.f,float(WIDTH-90));
            w.hero.pos.y=clamp(w.hero.pos.y+dir.y*PLAYER_SPEED*dt,112.f,float(HEIGHT-86));
        }else if(w.scene==Scene::OUTSKIRTS){
            w.hero.pos.x=clamp(w.hero.pos.x+dir.x*PLAYER_SPEED*dt,20.f,float(WIDTH-20));
            w.hero.pos.y=clamp(w.hero.pos.y+dir.y*PLAYER_SPEED*dt,195.f,float(HEIGHT-20));
        }else{
            moveBody(w.hero.pos,{dir.x*PLAYER_SPEED,dir.y*PLAYER_SPEED},dt,11);
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
void drawDialogue(SDL_Renderer* r,const World& w,const ArtAssets& art){
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
    fillText(r,w.speaker,91,HEIGHT-214,3,243,217,156);
    SDL_Texture* portrait=art.hero;
    if(w.speaker=="MOTHER")portrait=art.mother;
    else if(w.speaker=="FATHER" || w.speaker=="ELDER ROWAN")portrait=art.father;
    else if(w.speaker=="LILY")portrait=art.lily;
    if(portrait){
        SDL_FRect dst{72.f,float(HEIGHT-169),103.f,104.f};
        SDL_RenderTexture(r,portrait,nullptr,&dst);
        outline(r,71,HEIGHT-170,105,106,218,177,109);
    }
    fillText(r,w.dialogue,portrait?193:75,HEIGHT-169,3,242,232,213);
    fillText(r,"PRESS SPACE OR E",WIDTH-279,HEIGHT-75,2,232,194,112);
}
void showDialogue(World& w,const char* speaker,const char* words) {
    w.speaker=speaker;w.dialogue=words;
}
void goVillage(World& w){
    w.scene=Scene::VILLAGE;
    w.hero.pos={477,480};
    showDialogue(w,"HERO","THE VILLAGE IS BUSY TODAY.\nI SHOULD FIND ELDER ROWAN.");
}
void goOutskirts(World& w){
    w.scene=Scene::OUTSKIRTS;
    w.hero.pos={138,355};
    showDialogue(w,"HERO","THAT SMOKE IS NOT FROM A CHIMNEY.\nSOMETHING IS WRONG NEAR THE ROAD.");
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
    if(w.story==0 && withinRange(w.hero.pos,FAMILY[0].p,72)){
        w.story=1;showDialogue(w,"MOTHER","GOOD MORNING, ELIAN!\nYOUR FATHER HAS SOMETHING TO SAY.");return;
    }
    if(w.story==1 && withinRange(w.hero.pos,FAMILY[1].p,72)){
        w.story=2;showDialogue(w,"FATHER","TAKE THIS OLD SWORD WHEN YOU GO.\nBUT SEE LILY BEFORE LEAVING.");return;
    }
    if(w.story==2 && withinRange(w.hero.pos,FAMILY[2].p,72)){
        w.story=3;showDialogue(w,"LILY","COME HOME BEFORE SUNSET!\nI MADE YOU A LUCKY CHARM.");return;
    }
    if(w.story==3 && w.hero.pos.y>HEIGHT-130 && w.hero.pos.x>395 && w.hero.pos.x<570){
        w.story=4;goVillage(w);return;
    }
    if(w.story==4 && withinRange(w.hero.pos,VILLAGERS[0].p,75)){
        w.story=5;showDialogue(w,"ELDER ROWAN","A STRANGE SMOKE RISES IN THE EAST.\nASK THE MERCHANT WHAT SHE SAW.");return;
    }
    if(w.story==5 && withinRange(w.hero.pos,VILLAGERS[1].p,75)){
        w.story=6;showDialogue(w,"MERCHANT","MY CART HORSES FLED THE EAST ROAD.\nTHE GUARD MAY KNOW MORE.");return;
    }
    if(w.story==6 && withinRange(w.hero.pos,VILLAGERS[2].p,75)){
        w.story=7;showDialogue(w,"GUARD","I HEARD SCREAMS BEYOND THE FIELD.\nTAKE CARE IF YOU INVESTIGATE.");return;
    }
    if(w.story==7 && withinRange(w.hero.pos,{825,340},80)){
        w.story=8;goOutskirts(w);return;
    }
    if(w.story==8 && withinRange(w.hero.pos,{782,285},90)){
        beginBattle(w);return;
    }
    if(w.scene==Scene::HOME) {
        for(auto& c:FAMILY)if(withinRange(w.hero.pos,c.p,72)){
            showDialogue(w,c.name,"BE SAFE OUT THERE, ELIAN.");return;
        }
    }
    if(w.scene==Scene::VILLAGE){
        for(auto& c:VILLAGERS)if(withinRange(w.hero.pos,c.p,75)){
            showDialogue(w,c.name,"MAY THE ROAD BE KIND TO YOU.");return;
        }
    }
}
void drawInteractPrompt(SDL_Renderer* r,const World& w){
    if(!w.dialogue.empty() || w.story>=9)return;
    Vec2 target{};
    switch(w.story){
        case 0:target=FAMILY[0].p;break;
        case 1:target=FAMILY[1].p;break;
        case 2:target=FAMILY[2].p;break;
        case 3:target={477,float(HEIGHT-89)};break;
        case 4:target=VILLAGERS[0].p;break;
        case 5:target=VILLAGERS[1].p;break;
        case 6:target=VILLAGERS[2].p;break;
        case 7:target={825,340};break;
        case 8:target={782,285};break;
    }
    rect(r,int(target.x)-20,int(target.y)-77,40,29,34,35,43);
    outline(r,int(target.x)-20,int(target.y)-77,40,29,215,182,114);
    fillText(r,"E",int(target.x)-5,int(target.y)-70,3,250,226,154);
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
    World world;reset(world);
    bool running=true;
    Uint64 last=SDL_GetPerformanceCounter();
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_EVENT_QUIT)running=false;
            if(ev.type==SDL_EVENT_KEY_DOWN && ev.key.repeat==0){
                if(ev.key.key==SDLK_ESCAPE)running=false;
                if(ev.key.key==SDLK_R)reset(world);
                if(ev.key.key==SDLK_E || (ev.key.key==SDLK_SPACE && !world.dialogue.empty()))
                    interact(world);
                else if(ev.key.key==SDLK_SPACE)attack(world);
            }
        }
        Uint64 now=SDL_GetPerformanceCounter();
        float dt=static_cast<float>(now-last)/static_cast<float>(SDL_GetPerformanceFrequency());
        last=now;
        dt=clamp(dt,0.f,0.035f);
        update(world,SDL_GetKeyboardState(nullptr),dt);
        if(world.scene==Scene::HOME){
            if(art.home)drawBackground(renderer,art.home);
            else drawCottage(renderer,world.time);
            for(int i=0;i<int(FAMILY.size());++i){
                const auto& c=FAMILY[size_t(i)];
                // FAMILY order: mother, father, Lily; atlas order: father, mother, Lily.
                const int sheetPerson=i==0?1:(i==1?0:2);
                const bool talking=world.speaker==c.name && !world.dialogue.empty();
                const int frame=talking?int(world.time*3)%3:1;
                if(art.familyFrames[(sheetPerson*4)*3+frame])
                    drawFamilySprite(renderer,art,sheetPerson,int(c.p.x),int(c.p.y),0,frame);
                else drawPerson(renderer,c);
            }
        }else if(world.scene==Scene::VILLAGE){
            if(art.village)drawBackground(renderer,art.village);
            else drawMap(renderer,world.time);
            for(const auto& c:VILLAGERS)drawPerson(renderer,c);
            rect(renderer,809,303,70,67,145,116,76);
            outline(renderer,809,303,70,67,218,175,91);
            fillText(renderer,"EAST",813,324,3,241,218,167);
        }else{
            if(art.outskirts)drawBackground(renderer,art.outskirts);
            else drawOutskirts(renderer,world.time);
            for(const auto& e:world.enemies)if(e.hp>0)drawEnemy(renderer,e);
        }
        if(world.story>=9)
            drawKnight(renderer,world.hero.pos,world.hero.facing,world.hero.slashTimer,world.hero.hurtTimer>0);
        else drawYoungHero(renderer,world.hero.pos);
        if(world.story<9){
            storyPanel(renderer,world);
            drawInteractPrompt(renderer,world);
        }else drawHud(renderer,world);
        drawOverlay(renderer,world);
        drawDialogue(renderer,world,art);
        SDL_RenderPresent(renderer);
        if(!(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS))SDL_Delay(16);
    }
    destroyArtAssets(art);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
