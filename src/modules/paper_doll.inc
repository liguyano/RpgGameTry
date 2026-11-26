// Eight-facing / ten-pose Paper Doll character rendering.
// Both generated PNGs contain irregularly spaced rows. Do NOT divide their
// heights into eight equal strips: that mixes feet from one row with the head
// of the next row. The two art sheets also differ in resolution/layout.
namespace paper_doll {
constexpr int DIRECTIONS=8;
constexpr int POSES=10;
constexpr float FPS=12.f;
// Game order: down, down-left, left, up-left, up, up-right, right, down-right.
// New hero.png and torso_tunic.png share row order:
 // down, down-left, left, up-left, up, up-right, right, down-right.
 // Do not reverse the rows: doing so draws the wrong facing sprites.
constexpr int SHEET_ROW[DIRECTIONS]={0,1,2,3,4,5,6,7};
constexpr float HERO_REF_W=1402.f,HERO_REF_H=1122.f;
constexpr float ARMOR_REF_W=1448.f,ARMOR_REF_H=1086.f;
constexpr float HERO_CENTER[POSES]={
    97.f,235.f,376.f,513.f,651.f,787.f,919.f,1055.f,1186.f,1316.f
};
constexpr float ARMOR_CENTER[POSES]={
    104.f,241.f,381.f,521.f,660.f,794.f,927.f,1066.f,1206.f,1346.f
};
struct Band {float top,bottom;};
// Measured row crops, keeping adjacent walking rows out of each other.
// Original image's final row is truncated at its bottom edge; this cannot
// be reconstructed in code, and will need a corrected source artwork.
constexpr Band HERO_BANDS[DIRECTIONS]={
    {0.f,166.f},{163.f,312.f},{314.f,460.f},{463.f,601.f},
    {604.f,744.f},{746.f,882.f},{884.f,1015.f},{1017.f,1122.f}
};
constexpr Band ARMOR_BANDS[DIRECTIONS]={
    {44.f,124.f},{168.f,253.f},{292.f,379.f},{417.f,507.f},
    {543.f,636.f},{663.f,757.f},{789.f,885.f},{911.f,1015.f}
};
inline SDL_FRect frameRect(int direction,int pose,
                           float textureW,float textureH,bool armor){
    const int row=SHEET_ROW[static_cast<size_t>(direction)];
    const Band band=armor?ARMOR_BANDS[row]:HERO_BANDS[row];
    const float refW=armor?ARMOR_REF_W:HERO_REF_W;
    const float refH=armor?ARMOR_REF_H:HERO_REF_H;
    const float cropWidth=armor?72.f:110.f;
    const float center=armor?ARMOR_CENTER[pose]:HERO_CENTER[pose];
    const float sx=textureW/refW,sy=textureH/refH;
    return {(center-cropWidth*.5f)*sx,band.top*sy,
            cropWidth*sx,(band.bottom-band.top)*sy};
}
inline bool draw(SDL_Renderer* renderer,SDL_Texture* body,SDL_Texture* armor,
                 game::Vec2 position,game::Vec2 facing,bool moving,
                 float animationTime,bool wearingArmor){
    if(!body)return false;
    float bw=0.f,bh=0.f;
    if(!SDL_GetTextureSize(body,&bw,&bh)||bw<=0.f||bh<=0.f)return false;
    const int direction=hero_anim::direction(facing);
    // Use all ten poses; the idle frame is pose 0.
    const int pose=moving?
        (int(std::floor(std::max(0.f,animationTime)*FPS))%POSES):0;
    const SDL_FRect bodySource=frameRect(direction,pose,bw,bh,false);
    // Same physical feet anchor for all directions/poses.
    const SDL_FRect bodyTarget{position.x-28.f,position.y-64.f,56.f,78.f};
    SDL_RenderTexture(renderer,body,&bodySource,&bodyTarget);
    if(wearingArmor && armor){
        float aw=0.f,ah=0.f;
        if(SDL_GetTextureSize(armor,&aw,&ah) && aw>0.f && ah>0.f){
            const SDL_FRect armorSource=frameRect(direction,pose,aw,ah,true);
            // A consistent torso anchor: old art is not an exactly matching
            // pixel-aligned overlay, so its cropping/scale differs from body.
            // No time-dependent offset; wear/unequip switches this layer only.
            const SDL_FRect armorTarget{position.x-15.f,position.y-33.f,30.f,31.f};
            SDL_RenderTexture(renderer,armor,&armorSource,&armorTarget);
        }
    }
    return true;
}
} // namespace paper_doll
