// texture_atlas.h - 程序生成纹理图集
#pragma once
#include "raylib.h"

struct TextureAtlas {
    Texture2D texture;
    // 4×16=64 个槽位
    float u[64], v[64], w, h;
};

TextureAtlas atlasLoad();
void atlasUnload(TextureAtlas& atlas);
