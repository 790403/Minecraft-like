// chest.h - 箱子容器系统
#pragma once
#include "raylib.h"
#include "inventory.h"
#include "texture_atlas.h"
#include <unordered_map>
#include <vector>

constexpr int CHEST_SLOTS = 27;

struct ChestData {
    InventorySlot slots[CHEST_SLOTS];
    int facing; // 0=+Z, 1=+X, 2=-Z, 3=-X （锁扣朝向玩家）
    bool initialized = false;

    void init() {
        for (int i = 0; i < CHEST_SLOTS; ++i) slots[i] = { 0, 0 };
        facing = 0;
        initialized = true;
    }
};

// 方块坐标编码为 64 位 key
inline long long chestKey(int x, int y, int z) {
    return ((long long)(x & 0xFFFFFF) << 24)
         | ((long long)(y & 0xFF) << 16)
         | (long long)(z & 0xFFFFFF);
}

struct DroppedItem; // forward decl

struct ChestSystem {
    std::unordered_map<long long, ChestData> chests;

    ChestData& getOrCreate(int x, int y, int z);
    void remove(int x, int y, int z);
    void dropContents(std::vector<DroppedItem>& drops, int x, int y, int z);
};

// 箱子界面状态
struct ChestGUI {
    bool open;
    int cx, cy, cz;   // 箱子方块坐标
    InventorySlot grabbed; // 手持物品
};
