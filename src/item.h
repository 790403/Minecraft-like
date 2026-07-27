// item.h - 物品类型、工具等级、配方产物
#pragma once
#include "block.h"

enum ItemType : int {
    // 原有方块（作为物品）
    ITEM_GRASS = 1,
    ITEM_DIRT = 2,
    ITEM_STONE = 3,
    ITEM_SAND = 4,
    ITEM_WOOD = 5,
    ITEM_LEAVES = 6,
    ITEM_WATER = 7,   // 不可拾取
    ITEM_PLANK = 8,
    ITEM_COBBLE = 9,
    ITEM_BEDROCK = 10, // 不可拾取

    // 新增方块
    ITEM_COAL_ORE = 12,
    ITEM_IRON_ORE = 13,
    ITEM_GOLD_ORE = 14,
    ITEM_DIAMOND_ORE = 15,
    ITEM_CRAFTING_TABLE = 16,
    ITEM_FURNACE = 17,
    ITEM_CHEST = 18,

    // 原材料
    ITEM_COAL = 100,
    ITEM_RAW_IRON = 101,
    ITEM_RAW_GOLD = 102,
    ITEM_DIAMOND = 103,
    ITEM_STICK = 104,

    // 成品
    ITEM_IRON_INGOT = 200,
    ITEM_GOLD_INGOT = 201,

    // 工具
    ITEM_WOOD_PICKAXE = 300,
    ITEM_STONE_PICKAXE = 301,
    ITEM_IRON_PICKAXE = 302,
    ITEM_DIAMOND_PICKAXE = 303,
    ITEM_GOLD_PICKAXE = 304,

    // 食物
    ITEM_APPLE = 400,
    ITEM_BREAD = 401,
    ITEM_RAW_BEEF = 402,
    ITEM_RAW_PORK = 403,
    ITEM_RAW_CHICKEN = 404,

    ITEM_NONE = 0,
};

// 工具等级
enum ToolTier {
    TIER_HAND = 0,
    TIER_WOOD = 1,
    TIER_STONE = 2,
    TIER_IRON = 3,
    TIER_DIAMOND = 4,
};

// 获取物品对应的工具等级（非工具返回 TIER_HAND）
inline ToolTier getToolTier(int item) {
    switch (item) {
        case ITEM_WOOD_PICKAXE:   return TIER_WOOD;
        case ITEM_STONE_PICKAXE:  return TIER_STONE;
        case ITEM_IRON_PICKAXE:   return TIER_IRON;
        case ITEM_DIAMOND_PICKAXE:return TIER_DIAMOND;
        case ITEM_GOLD_PICKAXE:   return TIER_WOOD; // 金镐采集等级同木镐，但速度极快
        default:                  return TIER_HAND;
    }
}

// 工具对特定方块的效率系数（高于 1.0 即为加速）
// 返回 0 = 此工具不适用于该方块
inline float getToolEfficiency(int item, int blockType) {
    ToolTier tier = getToolTier(item);
    // 手：基础速度
    if (tier == TIER_HAND) return 1.0f;
    // 镐子：仅加速石头/矿石/圆石/熔炉等石质方块
    bool isPickBlock = (blockType == (int)BLOCK_STONE || blockType == (int)BLOCK_COBBLE
                     || blockType == (int)BLOCK_COAL_ORE || blockType == (int)BLOCK_IRON_ORE
                     || blockType == (int)BLOCK_GOLD_ORE || blockType == (int)BLOCK_DIAMOND_ORE
                     || blockType == (int)BLOCK_FURNACE);
    if (!isPickBlock) return 1.0f; // 非石质方块不加速
    switch (item) {
        case ITEM_WOOD_PICKAXE:   return 2.0f;
        case ITEM_STONE_PICKAXE:  return 4.0f;
        case ITEM_IRON_PICKAXE:   return 6.0f;
        case ITEM_DIAMOND_PICKAXE:return 9.0f;
        case ITEM_GOLD_PICKAXE:   return 12.0f; // 金镐速度极快
        default:                  return 1.0f;
    }
}

// 物品名称
inline const char* itemName(int item) {
    switch (item) {
        case ITEM_GRASS: return "草方块"; case ITEM_DIRT: return "泥土";
        case ITEM_STONE: return "石头"; case ITEM_SAND: return "沙子";
        case ITEM_WOOD: return "原木"; case ITEM_LEAVES: return "树叶";
        case ITEM_PLANK: return "木板"; case ITEM_COBBLE: return "圆石";
        case ITEM_COAL_ORE: return "煤矿"; case ITEM_IRON_ORE: return "铁矿";
        case ITEM_GOLD_ORE: return "金矿"; case ITEM_DIAMOND_ORE: return "钻石矿";
        case ITEM_CRAFTING_TABLE: return "工作台";
        case ITEM_FURNACE: return "熔炉";
        case ITEM_CHEST: return "箱子";
        case ITEM_COAL: return "煤炭"; case ITEM_RAW_IRON: return "粗铁";
        case ITEM_RAW_GOLD: return "粗金"; case ITEM_DIAMOND: return "钻石";
        case ITEM_STICK: return "木棍";
        case ITEM_IRON_INGOT: return "铁锭"; case ITEM_GOLD_INGOT: return "金锭";
        case ITEM_WOOD_PICKAXE: return "木镐"; case ITEM_STONE_PICKAXE: return "石镐";
        case ITEM_IRON_PICKAXE: return "铁镐"; case ITEM_DIAMOND_PICKAXE: return "钻石镐";
        case ITEM_GOLD_PICKAXE: return "金镐";
        case ITEM_APPLE: return "苹果"; case ITEM_BREAD: return "面包";
        case ITEM_RAW_BEEF: return "生牛肉"; case ITEM_RAW_PORK: return "生猪排";
        case ITEM_RAW_CHICKEN: return "生鸡肉";
        default: return "未知";
    }
}

// 食物饱腹值（0=不是食物）
inline int foodHunger(int item) {
    switch (item) {
        case ITEM_APPLE:  return 4;
        case ITEM_BREAD:  return 6;
        case ITEM_RAW_BEEF: return 3;
        case ITEM_RAW_PORK: return 3;
        case ITEM_RAW_CHICKEN: return 2;
        default:          return 0;
    }
}

// 是否为食物
inline bool isFood(int item) { return foodHunger(item) > 0; }
inline bool isPlaceableItem(int item) {
    return (item > 0 && item < 20) || item == ITEM_FURNACE;
}
inline BlockType itemToBlock(int item) {
    switch (item) {
        case ITEM_GRASS: return BLOCK_GRASS; case ITEM_DIRT: return BLOCK_DIRT;
        case ITEM_STONE: return BLOCK_STONE; case ITEM_SAND: return BLOCK_SAND;
        case ITEM_WOOD: return BLOCK_WOOD; case ITEM_LEAVES: return BLOCK_LEAVES;
        case ITEM_PLANK: return BLOCK_PLANK; case ITEM_COBBLE: return BLOCK_COBBLE;
        case ITEM_COAL_ORE: return BLOCK_COAL_ORE; case ITEM_IRON_ORE: return BLOCK_IRON_ORE;
        case ITEM_GOLD_ORE: return BLOCK_GOLD_ORE; case ITEM_DIAMOND_ORE: return BLOCK_DIAMOND_ORE;
        case ITEM_CRAFTING_TABLE: return BLOCK_CRAFTING_TABLE;
        case ITEM_FURNACE: return BLOCK_FURNACE;
        case ITEM_CHEST: return BLOCK_CHEST;
        default: return BLOCK_AIR;
    }
}
