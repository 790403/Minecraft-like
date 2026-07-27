// world.cpp - 无限世界管理 + 地形生成
#include "world.h"
#include "noise.h"
#include "drop.h"
#include "rlgl.h"
#include "raymath.h"
#include <cmath>
#include <algorithm>

// 地形参数
constexpr int SEA_LEVEL = 24;          // 海平面高度
constexpr int TERRAIN_BASE = 26;       // 基础地形高度
constexpr int TERRAIN_AMP = 14;        // 起伏幅度

// ---------- 地形生成 ----------
// 对给定区块坐标，填充方块数据。基于多层噪声：
//  - 高度噪声：决定地表高度
//  - 群系噪声：决定是草地还是沙漠
//  - 树木：随机在草地上种植
static void generateTerrain(Chunk& c, unsigned int seed) {
    int baseX = c.pos.cx * CHUNK_X;
    int baseZ = c.pos.cz * CHUNK_Z;

    for (int x = 0; x < CHUNK_X; ++x) {
        for (int z = 0; z < CHUNK_Z; ++z) {
            int wx = baseX + x;
            int wz = baseZ + z;

            // 高度：fbm 叠加
            float hnoise = fbm2D((float)wx * 0.025f, (float)wz * 0.025f, 4, 0.5f, 1.0f);
            // 再叠加一层低频大起伏，增加山脉感
            float mnoise = fbm2D((float)wx * 0.008f, (float)wz * 0.008f, 2, 0.5f, 1.0f);
            int height = TERRAIN_BASE + (int)(hnoise * TERRAIN_AMP) + (int)(mnoise * 18);
            if (height < 1) height = 1;
            if (height >= CHUNK_Y - 1) height = CHUNK_Y - 2;

            // 群系：低频噪声 < 0.35 为沙漠
            float bnoise = fbm2D((float)wx * 0.01f + 100.0f, (float)wz * 0.01f + 100.0f, 2, 0.5f, 1.0f);
            bool desert = (bnoise < 0.38f);

            for (int y = 0; y <= height; ++y) {
                BlockType t;
                if (y == 0) {
                    t = BLOCK_BEDROCK;
                } else if (y == height) {
                    // 地表层
                    if (height <= SEA_LEVEL + 1) {
                        t = BLOCK_SAND;     // 海边沙滩
                    } else if (desert) {
                        t = BLOCK_SAND;
                    } else {
                        t = BLOCK_GRASS;
                    }
                } else if (y >= height - 3) {
                    // 表层下几格
                    if (desert) t = BLOCK_SAND;
                    else t = BLOCK_DIRT;
                } else {
                    t = BLOCK_STONE;
                }
                c.blocks[x][y][z] = t;
            }

            // 海水填充
            if (height < SEA_LEVEL) {
                for (int y = height + 1; y <= SEA_LEVEL; ++y)
                    c.blocks[x][y][z] = BLOCK_WATER;
            }

            // --- 矿物生成（密度提升）---
            // --- 矿物生成 (独立检测，每种矿石用自己的随机数) ---
            for (int y = 1; y <= std::min(50, height - 2); ++y) {
                if (c.blocks[x][y][z] != BLOCK_STONE) continue;

                // 每种矿石独立随机，互不干扰
                auto tryOre = [&](int seedBase, float prob, BlockType ore, int minVein, int maxVein, int yMin, int yMax, bool cond) {
                    if (!cond || y < yMin || y > yMax) return;
                    unsigned h2 = (unsigned)(baseX + x) * (374761393u + (unsigned)(seedBase * 7))
                                + (unsigned)y * (668265263u + (unsigned)(seedBase * 13))
                                + (unsigned)(baseZ + z) * (83492791u + (unsigned)(seedBase * 17))
                                + seed * (1274126177u + (unsigned)(seedBase));
                    h2 = (h2 ^ (h2 >> 13)) * 1274126177u; h2 ^= h2 >> 16;
                    if ((float)(h2 & 0xFFFF) / 65536.0f >= prob) return;
                    int vn = minVein + (int)((float)(h2 & 0xFFFF) / 65536.0f * (maxVein - minVein + 1));
                    int placed = 0;
                    for (int i = 0; i < vn * 3 && placed < vn; ++i) {
                        unsigned rh = h2 * 374761393u + (unsigned)i * 668265263u;
                        rh = (rh ^ (rh >> 13)) * 1274126177u; rh ^= rh >> 16;
                        int ox2 = x + (int)(((rh & 0xFFFF) / 65536.0f) * 4.0f - 2.0f);
                        rh = rh * 19349663u; rh = (rh ^ (rh >> 13)) * 1274126177u; rh ^= rh >> 16;
                        int oy2 = y + (int)(((rh & 0xFFFF) / 65536.0f) * 4.0f - 2.0f);
                        rh = rh * 83492791u; rh = (rh ^ (rh >> 13)) * 1274126177u; rh ^= rh >> 16;
                        int oz2 = z + (int)(((rh & 0xFFFF) / 65536.0f) * 4.0f - 2.0f);
                        if (ox2 >= 0 && ox2 < CHUNK_X && oy2 >= 0 && oy2 < CHUNK_Y && oz2 >= 0 && oz2 < CHUNK_Z)
                            if (c.blocks[ox2][oy2][oz2] == BLOCK_STONE) { c.blocks[ox2][oy2][oz2] = ore; ++placed; }
                    }
                };

                // 煤矿: y=10~55,  1/40, 矿脉 3~12
                tryOre(1, 0.025f, BLOCK_COAL_ORE,    3, 12, 10, 55, true);
                // 铁矿: y=0~50,   1/65, 矿脉 2~7
                tryOre(2, 0.015f, BLOCK_IRON_ORE,    2,  7,  0, 50, true);
                // 金矿: y=0~28,   1/140,矿脉 1~4, 不在沙漠
                tryOre(3, 0.007f, BLOCK_GOLD_ORE,    1,  4,  0, 28, !desert);
                // 钻石: y=0~16,   1/250,矿脉 1~3
                tryOre(4, 0.004f, BLOCK_DIAMOND_ORE, 1,  3,  0, 16, true);
	            }

	        }
    }

    // ---------- 树木生成 ----------
    // 用确定性随机，让相同坐标长相同的树。
    for (int x = 2; x < CHUNK_X - 2; ++x) {
        for (int z = 2; z < CHUNK_Z - 2; ++z) {
            int wx = baseX + x;
            int wz = baseZ + z;

            // 树密度
            unsigned int h = (unsigned int)wx * 374761393u + (unsigned int)wz * 668265263u + seed * 1274126177u;
            h = (h ^ (h >> 13)) * 1274126177u;
            h ^= h >> 16;
            if ((h & 0x3F) != 0) continue;  // ~1/64 概率

            // 找地表高度
            int height = 0;
            for (int y = CHUNK_Y - 1; y >= 0; --y) {
                if (isSolid(c.blocks[x][y][z]) && c.blocks[x][y][z] != BLOCK_WATER) {
                    height = y;
                    break;
                }
            }
            // 仅在草地上种树
            if (c.blocks[x][height][z] != BLOCK_GRASS) continue;
            if (height + 6 >= CHUNK_Y) continue;

            int trunkH = 4 + (int)((h >> 8) & 0x3);  // 4-7 格树干

            // 树干
            for (int i = 1; i <= trunkH; ++i) {
                c.blocks[x][height + i][z] = BLOCK_WOOD;
            }
            int topY = height + trunkH;

            // 树叶球：以树顶为中心的 3x3x3 + 顶部十字
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    for (int dz = -2; dz <= 2; ++dz) {
                        if (dx == 0 && dz == 0 && dy <= 0) continue; // 留出树干位置
                        int lx = x + dx, ly = topY + dy, lz = z + dz;
                        if (lx < 0 || lx >= CHUNK_X || lz < 0 || lz >= CHUNK_Z) continue;
                        if (ly < 0 || ly >= CHUNK_Y) continue;
                        // 圆形边缘裁剪
                        if (std::abs(dx) == 2 && std::abs(dz) == 2) {
                            if (((h >> (dy + 10)) & 1) == 0) continue;
                        }
                        if (c.blocks[lx][ly][lz] == BLOCK_AIR) {
                            c.blocks[lx][ly][lz] = BLOCK_LEAVES;
                        }
                    }
                }
            }
            // 顶部树叶
            if (topY + 2 < CHUNK_Y) c.blocks[x][topY + 2][z] = BLOCK_LEAVES;
            if (x + 1 < CHUNK_X && topY + 1 < CHUNK_Y) c.blocks[x + 1][topY + 1][z] = BLOCK_LEAVES;
            if (x - 1 >= 0 && topY + 1 < CHUNK_Y) c.blocks[x - 1][topY + 1][z] = BLOCK_LEAVES;
            if (z + 1 < CHUNK_Z && topY + 1 < CHUNK_Y) c.blocks[x][topY + 1][z + 1] = BLOCK_LEAVES;
            if (z - 1 >= 0 && topY + 1 < CHUNK_Y) c.blocks[x][topY + 1][z - 1] = BLOCK_LEAVES;
        }
    }

    c.generated = true;
    c.meshDirty = true;
}

