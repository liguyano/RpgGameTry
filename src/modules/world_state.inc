// world_state.inc — implementation fragment; included inside main.cpp's anonymous namespace.
// Keep the relative include order when refactoring dependencies.
struct Enemy {
    Vec2 pos;
    int hp=equipment::ENEMY_HEALTH;
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
    // A new adventurer owns ordinary food, not a collection of rare gear.
    std::array<int,24> bag{{15,16,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
    std::array<int,6> equipped{{0,0,0,18,0,0}}; // simple tunic: DEF +1
    std::array<int,8> quickSlots{{15,16,0,0,0,0,0,0}};
    int quickSelected=0;
    int gold=18;
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
    std::array<SDL_Texture*,hero_anim::DIRECTIONS*hero_anim::FRAMES> heroFrames{};
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
