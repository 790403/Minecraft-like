// furnace.cpp - 熔炉冶炼系统
#include "furnace.h"
#include "item.h"
#include <cmath>

// 检查物品是否为可冶炼的矿石，返回产物
static int getSmeltResult(int item) {
    switch (item) {
        case ITEM_RAW_IRON: return ITEM_IRON_INGOT;
        case ITEM_RAW_GOLD: return ITEM_GOLD_INGOT;
        default: return 0;
    }
}

// 检查是否为燃料
static bool isFuel(int item) {
    return item == ITEM_COAL;
}

FurnaceData& FurnaceSystem::getOrCreate(int x, int y, int z) {
    long long key = furnaceKey(x, y, z);
    auto it = furnaces.find(key);
    if (it == furnaces.end()) {
        FurnaceData fd;
        fd.init();
        furnaces[key] = fd;
        return furnaces[key];
    }
    return it->second;
}

void FurnaceSystem::remove(int x, int y, int z) {
    furnaces.erase(furnaceKey(x, y, z));
}

void FurnaceSystem::dropContents(std::vector<DroppedItem>& drops, int x, int y, int z) {
    long long key = furnaceKey(x, y, z);
    auto it = furnaces.find(key);
    if (it == furnaces.end()) return;
    Vector3 pos = { (float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f };
    if (it->second.input.item > 0)  dropSpawn(drops, pos, it->second.input.item, it->second.input.count);
    if (it->second.fuel.item > 0)   dropSpawn(drops, pos, it->second.fuel.item, it->second.fuel.count);
    if (it->second.output.item > 0) dropSpawn(drops, pos, it->second.output.item, it->second.output.count);
    furnaces.erase(key);
}

void FurnaceSystem::updateAll(float dt) {
    for (auto& kv : furnaces) {
        auto& f = kv.second;
        if (!f.initialized) continue;

        // 如果输出槽已满或没有产物空间，停止冶炼
        bool outputFull = (f.output.item > 0 && f.output.count >= 64);

        // 检查是否有可冶炼的物品和燃料
        int result = getSmeltResult(f.input.item);
        bool canSmelt = (result > 0 && f.input.count > 0
                      && isFuel(f.fuel.item) && f.fuel.count > 0
                      && !outputFull
                      && (f.output.item == 0 || f.output.item == result));

        if (canSmelt) {
            f.smeltTime += dt;
            if (f.smeltTime >= f.smeltDuration) {
                // 冶炼完成
                f.smeltTime = 0;
                // 消耗燃料
                f.fuel.count--;
                if (f.fuel.count <= 0) f.fuel = {0, 0};
                // 消耗输入
                f.input.count--;
                if (f.input.count <= 0) f.input = {0, 0};
                // 产出
                if (f.output.item == 0) {
                    f.output = { result, 1 };
                } else if (f.output.item == result && f.output.count < 64) {
                    f.output.count++;
                }
            }
        } else {
            // 没有冶炼条件，进度缓慢归零
            if (f.smeltTime > 0) f.smeltTime -= dt * 2;
            if (f.smeltTime < 0) f.smeltTime = 0;
        }
    }
}
