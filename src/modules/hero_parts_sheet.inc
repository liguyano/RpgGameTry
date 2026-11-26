// Five-piece eight-facing cutout spritesheet exported as assets/hero.png.
// 8 directions x 3 poses, each column containing head, body, left arm,
// right arm, and combined shorts/legs. Coordinates measured from 1536x1024.
namespace hero_parts {
constexpr int WIDTH=1536, HEIGHT=1024;
constexpr int COLUMNS=24;
constexpr int X[COLUMNS]={
    157,222,285,358,417,472,539,597,651,713,769,824,
    886,944,999,1059,1112,1168,1228,1283,1339,1401,1453,1504
};
constexpr int Y[5]={126,294,449,603,752};
constexpr int H[5]={86,71,64,65,82};
constexpr int HALF_W[5]={28,24,16,16,24};
// sheet order: down, rightdown, right, rightup, up, leftup, left, leftdown
// gameplay direction order: down, leftdown, left, leftup, up, rightup, right, rightdown
constexpr int SHEET_GROUP[8]={0,7,6,5,4,3,2,1};

inline void part(SDL_Renderer* r,SDL_Texture* tex,int group,int pose,int piece,
                 float x,float y,float w,float h,float rotation=0.f) {
    const int col=SHEET_GROUP[static_cast<size_t>(group)]*3+pose;
    float actualW=0.f,actualH=0.f;
    if(!SDL_GetTextureSize(tex,&actualW,&actualH))return;
    // Scale crop coordinates if the asset was resized after export.
    const float sx=actualW/float(WIDTH),sy=actualH/float(HEIGHT);
    const SDL_FRect src{float(X[col]-HALF_W[piece])*sx,float(Y[piece])*sy,
                        float(2*HALF_W[piece])*sx,float(H[piece])*sy};
    const SDL_FRect dst{x-w*.5f,y,w,h};
    const SDL_FPoint center{w*.5f, h*.12f};
    SDL_RenderTextureRotated(r,tex,&src,&dst,rotation,&center,SDL_FLIP_NONE);
}
inline void armor(SDL_Renderer* r,SDL_Texture* tex,int direction,
                  float cx,float top,float w,float h){
    if(!tex)return;
    float aw=0.f,ah=0.f;
    if(!SDL_GetTextureSize(tex,&aw,&ah)||aw<=0.f||ah<=0.f)return;
    const int cell=(direction+4)%8;
    const float cw=aw/4.f,ch=ah/2.f;
    const SDL_FRect src{float(cell%4)*cw+cw*.16f,
                        float(cell/4)*ch+ch*.13f,cw*.68f,ch*.70f};
    const SDL_FRect dst{cx-w*.5f,top,w,h};
    SDL_RenderTexture(r,tex,&src,&dst);
}
inline bool draw(SDL_Renderer* r,SDL_Texture* sheet,game::Vec2 position,
                 game::Vec2 facing,bool moving,float seconds,
                 SDL_Texture* tunic,bool wearingTunic){
    if(!sheet)return false;
    const int direction=hero_anim::direction(facing);
    const int pose=moving?int(std::floor(std::max(0.f,seconds)*8.f))%3:0;
    const float gait=moving?std::sin(seconds*8.f*2.f):0.f;
    const float bob=moving?std::abs(gait)*.7f:0.f;
    const float feet=position.y+12.f-bob;
    const float x=position.x;
    // Near/far limb ordering keeps the front arm in front of worn armor.
    part(r,sheet,direction,pose,3,x+9.f,feet-45.f,11.f,22.f,gait*8.f);
    part(r,sheet,direction,pose,4,x,feet-28.f,24.f,32.f);
    part(r,sheet,direction,pose,1,x,feet-52.f,22.f,28.f);
    if(wearingTunic)
        armor(r,tunic,direction,x,feet-52.f,22.f,28.f);
    part(r,sheet,direction,pose,0,x,feet-77.f,31.f,34.f);
    part(r,sheet,direction,pose,2,x-9.f,feet-45.f,11.f,22.f,-gait*8.f);
    return true;
}
} // namespace hero_parts
