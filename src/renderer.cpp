// renderer.cpp - 渲染：天空、昼夜、HUD、菜单、背包界面
#include "renderer.h"
#include "world.h"
#include "raymath.h"
#include "crafting.h"
#include "rlgl.h"
#include <cmath>
#include <cstdio>
#include <unordered_map>

// ---- 字体 ----
static Font gFont = { 0 };
static bool gFontOK = false;

void loadGameFont() {
    const char* path = "C:/Windows/Fonts/simhei.ttf";
    if (FileExists(path)) {
        std::vector<int> codepoints;
        // ASCII 可打印字符
        for (int c = 32; c <= 126; ++c) codepoints.push_back(c);
        // 常用标点符号
        for (int c = 0x3000; c <= 0x303F; ++c) codepoints.push_back(c);
        // 全角字母数字
        for (int c = 0xFF01; c <= 0xFF5E; ++c) codepoints.push_back(c);
        // CJK 统一表意文字（常用汉字全部覆盖）
        for (int c = 0x4E00; c <= 0x9FFF; ++c) codepoints.push_back(c);

        gFont = LoadFontEx(path, 20, codepoints.data(), (int)codepoints.size());
        if (gFont.texture.id != 0) { SetTextureFilter(gFont.texture, TEXTURE_FILTER_POINT); gFontOK = true; return; }
    }
    gFont = GetFontDefault(); gFontOK = false;
}
void unloadGameFont() { if (gFontOK) { UnloadFont(gFont); gFont = { 0 }; gFontOK = false; } }

static void drawCN(const char* t, int x, int y, int fs, Color c) {
    if (gFontOK) { Vector2 p = { (float)x, (float)y }; DrawTextEx(gFont, t, p, (float)fs, 1.0f, c); }
    else DrawText(t, x, y, fs, c);
}
static int measCN(const char* t, int fs) {
    if (gFontOK) return (int)MeasureTextEx(gFont, t, (float)fs, 1.0f).x;
    return MeasureText(t, fs);
}

// ---- 颜色工具 ----
static Color lerpC(Color a, Color b, float t) {
    return { (unsigned char)(a.r+(b.r-a.r)*t), (unsigned char)(a.g+(b.g-a.g)*t),
             (unsigned char)(a.b+(b.b-a.b)*t), (unsigned char)(a.a+(b.a-a.a)*t) };
}
static float smoothstep(float e0, float e1, float x) {
    float t = (x - e0) / (e1 - e0);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return t * t * (3 - 2 * t);
}

struct SkyKey { float frac; Color top, bottom; };
static const SkyKey SKY_KEYS[4] = {
    {0.00f, {255,150,90,255},{255,200,150,255}}, {0.25f, {90,150,230,255},{175,215,245,255}},
    {0.50f, {200,90,60,255},{240,150,100,255}}, {0.75f, {10,15,40,255},{25,35,70,255}},
};

void getSkyColors(float frac, Color& top, Color& bot) {
    const SkyKey *k0 = &SKY_KEYS[3], *k1 = &SKY_KEYS[0];
    for (int i = 0; i < 3; ++i)
        if (frac >= SKY_KEYS[i].frac && frac < SKY_KEYS[i+1].frac) { k0 = &SKY_KEYS[i]; k1 = &SKY_KEYS[i+1]; break; }
    float span = k1->frac - k0->frac, f = frac - k0->frac;
    if (span < 0) span += 1.0f;
    if (f < 0) f += 1.0f;
    float t = (span > 0) ? (f / span) : 0.0f;
    top = lerpC(k0->top, k1->top, t); bot = lerpC(k0->bottom, k1->bottom, t);
}
float getDayLight(float frac) {
    float d = 0.3f + 0.7f * smoothstep(0.18f, 0.27f, frac) - 0.7f * smoothstep(0.45f, 0.55f, frac);
    if (d < 0.15f) d = 0.15f;
    return d;
}

static void drawGrad(Color top, Color bot) {
    int w = GetScreenWidth(), h = GetScreenHeight();
    for (int i = 0; i < 40; ++i) {
        float t = (float)i / 39.0f;
        DrawRectangle(0, (int)(t * h), w, (int)ceilf((float)h / 40) + 1, lerpC(top, bot, t));
    }
}
void drawSky(const GameTime& gt, const Camera3D& camera) {
    float f = dayFraction(gt); Color t, b; getSkyColors(f, t, b); drawGrad(t, b);
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    // 太阳/月亮：3D 位置投影到屏幕（随视角旋转）
    float angle = (f - 0.25f) * 6.28318f + 1.57f; // 正午时太阳在头顶
    float dist = 500.0f;
    Vector3 sunWorld = {
        camera.position.x + cosf(angle) * dist,
        camera.position.y + sinf(angle) * dist,
        camera.position.z + cosf(angle) * dist * 0.3f
    };
    Vector2 sunScreen = GetWorldToScreen(sunWorld, camera);

    if (f < 0.5f) {
        // 白天：太阳
        if (sunScreen.x > -50 && sunScreen.x < sw + 50 && sunScreen.y > -50 && sunScreen.y < sh + 50) {
            DrawCircle((int)sunScreen.x, (int)sunScreen.y, 30, Fade(YELLOW, 0.95f));
            DrawCircle((int)sunScreen.x, (int)sunScreen.y, 45, Fade(ORANGE, 0.35f));
        }
    }
}

