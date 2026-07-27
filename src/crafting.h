// crafting.h - MC 风格合成网格 + 带形状配方匹配
#pragma once
#include "item.h"
#include "inventory.h"

// ---- 合成网格（2x2 或 3x3）----
constexpr int GRID_SIZE = 3;

struct CraftingGrid {
    InventorySlot slots[GRID_SIZE][GRID_SIZE]; // 每个格子有堆叠数
    bool is3x3;  // true=3x3工作台, false=2x2手合成

    void clear() {
        for (int r = 0; r < GRID_SIZE; ++r)
            for (int c = 0; c < GRID_SIZE; ++c)
                slots[r][c] = { 0, 0 };
    }

    // 获取物品 ID（兼容旧代码）
    int itemAt(int r, int c) const { return slots[r][c].item; }

    // 消耗所有格子 1 个物品
    void consumeAll() {
        for (int r = 0; r < GRID_SIZE; ++r)
            for (int c = 0; c < GRID_SIZE; ++c)
                if (slots[r][c].item > 0 && slots[r][c].count > 0) {
                    slots[r][c].count--;
                    if (slots[r][c].count <= 0) slots[r][c].item = 0;
                }
    }
};

// ---- 带形状的配方 ----
struct ShapedRecipe {
    int resultItem;
    int resultCount;
    // 配方图案：3x3 网格，(0,0)=左上角
    int pattern[GRID_SIZE][GRID_SIZE];
    bool needs3x3;       // 是否需要 3x3（工作台）
    const char* name;
};

// 所有配方
extern const ShapedRecipe SHAPED_RECIPES[];
extern const int SHAPED_RECIPE_COUNT;

// 用当前合成网格匹配配方（返回索引，-1 为无匹配）
int matchRecipe(const CraftingGrid& grid);

// 执行合成：消耗网格中的物品，返回产物
int executeRecipe(CraftingGrid& grid, int recipeIdx);
