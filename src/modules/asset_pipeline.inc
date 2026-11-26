// asset_pipeline.inc — implementation fragment; included inside main.cpp's anonymous namespace.
// Keep the relative include order when refactoring dependencies.
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

// Pixel-measured crop bounds for the actual 1448x1086 hero.png.
// The characters are not laid out at even 1/8-height intervals; naive
// subdivision amputates boots and occasionally clips neighboring rows.
// Each crop is placed on the same transparent canvas, centered horizontally
// and bottom-aligned so walking poses don't bounce due to crop dimensions.
// Load either the untouched 1402x1122, 9x8 illustrated sprite sheet,
// or a future regular transparent 1152x1024, 9x8 armor-ready sheet.
void loadHeroFrames(SDL_Renderer* r,ArtAssets& art){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path sheetPath;
    for(const fs::path& candidate:{fs::path("assets/hero.png"),
                                   fs::path("../assets/hero.png"),
                                   fs::path("../../assets/hero.png")}){
        if(fs::exists(candidate)){sheetPath=candidate;break;}
    }
    if(sheetPath.empty()){
        std::cerr<<"hero.png not found; fallback hero enabled\n";
        return;
    }
    SDL_Surface* sheet=IMG_Load(sheetPath.string().c_str());
    if(!sheet){
        std::cerr<<"Cannot load hero.png: "<<SDL_GetError()<<"\n";
        return;
    }
    const bool original=(sheet->w==hero_anim::SHEET_W &&
                         sheet->h==hero_anim::SHEET_H);
    const bool grid=(sheet->w==hero_anim::GRID_W &&
                     sheet->h==hero_anim::GRID_H);
    if(!original&&!grid){
        std::cerr<<"Invalid hero.png dimensions "<<sheet->w<<"x"<<sheet->h
                 <<"; expected 1402x1122 (original 9x8) or "
                 <<hero_anim::GRID_W<<"x"<<hero_anim::GRID_H
                 <<" (128px cells). Falling back.\n";
        SDL_DestroySurface(sheet);
        return;
    }
    const int canvasW=original?hero_anim::CANVAS_W:hero_anim::GRID_CELL;
    const int canvasH=original?hero_anim::CANVAS_H:hero_anim::GRID_CELL;
    int loaded=0;
    for(int row=0;row<hero_anim::DIRECTIONS;row++){
        for(int pose=0;pose<hero_anim::FRAMES;pose++){
            const auto c=original?hero_anim::crop(row,pose):
                                  hero_anim::gridCrop(row,pose);
            const SDL_Rect src{int(c.x),int(c.y),int(c.w),int(c.h)};
            if(src.x<0||src.y<0||src.x+src.w>sheet->w||
               src.y+src.h>sheet->h||src.w>canvasW||src.h>canvasH)continue;
            SDL_Surface* cell=SDL_CreateSurface(canvasW,canvasH,
                                                SDL_PIXELFORMAT_RGBA32);
            if(!cell)continue;
            SDL_FillSurfaceRect(cell,nullptr,0);
            const SDL_Rect dst{(canvasW-src.w)/2,
                               original?canvasH-src.h-4:0,
                               src.w,src.h};
            if(SDL_BlitSurface(sheet,&src,cell,&dst)){
                if(original){
                    // The generated source has a nearly-white background.
                    // Remove only the neutral near-white matte connected to
                    // cell edges. Keep isolated light details of the shirt.
                    std::vector<Uint8> seen(size_t(canvasW)*size_t(canvasH),0);
                    std::vector<int> q;
                    q.reserve(seen.size());
                    auto isBackground=[&](int x,int y){
                        Uint8 red=0,green=0,blue=0,alpha=255;
                        if(!SDL_ReadSurfacePixel(cell,x,y,
                                                 &red,&green,&blue,&alpha))
                            return false;
                        const int lo=std::min({int(red),int(green),int(blue)});
                        const int hi=std::max({int(red),int(green),int(blue)});
                        return alpha<16 || (lo>215 && hi-lo<18);
                    };
                    auto addPixel=[&](int x,int y){
                        if(x<0||y<0||x>=canvasW||y>=canvasH)return;
                        const size_t k=size_t(y)*size_t(canvasW)+size_t(x);
                        if(seen[k]||!isBackground(x,y))return;
                        seen[k]=1;q.push_back(int(k));
                    };
                    for(int x=0;x<canvasW;x++){
                        addPixel(x,0);addPixel(x,canvasH-1);
                    }
                    for(int y=0;y<canvasH;y++){
                        addPixel(0,y);addPixel(canvasW-1,y);
                    }
                    for(size_t head=0;head<q.size();head++){
                        const int k=q[head],x=k%canvasW,y=k/canvasW;
                        SDL_WriteSurfacePixel(cell,x,y,0,0,0,0);
                        addPixel(x-1,y);addPixel(x+1,y);
                        addPixel(x,y-1);addPixel(x,y+1);
                    }
                }
                SDL_Texture* texture=SDL_CreateTextureFromSurface(r,cell);
                if(texture){
                    SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
                    SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
                    art.heroFrames[size_t(row)*hero_anim::FRAMES+
                                   size_t(pose)]=texture;
                    ++loaded;
                }
            }
            SDL_DestroySurface(cell);
        }
    }
    SDL_DestroySurface(sheet);
    std::cerr<<"Hero original 9x8 atlas: "<<loaded<<"/72 frames\n";
#else
    (void)r;(void)art;
#endif
}
int facingSpriteDirection(Vec2 facing){
    // Legacy four-way lookup is still used by family portraits and villagers.
    if(std::abs(facing.x)>std::abs(facing.y))return facing.x<0.f?1:2;
    return facing.y<0.f?3:0;
}
bool drawAnimatedHero(SDL_Renderer* r,const ArtAssets& art,
                      Vec2 position,Vec2 facing,bool moving,float animTime){
    const int direction=hero_anim::direction(facing);
    const int frame=hero_anim::frame(animTime,moving,direction);
    SDL_Texture* texture=art.heroFrames[size_t(direction)*hero_anim::FRAMES+size_t(frame)];
    if(!texture)texture=art.heroFrames[size_t(direction)*hero_anim::FRAMES];
    if(!texture)return false;
    // The south and south-west drawings are physically larger in the
    // original art. Scale them around the same foot anchor as all other
    // directions; don't change the player's collision/body position.
    constexpr float width=52.f,height=78.f;
    float sourceW=0.f,sourceH=0.f;
    SDL_GetTextureSize(texture,&sourceW,&sourceH);
    const bool gridAtlas=std::abs(sourceW-hero_anim::GRID_CELL)<.5f &&
                         std::abs(sourceH-hero_anim::GRID_CELL)<.5f;
    const float scale=gridAtlas?1.f:hero_anim::visualScale(direction);
    const float bottom=position.y+14.f;
    const float w=width*scale,h=height*scale;
    const SDL_FRect dst{position.x-w*.5f,bottom-h,w,h};
    SDL_RenderTexture(r,texture,nullptr,&dst);
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

// These generated combat atlases have a dark external matte. Remove only
// near-black pixels connected to the atlas canvas, preserving the dark
// outlines inside a goblin or a sword.
SDL_Texture* loadCombatAtlas(SDL_Renderer* renderer,const char* filename){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path path;
    for(const fs::path& candidate:{fs::path("assets")/filename,
                                   fs::path("../assets")/filename,
                                   fs::path("../../assets")/filename}){
        if(fs::exists(candidate)){path=candidate;break;}
    }
    if(path.empty())return nullptr;
    SDL_Surface* surface=IMG_Load(path.string().c_str());
    if(!surface)return nullptr;
    // Use a single background threshold: black used by shadows in the
    // actual sprites is kept unless it is directly connected to the matte.
    const int width=surface->w,height=surface->h;
    std::vector<Uint8> visited(size_t(width)*size_t(height),0);
    std::vector<int> queue;
    queue.reserve(visited.size());
    auto isMatte=[&](int x,int y){
        Uint8 red=0,green=0,blue=0,alpha=255;
        if(!SDL_ReadSurfacePixel(surface,x,y,&red,&green,&blue,&alpha))return false;
        return alpha<15 || (red<42 && green<42 && blue<42);
    };
    auto enqueue=[&](int x,int y){
        if(x<0||y<0||x>=width||y>=height)return;
        const size_t k=size_t(y)*size_t(width)+size_t(x);
        if(visited[k]||!isMatte(x,y))return;
        visited[k]=1;queue.push_back(int(k));
    };
    for(int x=0;x<width;x++){enqueue(x,0);enqueue(x,height-1);}
    for(int y=0;y<height;y++){enqueue(0,y);enqueue(width-1,y);}
    for(size_t head=0;head<queue.size();head++){
        const int index=queue[head],x=index%width,y=index/width;
        SDL_WriteSurfacePixel(surface,x,y,0,0,0,0);
        enqueue(x-1,y);enqueue(x+1,y);enqueue(x,y-1);enqueue(x,y+1);
    }
    SDL_Texture* texture=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_DestroySurface(surface);
    if(texture){
        SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
    }
    return texture;
#else
    (void)renderer;(void)filename;
    return nullptr;
#endif
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
    // Eastern battle clearing: single empty map with separately rendered goblins.
    result.outskirts=loadArt(renderer,"gablingather.png");
    if(!result.outskirts)result.outskirts=loadArt(renderer,"outskirts.bmp");
    result.hero=loadArt(renderer,"hero.bmp");
    result.father=loadArt(renderer,"father.bmp");
    result.mother=loadArt(renderer,"mother.bmp");
    result.lily=loadArt(renderer,"lily.bmp");
    loadFamilyFrames(renderer,result);
    loadHeroFrames(renderer,result);
    // Per-direction modular bones. leftdwon.png is the uploaded filename;
    // accept corrected spelling if the asset gets renamed later.
    const char* boneFiles[8]={"down.png","leftdwon.png","left.png",
                               "leftup.png","up.png","rightup.png",
                               "right.png","rightdown.png"};
    for(int facing=0;facing<8;facing++){
        bool ok=skeletal::load(renderer,result.skeletalAtlases[size_t(facing)],
                               boneFiles[facing]);
        if(!ok && facing==1)
            ok=skeletal::load(renderer,result.skeletalAtlases[size_t(facing)],
                              "leftdown.png");
        std::cerr<<"Skeletal facing "<<facing<<": "
                 <<(ok?"ready":"missing/invalid")<<", parts detected: "
                 <<result.skeletalAtlases[size_t(facing)].components<<"\\n";
    }

    loadVillageProps(renderer,result);
    loadVillagerFrames(renderer,result);
    result.uiAtlas=loadArt(renderer,"ui.png");
    result.itemsAtlas=loadArt(renderer,"items.png");
    result.goblinAtlas=loadCombatAtlas(renderer,"goblin.png");
    result.swordAtlas=loadCombatAtlas(renderer,"sword_attack.png");
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
    for(auto& skeleton:art.skeletalAtlases)
        if(skeleton.texture)SDL_DestroyTexture(skeleton.texture);
    for(SDL_Texture* t:art.villageProps)
        if(t)SDL_DestroyTexture(t);
    for(SDL_Texture* t:art.fountainFrames)
        if(t)SDL_DestroyTexture(t);
    if(art.fountainBase)SDL_DestroyTexture(art.fountainBase);
    if(art.fountainWater)SDL_DestroyTexture(art.fountainWater);
    if(art.fountainAnimation)SDL_DestroyTexture(art.fountainAnimation);
    if(art.uiAtlas)SDL_DestroyTexture(art.uiAtlas);
    if(art.itemsAtlas)SDL_DestroyTexture(art.itemsAtlas);
    if(art.goblinAtlas)SDL_DestroyTexture(art.goblinAtlas);
    if(art.swordAtlas)SDL_DestroyTexture(art.swordAtlas);
    if(art.shopBackground)SDL_DestroyTexture(art.shopBackground);
    for(SDL_Texture* t:art.shopFurniture)if(t)SDL_DestroyTexture(t);
    for(SDL_Texture* t:art.villagerFrames)if(t)SDL_DestroyTexture(t);
}