// ---------- 区块查询辅助 ----------
static Chunk* getChunkAt(World& w, int cx, int cz) {
    auto it = w.chunks.find({ cx, cz });
    if (it == w.chunks.end()) return nullptr;
    return it->second;
}
static const Chunk* getChunkAtConst(const World& w, int cx, int cz) {
    auto it = w.chunks.find({ cx, cz });
    if (it == w.chunks.end()) return nullptr;
    return it->second;
}

// 确保区块存在并已生成（用于邻居查询时按需生成边界区块）
static Chunk* ensureChunk(World& w, int cx, int cz) {
    Chunk* c = getChunkAt(w, cx, cz);
    if (!c) {
        c = new Chunk();
        chunkInit(*c, cx, cz);
        generateTerrain(*c, w.seed);
        w.chunks[{ cx, cz }] = c;
    }
    return c;
}

// ---------- 公共接口 ----------
void worldInit(World& w, unsigned int seed, int loadRadius) {
    w.chunks.clear();
    w.seed = seed;
    w.loadRadius = loadRadius;
    noiseInit(seed);
    w.atlas = atlasLoad();

    // raylib 默认材质带纹理和光照
    w.material = LoadMaterialDefault();
    SetMaterialTexture(&w.material, MATERIAL_MAP_ALBEDO, w.atlas.texture);
    w.material.maps[MATERIAL_MAP_ALBEDO].color = WHITE;

    w.showFog = true;
    w.fogStart = (float)(loadRadius - 1) * 16.0f;
    w.fogEnd = (float)(loadRadius + 0.5f) * 16.0f;
    w.fogColor = { 150, 190, 230, 255 };
}

