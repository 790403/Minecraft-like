// chunk.h - 区块：方块数据 + 面剔除网格构建
// 使用 raylib Mesh 系统，配合 RL_MALLOC 分配和 vboId 清零修复。
#pragma once
#include "raylib.h"
#include "block.h"
#include "texture_atlas.h"
#include <cstddef>

constexpr int CHUNK_X = 16;
constexpr int CHUNK_Y = 64;
constexpr int CHUNK_Z = 16;

struct ChunkPos {
    int cx, cz;
    bool operator==(const ChunkPos& o) const { return cx == o.cx && cz == o.cz; }
};

struct ChunkPosHash {
    size_t operator()(const ChunkPos& p) const noexcept {
        return ((size_t)(unsigned int)p.cx << 32) | (unsigned int)p.cz;
    }
};

struct Chunk {
    ChunkPos pos;
    BlockType blocks[CHUNK_X][CHUNK_Y][CHUNK_Z];
    Mesh mesh;         // raylib Mesh（经 UploadMesh 后的 GPU 数据 + CPU 指针）
    bool meshReady;
    bool meshDirty;
    bool generated;
};

void chunkInit(Chunk& c, int cx, int cz);
void chunkUnloadMesh(Chunk& c);

inline BlockType chunkGet(const Chunk& c, int x, int y, int z) {
    return c.blocks[x][y][z];
}
inline void chunkSet(Chunk& c, int x, int y, int z, BlockType t) {
    c.blocks[x][y][z] = t;
    c.meshDirty = true;
}

void chunkBuildMesh(Chunk& c, const TextureAtlas& atlas,
                    BlockType (*getBlockGlobal)(int, int, int, void*),
                    void* userData);
