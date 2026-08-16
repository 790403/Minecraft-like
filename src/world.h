// world.h - 无限世界管理
// 维护以玩家为中心的区块集合，按需生成/卸载区块，
// 并提供跨区块的方块读写接口。包含地形（噪声）和树木生成。
#pragma once
#include "raylib.h"
#include "chunk.h"
#include "texture_atlas.h"
#include "chest.h"
#include "furnace.h"
#include "mob.h"
#include <unordered_map>
#include <vector>

// 前置声明
struct DroppedItem;

struct World {
    std::unordered_map<ChunkPos, Chunk*, ChunkPosHash> chunks;
    TextureAtlas atlas;
    Material material;           // 绘制所有区块共用的材质（绑定图集纹理）
    unsigned int seed;
    int loadRadius;              // 玩家周围加载半径（区块数）
    bool showFog;                // 是否启用雾效
    float fogStart, fogEnd;      // 雾的起止距离
    Color fogColor;

    // 掉落物系统
    std::vector<DroppedItem> drops;
    
    // 箱子系统
    ChestSystem chestSys;
    
    // 熔炉系统
    FurnaceSystem furnaceSys;

    // 生物系统（猪等）
    MobWorld mobs;
};

// 初始化世界（种子、加载半径、加载图集纹理）
void worldInit(World& w, unsigned int seed, int loadRadius);

// 释放所有区块和资源
void worldUnload(World& w);

// 每帧更新：根据玩家位置加载/卸载区块。
// 限制每帧最多生成的区块数避免卡顿。
void worldUpdate(World& w, Vector3 playerPos, int maxGenPerFrame = 1);

// 获取世界绝对坐标的方块。越界（y<0 或 y>=CHUNK_Y）返回 AIR。
BlockType worldGetBlock(const World& w, int x, int y, int z);

// 设置世界绝对坐标的方块（自动标记所在区块网格为脏）。
void worldSetBlock(World& w, int x, int y, int z, BlockType t);

// 同上但用于区块网格构建时的邻居查询（不修改状态）。
BlockType worldGetBlockForBuild(int x, int y, int z, void* userData);

// 重建所有脏区块的网格（每帧限量）
void worldBuildDirtyMeshes(World& w, int maxPerFrame = 2);

// 渲染所有已就绪的区块网格
void worldDraw(const World& w);

// 在某个世界坐标处寻找玩家可站立的地表高度（从顶往下第一个实体方块的上表面）
int worldFindGroundY(const World& w, int x, int z);
