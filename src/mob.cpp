// mob.cpp - 生物系统实现（猪/牛/羊：漫步+恐慌；僵尸：夜间追击攻击）
#include "mob.h"
#include "world.h"
#include "drop.h"
#include "block.h"
#include "rlgl.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

static const float MOB_GRAVITY = 26.0f;          // 与玩家一致
static const float MOB_FALL_DAMAGE_BASE = 3.0f;  // 超过 3 格开始摔伤
static const float MOB_UNLOAD_DIST = 100.0f;     // 距玩家过远自动清除

// ========== 辅助 ==========
static bool mobInWater(const World& w, const Mob& m) {
    Vector3 body = { m.pos.x, m.pos.y + 0.4f, m.pos.z };
    return worldGetBlock(w, (int)floorf(body.x), (int)floorf(body.y), (int)floorf(body.z)) == BLOCK_WATER;
}

// AABB 与方块碰撞
static bool collidesAt(const World& w, const Mob& m, Vector3 testPos) {
    float hw = mobWidth(m.type) / 2;
    int x0 = (int)floorf(testPos.x - hw);
    int x1 = (int)floorf(testPos.x + hw);
    int y0 = (int)floorf(testPos.y);
    int y1 = (int)floorf(testPos.y + mobHeight(m.type));
    int z0 = (int)floorf(testPos.z - hw);
    int z1 = (int)floorf(testPos.z + hw);
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                if (isSolid(worldGetBlock(w, x, y, z))) return true;
    return false;
}

// ========== 初始化 ==========
void mobInit(MobWorld& mw, int maxMobs, int maxZombies) {
    mw.mobs.clear();
    mw.spawnTimer = 1.0f;
    mw.zombieSpawnTimer = 6.0f;
    mw.maxMobs = maxMobs;
    mw.maxZombies = maxZombies;
}

// ========== 生成 ==========
static Mob makeMob(MobType t, Vector3 pos) {
    Mob m = {};
    m.type = t;
    m.pos = pos;
    m.vel = { 0, 0, 0 };
    m.yaw = (float)rand() / RAND_MAX * 2.0f * PI;
    m.hp = mobMaxHp(t);
    m.maxHp = mobMaxHp(t);
    m.onGround = false;
    m.fallDistance = 0.0f;
    m.thinkTimer = 1.0f + (float)rand() / RAND_MAX * 2.0f;
    m.panicTimer = 0.0f;
    m.stuckTimer = 0.0f;
    m.tx = m.tz = 0;
    m.hasTarget = false;
    m.bobPhase = 0.0f;
    m.hurtFlash = 0.0f;
    m.attackCooldown = 0.0f;
    m.dead = false;
    return m;
}

// 检查某位置是否能容纳生物（地面可站、上方有空间、不与其他生物重叠）
static bool spawnPosOk(const MobWorld& mw, const World& w, int x, int z, int gy) {
    if (gy <= 1 || gy >= CHUNK_Y - 2) return false;
    BlockType ground = worldGetBlock(w, x, gy - 1, z);
    if (ground == BLOCK_WATER || ground == BLOCK_LEAVES) return false;
    if (!isSolid(ground)) return false;
    if (worldGetBlock(w, x, gy, z) != BLOCK_AIR) return false;
    if (worldGetBlock(w, x, gy + 1, z) != BLOCK_AIR) return false;
    for (const auto& o : mw.mobs) {
        float d = Vector2Distance({ o.pos.x, o.pos.z }, { (float)x + 0.5f, (float)z + 0.5f });
        if (d < 1.5f) return false;
    }
    return true;
}

// 被动生物（猪/牛/羊）：玩家周围 20~40 格环带，草/沙/泥土/圆石地表
static bool trySpawnPassive(MobWorld& mw, World& w, Vector3 playerPos) {
    if ((int)mw.mobs.size() >= mw.maxMobs) return false;

    float ang = (float)rand() / RAND_MAX * 2.0f * PI;
    float rad = 20.0f + (float)rand() / RAND_MAX * 20.0f;
    int x = (int)floorf(playerPos.x + cosf(ang) * rad);
    int z = (int)floorf(playerPos.z + sinf(ang) * rad);

    int gy = worldFindGroundY(w, x, z);
    BlockType ground = worldGetBlock(w, x, gy - 1, z);
    if (ground != BLOCK_GRASS && ground != BLOCK_SAND
        && ground != BLOCK_DIRT && ground != BLOCK_COBBLE) return false;
    if (!spawnPosOk(mw, w, x, z, gy)) return false;

    MobType t = (MobType)(MOB_PIG + rand() % 3); // 猪/牛/羊
    mw.mobs.push_back(makeMob(t, { (float)x + 0.5f, (float)gy, (float)z + 0.5f }));
    return true;
}

