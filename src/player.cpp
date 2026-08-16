// player.cpp - 玩家：物理、碰撞、拾取、生存属性、采矿系统
#include "player.h"
#include "world.h"
#include "drop.h"
#include "mob.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

Vector3 playerForward(const Player& p) {
    float cp = cosf(p.pitch);
    return { cp * sinf(p.yaw), sinf(p.pitch), cp * cosf(p.yaw) };
}
static Vector3 playerRight(const Player& p) {
    // right = forward × up（右手系：forward 朝向 +Z，则 right 是 -X）
    Vector3 fwd = playerForward(p);
    return { -fwd.z, 0.0f, fwd.x };
}

// ---- AABB 碰撞检测 ----
static bool collidesAt(World& w, const Player& p, Vector3 testPos) {
    int x0 = (int)floorf(testPos.x - PLAYER_WIDTH / 2);
    int x1 = (int)floorf(testPos.x + PLAYER_WIDTH / 2);
    int y0 = (int)floorf(testPos.y);
    int y1 = (int)floorf(testPos.y + PLAYER_HEIGHT);
    int z0 = (int)floorf(testPos.z - PLAYER_WIDTH / 2);
    int z1 = (int)floorf(testPos.z + PLAYER_WIDTH / 2);
    for (int x = x0; x <= x1; ++x)
        for (int y = y0; y <= y1; ++y)
            for (int z = z0; z <= z1; ++z)
                if (isSolid(worldGetBlock(w, x, y, z))) return true;
    return false;
}

void playerInit(Player& p, World& w, int spawnX, int spawnZ) {
    int gy = worldFindGroundY(w, spawnX, spawnZ);
    p.pos = { (float)spawnX + 0.5f, (float)gy + 0.1f, (float)spawnZ + 0.5f };
    p.vel = { 0, 0, 0 };
    p.yaw = 0.0f;
    p.pitch = -0.2f;
    p.onGround = false;
    p.hp = 20.0f;
    p.maxHp = 20.0f;
    p.hunger = 20.0f;
    p.hungerTimer = 0.0f;
    p.fallDistance = 0.0f;
    p.healTimer = 0.0f;
    p.miningTimer = 0.0f;
    p.miningTarget = 0;
    p.miningDuration = 0.0f;
    p.camera = {};
    p.camera.up = { 0, 1, 0 };
    p.camera.fovy = 70.0f;
    p.camera.projection = CAMERA_PERSPECTIVE;
    p.inventory.init();
    p.craftGrid.clear();
    p.craftGrid.is3x3 = false;
    p.grabbed = { 0, 0 };
    p.inventoryOpen = false;
    p.nearWorkbench = false;
    p.pickupCooldown = 0.0f;
}

static void syncCamera(Player& p) {
    p.camera.position = { p.pos.x, p.pos.y + PLAYER_EYE, p.pos.z };
    Vector3 f = playerForward(p);
    p.camera.target = {
        p.camera.position.x + f.x,
        p.camera.position.y + f.y,
        p.camera.position.z + f.z
    };
}

// ---- 挖矿编码工具 ----
static int encodeMiningTarget(int bx, int by, int bz) {
    return (bx & 0xFFFF) | ((by & 0xFF) << 16) | ((bz & 0xFFFF) << 24);
}

// 检测某点是否在水中
static bool inWater(const World& w, Vector3 pt) {
    return worldGetBlock(w, (int)floorf(pt.x), (int)floorf(pt.y), (int)floorf(pt.z)) == BLOCK_WATER;
}
static bool playerInWater(const Player& p, const World& w) {
    Vector3 head = { p.pos.x, p.pos.y + PLAYER_EYE, p.pos.z };
    Vector3 body = { p.pos.x, p.pos.y + 0.1f, p.pos.z };
    return inWater(w, head) || inWater(w, body);
}

