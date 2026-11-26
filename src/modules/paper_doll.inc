// Paper-doll rendering for the generated 1774x887 eight-direction/eight-pose atlases.
// This atlas is irregular: large transparent margin/labels at the left,
// and row heights are not uniform. Only pixels in the eight character
// columns are sampled. Equipment is aligned to the torso, not drawn as
// a full-sized second character.
namespace paper_doll {
constexpr int DIRECTIONS=8;
constexpr int POSES=8;
constexpr float SOURCE_WIDTH=1774.f;
constexpr float SOURCE_HEIGHT=887.f;
// Sprite groups, from left to right, repeat one direction's walking poses.
constexpr float CENTERS[POSES]={
    432.f,562.f,692.f,821.f,952.f,1081.f,1212.f,1343.f
};
// Direction row boundaries measured on the supplied art.
constexpr float TOPS[DIRECTIONS+1]={
    0.f,123.f,241.f,354.f,464.f,584.f,686.f,786.f,887.f
};
// Sheet row order: down, down-right, right, up-right,
// up, up-left, left, down-left.
// Direction order in game: down, down-left, left, up-left,
// up, up-right, right, down-right.
constexpr int SHEET_ROW[DIRECTIONS]={0,7,6,5,4,3,2,1};
inline int direction(game::Vec2 facing){
    return hero_anim::direction(facing);
}
inline int frame(float seconds,bool moving){
    return moving ? (int(std::floor(std::max(0.f,seconds)*12.f))%POSES) : 0;
}
inline SDL_FRect bodySource(int direction,int pose,float w,float h){
    const int row=SHEET_ROW[direction];
    const float sx=w/SOURCE_WIDTH, sy=h/SOURCE_HEIGHT;
    // Each character occupies about 65 pixels in a 130-pixel-wide column.
    // Leave room for hair and moving feet without capturing its neighbours.
    constexpr float HALF=48.f;
    return {(CENTERS[pose]-HALF)*sx,TOPS[row]*sy,
            HALF*2.f*sx,(TOPS[row+1]-TOPS[row])*sy};
}
inline SDL_FRect armorSource(int direction,int pose,float w,float h){
    const int row=SHEET_ROW[direction];
    const float sx=w/SOURCE_WIDTH, sy=h/SOURCE_HEIGHT;
    // Crop the armor itself rather than its large transparent sheet cell.
    // This avoids scaling a chestplate as though it were a whole character.
    return {(CENTERS[pose]-43.f)*sx,(TOPS[row]+17.f)*sy,
            86.f*sx,80.f*sy};
}
inline bool draw(SDL_Renderer* renderer,SDL_Texture* body,SDL_Texture* armor,
                 game::Vec2 position,game::Vec2 facing,bool moving,
                 float animationTime,bool wearingArmor){
    if(!body)return false;
    float bw=0.f,bh=0.f;
    if(!SDL_GetTextureSize(body,&bw,&bh)||bw<=0.f||bh<=0.f)return false;
    const int dir=direction(facing);
    const int pose=frame(animationTime,moving);
    const SDL_FRect bodySrc=bodySource(dir,pose,bw,bh);
    const SDL_FRect bodyDst{position.x-28.f,position.y-64.f,56.f,78.f};
    SDL_RenderTexture(renderer,body,&bodySrc,&bodyDst);
    if(wearingArmor && armor){
        float aw=0.f,ah=0.f;
        if(SDL_GetTextureSize(armor,&aw,&ah) && aw>0.f && ah>0.f){
            const SDL_FRect armorSrc=armorSource(dir,pose,aw,ah);
            // Independent chest anchor: shoulders below head, hem above shorts.
            const SDL_FRect armorDst{position.x-14.f,position.y-27.f,
                                      28.f,23.f};
            SDL_RenderTexture(renderer,armor,&armorSrc,&armorDst);
        }
    }
    return true;
}
} // namespace paper_doll
