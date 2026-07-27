// player.h - 玩家：第一人称相机、移动物理、AABB 碰撞、方块拾取、生存属性
#pragma once
#include "raylib.h"
#include "block.h"
#include "inventory.h"
#include "crafting.h"

struct World;

constexpr float PLAYER_WIDTH = 0.6f;
constexpr float PLAYER_HEIGHT = 1.8f;
constexpr float PLAYER_EYE = 1.62f;
constexpr float MOVE_SPEED = 4.5f;
constexpr float SPRINT_SPEED = 8.0f;
constexpr float GRAVITY = 26.0f;
constexpr float JUMP_SPEED = 8.5f;
constexpr float REACH = 5.0f;
constexpr float STEP = 0.05f;

struct RaycastHit {
    bool hit;
    int bx, by, bz;
    int nx, ny, nz;
    Vector3 point;
};

struct Player {
    Vector3 pos;
    Vector3 vel;
    float yaw, pitch;
    bool onGround;

    // ---- 生存属性 ----
    float hp;            // 当前生命值 (0~20)
    float maxHp;         // 最大生命值 (20)
    float hunger;        // 饱食度 (0~20)
    float hungerTimer;   // 饱食度消耗累计器
    float fallDistance;  // 用于计算坠落伤害
    float healTimer;     // 回血计时器

    // ---- 采矿 ----
    float miningTimer;   // 挖掘计时器（按住时累计）
    int   miningTarget;  // 正在挖的方块（世界坐标编码）
    float miningDuration;// 当前目标总挖掘时间

    Camera3D camera;
    Inventory inventory;  // 背包
    CraftingGrid craftGrid; // 合成网格状态
    InventorySlot grabbed;  // 鼠标手持物品

    bool inventoryOpen;   // 背包/合成界面是否打开
    bool nearWorkbench;   // 玩家附近是否有工作台
    float pickupCooldown; // 拾取冷却（秒），防止瞬间吸走一堆
};

inline float pminX(const Player& p) { return p.pos.x - PLAYER_WIDTH / 2; }
inline float pmaxX(const Player& p) { return p.pos.x + PLAYER_WIDTH / 2; }
inline float pminY(const Player& p) { return p.pos.y; }
inline float pmaxY(const Player& p) { return p.pos.y + PLAYER_HEIGHT; }
inline float pminZ(const Player& p) { return p.pos.z - PLAYER_WIDTH / 2; }
inline float pmaxZ(const Player& p) { return p.pos.z + PLAYER_WIDTH / 2; }

void playerInit(Player& p, World& w, int spawnX, int spawnZ);
void playerUpdate(Player& p, World& w, float dt);
RaycastHit playerRaycast(const Player& p, const World& w);
Vector3 playerForward(const Player& p);

// 检查某世界坐标附近是否有工作台（3 格曼哈顿距离内）
bool playerNearWorkbench(const World& w, const Player& p);
