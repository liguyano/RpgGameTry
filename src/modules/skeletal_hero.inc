// skeletal_hero.inc — experimental modular 8-facing cutout character.
// Included from main.cpp after world_state and before asset_pipeline.
// This does not replace the existing hero sprite frames.
namespace skeletal {
constexpr int FACINGS=8;
constexpr int PARTS=14;
enum Part { Head=0,Torso,UpperFar,UpperNear,ForeFar,ForeNear,
            HandFar,HandNear,ThighFar,ThighNear,ShinFar,ShinNear,BootFar,BootNear };
struct Rect { int x=0,y=0,w=0,h=0; int area=0; };
struct Atlas {
    SDL_Texture* texture=nullptr;
    std::array<SDL_FRect,PARTS> source{};
    bool ready=false;
    int components=0;
};
// Scan connected, nontransparent islands at load time: the AI-generated
// files use different layouts (2, 4, or sometimes 3 parts per row).
// This avoids assuming a uniform grid and preserves the original pixels.
inline std::vector<Rect> components(SDL_Surface* surface){
    SDL_Surface* rgba=SDL_ConvertSurface(surface,SDL_PIXELFORMAT_RGBA32);
    if(!rgba)return {};
    const int w=rgba->w,h=rgba->h;
    std::vector<Rect> result;
    std::vector<Uint8> visited(size_t(w)*size_t(h),0);
    std::vector<int> queue;
    if(!SDL_LockSurface(rgba)){SDL_DestroySurface(rgba);return {};}
    const auto* pixels=static_cast<const Uint8*>(rgba->pixels);
    const int pitch=rgba->pitch;
    auto visible=[&](int x,int y)->bool{
        return pixels[size_t(y)*size_t(pitch)+size_t(x)*4+3]>36;
    };
    for(int y=0;y<h;y++){
        for(int x=0;x<w;x++){
            const int start=y*w+x;
            if(visited[size_t(start)]||!visible(x,y))continue;
            visited[size_t(start)]=1;
            queue.clear();
            queue.push_back(start);
            int x0=x,x1=x,y0=y,y1=y,area=0;
            for(size_t head=0;head<queue.size();head++){
                const int id=queue[head],cx=id%w,cy=id/w;
                ++area;
                x0=std::min(x0,cx);x1=std::max(x1,cx);
                y0=std::min(y0,cy);y1=std::max(y1,cy);
                const int dx[4]={-1,1,0,0},dy[4]={0,0,-1,1};
                for(int k=0;k<4;k++){
                    const int nx=cx+dx[k],ny=cy+dy[k];
                    if(nx<0||ny<0||nx>=w||ny>=h)continue;
                    const size_t ni=size_t(ny)*size_t(w)+size_t(nx);
                    if(visited[ni]||!visible(nx,ny))continue;
                    visited[ni]=1;
                    queue.push_back(int(ni));
                }
            }
            if(area>550 && x1-x0>=18 && y1-y0>=20)
                result.push_back({x0,y0,x1-x0+1,y1-y0+1,area});
        }
    }
    SDL_UnlockSurface(rgba);
    SDL_DestroySurface(rgba);
    return result;
}
inline std::vector<std::vector<Rect>> rows(std::vector<Rect> items){
    std::sort(items.begin(),items.end(),[](const Rect&a,const Rect&b){
        return (a.y+a.h/2)<(b.y+b.h/2);
    });
    std::vector<std::vector<Rect>> groups;
    for(const auto& rect:items){
        // The islands within one horizontal strip have similar vertical
        // centers. Compare against the first center of the group.
        if(groups.empty()||
           std::abs((rect.y+rect.h/2)-
                    (groups.back()[0].y+groups.back()[0].h/2))>43)
            groups.push_back({});
        groups.back().push_back(rect);
    }
    for(auto& group:groups)
        std::sort(group.begin(),group.end(),[](const Rect&a,const Rect&b){
            return a.x<b.x;
        });
    return groups;
}
inline bool assign(Atlas& atlas,const std::vector<std::vector<Rect>>& groups){
    if(groups.size()<6 || groups[0].size()<2)return false;
    const auto choose=[&](size_t row,bool far)->Rect{
        if(row>=groups.size()||groups[row].empty())return {};
        const auto& band=groups[row];
        return far?band.front():band.back();
    };
    const size_t boots=groups.size()-1;
    const size_t shins=groups.size()>=7?boots-1:boots-1;
    const size_t thighs=groups.size()>=7?boots-2:boots-1;
    const Rect head=choose(0,true),torso=choose(0,false);
    const bool combinedLegs=(groups.size()==6 && groups[thighs].size()>=4);
    const Rect thighFar=combinedLegs?groups[thighs][0]:choose(thighs,true);
    const Rect thighNear=combinedLegs?groups[thighs][3]:choose(thighs,false);
    const Rect shinFar=combinedLegs?groups[shins][1]:choose(shins,true);
    const Rect shinNear=combinedLegs?groups[shins][2]:choose(shins,false);
    const std::array<Rect,PARTS> rects{{
        head,torso,
        choose(1,true),choose(1,false),
        choose(2,true),choose(2,false),
        choose(3,true),choose(3,false),
        thighFar,thighNear,
        shinFar,shinNear,
        choose(boots,true),choose(boots,false)
    }};
    for(int i=0;i<PARTS;i++){
        if(rects[size_t(i)].w<=0||rects[size_t(i)].h<=0)return false;
        const auto& p=rects[size_t(i)];
        atlas.source[size_t(i)]={float(p.x),float(p.y),float(p.w),float(p.h)};
    }
    atlas.ready=true;
    return true;
}
inline bool load(SDL_Renderer* renderer,Atlas& atlas,const char* filename){
#if RPG_HAS_SDL3_IMAGE
    namespace fs=std::filesystem;
    fs::path path;
    for(const auto& root:{"assets/hero_bones","../assets/hero_bones",
                          "../../assets/hero_bones"}){
        const fs::path candidate=fs::path(root)/filename;
        if(fs::exists(candidate)){path=candidate;break;}
    }
    if(path.empty())return false;
    SDL_Surface* surface=IMG_Load(path.string().c_str());
    if(!surface)return false;
    const auto islands=components(surface);
    const auto groups=rows(islands);
    const bool ok=assign(atlas,groups);
    if(ok){
        atlas.texture=SDL_CreateTextureFromSurface(renderer,surface);
        atlas.ready=atlas.texture!=nullptr;
        if(atlas.ready){
            SDL_SetTextureScaleMode(atlas.texture,SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(atlas.texture,SDL_BLENDMODE_BLEND);
        }
    }
    atlas.components=int(islands.size());
    SDL_DestroySurface(surface);
    return atlas.ready;
#else
    (void)renderer;(void)atlas;(void)filename;return false;
#endif
}
inline void drawPart(SDL_Renderer* r,const Atlas& a,Part part,
                     float x,float y,float w,float h,float degrees,
                     float pivotX=.5f,float pivotY=.1f){
    if(!a.ready)return;
    const SDL_FRect dst{x-pivotX*w,y-pivotY*h,w,h};
    const SDL_FPoint pivot{pivotX*w,pivotY*h};
    SDL_RenderTextureRotated(r,a.texture,&a.source[size_t(part)],&dst,
                             degrees,&pivot,SDL_FLIP_NONE);
}
// Attachments are in logical game coordinates, not sheet coordinates.
// Rotations only change the body segment, not the character collision.
inline void draw(SDL_Renderer* renderer,const Atlas& a,
                 game::Vec2 pos,bool moving,float animation,float slashTimer,
                 bool debug,bool verticalWalk,SDL_Texture* tunicAtlas,int facing,bool showTunic){
    if(!a.ready)return;
    (void)verticalWalk;
    const float phase=moving?animation*8.04f:0.f;
    const float step=std::sin(phase);
    const float bob=moving?std::abs(std::sin(phase))*.9f:0.f;
    const float feet=pos.y+12.f-bob;
    const float x=pos.x;
    constexpr float rad=3.14159265358979323846f/180.f;
    auto drawLeg=[&](bool far){
        const float side=far?-1.f:1.f;
        const float hipX=x+side*4.5f,hipY=feet-29.f;
        const float swing=step*(far?-24.f:24.f);
        const float kneeX=hipX-std::sin(swing*rad)*14.f;
        const float kneeY=hipY+std::cos(swing*rad)*14.f;
        const float lower=step*(far?12.f:-12.f);
        const float ankleX=kneeX-std::sin(lower*rad)*13.f;
        const float ankleY=kneeY+std::cos(lower*rad)*13.f;
        drawPart(renderer,a,far?ThighFar:ThighNear,hipX,hipY,10.f,18.f,swing);
        drawPart(renderer,a,far?ShinFar:ShinNear,kneeX,kneeY,9.f,15.f,lower);
        drawPart(renderer,a,far?BootFar:BootNear,ankleX,ankleY,13.f,10.f,
                 -swing*.15f,.45f,.10f);
    };
    auto drawArm=[&](bool far){
        const float side=far?-1.f:1.f;
        const float shoulderX=x+side*10.f,shoulderY=feet-51.f;
        const bool slashing=slashTimer>0.f && !far;
        const float attackPhase=slashing?1.f-slashTimer/combat::SWING_DURATION:0.f;
        const float swing=slashing?(-65.f+attackPhase*125.f):
                          step*(far?23.f:-23.f);
        const float elbowX=shoulderX-std::sin(swing*rad)*13.f;
        const float elbowY=shoulderY+std::cos(swing*rad)*13.f;
        drawPart(renderer,a,far?UpperFar:UpperNear,shoulderX,shoulderY,
                 9.f,17.f,swing);
        drawPart(renderer,a,far?ForeFar:ForeNear,elbowX,elbowY,
                 8.f,13.f,swing*.7f);
        drawPart(renderer,a,far?HandFar:HandNear,
                 elbowX-std::sin(swing*rad)*11.f,
                 elbowY+std::cos(swing*rad)*11.f,8.f,9.f,swing*.7f);
    };
    drawLeg(true);
    drawArm(true);
    drawPart(renderer,a,Torso,x,feet-51.f,25.f,29.f,
             step*(moving?2.f:0.f),.5f,.02f);
    // Two rows of four: up, rightup, right, rightdown; down, leftdown, left, leftup.
    if(showTunic && tunicAtlas){
        float aw=0.f,ah=0.f;
        if(SDL_GetTextureSize(tunicAtlas,&aw,&ah) && aw>0.f && ah>0.f){
            const int cell=(facing+4)%8;
            const float cw=aw/4.f,ch=ah/2.f;
            const SDL_FRect src{float(cell%4)*cw+cw*.16f,float(cell/4)*ch+ch*.13f,cw*.68f,ch*.70f};
            const SDL_FRect dst{x-16.f,feet-53.f,32.f,35.f};
            SDL_RenderTexture(renderer,tunicAtlas,&src,&dst);
        }
    }
    drawPart(renderer,a,Head,x,feet-65.f,31.f,27.f,
             step*(moving?2.f:0.f),.5f,.25f);
    drawLeg(false);
    drawArm(false);
    if(debug){
        SDL_SetRenderDrawColor(renderer,89,230,127,255);
        SDL_RenderPoint(renderer,x,feet-51.f);
        SDL_RenderPoint(renderer,x-10.f,feet-51.f);
        SDL_RenderPoint(renderer,x+10.f,feet-51.f);
        SDL_RenderPoint(renderer,x-5.f,feet-29.f);
        SDL_RenderPoint(renderer,x+5.f,feet-29.f);
    }
}
} // namespace skeletal