void drawBlockHighlight(const RaycastHit& hit) {
    (void)hit;
    // 不再绘制方块包边
}
void drawCrosshair() {
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
    DrawLine(cx - 10, cy, cx + 10, cy, WHITE);
    DrawLine(cx, cy - 10, cx, cy + 10, WHITE);
}

// ---- 物品纹理辅助 ----
static int getItemTexSlot(int item) {
    // 方块物品：用方块纹理
    if (item >= 1 && item <= 19) {
        BlockType bt = itemToBlock(item);
        if (bt != BLOCK_AIR) return sideTex(bt);
    }
    // 带专属纹理的物品
    switch (item) {
        case ITEM_COAL:         return TEX_ITEM_COAL;
        case ITEM_RAW_IRON:     return TEX_ITEM_RAW_IRON;
        case ITEM_RAW_GOLD:     return TEX_ITEM_RAW_GOLD;
        case ITEM_DIAMOND:      return TEX_ITEM_DIAMOND;
        case ITEM_STICK:        return TEX_ITEM_STICK;
        case ITEM_IRON_INGOT:   return TEX_ITEM_IRON_INGOT;
        case ITEM_GOLD_INGOT:   return TEX_ITEM_GOLD_INGOT;
        case ITEM_WOOD_PICKAXE: return TEX_ITEM_WOOD_PICKAXE;
        case ITEM_STONE_PICKAXE:return TEX_ITEM_STONE_PICKAXE;
        case ITEM_IRON_PICKAXE: return TEX_ITEM_IRON_PICKAXE;
        case ITEM_GOLD_PICKAXE: return TEX_ITEM_GOLD_PICKAXE;
        case ITEM_DIAMOND_PICKAXE:return TEX_ITEM_DIAMOND_PICKAXE;
        case ITEM_APPLE:        return TEX_ITEM_APPLE;
        case ITEM_BREAD:        return TEX_ITEM_BREAD;
        case ITEM_RAW_BEEF:     return TEX_ITEM_RAW_BEEF;
        case ITEM_RAW_PORK:     return TEX_ITEM_RAW_PORK;
        case ITEM_RAW_CHICKEN:  return TEX_ITEM_RAW_CHICKEN;
        default:                return -1;
    }
}

// ---- 离屏 3D 立方体纹理缓存 ----
// 在 3D 模式下渲染方块并缓存在 RenderTexture 中，避免 rlgl 在 2D 模式下失效的问题
struct CubeCacheEntry { RenderTexture2D rt; bool ready; };
static std::unordered_map<BlockType, CubeCacheEntry> g_cubeCache;

static Texture2D getBlockCubeTex(BlockType bt, const TextureAtlas& atlas) {
    auto it = g_cubeCache.find(bt);
    if (it != g_cubeCache.end() && it->second.ready) return it->second.rt.texture;

    // 高分辨率离屏渲染，最近点采样保持像素风格
    int texSize = 96;
    RenderTexture2D rt = LoadRenderTexture(texSize, texSize);
    if (rt.texture.id == 0) return (Texture2D){ 0 };
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);

    BeginTextureMode(rt);
    ClearBackground((Color){ 0, 0, 0, 0 });

	    // MC 原版等距视角：相机位于 (+X, +Y, +Z)，观察 +Z（左）、+X（右）、+Y（顶）三面
	    // 呈现从左下到右上的 / 形透视
	    Camera3D cam = { 0 };
	    cam.position  = { 1.5f, 0.9f, 1.5f };
    cam.target    = { 0, 0, 0 };
    cam.up        = { 0, 1, 0 };
    cam.fovy      = 1.6f;       // 正交投影下直接控制视野高度（世界单位）
    cam.projection = CAMERA_ORTHOGRAPHIC;

    BeginMode3D(cam);

    float sU = atlas.u[sideTex(bt)],   sV = atlas.v[sideTex(bt)];
    float tU = atlas.u[topTex(bt)],    tV = atlas.v[topTex(bt)];
    float bU = atlas.u[bottomTex(bt)], bV = atlas.v[bottomTex(bt)];
    float tw = atlas.w, th = atlas.h;
    const float h = 0.5f;

    rlSetTexture(atlas.texture.id);

    // 前 (+Z)
    rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(sU, sV+th); rlVertex3f(-h, -h,  h);
    rlTexCoord2f(sU+tw, sV+th); rlVertex3f( h, -h,  h);
    rlTexCoord2f(sU+tw, sV);   rlVertex3f( h,  h,  h);
    rlTexCoord2f(sU, sV);      rlVertex3f(-h,  h,  h);
    rlEnd();
    // 后 (-Z)
    rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(sU, sV+th); rlVertex3f( h, -h, -h);
    rlTexCoord2f(sU+tw, sV+th); rlVertex3f(-h, -h, -h);
    rlTexCoord2f(sU+tw, sV);   rlVertex3f(-h,  h, -h);
    rlTexCoord2f(sU, sV);      rlVertex3f( h,  h, -h);
    rlEnd();
    // 上 (+Y)
    rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(tU, tV+th); rlVertex3f(-h,  h,  h);
    rlTexCoord2f(tU+tw, tV+th); rlVertex3f( h,  h,  h);
    rlTexCoord2f(tU+tw, tV);   rlVertex3f( h,  h, -h);
    rlTexCoord2f(tU, tV);      rlVertex3f(-h,  h, -h);
    rlEnd();
    // 下 (-Y)
    rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(bU, bV+th); rlVertex3f(-h, -h, -h);
    rlTexCoord2f(bU+tw, bV+th); rlVertex3f( h, -h, -h);
    rlTexCoord2f(bU+tw, bV);   rlVertex3f( h, -h,  h);
    rlTexCoord2f(bU, bV);      rlVertex3f(-h, -h,  h);
    rlEnd();
    // 左 (-X)
    rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(sU, sV+th); rlVertex3f(-h, -h, -h);
    rlTexCoord2f(sU+tw, sV+th); rlVertex3f(-h, -h,  h);
    rlTexCoord2f(sU+tw, sV);   rlVertex3f(-h,  h,  h);
    rlTexCoord2f(sU, sV);      rlVertex3f(-h,  h, -h);
    rlEnd();
    // 右 (+X)
    rlBegin(RL_QUADS); rlColor4ub(255,255,255,255);
    rlTexCoord2f(sU, sV+th); rlVertex3f( h, -h,  h);
    rlTexCoord2f(sU+tw, sV+th); rlVertex3f( h, -h, -h);
    rlTexCoord2f(sU+tw, sV);   rlVertex3f( h,  h, -h);
    rlTexCoord2f(sU, sV);      rlVertex3f( h,  h,  h);
    rlEnd();

    rlSetTexture(0);
    EndMode3D();
    EndTextureMode();

    g_cubeCache[bt] = { rt, true };
    return rt.texture;
}

