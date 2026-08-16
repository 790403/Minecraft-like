// mob.h - 生物系统：猪（后续可扩展牛/鸡/羊）
#pragma once
#include "raylib.h"
#include <vector>

struct World; // 前置声明（避免循环依赖）

enum MobType : int {
    MOB_NONE = 0,
    MOB_PIG = 1,
};

// 猪的碰撞箱尺寸（与 MC 猪一致：0.9x0.9x0.9）
constexpr float MOB_WIDTH = 0.9f;
constexpr float MOB_HEIGHT = 0.9f;
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

    bool dead;
};

// 碰撞箱辅助（脚底 pos，中心 pos.y + MOB_HEIGHT/2）
inline float mobMinX(const Mob& m) { return m.pos.x - MOB_WIDTH / 2; }
inline float mobMaxX(const Mob& m) { return m.pos.x + MOB_WIDTH / 2; }
inline float mobMinY(const Mob& m) { return m.pos.y; }
inline float mobMaxY(const Mob& m) { return m.pos.y + MOB_HEIGHT; }
inline float mobMinZ(const Mob& m) { return m.pos.z - MOB_WIDTH / 2; }
inline float mobMaxZ(const Mob& m) { return m.pos.z + MOB_WIDTH / 2; }

struct MobWorld {
    std::vector<Mob> mobs;
    float spawnTimer;   // 生成尝试计时器
    int maxMobs;        // 世界生物数量上限
};

// 初始化生物世界（maxMobs 为数量上限）
void mobInit(MobWorld& mw, int maxMobs);

// 每帧更新：生成、AI、物理、清理
void mobUpdate(MobWorld& mw, World& w, Vector3 playerPos, float dt);

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