// 僵尸：夜晚在玩家周围 24~40 格，任意实体地面
static void trySpawnZombie(MobWorld& mw, World& w, Vector3 playerPos) {
    int zombieCount = 0;
    for (const auto& m : mw.mobs)
        if (m.type == MOB_ZOMBIE) ++zombieCount;
    if (zombieCount >= mw.maxZombies) return;

    float ang = (float)rand() / RAND_MAX * 2.0f * PI;
    float rad = 24.0f + (float)rand() / RAND_MAX * 16.0f;
    int x = (int)floorf(playerPos.x + cosf(ang) * rad);
    int z = (int)floorf(playerPos.z + sinf(ang) * rad);

    int gy = worldFindGroundY(w, x, z);
    if (!spawnPosOk(mw, w, x, z, gy)) return;

    mw.mobs.push_back(makeMob(MOB_ZOMBIE, { (float)x + 0.5f, (float)gy, (float)z + 0.5f }));
}

// ========== 地面检测 ==========
// 从 yStart 向下找最近的实体方块顶面（最多扫 8 格），找不到返回 -1
static int groundBelow(const World& w, int x, int z, int yStart) {
    for (int y = yStart; y >= yStart - 8; --y) {
        if (isSolid(worldGetBlock(w, x, y, z))) return y + 1;
    }
    return -1;
}

// ========== 目标选择（仿 MC WanderGoal / PanicGoal）==========
// 正常漫步：当前位置 ±7 格内随机；恐慌：远离玩家方向 5~10 格
// 目标必须可站立：有实体地面、上方有空间、高度差在 3 格内（恐慌无视）
static void mobPickTarget(World& w, Mob& m, Vector3 playerPos) {
    int here = groundBelow(w, (int)floorf(m.pos.x), (int)floorf(m.pos.z), (int)floorf(m.pos.y));
    for (int attempt = 0; attempt < 8; ++attempt) {
        int tx, tz;
        if (m.panicTimer > 0.0f) {
            // 恐慌：远离玩家方向，带随机摆动
            float ang = atan2f(m.pos.z - playerPos.z, m.pos.x - playerPos.x)
                        + ((float)rand() / RAND_MAX - 0.5f) * 0.8f;
            float rad = 5.0f + (float)rand() / RAND_MAX * 5.0f;
            tx = (int)floorf(m.pos.x + cosf(ang) * rad);
            tz = (int)floorf(m.pos.z + sinf(ang) * rad);
        } else {
            tx = (int)floorf(m.pos.x) + (rand() % 15) - 7; // ±7 格
            tz = (int)floorf(m.pos.z) + (rand() % 15) - 7;
        }
        // 目标列必须可站立：有实体地面、上方有空间
        int gy = groundBelow(w, tx, tz, (int)floorf(m.pos.y) + 3);
        if (gy < 0) continue;
        BlockType g = worldGetBlock(w, tx, gy - 1, tz);
        if (g == BLOCK_WATER || g == BLOCK_LEAVES) continue;
        if (worldGetBlock(w, tx, gy, tz) != BLOCK_AIR) continue;
        if (worldGetBlock(w, tx, gy + 1, tz) != BLOCK_AIR) continue;
        // 正常漫步不走太高/太低的目标（恐慌中无视高度差）
        if (m.panicTimer <= 0.0f && here >= 0 && (gy > here + 3 || gy < here - 3)) continue;
        // 别选脚边目标
        if (abs(tx - (int)floorf(m.pos.x)) + abs(tz - (int)floorf(m.pos.z)) < 2) continue;
        m.tx = tx;
        m.tz = tz;
        m.hasTarget = true;
        m.stuckTimer = 0.0f;
        return;
    }
    // 全部失败：原地站一会儿再试
    m.hasTarget = false;
    m.thinkTimer = 1.0f + (float)rand() / RAND_MAX * 2.0f;
}

