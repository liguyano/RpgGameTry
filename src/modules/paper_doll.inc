// Paper Doll renderer for the *actual* 1330x1182 image assets.
// These PNGs have a LEFT LABEL COLUMN and uneven sprite spacing.
// hero.png is the unarmored base. Draw the separate torso_tunic.png
// only when the armor equipment slot holds the starter tunic (ID 18).
namespace paper_doll {
constexpr int DIRECTIONS=8;
constexpr int POSES=10;
constexpr float FPS=12.f;
// Game order: down, down-left, left, up-left, up, up-right, right, down-right.
// Actual art row order: up, up-right, right, down-right, down,
//                       down-left, left, up-left.
constexpr int SHEET_ROW[DIRECTIONS]={4,5,6,7,0,1,2,3};
struct Band {float top,bottom;};
// Actual content bands, measured on the 1330x1182 hero.png.
constexpr Band HERO_ROWS[DIRECTIONS] = {
    {8.f,158.f},{163.f,309.f},{315.f,461.f},{469.f,606.f},
    {609.f,747.f},{751.f,881.f},{882.f,1017.f},{1020.f,1166.f}
};
constexpr float HERO_CENTER[POSES]={
    172.f,305.f,427.f,554.f,678.f,803.f,927.f,1050.f,1169.f,1284.f
};
// torso_tunic.png currently exposes nine foreground columns despite
// being labeled as a 10-frame sheet.  Do not read off the right edge.
constexpr int ARMOR_POSES=9;
constexpr Band ARMOR_ROWS[DIRECTIONS]={
    {51.f,132.f},{187.f,282.f},{339.f,432.f},{483.f,579.f},
    {632.f,719.f},{758.f,851.f},{890.f,979.f},{1029.f,1120.f}
};
constexpr float ARMOR_CENTER[ARMOR_POSES]={
    176.f,306.f,438.f,571.f,703.f,836.f,969.f,1102.f,1235.f
};
inline SDL_FRect source(int direction,int pose,float width,float height,bool armor){
    const int row=SHEET_ROW[static_cast<size_t>(direction)];
    const Band range=armor?ARMOR_ROWS[row]:HERO_ROWS[row];
    const float center=armor?ARMOR_CENTER[std::min(pose,ARMOR_POSES-1)]:
                              HERO_CENTER[pose];
    const float cropW=armor?106.f:108.f;
    const float sx=width/1330.f,sy=height/1182.f;
    const float left=std::max(0.f,center-cropW*.5f);
    return {left*sx,range.top*sy,
            std::min(cropW,1330.f-left)*sx,(range.bottom-range.top)*sy};
}
inline bool draw(SDL_Renderer* renderer,SDL_Texture* baseHero,
                 SDL_Texture* armor,game::Vec2 position,game::Vec2 facing,
                 bool moving,float animationTime,bool wearingArmor){
    if(!baseHero)return false;
    const int direction=hero_anim::direction(facing);
    const int pose=moving?
        int(std::floor(std::max(0.f,animationTime)*FPS))%POSES:0;
    float w=0.f,h=0.f;
    if(!SDL_GetTextureSize(baseHero,&w,&h)||w<=0.f||h<=0.f)return false;
    const SDL_FRect src=source(direction,pose,w,h,false);
    const SDL_FRect dst{position.x-28.f,position.y-64.f,56.f,78.f};
    // The base is always drawn, independently of the equipped items.
    SDL_RenderTexture(renderer,baseHero,&src,&dst);
    if(wearingArmor && armor){
        float aw=0.f,ah=0.f;
        if(SDL_GetTextureSize(armor,&aw,&ah) && aw>0.f && ah>0.f){
            // Armor sprites are separately generated and have different
            // content bounds from the body; use their own per-row crops.
            // There are only nine reliably detected armor columns: map
            // ten walk frames across the nine sprites without crossing
            // into an adjacent label or empty margin.
            const int armorPose=std::min(pose,ARMOR_POSES-1);
            const SDL_FRect armorSrc=source(direction,armorPose,aw,ah,true);
            // The armor occupies just the torso between neck and waist.
            // Keep it in game-space, not relative to the source image.
            const SDL_FRect armorDst{position.x-15.f,
                                    position.y-34.f,30.f,29.f};
            SDL_RenderTexture(renderer,armor,&armorSrc,&armorDst);
        }
    }
    return true;
}
} // namespace paper_doll
