// hud_dialogue.inc — implementation fragment; included inside main.cpp's anonymous namespace.
// Keep the relative include order when refactoring dependencies.
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