// ========== 死亡掉落 ==========
static void dropMobLoot(World& w, const Mob& m) {
    Vector3 center = { m.pos.x, m.pos.y + mobHeight(m.type) / 2, m.pos.z };
    switch (m.type) {
        case MOB_PIG:
            dropSpawn(w.drops, center, ITEM_RAW_PORK, 1 + rand() % 3);
            break;
        case MOB_COW:
            dropSpawn(w.drops, center, ITEM_RAW_BEEF, 1 + rand() % 3);
            break;
        case MOB_SHEEP:
            dropSpawn(w.drops, center, ITEM_WOOL, 1);
            dropSpawn(w.drops, center, ITEM_RAW_MUTTON, 1 + rand() % 2);
            break;
        case MOB_ZOMBIE: {
            int r = rand() % 100;
            if (r < 50)      dropSpawn(w.drops, center, ITEM_ROTTEN_FLESH, 1);
            else if (r < 75) dropSpawn(w.drops, center, ITEM_ROTTEN_FLESH, 2);
            break;
        }
        default: break;
    }
}

// ========== 每帧更新 ==========
float mobUpdate(MobWorld& mw, World& w, Vector3 playerPos, float dt, bool isNight) {
    float damage = 0.0f;

    // ---- 生成 ----
    mw.spawnTimer -= dt;
    if (mw.spawnTimer <= 0.0f) {
        mw.spawnTimer = 2.0f + (float)rand() / RAND_MAX * 2.0f; // 2~4 秒
        // 每次多试几个位置提高成功率（每轮最多生成 2 只）
        int spawned = 0;
        for (int k = 0; k < 3 && spawned < 2; ++k)
            if (trySpawnPassive(mw, w, playerPos)) ++spawned;
    }
    mw.zombieSpawnTimer -= dt;
    if (mw.zombieSpawnTimer <= 0.0f) {
        mw.zombieSpawnTimer = 5.0f + (float)rand() / RAND_MAX * 3.0f;
        if (isNight) trySpawnZombie(mw, w, playerPos);
    }

    // ---- 逐个更新 ----
    for (size_t i = 0; i < mw.mobs.size(); ) {
        Mob& m = mw.mobs[i];

        // 清理：死亡 / 掉出世界 / 距玩家过远
        float hDist = Vector2Distance({ m.pos.x, m.pos.z }, { playerPos.x, playerPos.z });
        if (m.dead || m.pos.y < -10.0f || hDist > MOB_UNLOAD_DIST) {
            // 自然死亡（摔死等）也要掉战利品
            if (!m.dead && m.hp <= 0.0f) dropMobLoot(w, m);
            m = mw.mobs.back();
            mw.mobs.pop_back();
            continue;
        }
        if (m.hp <= 0.0f) {
            dropMobLoot(w, m); // 摔死等自然死亡：同样掉落
            m = mw.mobs.back();
            mw.mobs.pop_back();
            continue;
        }

        m.hurtFlash -= dt;
        if (m.panicTimer > 0.0f) m.panicTimer -= dt;

        bool hostile = mobIsHostile(m.type);
        // 僵尸：夜晚追击玩家（白天漫游）
        bool chase = hostile && isNight;
        if (chase) {
            float pd = Vector2Distance({ m.pos.x, m.pos.z }, { playerPos.x, playerPos.z });
            if (pd < 20.0f) {
                m.hasTarget = true;
                m.tx = (int)floorf(playerPos.x);
                m.tz = (int)floorf(playerPos.z);
                m.stuckTimer = 0.0f;
            } else if (pd > 28.0f) {
                m.hasTarget = false;
            }
            // 近身攻击
            m.attackCooldown -= dt;
            float hd = Vector2Distance({ m.pos.x, m.pos.z }, { playerPos.x, playerPos.z });
            bool vertOverlap = m.pos.y < playerPos.y + 1.8f
                            && m.pos.y + mobHeight(m.type) > playerPos.y;
            if (hd < 1.3f && vertOverlap && m.attackCooldown <= 0.0f) {
                damage += 3.0f; // 每次攻击 3 点伤害（MC 普通难度）
                m.attackCooldown = 1.0f;
            }
        }

        // ---- AI：目标点漫步 ----
        if (m.hasTarget && !chase) {
            // 到达目标：站一会儿再选新目标
            float dx = m.tx + 0.5f - m.pos.x;
            float dz = m.tz + 0.5f - m.pos.z;
            if (sqrtf(dx * dx + dz * dz) < 0.5f) {
                m.hasTarget = false;
                m.thinkTimer = 1.0f + (float)rand() / RAND_MAX * 2.0f;
            }
        } else if (!m.hasTarget) {
            // 休息结束 → 选新目标
            m.thinkTimer -= dt;
            if (m.thinkTimer <= 0.0f) mobPickTarget(w, m, playerPos);
        }

        Vector3 prevPos = m.pos;

        // ---- 移动（朝目标）----
        if (m.hasTarget) {
            float dx = m.tx + 0.5f - m.pos.x;
            float dz = m.tz + 0.5f - m.pos.z;
            float hDist = sqrtf(dx * dx + dz * dz);
            if (hDist > 0.001f) {
                // 悬崖回避（仿 MC：寻路不选悬空节点；水中/追击中跳过）
                if (!mobInWater(w, m) && !chase) {
                    int hereY = groundBelow(w, (int)floorf(m.pos.x), (int)floorf(m.pos.z), (int)floorf(m.pos.y));
                    int fwdY = groundBelow(w, (int)floorf(m.pos.x + dx / hDist * 0.7f),
                                             (int)floorf(m.pos.z + dz / hDist * 0.7f),
                                             (int)floorf(m.pos.y) + 1);
                    if (hereY >= 0 && (fwdY < 0 || fwdY < hereY - 1)) {
                        // 前方悬空/深渊 → 停下立即换目标
                        m.hasTarget = false;
                        m.thinkTimer = 0.1f;
                        hDist = 0.0f;
                    }
                }
                if (hDist > 0.001f) {
                    float speed = 0.5f;
                    if (m.panicTimer > 0.0f) speed = 1.2f; // 恐慌约 2.4 倍速
                    if (chase) speed = 0.9f;               // 僵尸追击
                    m.vel.x = dx / hDist * speed;
                    m.vel.z = dz / hDist * speed;
                    m.bobPhase += dt * 7.0f; // 走路摆动
                }
            }
        }
        if (!m.hasTarget) m.vel.x = m.vel.z = 0.0f;

        // 朝向平滑转向移动方向
        if (m.hasTarget) {
            float dx = m.tx + 0.5f - m.pos.x;
            float dz = m.tz + 0.5f - m.pos.z;
            float targetYaw = atan2f(dx, dz);
            float diff = targetYaw - m.yaw;
            while (diff > PI) diff -= 2.0f * PI;
            while (diff < -PI) diff += 2.0f * PI;
            m.yaw += diff * std::min(1.0f, dt * 6.0f);
        }

        // ---- 重力 / 浮水 ----
        if (mobInWater(w, m)) {
            // 生物会游泳：在水中固定上浮，直到露出水面
            m.vel.y = 0.8f;
            m.onGround = false;
        } else {
            m.vel.y -= MOB_GRAVITY * dt;
            if (m.vel.y < -55.0f) m.vel.y = -55.0f;
        }

        // ---- 逐轴碰撞（X→Z→Y）----
        bool hitX = false, hitZ = false;
        Vector3 delta = { m.vel.x * dt, m.vel.y * dt, m.vel.z * dt };
        m.onGround = false;
        { Vector3 t = m.pos; t.x += delta.x; if (!collidesAt(w, m, t)) m.pos.x = t.x; else { m.vel.x = 0; hitX = true; } }
        { Vector3 t = m.pos; t.z += delta.z; if (!collidesAt(w, m, t)) m.pos.z = t.z; else { m.vel.z = 0; hitZ = true; } }
        { Vector3 t = m.pos; t.y += delta.y; if (!collidesAt(w, m, t)) m.pos.y = t.y; else { if (delta.y < 0) m.onGround = true; m.vel.y = 0; } }

        // ---- 上台阶（仿 MC step height 1：前方被挡且高一格可通行）----
        if ((hitX || hitZ) && m.hasTarget) {
            Vector3 up = m.pos;
            up.y += 1.0f;
            if (!collidesAt(w, m, up)) {
                // 抬上去后脚底需有支撑（不能浮空）
                int sy = (int)floorf(up.y - 0.05f);
                if (isSolid(worldGetBlock(w, (int)floorf(up.x), sy, (int)floorf(up.z)))) {
                    m.pos = up;
                    // 抬上去后重试一次水平移动（走完整台阶）
                    { Vector3 t = m.pos; t.x += delta.x; if (!collidesAt(w, m, t)) m.pos.x = t.x; }
                    { Vector3 t = m.pos; t.z += delta.z; if (!collidesAt(w, m, t)) m.pos.z = t.z; }
                }
            }
        }

        // ---- 卡住检测：想走却没动超过 1.5 秒 → 换目标（防卡墙角）----
        if (m.hasTarget) {
            float moved = Vector2Distance({ m.pos.x, m.pos.z }, { prevPos.x, prevPos.z });
            if (moved < 0.01f) {
                m.stuckTimer += dt;
                if (m.stuckTimer > 1.5f) {
                    m.hasTarget = false;
                    m.thinkTimer = 0.1f;
                    m.stuckTimer = 0.0f;
                }
            } else {
                m.stuckTimer = 0.0f;
            }
        } else {
            m.stuckTimer = 0.0f;
        }

        // ---- 摔落伤害（MC 公式）----
        if (!m.onGround) {
            m.fallDistance += -m.vel.y * dt;
        } else {
            if (m.fallDistance > MOB_FALL_DAMAGE_BASE) {
                int e = (int)std::round(m.fallDistance - MOB_FALL_DAMAGE_BASE);
                if (e > 0) m.hp -= (float)e;
            }
            m.fallDistance = 0.0f;
        }

        // ---- 玩家推开（防止互相穿模）----
        {
            Vector3 pc = { playerPos.x, playerPos.y + 0.9f, playerPos.z }; // 玩家身体中心
            Vector3 mc = { m.pos.x, m.pos.y + mobHeight(m.type) / 2, m.pos.z };
            float dx = mc.x - pc.x, dz = mc.z - pc.z;
            float hd = sqrtf(dx * dx + dz * dz);
            float overlap = 0.75f - hd; // 0.45+0.3 - 距离
            if (overlap > 0.0f && mc.y > playerPos.y && mc.y < playerPos.y + 1.8f) {
                if (hd > 0.001f) {
                    m.pos.x += dx / hd * overlap;
                    m.pos.z += dz / hd * overlap;
                } else {
                    m.pos.x += 0.5f * overlap; // 完全重叠时往 +X 推
                }
            }
        }

        ++i;
    }
    return damage;
}

