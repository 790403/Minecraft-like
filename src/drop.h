// drop.h - 掉落物实体系统
#pragma once
#include "raylib.h"
#include "texture_atlas.h"
#include "item.h"
#include <vector>

struct World; // 前置声明

struct DroppedItem {
    Vector3 pos;
    Vector3 vel;
    int item;
    int count;
    float lifetime;    // 剩余存在时间（秒）
    float pickupDelay; // 拾取延迟（秒），防止立即拾取
    bool onGround;
};

// 生成掉落物（pos位置, item物品, count数量, randomVel=true随机扩散, vel定向速度）
void dropSpawn(std::vector<DroppedItem>& drops, Vector3 pos, int item, int count, bool randomVel = true, Vector3 dir = {0,0,0});

// 每帧更新掉落物物理（需要世界用于地面碰撞检测）
void dropUpdate(std::vector<DroppedItem>& drops, float dt, const World& w);

// 绘制掉落物（billboard）
void dropDraw(const std::vector<DroppedItem>& drops, const Camera3D& camera, const TextureAtlas& atlas);

// 拾取附近掉落物
int dropPickup(std::vector<DroppedItem>& drops, Vector3 playerPos, float reach, int item, int maxCount);
