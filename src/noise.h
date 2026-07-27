// noise.h - 值噪声 + fbm（分形布朗运动）
// 用于地形高度生成。基于哈希的确定性伪随机，给定种子可复现。
#pragma once

// 初始化噪声发生器种子
void noiseInit(unsigned int seed);

// 二维值噪声，返回 [0,1]
float noise2D(float x, float y);

// fbm 分形叠加，octaves 为叠加层数，返回 [0,1]
// 频率随层数翻倍，振幅按 persistence 衰减，模拟自然地形起伏。
float fbm2D(float x, float y, int octaves, float persistence, float frequency);