// ---- 非方块物品 3D 长方体缓存（铤/条状物等）----
static std::unordered_map<int, CubeCacheEntry> g_itemCubeCache;

static Texture2D getItemCuboidTex(int item, const TextureAtlas& atlas) {
    auto it = g_itemCubeCache.find(item);
    if (it != g_itemCubeCache.end() && it->second.ready) return it->second.rt.texture;

    // 颜色和尺寸（长方体形状：扁窄砖形，参考 0.9*0.4*0.2）
    Color baseColor;
    float dx = 1.2, dy = 0.5, dz = 0.7f;
    switch (item) {
        case ITEM_IRON_INGOT: baseColor = (Color){ 210, 210, 220, 255 }; break; // 银白
        case ITEM_GOLD_INGOT: baseColor = (Color){ 220, 200, 60, 255 };  break; // 金黄
        default: return (Texture2D){ 0 };
    }

    int texSize = 96;
    RenderTexture2D rt = LoadRenderTexture(texSize, texSize);
    if (rt.texture.id == 0) return (Texture2D){ 0 };
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);

    BeginTextureMode(rt);
    ClearBackground((Color){ 0, 0, 0, 0 });

    Camera3D cam = { 0 };
    cam.position  = { 1.5f, 0.9f, 1.5f };
    cam.target    = { 0, 0, 0 };
    cam.up        = { 0, 1, 0 };
    cam.fovy      = 1.6f;
    cam.projection = CAMERA_ORTHOGRAPHIC;

    BeginMode3D(cam);

    // 各面亮度（模拟光照）
    struct FaceColor { int r, g, b; };
    auto shade = [&](float f) -> FaceColor {
        return { (int)(baseColor.r * f), (int)(baseColor.g * f), (int)(baseColor.b * f) };
    };
    FaceColor cTop  = shade(1.00f);
    FaceColor cSide = shade(0.72f);  // 侧面统一稍暗
    FaceColor cBot  = shade(0.45f);

    const float hx = dx/2, hy = dy/2, hz = dz/2;

    rlSetTexture(0); // 无纹理，纯色

    // 前 (+Z)
    rlBegin(RL_QUADS);
    rlColor4ub(cSide.r, cSide.g, cSide.b, 255);
    rlVertex3f(-hx, -hy,  hz); rlVertex3f( hx, -hy,  hz);
    rlVertex3f( hx,  hy,  hz); rlVertex3f(-hx,  hy,  hz);
    rlEnd();
    // 后 (-Z)
    rlBegin(RL_QUADS);
    rlColor4ub(cSide.r, cSide.g, cSide.b, 255);
    rlVertex3f( hx, -hy, -hz); rlVertex3f(-hx, -hy, -hz);
    rlVertex3f(-hx,  hy, -hz); rlVertex3f( hx,  hy, -hz);
    rlEnd();
    // 上 (+Y)
    rlBegin(RL_QUADS);
    rlColor4ub(cTop.r, cTop.g, cTop.b, 255);
    rlVertex3f(-hx,  hy,  hz); rlVertex3f( hx,  hy,  hz);
    rlVertex3f( hx,  hy, -hz); rlVertex3f(-hx,  hy, -hz);
    rlEnd();
    // 下 (-Y)
    rlBegin(RL_QUADS);
    rlColor4ub(cBot.r, cBot.g, cBot.b, 255);
    rlVertex3f(-hx, -hy, -hz); rlVertex3f( hx, -hy, -hz);
    rlVertex3f( hx, -hy,  hz); rlVertex3f(-hx, -hy,  hz);
    rlEnd();
    // 左 (-X)
    rlBegin(RL_QUADS);
    rlColor4ub(cSide.r, cSide.g, cSide.b, 255);
    rlVertex3f(-hx, -hy, -hz); rlVertex3f(-hx, -hy,  hz);
    rlVertex3f(-hx,  hy,  hz); rlVertex3f(-hx,  hy, -hz);
    rlEnd();
    // 右 (+X)
    rlBegin(RL_QUADS);
    rlColor4ub(cSide.r, cSide.g, cSide.b, 255);
    rlVertex3f( hx, -hy,  hz); rlVertex3f( hx, -hy, -hz);
    rlVertex3f( hx,  hy, -hz); rlVertex3f( hx,  hy,  hz);
    rlEnd();

    rlSetTexture(0);
    EndMode3D();
    EndTextureMode();

    g_itemCubeCache[item] = { rt, true };
    return rt.texture;
}

