// block.h - 方块类型定义与属性查表（含矿石、工作台、工具等级）
#pragma once
#include "raylib.h"

// ===== 方块类型 =====
enum BlockType : unsigned char {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_SAND,
    BLOCK_WOOD,
    BLOCK_LEAVES,
    BLOCK_WATER,
    BLOCK_PLANK,
    BLOCK_COBBLE,
    BLOCK_BEDROCK,
    // ---- 新增矿石 ----
    BLOCK_COAL_ORE,
    BLOCK_IRON_ORE,
    BLOCK_GOLD_ORE,
    BLOCK_DIAMOND_ORE,
    // ---- 工作台 ----
    BLOCK_CRAFTING_TABLE,
    BLOCK_FURNACE,
    BLOCK_CHEST,
    BLOCK_COUNT
};

// ===== 纹理索引（图集中位置）=====
enum TexIndex {
    TEX_GRASS_TOP = 0,
    TEX_GRASS_SIDE,
    TEX_DIRT,
    TEX_STONE,
    TEX_SAND,
    TEX_WOOD_SIDE,
    TEX_WOOD_TOP,
    TEX_LEAVES,
    TEX_PLANK,
    TEX_COBBLE,
    TEX_BEDROCK,
    TEX_WATER,
    // 新纹理（原 12-15 空槽）
    TEX_COAL_ORE = 12,
    TEX_IRON_ORE,
    TEX_GOLD_ORE,
    TEX_DIAMOND_ORE,
    TEX_CRAFT_TOP,
    TEX_CRAFT_SIDE,
    // TEX_CRAFT_BOTTOM = TEX_CRAFT_SIDE（复用）
	    TEX_FURNACE = 18,
	    TEX_CHEST = 19,
	    TEX_FURNACE_UNLIT = 20,  // 熔炉未点燃状态
    // ---- 物品纹理（无对应方块的全新图标）----
    TEX_ITEM_COAL = 21,
    TEX_ITEM_RAW_IRON,
    TEX_ITEM_RAW_GOLD,
    TEX_ITEM_DIAMOND,
    TEX_ITEM_STICK,
    TEX_ITEM_IRON_INGOT,
    TEX_ITEM_GOLD_INGOT,
    TEX_ITEM_WOOD_PICKAXE,
    TEX_ITEM_STONE_PICKAXE,
    TEX_ITEM_IRON_PICKAXE,
    TEX_ITEM_GOLD_PICKAXE,
    TEX_ITEM_DIAMOND_PICKAXE,
    // ---- 食物（继续使用后续槽位）----
    TEX_ITEM_APPLE = 33,
    TEX_ITEM_BREAD,
    TEX_ITEM_RAW_BEEF,
	    TEX_ITEM_RAW_PORK,
	    TEX_ITEM_RAW_CHICKEN,
	    TEX_CHEST_SIDE = 38,
	    TEX_ITEM_COUNT
};

// ===== 图集尺寸 =====
constexpr int ATLAS_COLS = 4;
constexpr int ATLAS_ROWS = 16;          // 4×16=64 槽, 64×256 像素 (2^k)
constexpr int ATLAS_TILE = 16;
constexpr int ATLAS_W = ATLAS_COLS * ATLAS_TILE;   // 64
constexpr int ATLAS_H = ATLAS_ROWS * ATLAS_TILE;   // 128 (2^7, NPOT safe)

// ===== 物理属性 =====
inline bool isSolid(BlockType t) {
    switch (t) {
        case BLOCK_AIR:    return false;
        case BLOCK_WATER:  return false;
        default:           return true;
    }
}
inline bool isTransparent(BlockType t) {
    return t == BLOCK_AIR || t == BLOCK_WATER || t == BLOCK_LEAVES;
}
inline bool isBreakable(BlockType t) {
    return t != BLOCK_AIR && t != BLOCK_BEDROCK;
}

// ===== 纹理查表 =====
inline int sideTex(BlockType t) {
    switch (t) {
        case BLOCK_GRASS:            return TEX_GRASS_SIDE;
        case BLOCK_WOOD:             return TEX_WOOD_SIDE;
        case BLOCK_DIRT:             return TEX_DIRT;
        case BLOCK_STONE:            return TEX_STONE;
        case BLOCK_SAND:             return TEX_SAND;
        case BLOCK_LEAVES:           return TEX_LEAVES;
        case BLOCK_WATER:            return TEX_WATER;
        case BLOCK_PLANK:            return TEX_PLANK;
        case BLOCK_COBBLE:           return TEX_COBBLE;
        case BLOCK_BEDROCK:          return TEX_BEDROCK;
        case BLOCK_COAL_ORE:         return TEX_COAL_ORE;
        case BLOCK_IRON_ORE:         return TEX_IRON_ORE;
        case BLOCK_GOLD_ORE:         return TEX_GOLD_ORE;
        case BLOCK_DIAMOND_ORE:      return TEX_DIAMOND_ORE;
        case BLOCK_CRAFTING_TABLE:   return TEX_CRAFT_SIDE;
	        case BLOCK_FURNACE:          return TEX_FURNACE;
	        case BLOCK_CHEST:            return TEX_CHEST_SIDE;  // 侧面无锁扣
	        default:                     return TEX_STONE;
    }
}
inline int topTex(BlockType t) {
    switch (t) {
        case BLOCK_GRASS:            return TEX_GRASS_TOP;
        case BLOCK_WOOD:             return TEX_WOOD_TOP;
        case BLOCK_CRAFTING_TABLE:   return TEX_CRAFT_TOP;
	        case BLOCK_FURNACE:          return TEX_COBBLE; // 熔炉顶面用圆石
	        case BLOCK_CHEST:            return TEX_CHEST_SIDE;
	        default:                     return sideTex(t);
    }
}
inline int bottomTex(BlockType t) {
    switch (t) {
        case BLOCK_GRASS:            return TEX_DIRT;
        case BLOCK_WOOD:             return TEX_WOOD_TOP;
        case BLOCK_CRAFTING_TABLE:   return TEX_CRAFT_SIDE;
	        case BLOCK_FURNACE:          return TEX_COBBLE; // 熔炉底面用圆石
	        case BLOCK_CHEST:            return TEX_CHEST_SIDE;
	        default:                     return sideTex(t);
	    }
	}
	inline int frontTex(BlockType t) {
	    switch (t) {
	        case BLOCK_CHEST:            return TEX_CHEST;   // 正面带锁扣
	        case BLOCK_FURNACE:          return TEX_FURNACE;
	        default:                     return sideTex(t);
	    }
	}

