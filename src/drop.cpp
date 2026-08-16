// drop.cpp - 掉落物实体实现
#include "drop.h"
#include "world.h"
#include "block.h"
#include "rlgl.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

static const float DROP_GRAVITY = 15.0f;
static const float DROP_LIFETIME = 60.0f;
static const float PICKUP_DELAY = 0.5f;
static const float PICKUP_RANGE = 1.8f;
static const float ITEM_CUBE_SIZE = 0.20f;   // 方块掉落物立方体大小
static const float ITEM_FLAT_SIZE = 0.22f;    // 非方块掉落物立方体大小

void dropSpawn(std::vector<DroppedItem>& drops, Vector3 pos, int item, int count, bool randomVel, Vector3 dir) {
    if (item <= 0 || count <= 0) return;
    DroppedItem d;
    d.pos = pos;
    if (randomVel) {
        // 挖掘掉落：小幅弹出（防止弹到上方方块里）
        d.vel = {
            ((float)rand() / RAND_MAX - 0.5f) * 2.0f,
            (float)rand() / RAND_MAX * 2.0f + 0.5f,
            ((float)rand() / RAND_MAX - 0.5f) * 2.0f
        };
    } else {
        // 定向投掷（Q 键）
        float speed = 4.0f;
        float spread = 0.15f;
        d.vel = {
            dir.x * speed + ((float)rand() / RAND_MAX - 0.5f) * spread,
            dir.y * speed + ((float)rand() / RAND_MAX - 0.5f) * spread + 1.0f,
            dir.z * speed + ((float)rand() / RAND_MAX - 0.5f) * spread
        };
    }
    d.item = item;
    d.count = count;
    d.lifetime = DROP_LIFETIME;
    d.pickupDelay = PICKUP_DELAY;
    d.onGround = false;
    drops.push_back(d);
}

// 检查物品中心是否在实体方块内
static inline bool centerInSolid(const World& w, Vector3 p) {
    return isSolid(worldGetBlock(w, (int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z)));
}