static void drawItemSlotTex(int x, int y, int size, int item, int count, bool selected, bool hover,
                             const TextureAtlas& atlas) {
    Color bg = selected ? Fade(WHITE, 0.65f) : (hover ? Fade(YELLOW, 0.2f) : Fade(BLACK, 0.45f));
    DrawRectangle(x, y, size, size, bg);
    DrawRectangleLines(x, y, size, size, selected ? Fade(YELLOW, 0.85f) : Fade(WHITE, 0.25f));

    int texSlot = getItemTexSlot(item);
    if (texSlot >= 0 && atlas.texture.id > 0) {
        // 方块物品（1-19）→ 3D 立方体
        if (item >= 1 && item <= 19) {
            BlockType bt = itemToBlock(item);
            if (bt != BLOCK_AIR) {
                Texture2D cubeTex = getBlockCubeTex(bt, atlas);
                if (cubeTex.id > 0) {
                    float cs = (float)(size - 8);
                    Rectangle dst = { (float)(x + 4), (float)(y + 4), cs, cs };
                    int tw = cubeTex.width, th = cubeTex.height;
                    Rectangle src = { 0, 0, (float)tw, (float)-th };
                    DrawTexturePro(cubeTex, src, dst, (Vector2){0,0}, 0.0f, WHITE);
                }
            } else {
                Rectangle dst = { (float)(x + 3), (float)(y + 3), (float)(size - 6), (float)(size - 6) };
                float u0 = atlas.u[texSlot], v0 = atlas.v[texSlot];
                Rectangle src = { u0 * atlas.texture.width, v0 * atlas.texture.height,
                                  atlas.w * atlas.texture.width, atlas.h * atlas.texture.height };
                DrawTexturePro(atlas.texture, src, dst, { 0, 0 }, 0.0f, WHITE);
            }
        }
        // 铤（长方体）→ 3D 彩色长方体
        else if (item == ITEM_IRON_INGOT || item == ITEM_GOLD_INGOT) {
            Texture2D cuboidTex = getItemCuboidTex(item, atlas);
            if (cuboidTex.id > 0) {
                float cs = (float)(size - 8);
                Rectangle dst = { (float)(x + 4), (float)(y + 4), cs, cs };
                int tw = cuboidTex.width, th = cuboidTex.height;
                Rectangle src = { 0, 0, (float)tw, (float)-th };
                DrawTexturePro(cuboidTex, src, dst, (Vector2){0,0}, 0.0f, WHITE);
            } else {
                Rectangle dst = { (float)(x + 3), (float)(y + 3), (float)(size - 6), (float)(size - 6) };
                float u0 = atlas.u[texSlot], v0 = atlas.v[texSlot];
                Rectangle src = { u0 * atlas.texture.width, v0 * atlas.texture.height,
                                  atlas.w * atlas.texture.width, atlas.h * atlas.texture.height };
                DrawTexturePro(atlas.texture, src, dst, { 0, 0 }, 0.0f, WHITE);
            }
        }
        // 其余物品 → 平面贴图
        else {
            Rectangle dst = { (float)(x + 3), (float)(y + 3), (float)(size - 6), (float)(size - 6) };
            float u0 = atlas.u[texSlot], v0 = atlas.v[texSlot];
            Rectangle src = { u0 * atlas.texture.width, v0 * atlas.texture.height,
                              atlas.w * atlas.texture.width, atlas.h * atlas.texture.height };
            DrawTexturePro(atlas.texture, src, dst, { 0, 0 }, 0.0f, WHITE);
        }
    } else if (item > 0) {
        DrawRectangle(x + 4, y + 4, size - 8, size - 8, GRAY);
    }
    if (count > 1) {
        char cnt[8]; snprintf(cnt, sizeof(cnt), "%d", count);
        drawCN(cnt, x + size - 20, y + size - 16, 12, WHITE);
    }
}

