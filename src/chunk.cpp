// chunk.cpp - 区块网格构建（面剔除算法）
// 使用 raylib Mesh/UploadMesh，但顶点数据用 RL_MALLOC 分配（兼容 RL_FREE）。
// UploadMesh 后手动清零 vboId[3][4] 消灭垃圾值，防止 UnloadMesh 卡死。
#include "chunk.h"
#include "texture_atlas.h"
#include "rlgl.h"
#include "world.h"       // 用于查询箱子朝向
#include <vector>
#include <cstring>
#include <cstdlib>

void chunkInit(Chunk& c, int cx, int cz) {
    c.pos = { cx, cz };
    for (int x = 0; x < CHUNK_X; ++x)
        for (int y = 0; y < CHUNK_Y; ++y)
            for (int z = 0; z < CHUNK_Z; ++z)
                c.blocks[x][y][z] = BLOCK_AIR;
    c.mesh = {};
    c.meshReady = false;
    c.meshDirty = false;
    c.generated = false;
}

void chunkUnloadMesh(Chunk& c) {
    if (c.meshReady) {
        // 再次确保 vboId 无垃圾值（安全堡垒）
        if (c.mesh.vboId) c.mesh.vboId[3] = c.mesh.vboId[4] = 0;
        UnloadMesh(c.mesh);
        c.mesh = {};
        c.meshReady = false;
    }
}

// ---------- 面定义 ----------
struct FaceDef {
    Vector3 normal;
    Vector3 corners[4];
    Vector2 uvCorners[4];
};

static const FaceDef FACES[6] = {
    { { 1, 0, 0 },
      { {1,0,1}, {1,0,0}, {1,1,0}, {1,1,1} },
      { {0,1}, {1,1}, {1,0}, {0,0} } },
    { { -1, 0, 0 },
      { {0,0,0}, {0,0,1}, {0,1,1}, {0,1,0} },
      { {0,1}, {1,1}, {1,0}, {0,0} } },
    { { 0, 1, 0 },
      { {0,1,1}, {1,1,1}, {1,1,0}, {0,1,0} },
      { {0,1}, {1,1}, {1,0}, {0,0} } },
    { { 0, -1, 0 },
      { {0,0,0}, {1,0,0}, {1,0,1}, {0,0,1} },
      { {0,1}, {1,1}, {1,0}, {0,0} } },
    { { 0, 0, 1 },
      { {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} },
      { {0,1}, {1,1}, {1,0}, {0,0} } },
    { { 0, 0, -1 },
      { {1,0,0}, {0,0,0}, {0,1,0}, {1,1,0} },
      { {0,1}, {1,1}, {1,0}, {0,0} } },
};

enum FaceKind { FK_SIDE, FK_TOP, FK_BOTTOM };
static const FaceKind faceKind[6] = {
    FK_SIDE, FK_SIDE, FK_TOP, FK_BOTTOM, FK_SIDE, FK_SIDE
};
static const int TRI_IDX[6] = { 0, 1, 2, 0, 2, 3 };