// ========== 视线与生物相交 ==========
int mobRaycast(const MobWorld& mw, Vector3 origin, Vector3 dir, float maxDist) {
    int best = -1;
    float bestT = maxDist;
    for (size_t i = 0; i < mw.mobs.size(); ++i) {
        const Mob& m = mw.mobs[i];
        float hw = mobWidth(m.type) / 2;
        Vector3 min = { m.pos.x - hw, m.pos.y, m.pos.z - hw };
        Vector3 max = { m.pos.x + hw, m.pos.y + mobHeight(m.type), m.pos.z + hw };

        // slab 法：射线与 AABB 求交
        float tmin = 0.0f, tmax = maxDist;
        if (fabsf(dir.x) < 1e-6f) {
            if (origin.x < min.x || origin.x > max.x) continue;
        } else {
            float t1 = (min.x - origin.x) / dir.x;
            float t2 = (max.x - origin.x) / dir.x;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            tmin = fmaxf(tmin, t1);
            tmax = fminf(tmax, t2);
        }
        if (fabsf(dir.y) < 1e-6f) {
            if (origin.y < min.y || origin.y > max.y) continue;
        } else {
            float t1 = (min.y - origin.y) / dir.y;
            float t2 = (max.y - origin.y) / dir.y;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            tmin = fmaxf(tmin, t1);
            tmax = fminf(tmax, t2);
        }
        if (fabsf(dir.z) < 1e-6f) {
            if (origin.z < min.z || origin.z > max.z) continue;
        } else {
            float t1 = (min.z - origin.z) / dir.z;
            float t2 = (max.z - origin.z) / dir.z;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            tmin = fmaxf(tmin, t1);
            tmax = fminf(tmax, t2);
        }
        if (tmax >= tmin && tmax >= 0.0f && tmin < bestT) {
            bestT = tmin;
            best = (int)i;
        }
    }
    return best;
}