// ---- 生存 HUD ----
static void drawHearts(int x, int y, float hp, float maxHp) {
    int totalHearts = (int)(maxHp / 2.0f);
    int fullHearts  = (int)(hp / 2.0f);
    bool halfHeart  = (fmodf(hp, 2.0f) >= 0.5f);
    for (int i = 0; i < totalHearts; ++i) {
        Color c = GRAY;
        if (i < fullHearts) c = RED;
        else if (i == fullHearts && halfHeart) c = ORANGE;
        DrawRectangle(x + i * 20, y, 16, 16, c);
        DrawRectangleLines(x + i * 20, y, 16, 16, Fade(BLACK, 0.5f));
    }
}
static void drawHunger(int x, int y, float hunger) {
    int full = (int)(hunger / 2.0f + 0.01f);
    for (int i = 0; i < 10; ++i) {
        Color c = (i < full) ? (Color){180,140,60,255} : (Color){80,80,60,255};
        DrawRectangle(x + i * 20, y, 16, 16, c);
        DrawRectangleLines(x + i * 20, y, 16, 16, Fade(BLACK, 0.5f));
    }
}
static void drawMiningBar(int sw, int sh, const Player& p) {
    if (p.miningTimer <= 0.0f || p.miningDuration <= 0.0f) return;
    float frac = p.miningTimer / p.miningDuration;
    if (frac > 1.0f) frac = 1.0f;
    int bw = 200, bh = 12;
    int x = sw / 2 - bw / 2, y = sh / 2 + 30;
    DrawRectangle(x, y, bw, bh, Fade(BLACK, 0.5f));
    DrawRectangle(x, y, (int)(bw * frac), bh, Fade(WHITE, 0.7f));
    DrawRectangleLines(x, y, bw, bh, WHITE);
}

void drawHUD(const Player& p, const World& w, const GameTime& gt, bool showDebug, const TextureAtlas& atlas) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    // 生命值（左上）
    drawHearts(10, 10, p.hp, p.maxHp);
    // 饱食度（紧挨 HP 右侧）
    drawHunger(10 + (int)(p.maxHp / 2) * 20 + 10, 10, p.hunger);

    // 快捷栏（底部）
    int slotS = 44, barW = slotS * 8;
    int bx = (sw - barW) / 2, by = sh - slotS - 10;
    for (int i = 0; i < 8; ++i) {
        int x = bx + i * slotS, y = by;
        drawItemSlotTex(x, y, slotS, p.inventory.slots[i].item, p.inventory.slots[i].count,
                        i == p.inventory.selectedSlot, false, atlas);
        char num[2] = { (char)('1' + i), 0 };
        drawCN(num, x + 3, y + 2, 12, WHITE);
    }

    // 选中的物品名
    int selIt = p.inventory.selectedItem();
    if (selIt > 0) {
        const char* nm = itemName(selIt);
        int tw = measCN(nm, 18);
        drawCN(nm, (sw - tw) / 2, by - 24, 18, WHITE);
    }

    // 挖掘进度条
    drawMiningBar(sw, sh, p);

    // 调试信息
    if (showDebug) {
        int y = 60;
        drawCN(TextFormat("FPS:%d  XYZ:%.0f,%.0f,%.0f", GetFPS(), p.pos.x, p.pos.y, p.pos.z), 10, y, 16, WHITE); y += 20;
        drawCN(TextFormat("Chunk:%d,%d  区块:%d  HP:%.0f 饥饿:%.0f",
            (int)floorf(p.pos.x/CHUNK_X), (int)floorf(p.pos.z/CHUNK_Z), (int)w.chunks.size(), p.hp, p.hunger), 10, y, 16, WHITE);
    }
}

// ---- 工作台/合成界面 (MC 风格，带贴图) ----
void drawInventoryScreen(const Player& p, const World& w, const TextureAtlas& atlas) {
    (void)w;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.68f));

    const int s = 42, g = 4, sd = s + g;
    Vector2 mouse = GetMousePosition();
    const char* tooltip = nullptr;

    // ---- 背包 ----
    int bx = sw / 2 - 6 * sd / 2, by = sh / 2 - 20;
    drawCN("背包", bx, by - 26, 20, WHITE);
    for (int i = 0; i < BACKPACK_SLOTS; ++i) {
        int r = i / 6, c = i % 6;
        int x = bx + c * sd, y = by + r * sd;
        int idx = HOTBAR_SLOTS + i;
        bool hover = CheckCollisionPointRec(mouse, { (float)x, (float)y, s, s });
        drawItemSlotTex(x, y, s, p.inventory.slots[idx].item, p.inventory.slots[idx].count, false, hover, atlas);
        if (hover && p.inventory.slots[idx].item > 0) tooltip = itemName(p.inventory.slots[idx].item);
    }

    // ---- 快捷栏 ----
    int hby = by + 3 * sd + 14;
    drawCN("快捷栏", bx, hby - 20, 16, Fade(WHITE, 0.6f));
    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        int x = bx + i * sd, y = hby;
        bool hover = CheckCollisionPointRec(mouse, { (float)x, (float)y, s, s });
        drawItemSlotTex(x, y, s, p.inventory.slots[i].item, p.inventory.slots[i].count,
                        i == p.inventory.selectedSlot, hover, atlas);
        if (hover && p.inventory.slots[i].item > 0) tooltip = itemName(p.inventory.slots[i].item);
    }

    // ---- 合成网格 ----
    int gx = bx + 6 * sd + 40, gy = by;
    bool hasWB = p.nearWorkbench;
    int gs = hasWB ? 3 : 2;
    drawCN(hasWB ? "工作台 3x3 合成" : "2x2 手合成", gx, gy - 20, 16, hasWB ? LIME : Fade(YELLOW, 0.8f));
    for (int r = 0; r < gs; ++r) {
        for (int c = 0; c < gs; ++c) {
            int x = gx + c * sd, y = gy + 8 + r * sd;
            bool hover = CheckCollisionPointRec(mouse, { (float)x, (float)y, s, s });
            drawItemSlotTex(x, y, s, p.craftGrid.slots[r][c].item, p.craftGrid.slots[r][c].count, false, hover, atlas);
            if (hover && p.craftGrid.slots[r][c].item > 0) tooltip = itemName(p.craftGrid.slots[r][c].item);
        }
    }

    // ---- 输出槽 ----
    int ox = gx + gs * sd + 20, oy = gy + 8 + (gs - 1) * sd / 2;
    int ri = matchRecipe(p.craftGrid);
    bool outHover = CheckCollisionPointRec(mouse, { (float)ox, (float)oy, s, s });
    if (ri >= 0) {
        const ShapedRecipe& rec = SHAPED_RECIPES[ri];
        drawItemSlotTex(ox, oy, s, rec.resultItem, rec.resultCount, false, outHover, atlas);
        drawCN(rec.name, ox, oy + s + 4, 13, LIME);
        if (outHover) tooltip = itemName(rec.resultItem);
    } else {
        DrawRectangle(ox, oy, s, s, Fade(WHITE, 0.08f));
        DrawRectangleLines(ox, oy, s, s, Fade(WHITE, 0.15f));
    }

    // ---- 手持物品（跟随鼠标）----
    if (p.grabbed.item > 0) {
        drawItemSlotTex((int)mouse.x - s / 2, (int)mouse.y - s / 2, s,
                        p.grabbed.item, p.grabbed.count, false, false, atlas);
    }

    // ---- 悬停提示 ----
    if (tooltip) {
        int tw = measCN(tooltip, 15);
        int tx = (int)mouse.x - tw / 2, ty = (int)mouse.y - 28;
        if (tx < 4) tx = 4;
        if (tx + tw + 8 > sw) tx = sw - tw - 8;
        DrawRectangle(tx - 2, ty, tw + 6, 20, Fade(BLACK, 0.85f));
        drawCN(tooltip, tx + 2, ty + 2, 15, WHITE);
    }

    drawCN("左键交换/合并 | 右键取半/放一 | E - 关闭",
           (sw - measCN("左键交换/合并 | 右键取半/放一 | E - 关闭", 16)) / 2, sh - 24, 16, Fade(WHITE, 0.5f));
}

