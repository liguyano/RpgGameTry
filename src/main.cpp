#include "game_logic.h"
#include "village_map.h"
#include "shop_map.h"
#include "equipment.h"
#include "hero_animation.h"
#include "combat.h"
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

// The game still uses a single translation unit to retain internal helpers
// and avoid linkage changes while its subsystems are being modularized.
// These fragments are ordered by the dependencies of the original code.
#include "modules/world_state.inc"
#include "modules/asset_pipeline.inc"
#include "modules/world_renderer.inc"
#include "modules/gameplay.inc"
#include "modules/characters_scenes.inc"
#include "modules/hud_dialogue.inc"
#include "modules/scene_transitions.inc"
#include "modules/game_over.inc"
#include "modules/inventory_shop_ui.inc"

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
