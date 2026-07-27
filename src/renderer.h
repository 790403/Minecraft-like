// renderer.h - 渲染辅助：天空、昼夜、HUD、菜单、背包界面
#pragma once
#include "raylib.h"
#include "player.h"
#include "texture_atlas.h"
#include "chest.h"
#include "furnace.h"
#include <cmath>

struct World;

struct GameTime {
    float time;
    float dayLength;
};

inline float dayFraction(const GameTime& gt) {
    return fmodf(gt.time / gt.dayLength, 1.0f);
}

void loadGameFont();
void unloadGameFont();
void getSkyColors(float frac, Color& topOut, Color& bottomOut);
float getDayLight(float frac);
void drawSky(const GameTime& gt, const Camera3D& camera);
void drawBlockHighlight(const RaycastHit& hit);
void drawCrosshair();
void drawHUD(const Player& p, const World& w, const GameTime& gt, bool showDebug, const TextureAtlas& atlas);
void drawInventoryScreen(const Player& p, const World& w, const TextureAtlas& atlas);
int drawPausedOverlay();  // 返回 0=无操作 1=继续 2=退出
int drawTitleScreen(const GameTime& gt, bool checkClick);
void drawOldStartHint();
void drawChestScreen(const Player& p, const World& w, const ChestGUI& gui, const TextureAtlas& atlas);
void drawFurnaceScreen(const Player& p, const World& w, const FurnaceGUI& gui, const TextureAtlas& atlas);
void unloadBlockCubeCache();