// ========== 受伤 / 死亡 ==========
bool mobHurt(MobWorld& mw, World& w, int idx, float dmg) {
    if (idx < 0 || idx >= (int)mw.mobs.size()) return false;
    Mob& m = mw.mobs[idx];
    if (m.dead) return false;
    m.hp -= dmg;
    m.hurtFlash = 0.25f;
    // 被动生物被攻击会逃跑 5 秒（僵尸不逃跑）
    if (!mobIsHostile(m.type)) m.panicTimer = 5.0f;
    if (m.hp <= 0.0f) {
        m.dead = true;
        dropMobLoot(w, m);
        m = mw.mobs.back();
        mw.mobs.pop_back();
        return true;
    }
    return false;
}

const char* mobName(MobType t) {
    switch (t) {
        case MOB_PIG:    return "猪";
        case MOB_COW:    return "牛";
        case MOB_SHEEP:  return "羊";
        case MOB_ZOMBIE: return "僵尸";
        default:         return "未知生物";
    }
}

// ========== 渲染 ==========
// 画一个六面明暗的彩色方块（顶 100% / 侧 72% / 底 45%）
static void drawColoredBox(Vector3 center, Vector3 size, Color base) {
    float hx = size.x / 2, hy = size.y / 2, hz = size.z / 2;
    Color cTop = { (unsigned char)(base.r), (unsigned char)(base.g), (unsigned char)(base.b), 255 };
    Color cSide = { (unsigned char)(base.r * 0.72f), (unsigned char)(base.g * 0.72f), (unsigned char)(base.b * 0.72f), 255 };
    Color cBot  = { (unsigned char)(base.r * 0.45f), (unsigned char)(base.g * 0.45f), (unsigned char)(base.b * 0.45f), 255 };

    // 前 (+Z)
    rlBegin(RL_QUADS); rlColor4ub(cSide.r, cSide.g, cSide.b, 255);
    rlVertex3f(center.x - hx, center.y - hy, center.z + hz); rlVertex3f(center.x + hx, center.y - hy, center.z + hz);
    rlVertex3f(center.x + hx, center.y + hy, center.z + hz); rlVertex3f(center.x - hx, center.y + hy, center.z + hz); rlEnd();
    // 后 (-Z)
    rlBegin(RL_QUADS); rlColor4ub(cSide.r, cSide.g, cSide.b, 255);
    rlVertex3f(center.x + hx, center.y - hy, center.z - hz); rlVertex3f(center.x - hx, center.y - hy, center.z - hz);
    rlVertex3f(center.x - hx, center.y + hy, center.z - hz); rlVertex3f(center.x + hx, center.y + hy, center.z - hz); rlEnd();
    // 上 (+Y)
    rlBegin(RL_QUADS); rlColor4ub(cTop.r, cTop.g, cTop.b, 255);
    rlVertex3f(center.x - hx, center.y + hy, center.z + hz); rlVertex3f(center.x + hx, center.y + hy, center.z + hz);
    rlVertex3f(center.x + hx, center.y + hy, center.z - hz); rlVertex3f(center.x - hx, center.y + hy, center.z - hz); rlEnd();
    // 下 (-Y)
    rlBegin(RL_QUADS); rlColor4ub(cBot.r, cBot.g, cBot.b, 255);
    rlVertex3f(center.x - hx, center.y - hy, center.z - hz); rlVertex3f(center.x + hx, center.y - hy, center.z - hz);
    rlVertex3f(center.x + hx, center.y - hy, center.z + hz); rlVertex3f(center.x - hx, center.y - hy, center.z + hz); rlEnd();
    // 左 (-X)
    rlBegin(RL_QUADS); rlColor4ub(cSide.r, cSide.g, cSide.b, 255);
    rlVertex3f(center.x - hx, center.y - hy, center.z - hz); rlVertex3f(center.x - hx, center.y - hy, center.z + hz);
    rlVertex3f(center.x - hx, center.y + hy, center.z + hz); rlVertex3f(center.x - hx, center.y + hy, center.z - hz); rlEnd();
    // 右 (+X)
    rlBegin(RL_QUADS); rlColor4ub(cSide.r, cSide.g, cSide.b, 255);
    rlVertex3f(center.x + hx, center.y - hy, center.z + hz); rlVertex3f(center.x + hx, center.y - hy, center.z - hz);
    rlVertex3f(center.x + hx, center.y + hy, center.z - hz); rlVertex3f(center.x + hx, center.y + hy, center.z + hz); rlEnd();
}

