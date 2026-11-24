// game_over.inc — implementation fragment; included inside main.cpp's anonymous namespace.
// Keep the relative include order when refactoring dependencies.
void drawOverlay(SDL_Renderer* r,const World& w){
    if(!w.won&&!w.lost)return;
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    color(r,10,18,24,210);
    SDL_FRect full{0,0,float(WIDTH),float(HEIGHT)};SDL_RenderFillRect(r,&full);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    rect(r,WIDTH/2-290,HEIGHT/2-108,580,215,30,39,44);
    outline(r,WIDTH/2-290,HEIGHT/2-108,580,215,197,165,105);
    fillText(r,w.won?"VICTORY!":"YOU HAVE FALLEN",WIDTH/2-175,HEIGHT/2-55,5,239,202,118);
    fillText(r,w.won?"ASHVALE IS SAFE":"THE VILLAGE NEEDS YOU",WIDTH/2-156,HEIGHT/2+12,3,221,211,188);
    fillText(r,"PRESS R TO RESTART",WIDTH/2-132,HEIGHT/2+65,3,213,176,113);
}

