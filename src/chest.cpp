// chest.cpp - 箱子容器实现
#include "chest.h"
#include "drop.h"

ChestData& ChestSystem::getOrCreate(int x, int y, int z) {
    long long key = chestKey(x, y, z);
    auto it = chests.find(key);
    if (it == chests.end()) {
        ChestData cd;
        cd.init();
        chests[key] = cd;
        return chests[key];
    }
    return it->second;
}

void ChestSystem::remove(int x, int y, int z) {
    chests.erase(chestKey(x, y, z));
}

void ChestSystem::dropContents(std::vector<DroppedItem>& drops, int x, int y, int z) {
    long long key = chestKey(x, y, z);
    auto it = chests.find(key);
    if (it == chests.end()) return;

    Vector3 pos = { (float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f };
    for (int i = 0; i < CHEST_SLOTS; ++i) {
        if (it->second.slots[i].item > 0 && it->second.slots[i].count > 0) {
            dropSpawn(drops, pos, it->second.slots[i].item, it->second.slots[i].count);
        }
    }
    chests.erase(key);
}