// ---- 暂停遮罩 ----
int drawPausedOverlay() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.55f));

    // 标题
    const char* title = "已暂停";
    int tw = measCN(title, 40);
    drawCN(title, (sw - tw) / 2, sh / 2 - 70, 40, WHITE);

    // 按钮尺寸
    int bw = 160, bh = 44, gap = 20;
    int bx = sw / 2 - bw / 2, by = sh / 2 - 10;
    Vector2 mouse = GetMousePosition();

    int result = 0;

    // 继续按钮
    bool hover1 = CheckCollisionPointRec(mouse, { (float)bx, (float)by, (float)bw, (float)bh });
    DrawRectangle(bx, by, bw, bh, hover1 ? Fade(GREEN, 0.6f) : Fade(WHITE, 0.15f));
    DrawRectangleLines(bx, by, bw, bh, Fade(WHITE, 0.6f));
    int t1w = measCN("继续", 22);
    drawCN("继续", bx + (bw - t1w) / 2, by + (bh - 24) / 2, 22, WHITE);
    if (hover1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) result = 1;

    // 退出按钮
    int qy = by + bh + gap;
    bool hover2 = CheckCollisionPointRec(mouse, { (float)bx, (float)qy, (float)bw, (float)bh });
    DrawRectangle(bx, qy, bw, bh, hover2 ? Fade(RED, 0.6f) : Fade(WHITE, 0.15f));
    DrawRectangleLines(bx, qy, bw, bh, Fade(WHITE, 0.6f));
    int t2w = measCN("退出游戏", 22);
    drawCN("退出游戏", bx + (bw - t2w) / 2, qy + (bh - 24) / 2, 22, WHITE);
    if (hover2 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) result = 2;

    return result;
}

// ---- 新主菜单 ----
// 返回: 0=无操作, 1=开始游戏, 2=退出
int drawTitleScreen(const GameTime& gt, bool checkClick) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    // 使用昼夜时间绘制动态天空背景
    Color skyTop, skyBot;
    getSkyColors(dayFraction(gt), skyTop, skyBot);
    drawGrad(skyTop, skyBot);
    
    const char* title = "体素沙盒";
    int tw = measCN(title, 60);
    drawCN(title, (sw - tw) / 2 + 3, sh / 2 - 117, 60, Fade(BLACK, 0.5f));
    drawCN(title, (sw - tw) / 2, sh / 2 - 120, 60, YELLOW);
    
    drawCN("WASD移动  空格跳  鼠标视角", (sw - measCN("WASD移动  空格跳  鼠标视角", 20)) / 2, sh / 2, 20, WHITE);
    drawCN("左键破坏  右键放置  1-8选块  E背包  F1调试", (sw - measCN("左键破坏  右键放置  1-8选块  E背包  F1调试", 20)) / 2, sh / 2 + 30, 20, WHITE);

    drawCN("点击开始游戏", (sw - measCN("点击开始游戏", 26)) / 2, sh / 2 + 90, 26, LIME);

    if (checkClick && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 1;
    if (checkClick && IsKeyPressed(KEY_SPACE)) return 1;
    if (IsKeyPressed(KEY_ESCAPE)) return 2;
    return 0;
}

