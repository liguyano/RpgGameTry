// Paper Doll: two independent 8-direction x 10-frame sheets.
// hero.png: 1402x1122; torso_tunic.png: 1448x1086.
// The image generators left different margins, so source rectangles MUST
// be calculated separately. Both layers use the same logical destination
// rectangle to avoid direction/frame-dependent on-screen drift.
namespace paper_doll {
constexpr int DIRECTIONS=8;
constexpr int POSES=10;
constexpr float FPS=12.f;

// Game order: down, down-left, left, up-left, up, up-right, right, down-right.
// Image row order: down, down-right, right, up-right, up, up-left, left, down-left.
constexpr int SHEET_ROW[DIRECTIONS]={0,7,6,5,4,3,2,1};

// Measured sprite-column centres (in original atlas pixels).
// These are intentionally NOT regular 1/10-width divisions: both generated
// images include margins and the horizontal spacing changes slightly.
constexpr float HERO_CENTERS[POSES]={
    97.f,235.f,376.f,513.f,651.f,787.f,919.f,1055.f,1186.f,1316.f
};
constexpr float ARMOR_CENTERS[POSES]={
    104.f,241.f,381.f,521.f,660.f,794.f,927.f,1066.f,1206.f,1346.f
};
constexpr float HERO_REF_W=1402.f,HERO_REF_H=1122.f;
constexpr float ARMOR_REF_W=1448.f,ARMOR_REF_H=1086.f;
// Each crop is centred on the visible sprite, with enough transparent
// padding to accommodate its movement and preserve a common local anchor.
constexpr float SOURCE_CROP_W=110.f;
constexpr float ARMOR_Y_OFFSET=8.f; // generated armor sits ~8 source pixels too low

inline SDL_FRect sourceRect(int direction,int pose,
                            float imageW,float imageH,bool armor) {
    const int row=SHEET_ROW[static_cast<size_t>(direction)];
    const float refW=armor?ARMOR_REF_W:HERO_REF_W;
    const float refH=armor?ARMOR_REF_H:HERO_REF_H;
    const float sx=imageW/refW,sy=imageH/refH;
    const float center=armor?ARMOR_CENTERS[pose]:HERO_CENTERS[pose];
    const float rowHeight=refH/float(DIRECTIONS);
    const float yOffset=armor?ARMOR_Y_OFFSET:0.f;
    return {(center-SOURCE_CROP_W*.5f)*sx,
            (float(row)*rowHeight+yOffset)*sy,
            SOURCE_CROP_W*sx,rowHeight*sy};
}

inline bool draw(SDL_Renderer* renderer,SDL_Texture* body,SDL_Texture* armor,
                 game::Vec2 position,game::Vec2 facing,bool moving,
                 float animationTime,bool wearingArmor) {
    if(!body)return false;
    float bw=0.f,bh=0.f;
    if(!SDL_GetTextureSize(body,&bw,&bh)||bw<=0.f||bh<=0.f)return false;
    const int dir=hero_anim::direction(facing);
    const int pose=moving?
        (int(std::floor(std::max(0.f,animationTime)*FPS))%POSES):0;
    // Common logical frame: foot anchor remains at position.y+14.
    const SDL_FRect dst{position.x-28.f,position.y-64.f,56.f,78.f};
    const SDL_FRect bodySrc=sourceRect(dir,pose,bw,bh,false);
    SDL_RenderTexture(renderer,body,&bodySrc,&dst);
    if(wearingArmor && armor){
        float aw=0.f,ah=0.f;
        if(SDL_GetTextureSize(armor,&aw,&ah) && aw>0.f && ah>0.f){
            const SDL_FRect armorSrc=sourceRect(dir,pose,aw,ah,true);
            SDL_RenderTexture(renderer,armor,&armorSrc,&dst);
        }
    }
    return true;
}
} // namespace paper_doll