void worldUnload(World& w) {
    for (auto& kv : w.chunks) {
        chunkUnloadMesh(*kv.second);
        delete kv.second;
    }
    w.chunks.clear();
    atlasUnload(w.atlas);
}

BlockType worldGetBlockForBuild(int x, int y, int z, void* userData) {
    World* w = (World*)userData;
    if (y < 0 || y >= CHUNK_Y) return BLOCK_AIR;
    int cx = (x < 0) ? (x - CHUNK_X + 1) / CHUNK_X : x / CHUNK_X;
    int cz = (z < 0) ? (z - CHUNK_Z + 1) / CHUNK_Z : z / CHUNK_Z;
    const Chunk* c = getChunkAtConst(*w, cx, cz);
    if (!c || !c->generated) return BLOCK_AIR;
    int lx = x - cx * CHUNK_X;
    int lz = z - cz * CHUNK_Z;
    return c->blocks[lx][y][lz];
}

BlockType worldGetBlock(const World& w, int x, int y, int z) {
    if (y < 0 || y >= CHUNK_Y) return BLOCK_AIR;
    int cx = (x < 0) ? (x - CHUNK_X + 1) / CHUNK_X : x / CHUNK_X;
    int cz = (z < 0) ? (z - CHUNK_Z + 1) / CHUNK_Z : z / CHUNK_Z;
    const Chunk* c = getChunkAtConst(w, cx, cz);
    if (!c || !c->generated) return BLOCK_AIR;
    int lx = x - cx * CHUNK_X;
    int lz = z - cz * CHUNK_Z;
    return c->blocks[lx][y][lz];
}