void playerUpdate(Player& p, World& w, float dt) {
	    // ---- 生存：饱食度消耗与回血 ----
	    p.hungerTimer += dt;
	    if (p.hungerTimer >= 4.0f) {
	        p.hungerTimer -= 4.0f;
	        p.hunger -= 0.1f;
	        if (p.hunger < 0.0f) p.hunger = 0.0f;
	    }
	    p.healTimer += dt;
	    if (p.healTimer >= 4.0f) {
	        p.healTimer -= 4.0f;
	        if (p.hunger > 16.0f && p.hp < p.maxHp) p.hp += 0.5f;
	        if (p.hunger <= 0.0f && p.hp > 0.0f) p.hp -= 0.5f;
	        if (p.hp > p.maxHp) p.hp = p.maxHp;
	        if (p.hp < 0.0f) p.hp = 0.0f;
	    }

	    // ---- 坠落伤害（MC 公式：round(距离-3)，int 防浮点精度）----
	    if (!p.onGround) {
	        p.fallDistance += -p.vel.y * dt;
	    } else {
	        // 落在水中免摔落伤害
	        bool landingInWater = false;
	        Vector3 feet = { p.pos.x, p.pos.y - 0.1f, p.pos.z };
	        if (inWater(w, feet)) landingInWater = true;
	        if (!landingInWater && p.fallDistance > 3.0f) {
	            int e = (int)std::round(p.fallDistance - 3.0f);
	            if (e > 0) p.hp -= (float)e;
	        }
	        p.fallDistance = 0.0f;
	    }
	    if (p.hp <= 0.0f) {
	        // 死亡：重置位置到世界出生点（8,8）的地表
	        int gy = worldFindGroundY(w, 8, 8);
	        if (gy < 1) gy = 1;
	        // 确保出生点不卡在方块里（往上找空位）
	        for (int attempt = 0; attempt < 5; ++attempt) {
	            if (!isSolid(worldGetBlock(w, 8, gy + attempt, 8))) {
	                gy = gy + attempt;
	                break;
	            }
	        }
	        p.pos = { 8.5f, (float)gy + 0.5f, 8.5f };
	        p.vel = { 0, 0, 0 };
	        p.hp = p.maxHp;
	        p.hunger = 16.0f;
	        p.fallDistance = 0.0f;
	    }
	    // 限制 HP 最小值避免负值穿透
	    if (p.hp < 0.0f) p.hp = 0.0f;

    if (p.inventoryOpen) return; // 背包界面打开时不移动

    // ---- 鼠标视角 ----
    Vector2 mouse = GetMouseDelta();
    p.yaw -= mouse.x * 0.0025f;
    p.pitch -= mouse.y * 0.0025f;
    float lim = 1.553f;
    if (p.pitch > lim) p.pitch = lim;
    if (p.pitch < -lim) p.pitch = -lim;

    // ---- WASD 移动 ----
    Vector3 fwd = playerForward(p);
    fwd.y = 0;
    if (Vector3Length(fwd) > 0.001f) fwd = Vector3Normalize(fwd);
    Vector3 right = playerRight(p);
    Vector3 wish = { 0, 0, 0 };
    if (IsKeyDown(KEY_W)) wish = Vector3Add(wish, fwd);
    if (IsKeyDown(KEY_S)) wish = Vector3Subtract(wish, fwd);
    if (IsKeyDown(KEY_D)) wish = Vector3Add(wish, right);
    if (IsKeyDown(KEY_A)) wish = Vector3Subtract(wish, right);
    if (Vector3Length(wish) > 0.001f) wish = Vector3Normalize(wish);

    bool sprint = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SHIFT);
    float speed = sprint ? SPRINT_SPEED : MOVE_SPEED;
	    p.vel.x = wish.x * speed;
	    p.vel.z = wish.z * speed;

	    // ---- 跳跃 / 游泳 ----
	    bool sw = playerInWater(p, w);
	    if (sw) {
	        // 水中：大幅减弱重力、空格上浮、Shift 下沉
	        p.vel.y -= GRAVITY * 0.15f * dt;
	        if (p.vel.y < -8.0f) p.vel.y = -8.0f;
	        if (IsKeyDown(KEY_SPACE)) { p.vel.y = 4.0f; p.onGround = false; }
	        if (IsKeyDown(KEY_LEFT_SHIFT)) { p.vel.y -= 6.0f * dt; }
	        // 水平移动变慢
	        float waterSlow = 0.5f;
	        p.vel.x *= (1.0f - (1.0f - waterSlow) * dt * 5.0f);
	        p.vel.z *= (1.0f - (1.0f - waterSlow) * dt * 5.0f);
	    } else {
	        if (IsKeyPressed(KEY_SPACE) && p.onGround) {
	            p.vel.y = JUMP_SPEED;
	            p.onGround = false;
	        }
	        p.vel.y -= GRAVITY * dt;
	        if (p.vel.y < -55.0f) p.vel.y = -55.0f;
	    }

    // ---- 逐轴碰撞 ----
    Vector3 delta = { p.vel.x * dt, p.vel.y * dt, p.vel.z * dt };
    p.onGround = false;
    { Vector3 t = p.pos; t.x += delta.x; if (!collidesAt(w, p, t)) p.pos.x = t.x; else p.vel.x = 0; }
    { Vector3 t = p.pos; t.z += delta.z; if (!collidesAt(w, p, t)) p.pos.z = t.z; else p.vel.z = 0; }
    { Vector3 t = p.pos; t.y += delta.y; if (!collidesAt(w, p, t)) p.pos.y = t.y; else { if (delta.y < 0) p.onGround = true; p.vel.y = 0; } }

    // ---- 采矿进度（每帧更新）----
    RaycastHit hit = playerRaycast(p, w);
    // 视线命中生物时优先攻击目标，不挖方块（攻击由 main 处理）
    Vector3 fdir = playerForward(p);
    bool aimMob = mobRaycast(w.mobs, p.camera.position, fdir, REACH) >= 0;
    int curTarget = (hit.hit && !aimMob) ? encodeMiningTarget(hit.bx, hit.by, hit.bz) : 0;

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hit.hit && !aimMob) {
        BlockType bt = worldGetBlock(w, hit.bx, hit.by, hit.bz);
        if (!isBreakable(bt)) { p.miningTimer = 0.0f; p.miningTarget = 0; }
        else {
            if (curTarget != p.miningTarget) {
                p.miningTimer = 0.0f;
                p.miningTarget = curTarget;
                int toolTier = getToolTier(p.inventory.selectedItem());
                int needTier = getMiningLevel(bt);
                if (toolTier < needTier) {
                    p.miningDuration = 999.0f; // 永远挖不完
                } else {
                    float hardness = getHardness(bt);
                    float eff = getToolEfficiency(p.inventory.selectedItem(), (int)bt);
                    p.miningDuration = hardness / eff * 0.3f; // 基础 0.3 秒/硬度
                }
            }
            p.miningTimer += dt;
            if (p.miningTimer >= p.miningDuration && p.miningDuration < 999.0f) {
                int drop = getDropItem(bt);
                // 在被挖方块中心生成（此时方块尚未移除，但下一帧即为空气）
                Vector3 blockCenter = { (float)hit.bx + 0.5f, (float)hit.by + 0.5f, (float)hit.bz + 0.5f };
                // 树叶：不掉自己，只有 30% 概率额外掉落苹果
                if (bt == BLOCK_LEAVES) {
                    if (rand() < RAND_MAX * 0.3f) {
                        dropSpawn(w.drops, blockCenter, ITEM_APPLE, 1);
                    }
                    drop = 0; // 不掉树叶本身
                }
                if (drop > 0) {
                    dropSpawn(w.drops, blockCenter, drop, 1);
                }
                worldSetBlock(w, hit.bx, hit.by, hit.bz, BLOCK_AIR);
                p.miningTimer = 0.0f;
                p.miningTarget = 0;
            }
        }
    } else {
        p.miningTimer = 0.0f;
        p.miningTarget = 0;
    }

	    // ---- 工作台临近检测 ----
	    p.nearWorkbench = playerNearWorkbench(w, p);

		    // ---- 自动拾取掉落物 ----
		    {
		        // 拾取冷却递减
		        if (p.pickupCooldown > 0.0f) p.pickupCooldown -= dt;
		        if (p.pickupCooldown < 0.0f) p.pickupCooldown = 0.0f;

		        Vector3 ppos = { p.pos.x, p.pos.y + 0.5f, p.pos.z };
		        for (size_t i = 0; i < w.drops.size(); ) {
		            auto& d = w.drops[i];
		            if (d.count <= 0 || d.lifetime <= 0) {
		                d = w.drops.back(); w.drops.pop_back();
		                continue;
		            }
		            // 拾取延迟内不可拾取
		            if (d.pickupDelay > 0.0f) { ++i; continue; }
		            // 冷却中不可拾取
		            if (p.pickupCooldown > 0.0f) break;
		            float dist = Vector3Distance(d.pos, ppos);
		            if (dist < 1.8f) {
		                int added = p.inventory.addItem(d.item, d.count);
		                if (added > 0) {
		                    d.count -= added;
		                    p.pickupCooldown = 0.15f; // 每次拾取后短冷却
		                }
		                if (d.count <= 0) {
		                    d = w.drops.back(); w.drops.pop_back();
		                    continue;
		                }
		            }
		            ++i;
	        }
	    }

	    syncCamera(p);
	}