void dropUpdate(std::vector<DroppedItem>& drops, float dt, const World& w) {
    for (size_t i = 0; i < drops.size(); ) {
        auto& d = drops[i];
        d.lifetime -= dt;
        if (d.lifetime <= 0.0f || d.count <= 0) {
            d = drops.back();
            drops.pop_back();
            continue;
        }
        if (d.pickupDelay > 0.0f) {
            d.pickupDelay -= dt;
            if (d.pickupDelay < 0.0f) d.pickupDelay = 0.0f;
        }

        const float h2 = ITEM_CUBE_SIZE / 2;

        // ---- 初始穿透修正（只触发一次，不持续干扰）----
        if (d.pickupDelay > PICKUP_DELAY - dt && centerInSolid(w, d.pos)) {
            int cy = (int)floorf(d.pos.y);
            d.pos.y = (float)(cy + 1) + h2 + 0.001f;
            d.vel.y = 0;
            d.onGround = false;
            d.pos.x = (float)(int)floorf(d.pos.x) + 0.5f;
            d.pos.z = (float)(int)floorf(d.pos.z) + 0.5f;
        }

        // ---- 地面支撑检测（检测物品底部下方）----
        int gx = (int)floorf(d.pos.x);
        int gy = (int)floorf(d.pos.y - h2 - 0.01f);
        int gz = (int)floorf(d.pos.z);
        bool hasSupport = isSolid(worldGetBlock(w, gx, gy, gz));

        if (hasSupport) {
            // 停留在地面
            float targetY = (float)(gy + 1) + h2 + 0.001f;
            if (d.pos.y < targetY) d.pos.y = targetY;
            d.vel.y = 0;
            d.onGround = true;
            d.vel.x *= 0.85f;
            d.vel.z *= 0.85f;
            if (fabsf(d.vel.x) < 0.005f) d.vel.x = 0;
            if (fabsf(d.vel.z) < 0.005f) d.vel.z = 0;
        } else {
            // 空中：施加重力
            d.onGround = false;
            d.vel.y -= DROP_GRAVITY * dt;
            if (d.vel.y < -30.0f) d.vel.y = -30.0f;
        }

        // ---- 移动 + 碰撞回退 ----
        Vector3 old = d.pos;
        if (!d.onGround) {
            d.pos.y += d.vel.y * dt;
        }
        d.pos.x += d.vel.x * dt;
        d.pos.z += d.vel.z * dt;

        // 新位置卡在实体方块里 → 逐轴回退
        if (centerInSolid(w, d.pos)) {
            // 回退 Y
            Vector3 tryY = { old.x, d.pos.y, old.z };
            if (centerInSolid(w, tryY)) {
                d.pos.y = old.y;
                if (d.vel.y <= 0.0f) {
                    // 下落撞到头顶方块 → 吸到该方块顶面
                    float topY = (float)((int)floorf(old.y)) + 1.0f + h2 + 0.001f;
                    d.pos.y = topY;
                    d.onGround = true;
                    d.vel.y = 0;
                } else {
                    // 上跳碰天花板 → 停住
                    d.vel.y = 0;
                }
            } else {
                d.pos.x = old.x; d.pos.z = old.z;
            }
            // 再回退 X
            if (centerInSolid(w, d.pos)) {
                Vector3 tryX = { d.pos.x, d.pos.y, old.z };
                if (centerInSolid(w, tryX)) { d.pos.x = old.x; d.vel.x = 0; }
                else { d.pos.z = old.z; d.vel.z = 0; }
            }
        }

        // ---- 兜底：仍然卡住 → 强制推到头顶 ----
        if (centerInSolid(w, d.pos)) {
            int cy = (int)floorf(d.pos.y);
            d.pos.y = (float)(cy + 1) + h2 + 0.001f;
            d.vel.y = 0;
            d.onGround = true;
        }

        if (d.pos.y < -10.0f) {
            d.lifetime = 0;
            d = drops.back();
            drops.pop_back();
            continue;
        }
        ++i;
    }
}

// 判断物品是否有完整方块形态
static bool isBlockItem(int item) {
    return item >= 1 && item <= 19;
}

// 获取物品纹理索引（方块物品返回 sideTex，其他返回专属纹理）
static int getDropTexSlot(int item) {
    if (isBlockItem(item)) {
        BlockType bt = itemToBlock(item);
        if (bt != BLOCK_AIR) return sideTex(bt);
    }
    switch (item) {
        case ITEM_COAL:         return TEX_ITEM_COAL;
        case ITEM_RAW_IRON:     return TEX_ITEM_RAW_IRON;
        case ITEM_RAW_GOLD:     return TEX_ITEM_RAW_GOLD;
        case ITEM_DIAMOND:      return TEX_ITEM_DIAMOND;
        case ITEM_STICK:        return TEX_ITEM_STICK;
        case ITEM_IRON_INGOT:   return TEX_ITEM_IRON_INGOT;
        case ITEM_GOLD_INGOT:   return TEX_ITEM_GOLD_INGOT;
        case ITEM_WOOD_PICKAXE: return TEX_ITEM_WOOD_PICKAXE;
        case ITEM_STONE_PICKAXE:return TEX_ITEM_STONE_PICKAXE;
        case ITEM_IRON_PICKAXE: return TEX_ITEM_IRON_PICKAXE;
        case ITEM_GOLD_PICKAXE: return TEX_ITEM_GOLD_PICKAXE;
        case ITEM_DIAMOND_PICKAXE:return TEX_ITEM_DIAMOND_PICKAXE;
        case ITEM_WOOD_SWORD:   return TEX_ITEM_WOOD_SWORD;
        case ITEM_STONE_SWORD:  return TEX_ITEM_STONE_SWORD;
        case ITEM_IRON_SWORD:   return TEX_ITEM_IRON_SWORD;
        case ITEM_GOLD_SWORD:   return TEX_ITEM_GOLD_SWORD;
        case ITEM_DIAMOND_SWORD:return TEX_ITEM_DIAMOND_SWORD;
        case ITEM_APPLE:        return TEX_ITEM_APPLE;
        case ITEM_BREAD:        return TEX_ITEM_BREAD;
        case ITEM_RAW_BEEF:     return TEX_ITEM_RAW_BEEF;
        case ITEM_RAW_PORK:     return TEX_ITEM_RAW_PORK;
        case ITEM_RAW_CHICKEN:  return TEX_ITEM_RAW_CHICKEN;
        case ITEM_RAW_MUTTON:   return TEX_ITEM_RAW_MUTTON;
        case ITEM_WOOL:         return TEX_ITEM_WOOL;
        case ITEM_ROTTEN_FLESH: return TEX_ITEM_ROTTEN_FLESH;
        default:                return -1;
    }
}

