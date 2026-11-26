#pragma once
#include <array>
#include <algorithm>

// Pure C++ equipment rules; also used by the standalone game tests.
// Item IDs 1..16 are kept for compatibility with the existing item atlas.
namespace equipment {
struct Item {
    const char* name;
    int iconColumn,iconRow;
    int slot;        // 0 weapon, 1 shield, 2 helm, 3 armor, 4 boots, 5 accessory
    int attack,defense;
    int heal;        // health restored when consumed; 0 means not a health item
    bool consumable;
};
inline constexpr std::array<Item,19> ITEMS{{
    {"EMPTY",0,0,-1,0,0,0,false},
    {"IRON SWORD",0,0,0,8,0,0,false},
    {"BATTLE AXE",1,0,0,11,0,0,false},
    {"HUNTING BOW",2,0,0,7,0,0,false},
    {"OAK STAFF",3,0,0,6,0,0,false},
    {"ROYAL SHIELD",4,0,1,0,5,0,false},
    {"STEEL HELM",5,0,2,0,3,0,false},
    {"CHAIN ARMOR",0,1,3,0,6,0,false},
    {"LEATHER BOOTS",1,1,4,0,2,0,false},
    {"WORK GLOVES",2,1,5,0,1,0,false},
    {"SILVER RING",3,1,5,1,1,0,false},
    {"RUBY AMULET",4,1,5,2,1,0,false},
    {"HEALTH POTION",5,1,-1,0,0,40,true},
    {"MANA POTION",0,2,-1,0,0,0,true},
    {"HERBS",1,2,-1,0,0,15,true},
    {"BREAD",2,2,-1,0,0,12,true},
    {"APPLE",3,2,-1,0,0,8,true},
    {"FATHER'S CHIPPED SWORD",0,0,0,3,0,0,false}, // weathered starter blade
    {"PLAIN TUNIC",0,1,3,0,1,0,false} // reuses armor icon
}};
inline constexpr const Item& item(int id){
    return ITEMS[static_cast<size_t>(id>=0 && id<static_cast<int>(ITEMS.size())?id:0)];
}
inline constexpr int BASE_ATTACK=2;
inline constexpr int BASE_DEFENSE=0;
inline constexpr int ENEMY_ATTACK=14;
inline constexpr int ENEMY_DEFENSE=1;
inline constexpr int ENEMY_HEALTH=10;
inline int attack(const std::array<int,6>& worn){
    int result=BASE_ATTACK;
    for(size_t i=0;i<worn.size();i++){
        const auto& data=item(worn[i]);
        if(data.slot==static_cast<int>(i))result+=data.attack;
    }
    return result;
}
inline int defense(const std::array<int,6>& worn){
    int result=BASE_DEFENSE;
    for(size_t i=0;i<worn.size();i++){
        const auto& data=item(worn[i]);
        if(data.slot==static_cast<int>(i))result+=data.defense;
    }
    return result;
}
inline constexpr int hitDamage(int attackValue,int defenseValue){
    // Even a well-armored target takes at least one damage.
    return std::max(1,attackValue-std::max(0,defenseValue));
}
inline bool equip(std::array<int,24>& bag,std::array<int,6>& worn,int bagIndex){
    if(bagIndex<0||bagIndex>=static_cast<int>(bag.size()))return false;
    const int id=bag[static_cast<size_t>(bagIndex)];
    const int slot=item(id).slot;
    if(slot<0||slot>=static_cast<int>(worn.size()))return false;
    std::swap(bag[static_cast<size_t>(bagIndex)],worn[static_cast<size_t>(slot)]);
    return true;
}
inline bool addToBag(std::array<int,24>& bag,int id){
    if(id<=0||id>=static_cast<int>(ITEMS.size()))return false;
    for(int& current:bag)if(current==0){current=id;return true;}
    return false;
}
} // namespace equipment
