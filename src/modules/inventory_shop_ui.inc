// inventory_shop_ui.inc — implementation fragment; included inside main.cpp's anonymous namespace.
// Keep the relative include order when refactoring dependencies.
// First-playable inventory: item ids are fixed positions on items.png.
// Inventory, shop and combat share one authoritative definition per item.
inline const equipment::Item& itemView(int id){return equipment::item(id);}
void useInventoryItem(World& world,int index){
    if(index<0||index>=int(world.bag.size()))return;
    const int id=world.bag[size_t(index)];
    if(id<=0)return;
    const auto& def=itemView(id);
    if(def.consumable){
        // Mana regeneration will be added when a mana system exists; do not
        // consume a mana potion for no effect.
        if(def.heal==0 || world.hero.hp>=MAX_HEALTH)return;
        world.hero.hp=std::min(MAX_HEALTH,world.hero.hp+def.heal);
        world.bag[size_t(index)]=0;
    }else if(def.slot>=0){
        equipment::equip(world.bag,world.equipped,index);
    }
}
bool useQuickSlot(World& world,int slot){
    if(slot<0||slot>=int(world.quickSlots.size()))return false;
    world.quickSelected=slot;
    const int id=world.quickSlots[size_t(slot)];
    if(!id)return false;
    for(int i=0;i<int(world.bag.size());i++){
        if(world.bag[size_t(i)]==id){
            if(itemView(id).consumable){useInventoryItem(world,i);return true;}
            return false;
        }
    }
    return false;
}
void clickInventory(World& world,float x,float y){
    constexpr float bagX=80.f,bagY=230.f,bagStep=58.f;
    if(x>=bagX&&x<bagX+8*bagStep&&y>=bagY&&y<bagY+3*bagStep){
        const int col=int((x-bagX)/bagStep),row=int((y-bagY)/bagStep);
        world.inventorySelection=row*8+col;
        return;
    }
    // Click equipment slots to unequip into the first empty backpack cell.
    constexpr float gearX=639.f,gearY=230.f,gearStep=79.f;
    if(x>=gearX&&x<gearX+3*gearStep&&y>=gearY&&y<gearY+2*gearStep){
        const int slot=int((y-gearY)/gearStep)*3+int((x-gearX)/gearStep);
        if(world.equipped[size_t(slot)]){
            for(auto& item:world.bag)if(!item){
                item=world.equipped[size_t(slot)];
                world.equipped[size_t(slot)]=0;
                break;
            }
        }
        return;
    }
    // Click on quickbar assigns the currently selected backpack item.
    if(y>=538.f && y<598.f && x>=280.f && x<680.f){
        const int slot=int((x-280.f)/50.f);
        world.quickSlots[size_t(slot)]=world.bag[size_t(world.inventorySelection)];
        world.quickSelected=slot;
    }
}
void atlasImage(SDL_Renderer* renderer,SDL_Texture* texture,
                float sx,float sy,float sw,float sh,
                float x,float y,float w,float h,float refW,float refH){
    if(!texture)return;
    float tw=0.f,th=0.f;
    if(!SDL_GetTextureSize(texture,&tw,&th)||tw<=0||th<=0)return;
    const SDL_FRect source{sx*tw/refW,sy*th/refH,sw*tw/refW,sh*th/refH};
    const SDL_FRect destination{x,y,w,h};
    SDL_RenderTexture(renderer,texture,&source,&destination);
}
void itemIcon(SDL_Renderer* renderer,const ArtAssets& art,int id,float x,float y,float size){
    if(id<=0 || !art.itemsAtlas)return;
    const auto& info=itemView(id);
    // First three rows of items.png have six centered icons per row.
    // Crop well inside each tile to exclude neighbouring art.
    constexpr float centersX[6]={141.f,365.f,586.f,810.f,1037.f,1253.f};
    constexpr float centersY[3]={109.f,278.f,447.f};
    // The starter blade shares the iron-sword atlas cell, but its faded
    // rusty tint keeps it visually distinct without a new texture asset.
    if(id==17)SDL_SetTextureColorMod(art.itemsAtlas,170,130,94);
    atlasImage(renderer,art.itemsAtlas,
               centersX[info.iconColumn]-77.f,centersY[info.iconRow]-73.f,154.f,146.f,
               x,y,size,size,1408.f,1024.f);
    if(id==17)SDL_SetTextureColorMod(art.itemsAtlas,255,255,255);
}
void uiBox(SDL_Renderer* renderer,float x,float y,float w,float h){
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,19,25,36,242);
    SDL_FRect fill{x,y,w,h};
    SDL_RenderFillRect(renderer,&fill);
    SDL_SetRenderDrawColor(renderer,197,159,91,255);
    SDL_RenderRect(renderer,&fill);
    SDL_FRect inner{x+4.f,y+4.f,w-8.f,h-8.f};
    SDL_SetRenderDrawColor(renderer,104,87,67,245);
    SDL_RenderRect(renderer,&inner);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
}
void drawQuickbar(SDL_Renderer* renderer,const World& w,const ArtAssets& art){
    // Draw during play as well as in the inventory screen.
    uiBox(renderer,271.f,528.f,418.f,76.f);
    for(int i=0;i<8;i++){
        const float x=281.f+50.f*i,y=541.f;
        uiBox(renderer,x,y,46.f,51.f);
        itemIcon(renderer,art,w.quickSlots[size_t(i)],x+6.f,y+3.f,34.f);
        if(i==w.quickSelected){
            color(renderer,240,211,116);
            SDL_FRect selected{x-2.f,y-2.f,50.f,55.f};
            SDL_RenderRect(renderer,&selected);
        }
        fillText(renderer,std::to_string(i+1),int(x+18),int(y+40),1,240,207,147);
    }
}
// Both mouse hit testing and rendering share the same item row rectangles.
int shopItemAt(float x,float y){
    constexpr float left=204.f,right=730.f,top=192.f,stride=45.f;
    if(x<left||x>=right||y<top)return -1;
    const int row=int((y-top)/stride);
    if(row<0||row>=int(SHOP_GOODS.size()))return -1;
    return y<top+float(row)*stride+43.f?row:-1;
}
bool shopCloseAt(float x,float y){
    return x>=715.f&&x<=765.f&&y>=119.f&&y<=165.f;
}
void purchaseShopSelection(World& world){
    const PurchaseResult outcome=buyShopItem(world,world.shopSelection);
    world.shopFeedback=outcome==PurchaseResult::BOUGHT?"PURCHASE COMPLETE":
        outcome==PurchaseResult::NO_GOLD?"NOT ENOUGH GOLD":
        outcome==PurchaseResult::BAG_FULL?"BACKPACK IS FULL":"CANNOT BUY ITEM";
}
void handleShopClick(World& world,float x,float y){
    if(shopCloseAt(x,y)){world.shopMenu=false;return;}
    const int selected=shopItemAt(x,y);
    if(selected>=0){
        world.shopSelection=selected;
        purchaseShopSelection(world);
    }
}

