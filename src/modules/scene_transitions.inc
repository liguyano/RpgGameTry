// scene_transitions.inc — implementation fragment; included inside main.cpp's anonymous namespace.
// Keep the relative include order when refactoring dependencies.
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
// Affordable starter gear and an expensive early upgrade.
inline constexpr std::array<int,7> SHOP_GOODS{{12,14,15,16,17,18,1}};
inline constexpr std::array<int,7> SHOP_PRICES{{22,8,6,5,14,12,80}};
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
    w.hero.pos=east_map::ENTRY;
    w.hero.facing={1.f,0.f};
    w.hero.moving=false;
    w.dialogue.clear();
}
void returnToVillage(World& w){
    w.scene=Scene::VILLAGE;
    w.hero.pos=w.largeVillage?village::pos(1376.f,751.f):Vec2{780.f,345.f};
    w.hero.facing={-1.f,0.f};
    w.hero.moving=false;
    w.won=false;
    w.dialogue.clear();
}
void beginBattle(World& w){
    w.story=9;
    w.hero.hp=100;
    w.hero.pos={430,450};
    w.enemies={
        {{740,319},equipment::ENEMY_HEALTH,0,0},{{600,291},equipment::ENEMY_HEALTH,0,1},
        {{750,450},equipment::ENEMY_HEALTH,0,2},{{705,500},equipment::ENEMY_HEALTH,0,3},
        {{645,420},equipment::ENEMY_HEALTH,0,4},{{860,390},equipment::ENEMY_HEALTH,0,5},
        {{867,475},equipment::ENEMY_HEALTH,0,6},{{550,510},equipment::ENEMY_HEALTH,0,7}
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
    // Father's old sword is a one-time gift, even if the player left home
    // before doing the family tutorial and returned much later.
    if(w.scene==Scene::HOME &&
       withinRange(w.hero.pos,w.family[1].position,72) &&
       !w.fatherSwordReceived){
        bool received=false;
        if(w.equipped[0]==0){
            w.equipped[0]=17; // start wearing the weak sword immediately
            received=true;
        }else{
            received=equipment::addToBag(w.bag,17);
        }
        if(received){
            w.fatherSwordReceived=true;
            if(w.story==1)w.story=2;
            showDialogue(w,"FATHER",
                "THIS WAS MY FIRST BLADE. ITS EDGE IS CHIPPED, BUT IT STILL WORKS.\n"
                "TAKE IT WITH YOU. FATHER'S CHIPPED SWORD - ATK +3.");
        }else{
            showDialogue(w,"FATHER","YOUR PACK IS FULL. MAKE SOME ROOM FOR MY OLD SWORD.");
        }
        return;
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
    if(w.story==8 && w.scene==Scene::OUTSKIRTS &&
       withinRange(w.hero.pos,east_map::CAMP_TRIGGER,90)){
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
            fillText(r,atDoor?"OUT":"E",int(target.x)-(atDoor?14:6),
                     int(target.y)-66,atDoor?2:3,250,226,154);
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
        case 8:target=east_map::CAMP_TRIGGER;break;
    }
    if(w.scene==Scene::VILLAGE && w.largeVillage){
        Vec2 camera=village::camera(w.hero.pos);
        target.x-=camera.x;target.y-=camera.y;
    }
    rect(r,int(target.x)-20,int(target.y)-77,40,29,34,35,43);
    outline(r,int(target.x)-20,int(target.y)-77,40,29,215,182,114);
    const bool automaticDoor=(w.scene==Scene::HOME && w.story==3)||
        (w.scene==Scene::VILLAGE && w.story==7);
    fillText(r,automaticDoor?"GO":"E",int(target.x)-(automaticDoor?12:5),
             int(target.y)-70,automaticDoor?2:3,250,226,154);
    if(w.scene==Scene::VILLAGE){
        Vec2 homeDoor=w.largeVillage?village::pos(405.f,362.f):Vec2{400.f,275.f};
        if(withinRange(w.hero.pos,homeDoor,104.f)){
            if(w.largeVillage){
                const Vec2 camera=village::camera(w.hero.pos);
                homeDoor.x-=camera.x;homeDoor.y-=camera.y;
            }
            rect(r,int(homeDoor.x)-39,int(homeDoor.y)-78,78,31,34,35,43);
            outline(r,int(homeDoor.x)-39,int(homeDoor.y)-78,78,31,215,182,114);
            fillText(r,"HOME",int(homeDoor.x)-27,int(homeDoor.y)-69,2,250,226,154);
        }
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