// 绘制方块形态掉落物（6 面不同纹理）
static void drawBlockDrop(const Vector3& pos, float angle, int item,
                           const TextureAtlas& atlas) {
    BlockType bt = itemToBlock(item);
    if (bt == BLOCK_AIR) return;

    float sU = atlas.u[sideTex(bt)], sV = atlas.v[sideTex(bt)];
    float tU = atlas.u[topTex(bt)], tV = atlas.v[topTex(bt)];
    float bU = atlas.u[bottomTex(bt)], bV = atlas.v[bottomTex(bt)];
    float tw = atlas.w, th = atlas.h;
    float hs = ITEM_CUBE_SIZE / 2;

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(angle, 0, 1, 0);
    rlRotatef(15.0f, 1, 0, 0);

    rlSetTexture(atlas.texture.id);

    // 前 (+Z)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(sU, sV+th); rlVertex3f(-hs, -hs,  hs);
    rlTexCoord2f(sU+tw, sV+th); rlVertex3f( hs, -hs,  hs);
    rlTexCoord2f(sU+tw, sV); rlVertex3f( hs,  hs,  hs);
    rlTexCoord2f(sU, sV); rlVertex3f(-hs,  hs,  hs);
    rlEnd(); }
    // 后 (-Z)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(sU, sV+th); rlVertex3f( hs, -hs, -hs);
    rlTexCoord2f(sU+tw, sV+th); rlVertex3f(-hs, -hs, -hs);
    rlTexCoord2f(sU+tw, sV); rlVertex3f(-hs,  hs, -hs);
    rlTexCoord2f(sU, sV); rlVertex3f( hs,  hs, -hs);
    rlEnd(); }
    // 上 (+Y)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(tU, tV+th); rlVertex3f(-hs,  hs,  hs);
    rlTexCoord2f(tU+tw, tV+th); rlVertex3f( hs,  hs,  hs);
    rlTexCoord2f(tU+tw, tV); rlVertex3f( hs,  hs, -hs);
    rlTexCoord2f(tU, tV); rlVertex3f(-hs,  hs, -hs);
    rlEnd(); }
    // 下 (-Y)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(bU, bV+th); rlVertex3f(-hs, -hs, -hs);
    rlTexCoord2f(bU+tw, bV+th); rlVertex3f( hs, -hs, -hs);
    rlTexCoord2f(bU+tw, bV); rlVertex3f( hs, -hs,  hs);
    rlTexCoord2f(bU, bV); rlVertex3f(-hs, -hs,  hs);
    rlEnd(); }
    // 左 (-X)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(sU, sV+th); rlVertex3f(-hs, -hs, -hs);
    rlTexCoord2f(sU+tw, sV+th); rlVertex3f(-hs, -hs,  hs);
    rlTexCoord2f(sU+tw, sV); rlVertex3f(-hs,  hs,  hs);
    rlTexCoord2f(sU, sV); rlVertex3f(-hs,  hs, -hs);
    rlEnd(); }
    // 右 (+X)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(sU, sV+th); rlVertex3f( hs, -hs,  hs);
    rlTexCoord2f(sU+tw, sV+th); rlVertex3f( hs, -hs, -hs);
    rlTexCoord2f(sU+tw, sV); rlVertex3f( hs,  hs, -hs);
    rlTexCoord2f(sU, sV); rlVertex3f( hs,  hs,  hs);
    rlEnd(); }

    rlSetTexture(0);
    rlPopMatrix();
}

