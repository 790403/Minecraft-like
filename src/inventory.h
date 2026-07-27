// inventory.h - 玩家背包（8 格快捷栏 + 18 格背包）
#pragma once
#include "item.h"

constexpr int HOTBAR_SLOTS = 8;
constexpr int BACKPACK_SLOTS = 18;
constexpr int TOTAL_SLOTS = HOTBAR_SLOTS + BACKPACK_SLOTS; // 26

struct InventorySlot {
    int item;   // ItemType（0 = 空）
    int count;  // 数量（0 = 无）
};

struct Inventory {
    InventorySlot slots[TOTAL_SLOTS];  // [0..7]=快捷栏, [8..25]=背包

    // 当前选中的快捷栏位
    int selectedSlot;

    void init() {
        for (int i = 0; i < TOTAL_SLOTS; ++i) { slots[i].item = 0; slots[i].count = 0; }
        selectedSlot = 0;
        // 初始空背包，无任何物品
    }

    // 获取快捷栏中当前选中的物品和数量
    int selectedItem() const { return slots[selectedSlot].item; }
    int selectedCount() const { return slots[selectedSlot].count; }

    // 尝试添加物品到背包（返回实际添加数量）
    int addItem(int item, int count) {
        if (item <= 0 || count <= 0) return 0;
        int added = 0;
        // 先尝试堆叠到已有同类物品上
        for (int i = 0; i < TOTAL_SLOTS && added < count; ++i) {
            if (slots[i].item == item && slots[i].count < 64) {
                int space = 64 - slots[i].count;
                int take = (count - added < space) ? (count - added) : space;
                slots[i].count += take;
                added += take;
            }
        }
        // 再放入空格
        for (int i = 0; i < TOTAL_SLOTS && added < count; ++i) {
            if (slots[i].item == 0) {
                int take = (count - added < 64) ? (count - added) : 64;
                slots[i].item = item;
                slots[i].count = take;
                added += take;
            }
        }
        return added;
    }

    // 移除物品，返回实际移除数量
    int removeItem(int item, int count) {
        if (item <= 0 || count <= 0) return 0;
        int removed = 0;
        for (int i = 0; i < TOTAL_SLOTS && removed < count; ++i) {
            if (slots[i].item == item) {
                int take = (count - removed < slots[i].count) ? (count - removed) : slots[i].count;
                slots[i].count -= take;
                removed += take;
                if (slots[i].count <= 0) { slots[i].item = 0; slots[i].count = 0; }
            }
        }
        return removed;
    }

    // 检查拥有某物品的数量
    int countItem(int item) const {
        int total = 0;
        for (int i = 0; i < TOTAL_SLOTS; ++i)
            if (slots[i].item == item) total += slots[i].count;
        return total;
    }

    // 消耗快捷栏当前选中的 1 个物品
    bool consumeSelected() {
        if (slots[selectedSlot].count > 0) {
            slots[selectedSlot].count--;
            if (slots[selectedSlot].count <= 0) slots[selectedSlot].item = 0;
            return true;
        }
        return false;
    }
};
