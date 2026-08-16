// mob.h - 生物系统：猪/牛/羊/僵尸（漫步 AI、追击、攻击、掉落）
#pragma once
#include "raylib.h"
#include <vector>

struct World; // 前置声明（避免循环依赖）

enum MobType : int {
    MOB_NONE = 0,
    MOB_PIG = 1,
    MOB_COW = 2,
    MOB_SHEEP = 3,
    MOB_ZOMBIE = 4,
};

// 碰撞箱尺寸（MC 参考：猪/牛/羊 0.9 宽，牛/羊 1.3 高，僵尸 0.6x1.8）
inline float mobWidth(MobType t) {
    switch (t) {
        case MOB_ZOMBIE: return 0.6f;
        default:         return 0.9f;
    }
}
inline float mobHeight(MobType t) {
    switch (t) {
        case MOB_COW:
        case MOB_SHEEP:  return 1.3f;
        case MOB_ZOMBIE: return 1.8f;
        default:         return 0.9f;
    }
}
inline float mobMaxHp(MobType t) {
    switch (t) {
        case MOB_SHEEP:  return 8.0f;
        case MOB_ZOMBIE: return 20.0f;
        default:         return 10.0f;
    }
}
inline bool mobIsHostile(MobType t) { return t == MOB_ZOMBIE; }

constexpr float MOB_REACH = 5.0f;   // 攻击距离（与方块拾取一致）

struct Mob {
    Vector3 pos;        // 脚底中心位置
    Vector3 vel;
    float yaw;          // 朝向（弧度，0=+Z）
    MobType type;
    float hp;           // 当前生命值
    float maxHp;
    bool onGround;
    float fallDistance; // 坠落距离（计算摔落伤害）

    // ---- AI 目标点漫步（仿 MC 被动生物 WanderGoal + PanicGoal）----
    float thinkTimer;   // 休息/重选目标计时器
    float panicTimer;   // 恐慌（被攻击逃跑）剩余时间
    float stuckTimer;   // 卡住不动累计时间
    int tx, tz;         // 当前目标方块坐标（xz）
    bool hasTarget;     // 是否正在走向目标
    float bobPhase;     // 走路身体起伏相位
    float hurtFlash;    // 受伤红闪剩余时间（秒）
    float attackCooldown; // 攻击冷却（僵尸）

    bool dead;
};

// 碰撞箱辅助（脚底 pos，中心 pos.y + mobHeight/2）
inline float mobMinX(const Mob& m) { return m.pos.x - mobWidth(m.type) / 2; }
inline float mobMaxX(const Mob& m) { return m.pos.x + mobWidth(m.type) / 2; }
inline float mobMinY(const Mob& m) { return m.pos.y; }
inline float mobMaxY(const Mob& m) { return m.pos.y + mobHeight(m.type); }
inline float mobMinZ(const Mob& m) { return m.pos.z - mobWidth(m.type) / 2; }
inline float mobMaxZ(const Mob& m) { return m.pos.z + mobWidth(m.type) / 2; }

struct MobWorld {
    std::vector<Mob> mobs;
    float spawnTimer;      // 被动生物生成尝试计时器
    float zombieSpawnTimer;// 僵尸生成尝试计时器
    int maxMobs;           // 被动生物（猪/牛/羊）数量上限
    int maxZombies;        // 僵尸数量上限
};

// 初始化生物世界
void mobInit(MobWorld& mw, int maxMobs, int maxZombies);

// 每帧更新：生成、AI、物理、清理。
// isNight：当前是否夜晚（僵尸夜间追击玩家）。
// 返回本帧敌对生物对玩家造成的总伤害。
float mobUpdate(MobWorld& mw, World& w, Vector3 playerPos, float dt, bool isNight);

// 绘制所有生物（需在 BeginMode3D 内调用）
void mobDraw(const MobWorld& mw);

// 视线与生物 AABB 相交检测（origin 起点, dir 方向, maxDist 最大距离）
// 返回命中的生物索引，未命中返回 -1
int mobRaycast(const MobWorld& mw, Vector3 origin, Vector3 dir, float maxDist);

// 对生物造成伤害。死亡时掉落物品并从世界移除。
// 返回 true 表示该生物死亡。
bool mobHurt(MobWorld& mw, World& w, int idx, float dmg);

// 生物名称
const char* mobName(MobType t);