// 绘制铤形掉落物（铁铤/金铤 → 彩色长方体）
static void drawIngotDrop(const Vector3& pos, float angle, int item) {
    Color base;
    // 按标准掉落物尺寸 0.22 等比例缩小：0.9→0.22, 0.4→0.10, 0.2→0.05
    float dx = 0.12f, dy = 0.07f, dz = 0.22f;
    switch (item) {
        case ITEM_IRON_INGOT: base = (Color){ 210, 210, 220, 255 }; break;
        case ITEM_GOLD_INGOT: base = (Color){ 220, 200,  60, 255 }; break;
        default: return;
    }

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(angle, 0, 1, 0);
    rlRotatef(15.0f, 1, 0, 0);

    rlSetTexture(0);

    struct { int r,g,b; } cTop = { base.r, base.g, base.b },
                           cSide = { (int)(base.r*0.72f), (int)(base.g*0.72f), (int)(base.b*0.72f) },
                           cBot  = { (int)(base.r*0.45f), (int)(base.g*0.45f), (int)(base.b*0.45f) };
    float hx = dx/2, hy = dy/2, hz = dz/2;

    // 前 (+Z)
    rlBegin(RL_QUADS); rlColor4ub(cSide.r,cSide.g,cSide.b,255);
    rlVertex3f(-hx,-hy, hz); rlVertex3f( hx,-hy, hz);
    rlVertex3f( hx, hy, hz); rlVertex3f(-hx, hy, hz); rlEnd();
    // 后 (-Z)
    rlBegin(RL_QUADS); rlColor4ub(cSide.r,cSide.g,cSide.b,255);
    rlVertex3f( hx,-hy,-hz); rlVertex3f(-hx,-hy,-hz);
    rlVertex3f(-hx, hy,-hz); rlVertex3f( hx, hy,-hz); rlEnd();
    // 上 (+Y)
    rlBegin(RL_QUADS); rlColor4ub(cTop.r,cTop.g,cTop.b,255);
    rlVertex3f(-hx, hy, hz); rlVertex3f( hx, hy, hz);
    rlVertex3f( hx, hy,-hz); rlVertex3f(-hx, hy,-hz); rlEnd();
    // 下 (-Y)
    rlBegin(RL_QUADS); rlColor4ub(cBot.r,cBot.g,cBot.b,255);
    rlVertex3f(-hx,-hy,-hz); rlVertex3f( hx,-hy,-hz);
    rlVertex3f( hx,-hy, hz); rlVertex3f(-hx,-hy, hz); rlEnd();
    // 左 (-X)
    rlBegin(RL_QUADS); rlColor4ub(cSide.r,cSide.g,cSide.b,255);
    rlVertex3f(-hx,-hy,-hz); rlVertex3f(-hx,-hy, hz);
    rlVertex3f(-hx, hy, hz); rlVertex3f(-hx, hy,-hz); rlEnd();
    // 右 (+X)
    rlBegin(RL_QUADS); rlColor4ub(cSide.r,cSide.g,cSide.b,255);
    rlVertex3f( hx,-hy, hz); rlVertex3f( hx,-hy,-hz);
    rlVertex3f( hx, hy,-hz); rlVertex3f( hx, hy, hz); rlEnd();

    rlSetTexture(0);
    rlPopMatrix();
}

