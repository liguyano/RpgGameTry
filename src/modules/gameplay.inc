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
void returnToVillage(World& w);

void attack(World& w){
    if(w.story<9 || w.scene!=Scene::OUTSKIRTS || !w.dialogue.empty() ||
       w.won || w.lost || w.hero.slashTimer>0.f)return;
    w.hero.slashTimer=combat::SWING_DURATION;
    for(auto& e:w.enemies){
        if(e.hp<=0 || !combat::swordHits(w.hero.pos,w.hero.facing,e.pos))continue;
        const int dealt=equipment::hitDamage(equipment::attack(w.equipped),
                                            equipment::ENEMY_DEFENSE);
        e.hp=game::damage(e.hp,dealt);
        e.damageNumber=dealt;
        e.floatDamage=.58f;
        e.flash=.24f;
        e.stagger=combat::ENEMY_STAGGER;
        e.preparing=false;
        e.windup=0.f;
        e.attackTimer=std::max(e.attackTimer,.35f);
        const Vec2 push=combat::toward(w.hero.pos,e.pos);
        east_map::move(e.pos,{push.x*75.f,push.y*75.f},.26f,10.f);
        if(e.hp==0)++w.hero.kills;
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
    const Vec2 previousFeet=w.hero.pos;
    if(dir.x!=0 || dir.y!=0){
        if(w.scene==Scene::HOME){
            w.hero.pos.x=clamp(w.hero.pos.x+dir.x*PLAYER_SPEED*dt,88.f,float(WIDTH-90));
            w.hero.pos.y=clamp(w.hero.pos.y+dir.y*PLAYER_SPEED*dt,112.f,float(HEIGHT-86));
        }else if(w.scene==Scene::SHOP){
            shop::move(w.hero.pos,{dir.x*PLAYER_SPEED,dir.y*PLAYER_SPEED},dt);
        }else if(w.scene==Scene::OUTSKIRTS){
            east_map::move(w.hero.pos,{dir.x*PLAYER_SPEED,dir.y*PLAYER_SPEED},
                           dt,10.f);
        }else{
            if(w.largeVillage)village::move(w.hero.pos,{dir.x*PLAYER_SPEED,dir.y*PLAYER_SPEED},dt,8);
            else moveBody(w.hero.pos,{dir.x*PLAYER_SPEED,dir.y*PLAYER_SPEED},dt,11);
        }
    }
    // Update lightweight footstep particles at the *actual* moved distance.
    // No dust is created when blocked by collision, indoors, or standing still.
    const int particleScene=w.scene==Scene::VILLAGE?1:
                            w.scene==Scene::OUTSKIRTS?2:0;
    footsteps::update(w.steps,particleScene,previousFeet,w.hero.pos,dt);
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
        // Left bridge connects back to the village even after the battle
        // has begun, rather than trapping the player in the clearing.
        if(w.scene==Scene::OUTSKIRTS && dir.x<-.30f &&
           east_map::atVillageExit(w.hero.pos)){
            returnToVillage(w);
            return;
        }
    }
    if(w.story<9 || w.scene!=Scene::OUTSKIRTS)return;
    // Each goblin has a persistent role/slot relative to the hero, so the
    // pack separates into a front threat, flankers and rear harassers.
    // Only a couple attack at once; others circle for an opening.
    int preparingCount=0;
    for(const auto& enemy:w.enemies)
        if(enemy.hp>0 && enemy.preparing)++preparingCount;
    for(size_t i=0;i<w.enemies.size();++i){
        Enemy& e=w.enemies[i];
        if(e.hp<=0)continue;
        e.wobble+=dt*5.f;
        e.attackTimer=std::max(0.f,e.attackTimer-dt);
        e.stagger=std::max(0.f,e.stagger-dt);
        e.flash=std::max(0.f,e.flash-dt);
        e.floatDamage=std::max(0.f,e.floatDamage-dt);
        const Vec2 toHero=combat::toward(e.pos,w.hero.pos);
        const float distance=std::sqrt(game::distanceSquared(e.pos,w.hero.pos));
        if(distance>1.f)e.facing=toHero;
        if(e.stagger>0.f)continue;
        if(e.preparing){
            e.windup-=dt;
            if(e.windup<=0.f){
                e.preparing=false;
                --preparingCount;
                e.attackTimer=combat::ENEMY_RECOVERY+
                              float(i%3)*.19f;
                // The player can still evade the committed swing by moving
                // out of range during its visible windup.
                if(distance<combat::ENEMY_ATTACK_RANGE && w.hero.hurtTimer<=0.f){
                    w.hero.hp=game::damage(w.hero.hp,equipment::hitDamage(
                        equipment::ENEMY_ATTACK,equipment::defense(w.equipped)));
                    w.hero.hurtTimer=combat::HERO_INVULNERABILITY;
                    if(w.hero.hp<=0)w.lost=true;
                }
            }
            continue;
        }
        if(enemy_ai::canWindup(distance,e.attackTimer,false,preparingCount)){
            e.preparing=true;
            e.windup=combat::ENEMY_WINDUP;
            ++preparingCount;
            continue;
        }
        if(distance>=combat::ENEMY_NOTICE_DISTANCE)continue;
        // Each role charges briefly at a different time, then returns to
        // its sector. This creates pressure without one solid enemy pile.
        const float phase=std::fmod(w.time+float(i)*.58f,3.8f);
        const bool engage=phase<1.4f && e.attackTimer<.44f &&
                          preparingCount<enemy_ai::MAX_ATTACKERS;
        const Vec2 heading=enemy_ai::steer(w.enemies,i,w.hero.pos,
                                          w.hero.facing,engage);
        if(enemy_ai::length(heading)<.05f)continue;
        const float speed=ENEMY_SPEED*enemy_ai::speedFactor(i);
        // Test a few small steering variations if a rock blocks the direct
        // route. Use the best valid advance rather than getting stuck.
        const Vec2 sideways=enemy_ai::perpendicular(heading);
        const Vec2 candidates[]{heading,
            enemy_ai::normalizedOr(enemy_ai::add(heading,
                                                enemy_ai::mul(sideways,.95f))),
            enemy_ai::normalizedOr(enemy_ai::sub(heading,
                                                enemy_ai::mul(sideways,.95f)))};
        Vec2 chosen=e.pos;
        float bestScore=-.01f;
        for(const Vec2 candidate:candidates){
            Vec2 destination=e.pos;
            east_map::move(destination,{candidate.x*speed,candidate.y*speed},
                           dt,10.f);
            const Vec2 actual=enemy_ai::sub(destination,e.pos);
            const float score=enemy_ai::dot(actual,heading);
            if(score>bestScore){bestScore=score;chosen=destination;}
        }
        e.pos=chosen;
    }
}