// ---- 射线拾取 ----
RaycastHit playerRaycast(const Player& p, const World& w) {
    RaycastHit res = { false };
    Vector3 origin = p.camera.position;
    Vector3 dir = playerForward(p);
    float dist = 0.0f;
    int prevX = (int)floorf(origin.x), prevY = (int)floorf(origin.y), prevZ = (int)floorf(origin.z);
    while (dist <= REACH) {
        int bx = (int)floorf(origin.x + dir.x * dist);
        int by = (int)floorf(origin.y + dir.y * dist);
        int bz = (int)floorf(origin.z + dir.z * dist);
        if (isSolid(worldGetBlock(w, bx, by, bz))) {
            res.hit = true;
            res.bx = bx; res.by = by; res.bz = bz;
            res.nx = prevX - bx; res.ny = prevY - by; res.nz = prevZ - bz;
            res.point = { origin.x + dir.x * dist, origin.y + dir.y * dist, origin.z + dir.z * dist };
            return res;
        }
        prevX = bx; prevY = by; prevZ = bz;
        dist += STEP;
    }
    return res;
}

bool playerNearWorkbench(const World& w, const Player& p) {
    int cx = (int)floorf(p.pos.x);
    int cy = (int)floorf(p.pos.y);
    int cz = (int)floorf(p.pos.z);
    for (int dx = -3; dx <= 3; ++dx)
        for (int dy = -3; dy <= 3; ++dy)
            for (int dz = -3; dz <= 3; ++dz)
                if (worldGetBlock(w, cx + dx, cy + dy, cz + dz) == BLOCK_CRAFTING_TABLE)
                    return true;
    return false;
}