// ===== 工具等级（镐子）=====
// 0=手  1=木镐  2=石镐  3=铁镐  4=钻石镐
inline int getMiningLevel(BlockType t) {
    switch (t) {
        case BLOCK_DIRT:   case BLOCK_GRASS:
        case BLOCK_SAND:   case BLOCK_LEAVES:
        case BLOCK_PLANK:  case BLOCK_WOOD:
        case BLOCK_CRAFTING_TABLE: case BLOCK_CHEST:
            return 0;  // 手即可
        case BLOCK_STONE:  case BLOCK_COBBLE:
        case BLOCK_COAL_ORE: case BLOCK_FURNACE:
            return 1;  // 木镐
        case BLOCK_IRON_ORE: case BLOCK_BEDROCK:
            return 2;  // 石镐
        case BLOCK_GOLD_ORE: case BLOCK_DIAMOND_ORE:
            return 3;  // 铁镐
        default: return 0;
    }
}

// 方块硬度（值越大挖得越慢，基数 1.0f 为泥土）
inline float getHardness(BlockType t) {
    switch (t) {
        case BLOCK_DIRT:   case BLOCK_GRASS: return 1.0f;
        case BLOCK_SAND:   case BLOCK_LEAVES: return 0.8f;
        case BLOCK_WOOD:   case BLOCK_PLANK: return 2.0f;
        case BLOCK_STONE:  case BLOCK_COBBLE: return 3.0f;
        case BLOCK_COAL_ORE:   return 4.0f;
        case BLOCK_IRON_ORE:   return 5.0f;
        case BLOCK_GOLD_ORE:   return 6.0f;
        case BLOCK_DIAMOND_ORE:return 8.0f;
        case BLOCK_CRAFTING_TABLE: return 2.5f;
        case BLOCK_FURNACE:   return 5.0f;
        case BLOCK_CHEST:     return 2.5f;
        case BLOCK_BEDROCK:   return 999.0f;
        default: return 1.0f;
    }
}

// 破坏后掉落的物品类型（显式映射到 item.h 的 ItemType）
inline int getDropItem(BlockType t) {
    switch (t) {
        case BLOCK_AIR:          return 0;
        case BLOCK_GRASS:        return 2;   // 掉落泥土
        case BLOCK_DIRT:         return 2;   // ITEM_DIRT
        case BLOCK_STONE:        return 9;   // ITEM_COBBLE
        case BLOCK_SAND:         return 4;   // ITEM_SAND
        case BLOCK_WOOD:         return 5;   // ITEM_WOOD
        case BLOCK_LEAVES:       return 6;   // ITEM_LEAVES
        case BLOCK_WATER:        return 0;   // 不可拾取
        case BLOCK_PLANK:        return 8;   // ITEM_PLANK
        case BLOCK_COBBLE:       return 9;   // ITEM_COBBLE
        case BLOCK_BEDROCK:      return 0;   // 不可破坏
        case BLOCK_COAL_ORE:     return 100; // ITEM_COAL
        case BLOCK_IRON_ORE:     return 101; // ITEM_RAW_IRON
        case BLOCK_GOLD_ORE:     return 102; // ITEM_RAW_GOLD
        case BLOCK_DIAMOND_ORE:  return 103; // ITEM_DIAMOND
        case BLOCK_CRAFTING_TABLE: return 16;// ITEM_CRAFTING_TABLE
        case BLOCK_FURNACE:        return 17;// ITEM_FURNACE
        case BLOCK_CHEST:          return 18;// ITEM_CHEST
        default:                 return 0;
    }
}

inline const char* blockName(BlockType t) {
    switch (t) {
        case BLOCK_AIR:         return "空气";
        case BLOCK_GRASS:       return "草方块";
        case BLOCK_DIRT:        return "泥土";
        case BLOCK_STONE:       return "石头";
        case BLOCK_SAND:        return "沙子";
        case BLOCK_WOOD:        return "原木";
        case BLOCK_LEAVES:      return "树叶";
        case BLOCK_WATER:       return "水";
        case BLOCK_PLANK:       return "木板";
        case BLOCK_COBBLE:      return "圆石";
        case BLOCK_BEDROCK:     return "基岩";
        case BLOCK_COAL_ORE:    return "煤矿";
        case BLOCK_IRON_ORE:    return "铁矿";
        case BLOCK_GOLD_ORE:    return "金矿";
        case BLOCK_DIAMOND_ORE: return "钻石矿";
        case BLOCK_CRAFTING_TABLE: return "工作台";
        case BLOCK_FURNACE:        return "熔炉";
        case BLOCK_CHEST:          return "箱子";
        default:                return "未知";
    }
}