void drawOldStartHint() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));
    int y = sh / 2 - 140;
    auto line = [&](const char* t, int fs, Color c) {
        int tw = measCN(t, fs);
        drawCN(t, (sw - tw) / 2, y, fs, c);
        y += fs + 12;
    };
    line("体素沙盒", 48, YELLOW);
    line("Voxel Sandbox (C++17 + raylib 5.5)", 16, Fade(WHITE, 0.7f));
    line("WASD - 移动     空格 - 跳跃     Shift/Ctrl - 加速", 20, WHITE);
    line("鼠标 - 视角     左键 - 破坏     右键 - 放置", 20, WHITE);
    line("1-8 / 滚轮 - 切换方块", 20, WHITE);
    line("E - 背包/合成", 20, WHITE);
    line("", 12, WHITE);
    line("点击屏幕开始游戏", 26, LIME);
}

// ---- 箱子界面绘制 ----
void drawChestScreen(const Player& p, const World& w, const ChestGUI& gui, const TextureAtlas& atlas) {
    (void)p;
    if (!gui.open) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.68f));

    const int S = 42, G = 4, sd = S + G;
    Vector2 mouse = GetMousePosition();
    const char* tooltip = nullptr;

    // 箱子库存（27 格，3 行 x 9 列）
    int chestCols = 9, chestRows = 3;
    int totalW = chestCols * sd;
    int chestBX = sw / 2 - totalW / 2, chestBY = sh / 2 - 30 - chestRows * sd;

    drawCN("箱子", chestBX, chestBY - 22, 18, WHITE);

    ChestData& chest = const_cast<ChestSystem&>(w.chestSys).getOrCreate(gui.cx, gui.cy, gui.cz);
    for (int r = 0; r < chestRows; ++r) {
        for (int c = 0; c < chestCols; ++c) {
            int idx = r * chestCols + c;
            int x = chestBX + c * sd, y = chestBY + r * sd;
            bool hover = CheckCollisionPointRec(mouse, { (float)x, (float)y, S, S });
            drawItemSlotTex(x, y, S, chest.slots[idx].item, chest.slots[idx].count, false, hover, atlas);
            if (hover && chest.slots[idx].item > 0) tooltip = itemName(chest.slots[idx].item);
        }
    }

    // 玩家背包 + 快捷栏（在箱子下方）
    int playerBX = sw / 2 - 6 * sd / 2, playerBY = chestBY + chestRows * sd + 30;
    drawCN("背包", playerBX, playerBY - 18, 16, Fade(WHITE, 0.6f));
    for (int i = 0; i < BACKPACK_SLOTS; ++i) {
        int r = i / 6, c = i % 6;
        int x = playerBX + c * sd, y = playerBY + r * sd;
        int idx = HOTBAR_SLOTS + i;
        bool hover = CheckCollisionPointRec(mouse, { (float)x, (float)y, S, S });
        drawItemSlotTex(x, y, S, p.inventory.slots[idx].item, p.inventory.slots[idx].count, false, hover, atlas);
        if (hover && p.inventory.slots[idx].item > 0) tooltip = itemName(p.inventory.slots[idx].item);
    }

    int hby = playerBY + 3 * sd + 14;
    drawCN("快捷栏", playerBX, hby - 18, 14, Fade(WHITE, 0.5f));
    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        int x = playerBX + i * sd, y = hby;
        bool hover = CheckCollisionPointRec(mouse, { (float)x, (float)y, S, S });
        drawItemSlotTex(x, y, S, p.inventory.slots[i].item, p.inventory.slots[i].count,
                        i == p.inventory.selectedSlot, hover, atlas);
        if (hover && p.inventory.slots[i].item > 0) tooltip = itemName(p.inventory.slots[i].item);
    }

    // 手持物品
    if (gui.grabbed.item > 0) {
        drawItemSlotTex((int)mouse.x - S / 2, (int)mouse.y - S / 2, S,
                        gui.grabbed.item, gui.grabbed.count, false, false, atlas);
    }

    // 悬浮提示
    if (tooltip) {
        int tw = measCN(tooltip, 15);
        int tx = (int)mouse.x - tw / 2, ty = (int)mouse.y - 28;
        if (tx < 4) tx = 4;
        if (tx + tw + 8 > sw) tx = sw - tw - 8;
        DrawRectangle(tx - 2, ty, tw + 6, 20, Fade(BLACK, 0.85f));
        drawCN(tooltip, tx + 2, ty + 2, 15, WHITE);
    }

    drawCN("左键转移物品 | 右键取半放一 | E/ESC - 关闭",
           (sw - measCN("左键转移物品 | 右键取半放一 | E/ESC - 关闭", 16)) / 2, sh - 24, 16, Fade(WHITE, 0.5f));
}