void drawShopMenu(SDL_Renderer* r,const World& w,const ArtAssets& art){
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r,4,9,16,205);
    SDL_FRect dim{0.f,0.f,float(WIDTH),float(HEIGHT)};
    SDL_RenderFillRect(r,&dim);
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_NONE);
    uiBox(r,176.f,102.f,608.f,460.f);
    fillText(r,"GENERAL STORE",225,131,4,246,207,138);
    uiBox(r,717.f,120.f,46.f,43.f);
    fillText(r,"X",732,129,3,246,207,138);
    fillText(r,"GOLD: "+std::to_string(w.gold),565,143,2,244,211,143);
    for(int i=0;i<int(SHOP_GOODS.size());i++){
        const int id=SHOP_GOODS[size_t(i)];
        const int x=217,y=200+45*i;
        if(i==w.shopSelection){
            uiBox(r,float(x-13),float(y-8),526.f,43.f);
        }
        itemIcon(r,art,id,float(x),float(y-3),34.f);
        fillText(r,itemView(id).name,x+68,y+4,2,239,218,183);
        fillText(r,std::to_string(SHOP_PRICES[size_t(i)])+" G",x+425,y+4,2,243,198,107);
    }
    fillText(r,w.shopFeedback.empty()?"MOUSE: CLICK TO BUY   X: CLOSE   UP/DOWN + E: BUY":w.shopFeedback,
             218,528,2,241,204,151);
}