// 绘制非方块掉落物（小立方体展示物品图标，所有面用同一纹理）
static void drawFlatDrop(const Vector3& pos, float angle, int item,
                          const TextureAtlas& atlas) {
    int texSlot = getDropTexSlot(item);
    if (texSlot < 0) return;

    float u0 = atlas.u[texSlot], v0 = atlas.v[texSlot];
    float tw = atlas.w, th = atlas.h;
    float s = ITEM_FLAT_SIZE, hs = s / 2;

    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(angle, 0, 1, 0);
    rlRotatef(15.0f, 1, 0, 0);

    rlSetTexture(atlas.texture.id);

    // 前 (+Z)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(u0, v0+th); rlVertex3f(-hs, -hs,  hs);
    rlTexCoord2f(u0+tw, v0+th); rlVertex3f( hs, -hs,  hs);
    rlTexCoord2f(u0+tw, v0); rlVertex3f( hs,  hs,  hs);
    rlTexCoord2f(u0, v0); rlVertex3f(-hs,  hs,  hs);
    rlEnd(); }
    // 后 (-Z)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(u0, v0+th); rlVertex3f( hs, -hs, -hs);
    rlTexCoord2f(u0+tw, v0+th); rlVertex3f(-hs, -hs, -hs);
    rlTexCoord2f(u0+tw, v0); rlVertex3f(-hs,  hs, -hs);
    rlTexCoord2f(u0, v0); rlVertex3f( hs,  hs, -hs);
    rlEnd(); }
    // 上 (+Y)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(u0, v0+th); rlVertex3f(-hs,  hs,  hs);
    rlTexCoord2f(u0+tw, v0+th); rlVertex3f( hs,  hs,  hs);
    rlTexCoord2f(u0+tw, v0); rlVertex3f( hs,  hs, -hs);
    rlTexCoord2f(u0, v0); rlVertex3f(-hs,  hs, -hs);
    rlEnd(); }
    // 下 (-Y)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(u0, v0+th); rlVertex3f(-hs, -hs, -hs);
    rlTexCoord2f(u0+tw, v0+th); rlVertex3f( hs, -hs, -hs);
    rlTexCoord2f(u0+tw, v0); rlVertex3f( hs, -hs,  hs);
    rlTexCoord2f(u0, v0); rlVertex3f(-hs, -hs,  hs);
    rlEnd(); }
    // 左 (-X)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(u0, v0+th); rlVertex3f(-hs, -hs, -hs);
    rlTexCoord2f(u0+tw, v0+th); rlVertex3f(-hs, -hs,  hs);
    rlTexCoord2f(u0+tw, v0); rlVertex3f(-hs,  hs,  hs);
    rlTexCoord2f(u0, v0); rlVertex3f(-hs,  hs, -hs);
    rlEnd(); }
    // 右 (+X)
    { rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(u0, v0+th); rlVertex3f( hs, -hs,  hs);
    rlTexCoord2f(u0+tw, v0+th); rlVertex3f( hs, -hs, -hs);
    rlTexCoord2f(u0+tw, v0); rlVertex3f( hs,  hs, -hs);
    rlTexCoord2f(u0, v0); rlVertex3f( hs,  hs,  hs);
    rlEnd(); }

    rlSetTexture(0);
    rlPopMatrix();
}

void dropDraw(const std::vector<DroppedItem>& drops, const Camera3D& camera, const TextureAtlas& atlas) {
    (void)camera;
    if (atlas.texture.id <= 0) return;

    for (const auto& d : drops) {
        if (d.count <= 0 || d.lifetime <= 0) continue;
        if (d.pos.y < -10.0f) continue;

        float angle = d.lifetime * 60.0f; // 持续旋转

        if (isBlockItem(d.item)) {
            drawBlockDrop(d.pos, angle, d.item, atlas);
        } else if (d.item == ITEM_IRON_INGOT || d.item == ITEM_GOLD_INGOT) {
            drawIngotDrop(d.pos, angle, d.item);
        } else {
            drawFlatDrop(d.pos, angle, d.item, atlas);
        }
    }
}
