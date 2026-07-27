// noise.cpp - 值噪声实现
#include "noise.h"
#include <cmath>

// ---------- 哈希随机 ----------
// 基于整数坐标的哈希函数，返回 [0,1) 的伪随机值。
// 使用经典的"位运算混淆"哈希，速度快且分布足够均匀。
static unsigned int g_seed = 1337;

void noiseInit(unsigned int seed) {
    g_seed = seed ? seed : 1337u;
}

// 对整数坐标做哈希，得到 [0,1)
static inline float hash2(int x, int y) {
    unsigned int h = (unsigned int)x * 374761393u + (unsigned int)y * 668265263u + g_seed * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return (float)(h & 0x00FFFFFF) / (float)0x01000000;
}

// 平滑插值（五次多项式，比 cubic 更平滑）
static inline float smooth(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float noise2D(float x, float y) {
    int xi = (int)floorf(x);
    int yi = (int)floorf(y);
    float xf = x - (float)xi;
    float yf = y - (float)yi;

    // 四个角点的随机值
    float v00 = hash2(xi,     yi);
    float v10 = hash2(xi + 1, yi);
    float v01 = hash2(xi,     yi + 1);
    float v11 = hash2(xi + 1, yi + 1);

    float u = smooth(xf);
    float v = smooth(yf);

    // 双线性插值
    float a = lerp(v00, v10, u);
    float b = lerp(v01, v11, u);
    return lerp(a, b, v);
}

float fbm2D(float x, float y, int octaves, float persistence, float frequency) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float max = 0.0f;
    float freq = frequency;
    for (int i = 0; i < octaves; ++i) {
        total += noise2D(x * freq, y * freq) * amplitude;
        max += amplitude;
        amplitude *= persistence;
        freq *= 2.0f;
    }
    return total / max;  // 归一化到 [0,1]
}
