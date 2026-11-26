// world_renderer.inc — implementation fragment; included inside main.cpp's anonymous namespace.
// Keep the relative include order when refactoring dependencies.
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
    rect(r,x-12,y-33,static_cast<int>(24.f*std::max(0,e.hp)/float(equipment::ENEMY_HEALTH)),3,188,66,57);
}
// Sprite atlas: 8 columns (walk/attack) x 4 directions.
// If goblin.png is missing, keep the existing procedural silhouette.
void drawCombatGoblin(SDL_Renderer* r,const ArtAssets& art,const Enemy& e){
    if(!art.goblinAtlas){drawEnemy(r,e);return;}
    float atlasW=0.f,atlasH=0.f;
    if(!SDL_GetTextureSize(art.goblinAtlas,&atlasW,&atlasH))return;
    int row=0;
    if(std::abs(e.facing.x)>std::abs(e.facing.y))
        row=e.facing.x<0.f?1:2;
    else if(e.facing.y<0.f)row=3;
    const int pose=e.preparing?4+(int(e.windup*8.f)%3):
                    e.stagger>0.f?7:(int(e.wobble*1.4f)%4);
    const float cw=atlasW/8.f,ch=atlasH/4.f;
    const SDL_FRect src{cw*float(pose)+cw*.04f,
                       ch*float(row)+ch*.04f,cw*.92f,ch*.92f};
    const float size=64.f;
    const SDL_FRect dst{e.pos.x-size/2.f,e.pos.y-59.f,size,73.f};
    if(e.flash>0.f)SDL_SetTextureColorMod(art.goblinAtlas,255,112,102);
    SDL_RenderTexture(r,art.goblinAtlas,&src,&dst);
    if(e.flash>0.f)SDL_SetTextureColorMod(art.goblinAtlas,255,255,255);
    // HP bar and enemy attack warning are rendered over the actual sprite.
    rect(r,int(e.pos.x)-20,int(e.pos.y)-66,40,5,55,31,31);
    rect(r,int(e.pos.x)-20,int(e.pos.y)-66,
         int(40.f*std::max(0,e.hp)/float(equipment::ENEMY_HEALTH)),
         5,198,69,66);
    if(e.preparing){
        // Readable windup cue, including when the atlas does not depict it.
        const int radius=int(20+8.f*(1.f-e.windup/combat::ENEMY_WINDUP));
        color(r,248,96,65);
        for(int i=0;i<28;i++){
            const float a=2.f*combat::PI*float(i)/28.f;
            SDL_RenderPoint(r,e.pos.x+float(radius)*std::cos(a),
                             e.pos.y+float(radius)*std::sin(a));
        }
        fillText(r,"!",int(e.pos.x)-4,int(e.pos.y)-80,3,247,113,82);
    }
    if(e.floatDamage>0.f){
        const int rise=int((.58f-e.floatDamage)*36.f);
        fillText(r,std::to_string(e.damageNumber),
                 int(e.pos.x)-7,int(e.pos.y)-92-rise,2,255,213,123);
    }
}
// Render an animated sword sweep in the facing direction, after the hero.
void drawSwordSwing(SDL_Renderer* r,const ArtAssets& art,
                    Vec2 hero,Vec2 facing,float remaining){
    if(remaining<=0.f)return;
    const float angle=std::atan2(facing.y,facing.x);
    // A lightweight procedural slash is always visible even if the supplied
    // sword sprite sheet is missing or has a different animation layout.
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    color(r,249,218,137,220);
    for(int i=0;i<20;i++){
        const float offset=(-combat::ATTACK_HALF_ANGLE+
                            2.f*combat::ATTACK_HALF_ANGLE*float(i)/19.f)+angle;
        const float range=38.f+float(i%3);
        SDL_RenderPoint(r,hero.x+std::cos(offset)*range,
                         hero.y-15.f+std::sin(offset)*range);
    }
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    if(!art.swordAtlas)return;
    float w=0.f,h=0.f;
    if(!SDL_GetTextureSize(art.swordAtlas,&w,&h))return;
    const int pose=combat::slashFrame(remaining,4);
    const SDL_FRect src{w*float(pose)/4.f,0.f,w/4.f,h/2.f};
    // The source's right-facing sweep is rotated toward the attack vector.
    const SDL_FRect dst{hero.x+facing.x*28.f-29.f,
                        hero.y+facing.y*28.f-45.f,58.f,58.f};
    SDL_RenderTextureRotated(r,art.swordAtlas,&src,&dst,
                             double(angle*180.f/combat::PI),
                             nullptr,SDL_FLIP_NONE);
}
// A few small translucent pixels near the boots. Particles are spawned in
// world coordinates, then shifted with the village camera during rendering.
// Draw before the hero so the feet always remain in the foreground.
void drawFootstepParticles(SDL_Renderer* renderer,
                           const footsteps::Emitter& emitter,Vec2 camera){
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    for(const auto& particle:emitter.particles){
        if(particle.life<=0.f || particle.lifetime<=0.f)continue;
        const float ratio=particle.life/particle.lifetime;
        const float x=particle.position.x-camera.x;
        const float y=particle.position.y-camera.y;
        if(x<-10.f || x>WIDTH+10.f || y<-10.f || y>HEIGHT+10.f)continue;
        const Uint8 alpha=Uint8(std::clamp(150.f*ratio,0.f,255.f));
        if(particle.grass){
            SDL_SetRenderDrawColor(renderer,135,177,80,alpha);
        }else if(emitter.scene==1){
            SDL_SetRenderDrawColor(renderer,187,154,111,alpha);
        }else{
            SDL_SetRenderDrawColor(renderer,174,157,124,alpha);
        }
        const float diameter=particle.size*(1.1f+.45f*(1.f-ratio));
        const SDL_FRect fragment{x-diameter*.5f,y-diameter*.5f,
                                 diameter,particle.grass?diameter*.58f:diameter};
        SDL_RenderFillRect(renderer,&fragment);
    }
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
}
void drawHud(SDL_Renderer* r,const World& w){
    // Persistent health bar is drawn separately in all scenes.
    fillText(r,"KNIGHT OF ASHVALE",23,14,2,231,202,135);
    rect(r,WIDTH-225,10,213,54,22,32,38);
    outline(r,WIDTH-225,10,213,54,189,153,88);
    fillText(r,"GOBLINS: "+std::to_string(w.hero.kills)+"/8",WIDTH-213,21,2,233,199,133);
    fillText(r,"ATK "+std::to_string(equipment::attack(w.equipped))+
             "   DEF "+std::to_string(equipment::defense(w.equipped)),
             WIDTH-213,46,2,235,213,181);
    rect(r,9,HEIGHT-35,WIDTH-18,25,22,32,38);
    outline(r,9,HEIGHT-35,WIDTH-18,25,189,153,88);
    fillText(r,"WASD MOVE   LEFT CLICK ATTACK   SPACE ATTACK   ESC MENU",23,HEIGHT-28,2,231,209,168);
}