void worldSetBlock(World& w, int x, int y, int z, BlockType t) {
    if (y < 0 || y >= CHUNK_Y) return;
    int cx = (x < 0) ? (x - CHUNK_X + 1) / CHUNK_X : x / CHUNK_X;
    int cz = (z < 0) ? (z - CHUNK_Z + 1) / CHUNK_Z : z / CHUNK_Z;
    Chunk* c = ensureChunk(w, cx, cz);
    int lx = x - cx * CHUNK_X;
    int lz = z - cz * CHUNK_Z;

        // 如果破坏的是箱子，掉落内容物
        if (c->blocks[lx][y][lz] == BLOCK_CHEST && t == BLOCK_AIR) {
            w.chestSys.dropContents(w.drops, x, y, z);
        }
        // 如果破坏的是熔炉，掉落内容物
        if (c->blocks[lx][y][lz] == BLOCK_FURNACE && t == BLOCK_AIR) {
            w.furnaceSys.dropContents(w.drops, x, y, z);
        }

    c->blocks[lx][y][lz] = t;
    c->meshDirty = true;

    // 若方块在区块边界，邻居区块也要重建（它们的面剔除依赖此方块）
    if (lx == 0) { Chunk* n = getChunkAt(w, cx - 1, cz); if (n) n->meshDirty = true; }
    if (lx == CHUNK_X - 1) { Chunk* n = getChunkAt(w, cx + 1, cz); if (n) n->meshDirty = true; }
    if (lz == 0) { Chunk* n = getChunkAt(w, cx, cz - 1); if (n) n->meshDirty = true; }
    if (lz == CHUNK_Z - 1) { Chunk* n = getChunkAt(w, cx, cz + 1); if (n) n->meshDirty = true; }
}

void worldUpdate(World& w, Vector3 playerPos, int maxGenPerFrame) {
    // 玩家所在区块
    int pcx = (int)floorf(playerPos.x / CHUNK_X);
    int pcz = (int)floorf(playerPos.z / CHUNK_Z);

    // 1) 卸载半径外的区块
    int unloadR = w.loadRadius + 2;
    for (auto it = w.chunks.begin(); it != w.chunks.end(); ) {
        int dx = it->first.cx - pcx;
        int dz = it->first.cz - pcz;
        if (std::abs(dx) > unloadR || std::abs(dz) > unloadR) {
            chunkUnloadMesh(*it->second);
            delete it->second;
            it = w.chunks.erase(it);
        } else {
            ++it;
        }
    }

    // 2) 按到玩家的距离排序，优先加载近的未生成区块
    std::vector<ChunkPos> candidates;
    for (int dx = -w.loadRadius; dx <= w.loadRadius; ++dx) {
        for (int dz = -w.loadRadius; dz <= w.loadRadius; ++dz) {
            if (dx * dx + dz * dz > w.loadRadius * w.loadRadius) continue;
            ChunkPos p{ pcx + dx, pcz + dz };
            if (w.chunks.find(p) == w.chunks.end()) {
                candidates.push_back(p);
            }
        }
    }
    std::sort(candidates.begin(), candidates.end(),
        [pcx, pcz](const ChunkPos& a, const ChunkPos& b) {
            int da = (a.cx - pcx) * (a.cx - pcx) + (a.cz - pcz) * (a.cz - pcz);
            int db = (b.cx - pcx) * (b.cx - pcx) + (b.cz - pcz) * (b.cz - pcz);
            return da < db;
        });

    int generated = 0;
    for (const ChunkPos& p : candidates) {
        if (generated >= maxGenPerFrame) break;
        ensureChunk(w, p.cx, p.cz);
        ++generated;
    }
}

void worldBuildDirtyMeshes(World& w, int maxPerFrame) {
    // 优先重建玩家所在区块
    int built = 0;
    // 先扫一遍按顺序重建（简单可靠）
    for (auto& kv : w.chunks) {
        if (built >= maxPerFrame) break;
        Chunk* c = kv.second;
        if (c->generated && c->meshDirty) {
            chunkBuildMesh(*c, w.atlas, worldGetBlockForBuild, (void*)&w);
            ++built;
        }
    }
}

void worldDraw(const World& w) {
    Matrix identity = MatrixIdentity();
    for (auto& kv : w.chunks) {
        const Chunk* c = kv.second;
        if (!c->generated || !c->meshReady || c->mesh.vertexCount <= 0) continue;
        DrawMesh(c->mesh, w.material, identity);
    }
    // 绘制掉落物（需要相机，但此处拿不到，由外面绘制）
}

int worldFindGroundY(const World& w, int x, int z) {
    for (int y = CHUNK_Y - 1; y >= 0; --y) {
        BlockType t = worldGetBlock(w, x, y, z);
        if (isSolid(t)) return y + 1;
    }
    return SEA_LEVEL + 1;
}