void chunkBuildMesh(Chunk& c, const TextureAtlas& atlas,
                    BlockType (*getBlockGlobal)(int, int, int, void*),
                    void* userData) {
    chunkUnloadMesh(c);

    std::vector<float> verts;
    std::vector<float> norms;
    std::vector<float> tcoords;

    int baseX = c.pos.cx * CHUNK_X;
    int baseZ = c.pos.cz * CHUNK_Z;

    for (int x = 0; x < CHUNK_X; ++x) {
        for (int y = 0; y < CHUNK_Y; ++y) {
            for (int z = 0; z < CHUNK_Z; ++z) {
                BlockType t = c.blocks[x][y][z];
                if (t == BLOCK_AIR) continue;

	                int wx = baseX + x, wy = y, wz = baseZ + z;
	                int texSide = sideTex(t), texTop = topTex(t), texBottom = bottomTex(t);
	
	                // 箱子正面朝向：查找 facing，决定哪个面用 frontTex
	                int frontFace = -1;
	                if (t == BLOCK_CHEST) {
	                    long long ck = chestKey(wx, wy, wz);
	                    auto& chestSys = ((World*)userData)->chestSys;
	                    auto cit = chestSys.chests.find(ck);
	                    if (cit != chestSys.chests.end() && cit->second.initialized) {
	                        int f = cit->second.facing; // 0=+Z,1=+X,2=-Z,3=-X
	                        if (f == 0) frontFace = 4;  // +Z 面
	                        else if (f == 1) frontFace = 0; // +X 面
	                        else if (f == 2) frontFace = 5; // -Z 面
	                        else if (f == 3) frontFace = 1; // -X 面
	                    }
	                }

                for (int f = 0; f < 6; ++f) {
                    const FaceDef& fd = FACES[f];
                    int nx = wx + (int)fd.normal.x;
                    int ny = wy + (int)fd.normal.y;
                    int nz = wz + (int)fd.normal.z;

                    if (ny < 0) continue;
                    BlockType neighbor;
                    if (ny >= CHUNK_Y) neighbor = BLOCK_AIR;
                    else neighbor = getBlockGlobal(nx, ny, nz, userData);

                    bool visible;
                    if (t == BLOCK_WATER) {
                        // 水：只对同层水隐藏面，对其他所有方块（空气/固体/叶）都渲染
                        visible = (neighbor != BLOCK_WATER);
                    } else {
                        visible = (neighbor == BLOCK_AIR) || (isTransparent(neighbor) && neighbor != t);
                    }
                    if (!visible) continue;

	                    int texIdx = texSide;
	                    if (faceKind[f] == FK_TOP) texIdx = texTop;
	                    else if (faceKind[f] == FK_BOTTOM) texIdx = texBottom;
	                    // 箱子正面覆盖
	                    if (t == BLOCK_CHEST && f == frontFace) texIdx = frontTex(t);

	                    float u0 = atlas.u[texIdx], v0 = atlas.v[texIdx];
	                    float uw = atlas.w, vh = atlas.h;
	                    // UV 内缩半像素，防止图集相邻瓦片边缘的纹理出血
	                    constexpr float UV_INSET_U = 0.5f / (float)ATLAS_W;
	                    constexpr float UV_INSET_V = 0.5f / (float)ATLAS_H;
	                    float uBase = u0 + UV_INSET_U, uScale = uw - 2.0f * UV_INSET_U;
	                    float vBase = v0 + UV_INSET_V, vScale = vh - 2.0f * UV_INSET_V;

	                    for (int tri = 0; tri < 6; ++tri) {
	                        int k = TRI_IDX[tri];
	                        verts.push_back((float)wx + fd.corners[k].x);
	                        verts.push_back((float)wy + fd.corners[k].y);
	                        verts.push_back((float)wz + fd.corners[k].z);
	                        norms.push_back(fd.normal.x);
	                        norms.push_back(fd.normal.y);
	                        norms.push_back(fd.normal.z);
	                        tcoords.push_back(uBase + fd.uvCorners[k].x * uScale);
	                        tcoords.push_back(vBase + fd.uvCorners[k].y * vScale);
	                    }
                }
            }
        }
    }

    if (verts.empty()) {
        c.mesh = {};
        c.meshReady = true;
        c.meshDirty = false;
        return;
    }

    // 用 RL_MALLOC (= malloc) 分配，UnloadMesh 的 RL_FREE (= free) 才能正确释放
    int vc = (int)(verts.size() / 3);
    float* v = (float*)RL_MALLOC(verts.size() * sizeof(float));
    float* n = (float*)RL_MALLOC(norms.size() * sizeof(float));
    float* t = (float*)RL_MALLOC(tcoords.size() * sizeof(float));
    memcpy(v, verts.data(),   verts.size() * sizeof(float));
    memcpy(n, norms.data(),   norms.size() * sizeof(float));
    memcpy(t, tcoords.data(), tcoords.size() * sizeof(float));

    Mesh m = {};
    m.vertexCount   = vc;
    m.triangleCount = vc / 3;
    m.vertices   = v;
    m.normals    = n;
    m.texcoords  = t;

    UploadMesh(&m, false);

    // ★★★★★ 关键修复：UploadMesh 用 RL_MALLOC 分配 vboId[5]，
    // 只填了前 3 项（pos/norm/tex），vboId[3]（颜色）和 vboId[4]（索引）是垃圾值。
    // UnloadMesh 无条件遍历释放全部 5 个 → glDeleteBuffers(垃圾) → GPU 卡死。
    // 这里手动清零。
    if (m.vboId) { m.vboId[3] = 0; m.vboId[4] = 0; }

    c.mesh = m;
    c.meshReady = true;
    c.meshDirty = false;
}