// 生物整体框架：绕 Y 旋转朝向，受伤闪红，走路起伏
// 回调函数绘制各部件（部件坐标以脚底中心为原点，+Z 为前方）
static void drawMobBase(const Mob& m, void (*drawParts)(const Mob&, float bob, Color body)) {
    rlPushMatrix();
    rlTranslatef(m.pos.x, m.pos.y, m.pos.z);
    rlRotatef(m.yaw * RAD2DEG, 0, 1, 0);

    // 受伤红闪
    bool flash = m.hurtFlash > 0.0f && (int)(m.hurtFlash * 20.0f) % 2 == 0;
    float bob = 0.0f;
    if (m.hasTarget) bob = fabsf(sinf(m.bobPhase)) * 0.06f;

    Color body = flash ? (Color){ 255, 110, 110, 255 } : (Color){ 255, 255, 255, 255 };
    drawParts(m, bob, body);

    rlPopMatrix();
}

// 红色覆盖：受伤时把部件染红（在 drawParts 内部用 blend 处理）
static Color hurtBlend(Color base, Color body) {
    if (body.g < 200) {
        // 闪红：向红色混合
        float f = 0.6f;
        return { (unsigned char)(base.r + (255 - base.r) * f),
                 (unsigned char)(base.g * (1.0f - f)),
                 (unsigned char)(base.b * (1.0f - f)), 255 };
    }
    return base;
}

