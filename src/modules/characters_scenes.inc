// characters_scenes.inc — implementation fragment; included inside main.cpp's anonymous namespace.
// Keep the relative include order when refactoring dependencies.
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
