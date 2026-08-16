// crafting.cpp - 带形状配方表 + 匹配逻辑
#include "crafting.h"

// --- 简记：P=plank, C=cobble, S=stick, I=iron_raw, G=gold_raw, D=diamond, W=log ---
enum : int { _ = 0, P = ITEM_PLANK, C = ITEM_COBBLE, S = ITEM_STICK,
              I = ITEM_RAW_IRON, G = ITEM_RAW_GOLD, D = ITEM_DIAMOND, W = ITEM_WOOD,
              II = 200, GG = 201 }; // 铁锭=200, 金锭=201

const ShapedRecipe SHAPED_RECIPES[] = {
    { ITEM_PLANK, 4, {{W,_,_},{_,_,_},{_,_,_}}, false, "木板" },
    { ITEM_STICK, 4, {{P,_,_},{P,_,_},{_,_,_}}, false, "木棍" },
    { ITEM_CRAFTING_TABLE, 1, {{P,P,_},{P,P,_},{_,_,_}}, false, "工作台" },
    { ITEM_FURNACE, 1, {{C,C,C},{C,_,C},{C,C,C}}, true, "熔炉" },
    // 箱子：8 木板围一圈
    { ITEM_CHEST, 1, {{P,P,P},{P,_,P},{P,P,P}}, true, "箱子" },
    // 工具（用铁锭/金锭而非粗矿）
    { ITEM_WOOD_PICKAXE, 1, {{P,P,P},{_,S,_},{_,S,_}}, true, "木镐" },
    { ITEM_STONE_PICKAXE, 1, {{C,C,C},{_,S,_},{_,S,_}}, true, "石镐" },
    { ITEM_IRON_PICKAXE, 1, {{II,II,II},{_,S,_},{_,S,_}}, true, "铁镐" },
    { ITEM_GOLD_PICKAXE, 1, {{GG,GG,GG},{_,S,_},{_,S,_}}, true, "金镐" },
    { ITEM_DIAMOND_PICKAXE, 1, {{D,D,D},{_,S,_},{_,S,_}}, true, "钻石镐" },
    // 剑（材料 x2 竖排 + 木棍 x1 竖排）
    { ITEM_WOOD_SWORD, 1, {{_,P,_},{_,P,_},{_,S,_}}, true, "木剑" },
    { ITEM_STONE_SWORD, 1, {{_,C,_},{_,C,_},{_,S,_}}, true, "石剑" },
    { ITEM_IRON_SWORD, 1, {{_,II,_},{_,II,_},{_,S,_}}, true, "铁剑" },
    { ITEM_GOLD_SWORD, 1, {{_,GG,_},{_,GG,_},{_,S,_}}, true, "金剑" },
    { ITEM_DIAMOND_SWORD, 1, {{_,D,_},{_,D,_},{_,S,_}}, true, "钻石剑" },
};
const int SHAPED_RECIPE_COUNT = sizeof(SHAPED_RECIPES) / sizeof(SHAPED_RECIPES[0]);

// ---- 匹配逻辑 ----
// 将网格裁剪为紧凑形式，与配方比较

static bool matchAt(const CraftingGrid& grid, int offR, int offC, const ShapedRecipe& rec) {
    int size = rec.needs3x3 ? 3 : 2;
    // 匹配图案
    for (int r = 0; r < size; ++r)
        for (int c = 0; c < size; ++c) {
            int gi = (r + offR < GRID_SIZE && c + offC < GRID_SIZE)
                     ? grid.slots[r + offR][c + offC].item : 0;
            if (gi != rec.pattern[r][c]) return false;
        }
    // (MC规则) 图案区域外不能有额外物品
    int checkR = grid.is3x3 ? GRID_SIZE : 2;
    for (int r = 0; r < checkR; ++r)
        for (int c = 0; c < checkR; ++c) {
            if (r >= offR && r < offR + size && c >= offC && c < offC + size) continue;
            if (grid.slots[r][c].item > 0) return false;
        }
    return true;
}

int matchRecipe(const CraftingGrid& grid) {
    for (int i = 0; i < SHAPED_RECIPE_COUNT; ++i) {
        const ShapedRecipe& rec = SHAPED_RECIPES[i];
        if (rec.needs3x3 && !grid.is3x3) continue;
        int sz = rec.needs3x3 ? 3 : 2;
        // 3x3网格中2x2配方可偏移0..1；2x2网格中任何2x2配方可平移1格覆盖全部4位置
        int mo = grid.is3x3 ? (3 - sz) : (sz == 2 ? 1 : 0);
        for (int offR = 0; offR <= mo; ++offR) {
            for (int offC = 0; offC <= mo; ++offC) {
                if (matchAt(grid, offR, offC, rec)) return i;
            }
        }
    }
    return -1;
}

int executeRecipe(CraftingGrid& grid, int recipeIdx) {
    if (recipeIdx < 0 || recipeIdx >= SHAPED_RECIPE_COUNT) return 0;
    grid.consumeAll();
    return SHAPED_RECIPES[recipeIdx].resultItem;
}