void drawInventory(SDL_Renderer* renderer,const World& w,const ArtAssets& art){
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,5,9,17,202);
    SDL_FRect shade{0.f,0.f,float(WIDTH),float(HEIGHT)};
    SDL_RenderFillRect(renderer,&shade);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    uiBox(renderer,54.f,112.f,513.f,407.f);
    uiBox(renderer,599.f,112.f,305.f,407.f);
    // Decorative upper carved title bars cropped from ui.png. The rest of
    // the panels remain readable even if the atlas is missing.
    atlasImage(renderer,art.uiAtlas,95,0,370,142,114,120,390,91,1408,1024);
    atlasImage(renderer,art.uiAtlas,610,0,250,145,633,122,242,92,1408,1024);
    fillText(renderer,"BACKPACK",87,183,3,247,222,151);
    fillText(renderer,"EQUIPMENT",627,183,3,247,222,151);
    for(int i=0;i<24;i++){
        const int col=i%8,row=i/8;
        const float x=80.f+float(col)*58.f,y=230.f+float(row)*58.f;
        uiBox(renderer,x,y,52.f,52.f);
        itemIcon(renderer,art,w.bag[size_t(i)],x+5.f,y+5.f,42.f);
        if(i==w.inventorySelection){
            color(renderer,253,222,112);
            SDL_FRect chosen{x-2.f,y-2.f,56.f,56.f};
            SDL_RenderRect(renderer,&chosen);
        }
    }
    constexpr std::array<const char*,6> labels{{
        "WEAPON","SHIELD","HELM","ARMOR","BOOTS","OTHER"
    }};
    for(int i=0;i<6;i++){
        const float x=639.f+float(i%3)*79.f,y=230.f+float(i/3)*79.f;
        uiBox(renderer,x,y,68.f,68.f);
        itemIcon(renderer,art,w.equipped[size_t(i)],x+9.f,y+5.f,50.f);
        fillText(renderer,labels[size_t(i)],int(x+2),int(y+55),1,201,180,127);
    }
    uiBox(renderer,78.f,416.f,464.f,86.f);
    const int selected=w.bag[size_t(w.inventorySelection)];
    fillText(renderer,itemView(selected).name,94,434,3,243,216,151);
    fillText(renderer,selected?(itemView(selected).consumable?"E: USE ITEM":"E: EQUIP ITEM"):
             "EMPTY SLOT",94,464,2,221,206,180);
    fillText(renderer,"GOLD: "+std::to_string(w.gold),645,393,2,241,203,114);
    fillText(renderer,"ATK: "+std::to_string(equipment::attack(w.equipped)),
             644,430,2,240,183,120);
    fillText(renderer,"DEF: "+std::to_string(equipment::defense(w.equipped)),
             644,458,2,158,212,238);
    const auto& detail=itemView(selected);
    if(selected>0 && !detail.consumable){
        fillText(renderer,"ATK +"+std::to_string(detail.attack)+
                 "  DEF +"+std::to_string(detail.defense),94,484,1,235,203,143);
    }
    fillText(renderer,"B / I: CLOSE     ARROWS: SELECT     E: USE / EQUIP",
             91,508,1,215,198,159);
    drawQuickbar(renderer,w,art);
}