// ---- 熔炉界面绘制 ----
void drawFurnaceScreen(const Player& p, const World& w, const FurnaceGUI& gui, const TextureAtlas& atlas) {
    (void)p;
    if (!gui.open) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.68f));

    const int S = 42, G = 4, sd = S + G;
    Vector2 mouse = GetMousePosition();
    const char* tooltip = nullptr;
    int cx = sw / 2, cy = sh / 2;
    int bx = cx - 70, by = cy - 55;

    drawCN("熔炉", bx, by - 22, 18, WHITE);

    FurnaceData& fur = const_cast<FurnaceSystem&>(w.furnaceSys).getOrCreate(gui.fx, gui.fy, gui.fz);

    // 输入槽（上）
    int inputX = bx, inputY = by;
    bool hIn = CheckCollisionPointRec(mouse, { (float)inputX, (float)inputY, S, S });
    drawItemSlotTex(inputX, inputY, S, fur.input.item, fur.input.count, false, hIn, atlas);
    if (hIn && fur.input.item > 0) tooltip = itemName(fur.input.item);

    // 燃料槽（左下）
    int fuelX = bx, fuelY = by + sd + 8;
    bool hFuel = CheckCollisionPointRec(mouse, { (float)fuelX, (float)fuelY, S, S });
    drawItemSlotTex(fuelX, fuelY, S, fur.fuel.item, fur.fuel.count, false, hFuel, atlas);
    if (hFuel && fur.fuel.item > 0) tooltip = itemName(fur.fuel.item);

    // 输出槽（右）
    int outX = bx + sd * 2 + 20, outY = by + sd / 2;
    bool hOut = CheckCollisionPointRec(mouse, { (float)outX, (float)outY, S, S });
    drawItemSlotTex(outX, outY, S, fur.output.item, fur.output.count, false, hOut, atlas);
    if (hOut && fur.output.item > 0) tooltip = itemName(fur.output.item);

    // 进度条
    if (fur.smeltTime > 0 && fur.smeltDuration > 0) {
        float frac = fur.smeltTime / fur.smeltDuration;
        if (frac > 1.0f) frac = 1.0f;
        int barX = bx + sd + 6, barY = by + sd / 2 + 4;
        int barW = 16, barH = 12;
        DrawRectangle(barX, barY, barW, barH, Fade(BLACK, 0.6f));
        DrawRectangle(barX, barY + (int)(barH * (1.0f - frac)), barW, (int)(barH * frac), ORANGE);
        DrawRectangleLines(barX, barY, barW, barH, Fade(WHITE, 0.4f));
    }
    // 火焰图标（燃料燃烧指示）
    if (fur.fuel.item > 0) {
        int flX = bx + sd + 4, flY = by + sd + 8;
        DrawRectangle(flX, flY, 8, 10, Fade(BLACK, 0.5f));
        DrawRectangle(flX + 1, flY + 2, 6, 6, Fade(ORANGE, 0.8f));
    }

    // 玩家背包
    drawCN("背包", cx - 6 * sd / 2, by + sd * 2 + 27, 16, Fade(WHITE, 0.6f));
    int playerBX = cx - 6 * sd / 2, playerBY = by + sd * 2 + 45;
    for (int i = 0; i < BACKPACK_SLOTS; ++i) {
        int r = i / 6, c = i % 6;
        int x = playerBX + c * sd, y = playerBY + r * sd;
        bool hover = CheckCollisionPointRec(mouse, { (float)x, (float)y, S, S });
        drawItemSlotTex(x, y, S, p.inventory.slots[HOTBAR_SLOTS + i].item,
                        p.inventory.slots[HOTBAR_SLOTS + i].count, false, hover, atlas);
        if (hover && p.inventory.slots[HOTBAR_SLOTS + i].item > 0)
            tooltip = itemName(p.inventory.slots[HOTBAR_SLOTS + i].item);
    }
    int hby = playerBY + 3 * sd + 14;
    drawCN("快捷栏", playerBX, hby - 18, 14, Fade(WHITE, 0.5f));
    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        int x = playerBX + i * sd, y = hby;
        bool hover = CheckCollisionPointRec(mouse, { (float)x, (float)y, S, S });
        drawItemSlotTex(x, y, S, p.inventory.slots[i].item, p.inventory.slots[i].count,
                        i == p.inventory.selectedSlot, hover, atlas);
        if (hover && p.inventory.slots[i].item > 0) tooltip = itemName(p.inventory.slots[i].item);
    }

    // 手持物品
    if (gui.grabbed.item > 0) {
        drawItemSlotTex((int)mouse.x - S / 2, (int)mouse.y - S / 2, S,
                        gui.grabbed.item, gui.grabbed.count, false, false, atlas);
    }

    if (tooltip) {
        int tw = measCN(tooltip, 15);
        int tx = (int)mouse.x - tw / 2, ty = (int)mouse.y - 28;
        if (tx < 4) tx = 4;
        if (tx + tw + 8 > sw) tx = sw - tw - 8;
        DrawRectangle(tx - 2, ty, tw + 6, 20, Fade(BLACK, 0.85f));
        drawCN(tooltip, tx + 2, ty + 2, 15, WHITE);
    }

    drawCN("冶炼时间 8 秒 | 左侧放矿石/燃料 右侧取产物 | E/ESC - 关闭",
	           (sw - measCN("冶炼时间 8 秒 | 左侧放矿石/燃料 右侧取产物 | E/ESC - 关闭", 14)) / 2, sh - 24, 14, Fade(WHITE, 0.5f));
	}

// ---- 清理 ----
void unloadBlockCubeCache() {
    for (auto& pair : g_cubeCache)
        UnloadRenderTexture(pair.second.rt);
    g_cubeCache.clear();
    for (auto& pair : g_itemCubeCache)
        UnloadRenderTexture(pair.second.rt);
    g_itemCubeCache.clear();
}
