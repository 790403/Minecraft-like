// furnace.h - 熔炉容器 + 冶炼系统
#pragma once
#include "raylib.h"
#include "inventory.h"
#include "texture_atlas.h"
#include "drop.h"
#include <unordered_map>
#include <vector>

struct FurnaceData {
    InventorySlot input;   // 冶炼物（如粗铁）
    InventorySlot fuel;    // 燃料（煤炭）
    InventorySlot output;  // 产物（如铁锭）
    float smeltTime;       // 当前冶炼进度（秒）
    float smeltDuration;   // 所需总时间（秒）
    bool initialized;

    void init() {
        input = {0, 0}; fuel = {0, 0}; output = {0, 0};
        smeltTime = 0; smeltDuration = 8.0f;
        initialized = true;
    }
};

inline long long furnaceKey(int x, int y, int z) {
    return ((long long)(x & 0xFFFFFF) << 24)
         | ((long long)(y & 0xFF) << 16)
         | (long long)(z & 0xFFFFFF);
}

struct FurnaceSystem {
    std::unordered_map<long long, FurnaceData> furnaces;

    FurnaceData& getOrCreate(int x, int y, int z);
    void remove(int x, int y, int z);
    void dropContents(std::vector<DroppedItem>& drops, int x, int y, int z);
    void updateAll(float dt);
};

struct FurnaceGUI {
    bool open;
    int fx, fy, fz;
    InventorySlot grabbed;
};
