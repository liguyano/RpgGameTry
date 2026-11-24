// gameplay.inc — implementation fragment; included inside main.cpp's anonymous namespace.
// Keep the relative include order when refactoring dependencies.
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
        // The opening narrator text should not force an E/Space press before
        // the player can simply walk out of the house.
        const bool walking=keys[SDL_SCANCODE_W]||keys[SDL_SCANCODE_A]||
                           keys[SDL_SCANCODE_S]||keys[SDL_SCANCODE_D];
        if(w.speaker=="NARRATOR" && walking)w.dialogue.clear();
        else {w.dialogueAge+=dt;return;}
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

