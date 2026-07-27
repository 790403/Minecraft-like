// mob.h - stub (reserved for future use)
#pragma once
#include "raylib.h"
#include <vector>
enum MobType { MOB_NONE=0 };
struct Mob { Vector3 pos; int type; };
struct EntityWorld { std::vector<Mob> mobs; };
inline void entityInit(EntityWorld&, unsigned int) {}
inline void entityUpdate(EntityWorld&, Vector3, float, float) {}
inline void entityDraw(const EntityWorld&) {}
inline int entityRaycast(const EntityWorld&, Vector3, Vector3, float, float&) { return -1; }
inline int mobDropItem(int) { return 0; }
inline const char* mobName(int) { return ""; }