// ---- 猪：身体 + 头 + 鼻子 + 耳朵 + 眼睛 + 4 腿 ----
static void drawPigParts(const Mob& m, float bob, Color blend) {
    const Color BODY = { 244, 168, 174, 255 };
    const Color NOSE = { 206, 122, 132, 255 };
    const Color EYE  = { 30, 20, 20, 255 };
    Color body = hurtBlend(BODY, blend);
    Color nose = hurtBlend(NOSE, blend);
    drawColoredBox({ 0, 0.45f + bob, 0 }, { 0.7f, 0.7f, 1.1f }, body);
    drawColoredBox({ 0, 0.75f + bob, 0.68f }, { 0.5f, 0.5f, 0.5f }, body);
    drawColoredBox({ 0, 0.72f + bob, 0.98f }, { 0.16f, 0.14f, 0.1f }, nose);
    drawColoredBox({ -0.28f, 0.98f + bob, 0.62f }, { 0.12f, 0.14f, 0.06f }, body);
    drawColoredBox({ 0.28f, 0.98f + bob, 0.62f }, { 0.12f, 0.14f, 0.06f }, body);
    drawColoredBox({ -0.24f, 0.78f + bob, 0.75f }, { 0.05f, 0.05f, 0.03f }, EYE);
    drawColoredBox({ 0.24f, 0.78f + bob, 0.75f }, { 0.05f, 0.05f, 0.03f }, EYE);
    drawColoredBox({ 0.18f, 0.2f, 0.35f }, { 0.25f, 0.4f, 0.25f }, body);
    drawColoredBox({ -0.18f, 0.2f, 0.35f }, { 0.25f, 0.4f, 0.25f }, body);
    drawColoredBox({ 0.18f, 0.2f, -0.35f }, { 0.25f, 0.4f, 0.25f }, body);
    drawColoredBox({ -0.18f, 0.2f, -0.35f }, { 0.25f, 0.4f, 0.25f }, body);
}

// ---- 牛：身体 + 头 + 角 + 鼻子 + 4 腿 ----
static void drawCowParts(const Mob& m, float bob, Color blend) {
    const Color BODY = { 110, 75, 40, 255 };
    const Color HEAD = { 90, 60, 35, 255 };
    const Color HORN = { 210, 205, 190, 255 };
    const Color SNOT = { 150, 95, 60, 255 };
    const Color EYE  = { 25, 20, 15, 255 };
    Color body = hurtBlend(BODY, blend);
    Color head = hurtBlend(HEAD, blend);
    drawColoredBox({ 0, 0.65f + bob, 0 }, { 0.7f, 0.7f, 1.4f }, body);
    drawColoredBox({ 0, 0.95f + bob, 0.85f }, { 0.5f, 0.5f, 0.5f }, head);
    drawColoredBox({ -0.24f, 1.18f + bob, 0.78f }, { 0.08f, 0.08f, 0.2f }, HORN);
    drawColoredBox({ 0.24f, 1.18f + bob, 0.78f }, { 0.08f, 0.08f, 0.2f }, HORN);
    drawColoredBox({ 0, 0.92f + bob, 1.12f }, { 0.3f, 0.15f, 0.1f }, SNOT);
    drawColoredBox({ -0.2f, 1.02f + bob, 0.95f }, { 0.05f, 0.05f, 0.03f }, EYE);
    drawColoredBox({ 0.2f, 1.02f + bob, 0.95f }, { 0.05f, 0.05f, 0.03f }, EYE);
    drawColoredBox({ 0.2f, 0.3f, 0.45f }, { 0.25f, 0.6f, 0.25f }, body);
    drawColoredBox({ -0.2f, 0.3f, 0.45f }, { 0.25f, 0.6f, 0.25f }, body);
    drawColoredBox({ 0.2f, 0.3f, -0.45f }, { 0.25f, 0.6f, 0.25f }, body);
    drawColoredBox({ -0.2f, 0.3f, -0.45f }, { 0.25f, 0.6f, 0.25f }, body);
}

// ---- 羊：白色身体 + 头 + 耳朵 + 4 腿 ----
static void drawSheepParts(const Mob& m, float bob, Color blend) {
    const Color BODY = { 225, 225, 220, 255 };
    const Color FACE = { 195, 180, 170, 255 };
    const Color EYE  = { 30, 25, 20, 255 };
    Color body = hurtBlend(BODY, blend);
    Color face = hurtBlend(FACE, blend);
    drawColoredBox({ 0, 0.72f + bob, 0 }, { 0.9f, 0.75f, 1.2f }, body);
    drawColoredBox({ 0, 0.92f + bob, 0.78f }, { 0.45f, 0.45f, 0.45f }, face);
    drawColoredBox({ -0.26f, 1.02f + bob, 0.7f }, { 0.1f, 0.12f, 0.04f }, face);
    drawColoredBox({ 0.26f, 1.02f + bob, 0.7f }, { 0.1f, 0.12f, 0.04f }, face);
    drawColoredBox({ 0, 0.88f + bob, 1.02f }, { 0.2f, 0.12f, 0.08f }, (Color){ 165, 145, 135, 255 });
    drawColoredBox({ -0.16f, 0.97f + bob, 0.88f }, { 0.05f, 0.05f, 0.03f }, EYE);
    drawColoredBox({ 0.16f, 0.97f + bob, 0.88f }, { 0.05f, 0.05f, 0.03f }, EYE);
    drawColoredBox({ 0.3f, 0.3f, 0.42f }, { 0.2f, 0.6f, 0.2f }, body);
    drawColoredBox({ -0.3f, 0.3f, 0.42f }, { 0.2f, 0.6f, 0.2f }, body);
    drawColoredBox({ 0.3f, 0.3f, -0.42f }, { 0.2f, 0.6f, 0.2f }, body);
    drawColoredBox({ -0.3f, 0.3f, -0.42f }, { 0.2f, 0.6f, 0.2f }, body);
}

// ---- 僵尸：头 + 身体 + 2 臂 + 2 腿（绿皮肤/蓝衬衫/蓝裤）----
static void drawZombieParts(const Mob& m, float bob, Color blend) {
    const Color SKIN = { 74, 106, 66, 255 };    // 皮肤绿
    const Color SHIRT = { 55, 90, 95, 255 };    // 衬衫蓝绿
    const Color PANTS = { 45, 60, 110, 255 };   // 裤子蓝
    const Color EYE  = { 20, 20, 20, 255 };
    Color skin = hurtBlend(SKIN, blend);
    Color shirt = hurtBlend(SHIRT, blend);
    Color pants = hurtBlend(PANTS, blend);
    // 腿
    drawColoredBox({ -0.13f, 0.3f, 0 }, { 0.22f, 0.6f, 0.22f }, pants);
    drawColoredBox({ 0.13f, 0.3f, 0 }, { 0.22f, 0.6f, 0.22f }, pants);
    // 身体
    drawColoredBox({ 0, 1.0f + bob, 0 }, { 0.5f, 0.7f, 0.28f }, shirt);
    // 手臂
    drawColoredBox({ -0.37f, 1.0f + bob, 0 }, { 0.2f, 0.6f, 0.2f }, skin);
    drawColoredBox({ 0.37f, 1.0f + bob, 0 }, { 0.2f, 0.6f, 0.2f }, skin);
    // 头 + 眼睛
    drawColoredBox({ 0, 1.55f + bob, 0 }, { 0.5f, 0.5f, 0.5f }, skin);
    drawColoredBox({ -0.11f, 1.58f + bob, 0.26f }, { 0.08f, 0.08f, 0.03f }, EYE);
    drawColoredBox({ 0.11f, 1.58f + bob, 0.26f }, { 0.08f, 0.08f, 0.03f }, EYE);
}

static void drawMob(const Mob& m) {
    switch (m.type) {
        case MOB_PIG:    drawMobBase(m, drawPigParts); break;
        case MOB_COW:    drawMobBase(m, drawCowParts); break;
        case MOB_SHEEP:  drawMobBase(m, drawSheepParts); break;
        case MOB_ZOMBIE: drawMobBase(m, drawZombieParts); break;
        default: break;
    }
}

void mobDraw(const MobWorld& mw) {
    rlSetTexture(0);
    for (const auto& m : mw.mobs) {
        if (m.dead) continue;
        drawMob(m);
    }
}
