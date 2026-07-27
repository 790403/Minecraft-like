// texture_atlas.cpp - 程序生成 4×5=20 槽纹理图集 (MC 风格矿石)
#include "texture_atlas.h"
#include "block.h"
#include <cmath>

static inline void setpix(Image& img, int x, int y, Color c) {
    if (x<0||y<0||x>=img.width||y>=img.height) return;
    ((Color*)img.data)[y*img.width+x] = c;
}
static inline int clamp(int v, int min, int max) { return v < min ? min : (v > max ? max : v); }
static inline Color shade(Color c, int d) {
    auto cl = [](int v){ return (unsigned char)(v<0?0:(v>255?255:v)); };
    return { cl(c.r+d), cl(c.g+d), cl(c.b+d), c.a };
}
static inline float rnd(int x, int y, int seed) {
    unsigned h = (unsigned)x*374761393u+(unsigned)y*668265263u+(unsigned)seed*1274126177u;
    h=(h^(h>>13))*1274126177u; h^=h>>16;
    return (float)(h&0xFFFFFF)/16777216.0f;
}

static void fillTile(Image& img, int tx, int ty, Color c) {
    int ox=tx*ATLAS_TILE, oy=ty*ATLAS_TILE;
    for(int y=0;y<ATLAS_TILE;++y) for(int x=0;x<ATLAS_TILE;++x) setpix(img,ox+x,oy+y,c);
}
static void noiseTile(Image& img, int tx, int ty, int amt, int seed) {
    int ox=tx*ATLAS_TILE, oy=ty*ATLAS_TILE;
    for(int y=0;y<ATLAS_TILE;++y) for(int x=0;x<ATLAS_TILE;++x)
        setpix(img,ox+x,oy+y,shade(*((Color*)img.data+(oy+y)*img.width+(ox+x)),(int)((rnd(x,y,seed)-0.5f)*2*amt)));
}

// --------- 原有纹理(略) ---------
static void drawGrassTop(Image& img, int tx, int ty) { fillTile(img,tx,ty,{90,160,60,255}); noiseTile(img,tx,ty,30,7); }
static void drawGrassSide(Image& img, int tx, int ty) {
    int ox=tx*16,oy=ty*16;
    for(int y=0;y<16;++y) for(int x=0;x<16;++x) setpix(img,ox+x,oy+y,(y<5)?Color{90,160,60,255}:Color{134,96,67,255});
    for(int x=0;x<16;++x){int e=4+(int)(rnd(x,0,11)*3); for(int y=4;y<e;++y) setpix(img,ox+x,oy+y,{90,160,60,255});}
    noiseTile(img,tx,ty,20,21);
}
static void drawDirt(Image& img, int tx, int ty)  { fillTile(img,tx,ty,{134,96,67,255}); noiseTile(img,tx,ty,25,3); }
static void drawStone(Image& img, int tx, int ty)  { fillTile(img,tx,ty,{128,128,128,255}); noiseTile(img,tx,ty,25,5); }
static void drawSand(Image& img, int tx, int ty)   { fillTile(img,tx,ty,{220,205,140,255}); noiseTile(img,tx,ty,15,17); }
static void drawWoodSide(Image& img, int tx, int ty) {
    int ox=tx*16,oy=ty*16;
    for(int y=0;y<16;++y) for(int x=0;x<16;++x){Color c={102,76,47,255}; if(x%4==0)c=shade(c,-25); setpix(img,ox+x,oy+y,c);}
    noiseTile(img,tx,ty,12,23);
}
static void drawWoodTop(Image& img, int tx, int ty) {
    int ox=tx*16,oy=ty*16,cx=8,cy=8;
    for(int y=0;y<16;++y) for(int x=0;x<16;++x){float d=sqrtf((float)((x-cx)*(x-cx)+(y-cy)*(y-cy))); Color c={150,110,70,255}; if((int)d%3==0)c=shade(c,-30); setpix(img,ox+x,oy+y,c);}
    noiseTile(img,tx,ty,10,31);
}
static void drawLeaves(Image& img, int tx, int ty)  { fillTile(img,tx,ty,{50,110,40,255}); noiseTile(img,tx,ty,40,41); }
static void drawPlank(Image& img, int tx, int ty) {
    int ox=tx*16,oy=ty*16;
    for(int y=0;y<16;++y) for(int x=0;x<16;++x){Color c={160,120,75,255}; if(y%8==0)c=shade(c,-40); if((x+y/8*3)%7==0)c=shade(c,-18); setpix(img,ox+x,oy+y,c);}
    noiseTile(img,tx,ty,12,51);
}
static void drawCobble(Image& img, int tx, int ty) {
    int ox=tx*16,oy=ty*16; fillTile(img,tx,ty,{110,110,110,255});
    // 不规则石块 + 深色缝隙
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            Color c = { 110, 110, 110, 255 };
            // 石块边界（缝隙）
            int gx = (x / 4) * 4, gy = (y / 4) * 4;
            if (gx == 0 || gy == 0 || gx == 12 || gy == 12) {
                // 边界稍微偏移制造不规则感
                int shift = ((gx/4 + gy/4) % 3 == 0) ? 1 : 0;
                if (x % 4 == shift || y % 4 == shift) c = { 60, 60, 60, 255 };
            }
            // 石块内部纹理变化
            int d = (int)((rnd(x, y, 61) - 0.5f) * 35);
            c.r = (unsigned char)clamp(c.r + d, 80, 160);
            c.g = (unsigned char)clamp(c.g + d, 80, 160);
            c.b = (unsigned char)clamp(c.b + d, 80, 160);
            setpix(img, ox+x, oy+y, c);
        }
    }
}
static void drawBedrock(Image& img, int tx, int ty) { fillTile(img,tx,ty,{60,60,60,255}); noiseTile(img,tx,ty,50,71); }
static void drawWater(Image& img, int tx, int ty)  { fillTile(img,tx,ty,{40,100,220,200}); noiseTile(img,tx,ty,18,83); }

// --------- MC 风格矿石 (槽边界 clamp) ---------
static void drawOreSpots(Image& img, int tx, int ty, int count, int seedBase, int maxR, Color spotColor) {
    int ox = tx * 16, oy = ty * 16; // 槽原点
    for (int i = 0; i < count; ++i) {
        // 圆心限制在 [maxR, 15-maxR] 内，确保斑块不越界
        int margin = maxR + 1;
        int x = margin + (int)(rnd(i, 0, seedBase) * (16 - margin * 2));
        int y = margin + (int)(rnd(i, 1, seedBase) * (16 - margin * 2));
        int r = 1 + (int)(rnd(i, 2, seedBase) * maxR);
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy > r * r) continue;
                int px = ox + x + dx, py = oy + y + dy;
                // 严格 clamp 到槽内
                if (px < ox || px >= ox + 16 || py < oy || py >= oy + 16) continue;
                if (px < 0 || py < 0 || px >= img.width || py >= img.height) continue;
                ((Color*)img.data)[py * img.width + px] = spotColor;
            }
    }
}

// 铁矿: 浅棕石底+铁锈斑
static void drawIronOre(Image& img, int tx, int ty) {
    fillTile(img, tx, ty, { 155, 145, 130, 255 });
    noiseTile(img, tx, ty, 18, 211);
    drawOreSpots(img, tx, ty, 10, 213, 3, { 200, 170, 140, 255 });
}
// 煤矿: 灰石底+黑瘤
static void drawCoalOre(Image& img, int tx, int ty) {
    fillTile(img, tx, ty, { 110, 110, 110, 255 });
    noiseTile(img, tx, ty, 18, 201);
    drawOreSpots(img, tx, ty, 14, 203, 4, { 18, 18, 18, 255 });
}
// 金矿: 石头底+亮金
static void drawGoldOre(Image& img, int tx, int ty) {
    drawStone(img, tx, ty);
    drawOreSpots(img, tx, ty, 7, 223, 3, { 240, 200, 40, 255 });
}
// 钻石矿: 石头底+青晶
static void drawDiamondOre(Image& img, int tx, int ty) {
    drawStone(img, tx, ty);
    drawOreSpots(img, tx, ty, 5, 233, 2, { 50, 220, 210, 255 });
}
// 工作台
static void drawCraftTop(Image& img, int tx, int ty) {
    drawPlank(img,tx,ty);
    int ox=tx*16,oy=ty*16;
    // 工具图案：斧头 + 锯子 深色线条
    Color toolColor = { 80, 60, 40, 255 };
    // 斧头（左上）
    for (int y = 2; y <= 5; ++y) {
        setpix(img, ox+3, oy+y, toolColor);
        setpix(img, ox+4, oy+y, toolColor);
    }
    setpix(img, ox+5, oy+3, toolColor);
    setpix(img, ox+6, oy+2, toolColor);
    // 锯子（右下）
    for (int x = 9; x <= 12; ++x) {
        setpix(img, ox+x, oy+11, toolColor);
        setpix(img, ox+x, oy+12, toolColor);
    }
    setpix(img, ox+11, oy+10, toolColor);
    setpix(img, ox+10, oy+9, toolColor);
    setpix(img, ox+9, oy+8, toolColor);
}
static void drawCraftSide(Image& img, int tx, int ty) {
    drawPlank(img, tx, ty);
    // 侧面加一点纹理变化，区分于普通木板
    int ox = tx*16, oy = ty*16;
    for (int x = 0; x < 16; ++x)
        setpix(img, ox+x, oy+7, shade(*((Color*)img.data+(oy+7)*img.width+ox+x), -15));
}

// 熔炉：圆石基底 + 深色炉膛 + 火焰
static void drawFurnace(Image& img, int tx, int ty) {
    drawCobble(img, tx, ty);
    int ox = tx * 16, oy = ty * 16;
    // 深色炉膛
    for (int y = 4; y <= 12; ++y)
        for (int x = 4; x <= 11; ++x)
            setpix(img, ox + x, oy + y, { 25, 25, 25, 255 });
    // 炉膛边框（更亮）
    for (int x = 3; x <= 12; ++x) {
        setpix(img, ox + x, oy + 3, { 60, 55, 50, 255 });
        setpix(img, ox + x, oy + 13, { 60, 55, 50, 255 });
    }
    for (int y = 4; y <= 12; ++y) {
        setpix(img, ox + 3, oy + y, { 60, 55, 50, 255 });
        setpix(img, ox + 12, oy + y, { 60, 55, 50, 255 });
    }
    // 火焰
    for (int y = 9; y <= 11; ++y) {
        int w = (y == 9) ? 2 : ((y == 10) ? 4 : 2);
        for (int x = 8 - w/2; x <= 8 + w/2; ++x)
            setpix(img, ox + x, oy + y, { 255, 180, 40, 255 });
    }
    // 火焰核心（更亮）
    setpix(img, ox + 7, oy + 10, { 255, 220, 80, 255 });
    setpix(img, ox + 8, oy + 9, { 255, 220, 80, 255 });
}

// 未点燃熔炉：与点燃版相同，但火焰区域变灰
static void drawFurnaceUnlit(Image& img, int tx, int ty) {
    drawFurnace(img, tx, ty);
    int ox = tx * 16, oy = ty * 16;
    // 将火焰区域覆盖为灰色
    for (int y = 9; y <= 11; ++y) {
        int w = (y == 9) ? 2 : ((y == 10) ? 4 : 2);
        for (int x = 8 - w/2; x <= 8 + w/2; ++x)
            setpix(img, ox + x, oy + y, { 40, 30, 20, 255 });
    }
    setpix(img, ox + 7, oy + 10, { 40, 30, 20, 255 });
    setpix(img, ox + 8, oy + 9, { 40, 30, 20, 255 });
}

// 箱子：木板纹理 + 金属边框
static void drawChest(Image& img, int tx, int ty) {
    drawPlank(img, tx, ty);
    int ox = tx * 16, oy = ty * 16;
    // 金属边框
    for (int x = 0; x < 16; ++x) {
        setpix(img, ox + x, oy + 0, { 80, 70, 50, 255 });
        setpix(img, ox + x, oy + 15, { 80, 70, 50, 255 });
    }
    for (int y = 0; y < 16; ++y) {
        setpix(img, ox + 0, oy + y, { 80, 70, 50, 255 });
        setpix(img, ox + 15, oy + y, { 80, 70, 50, 255 });
    }
    // 锁扣
    for (int y = 6; y <= 9; ++y)
        for (int x = 7; x <= 8; ++x)
            setpix(img, ox + x, oy + y, { 200, 180, 100, 255 });
    for (int y = 6; y <= 8; ++y)
	    setpix(img, ox + 7, oy + y, { 60, 50, 30, 255 });
	}
	
	static void drawChestSide(Image& img, int tx, int ty) {
	    drawPlank(img, tx, ty);
	    int ox = tx * 16, oy = ty * 16;
	    // 金属边框（无锁扣）
	    for (int x = 0; x < 16; ++x) {
	        setpix(img, ox + x, oy + 0, { 80, 70, 50, 255 });
	        setpix(img, ox + x, oy + 15, { 80, 70, 50, 255 });
	    }
	    for (int y = 0; y < 16; ++y) {
	        setpix(img, ox + 0, oy + y, { 80, 70, 50, 255 });
	        setpix(img, ox + 15, oy + y, { 80, 70, 50, 255 });
	    }
	}
	
	// ---- 物品纹理（无方块对应，透明背景图标）----
static void drawItemCoal(Image& img, int tx, int ty) {
    int ox = tx * 16, oy = ty * 16;
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    // 煤块：不规则暗色块
    for (int i = 0; i < 8; ++i) {
        int cx = 3 + (int)(rnd(i, 0, 101) * 10);
        int cy = 3 + (int)(rnd(i, 1, 101) * 10);
        int r = 2 + (int)(rnd(i, 2, 101) * 3);
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx)
                if (dx*dx+dy*dy <= r*r) {
                    int px = ox + cx + dx, py = oy + cy + dy;
                    if (px>=ox && px<ox+16 && py>=oy && py<oy+16)
                        setpix(img, px, py, { 25, 25, 25, 255 });
                }
    }
    // 高光
    setpix(img, ox+5, oy+5, { 50, 50, 50, 255 });
    setpix(img, ox+6, oy+5, { 40, 40, 40, 255 });
}
static void drawItemRawIron(Image& img, int tx, int ty) {
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    int ox = tx * 16, oy = ty * 16;
    // 铁矿石斑点
    for (int i = 0; i < 6; ++i) {
        int cx = 3 + (int)(rnd(i,0,211) * 10);
        int cy = 3 + (int)(rnd(i,1,211) * 10);
        int r = 2 + (int)(rnd(i,2,211) * 3);
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx)
                if (dx*dx+dy*dy <= r*r) {
                    int px = ox+cx+dx, py = oy+cy+dy;
                    if (px>=ox&&px<ox+16 && py>=oy&&py<oy+16)
                        setpix(img, px, py, { 180, 150, 120, 255 });
                }
    }
}
static void drawItemRawGold(Image& img, int tx, int ty) {
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    int ox = tx * 16, oy = ty * 16;
    for (int i = 0; i < 5; ++i) {
        int cx = 3 + (int)(rnd(i,0,221) * 10);
        int cy = 3 + (int)(rnd(i,1,221) * 10);
        int r = 2 + (int)(rnd(i,2,221) * 3);
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx)
                if (dx*dx+dy*dy <= r*r) {
                    int px = ox+cx+dx, py = oy+cy+dy;
                    if (px>=ox&&px<ox+16 && py>=oy&&py<oy+16)
                        setpix(img, px, py, { 240, 200, 40, 255 });
                }
    }
}
static void drawItemDiamond(Image& img, int tx, int ty) {
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    int ox = tx * 16, oy = ty * 16;
    // 大菱形晶体（占据更多空间）
    for (int dy = -6; dy <= 6; ++dy)
        for (int dx = -(6-abs(dy)); dx <= (6-abs(dy)); ++dx) {
            int px = ox + 8 + dx, py = oy + 8 + dy;
            int bright = 180 - abs(dy)*12 - abs(dx)*10;
            if (bright < 60) bright = 60;
            setpix(img, px, py, { (unsigned char)bright, 255, (unsigned char)bright, 255 });
        }
    // 外描边（深色轮廓）
    for (int dy = -6; dy <= 6; ++dy) {
        int dx = 6 - abs(dy);
        setpix(img, ox+8+dx, oy+8+dy, { 30, 160, 160, 255 });
        setpix(img, ox+8-dx, oy+8+dy, { 30, 160, 160, 255 });
    }
    // 高光点
    setpix(img, ox+7, oy+5, { 200, 255, 255, 255 });
    setpix(img, ox+8, oy+5, { 200, 255, 255, 255 });
}
static void drawItemStick(Image& img, int tx, int ty) {
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    int ox = tx * 16, oy = ty * 16;
    // 木棍：棕色细长条
    for (int y = 1; y < 15; ++y) {
        int w = (y < 3 || y > 12) ? 2 : 3;
        for (int x = 8 - w/2; x <= 8 + w/2; ++x)
            setpix(img, ox + x, oy + y, { 130, 90, 55, 255 });
    }
    // 木纹线
    for (int y = 2; y < 14; ++y)
        setpix(img, ox + 8, oy + y, { 100, 70, 40, 255 });
    // 两端加深
    for (int x = 7; x <= 9; ++x) {
        setpix(img, ox + x, oy + 1, { 100, 70, 40, 255 });
        setpix(img, ox + x, oy + 14, { 100, 70, 40, 255 });
    }
}
static void drawItemIngot(Image& img, int tx, int ty, Color base) {
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    int ox = tx * 16, oy = ty * 16;
    // MC 风格锭形：两端宽中间窄，高光在左侧
    int leftEdge[16] = {2,1,0,0,1,2,3,3,3,2,1,0,0,1,2,3};
    for (int y = 0; y < 16; ++y) {
        int left = leftEdge[y], right = 14 - leftEdge[y];
        for (int x = left; x <= right; ++x) {
            // 左侧受光，右侧背光
            int d = (x - 8) * 5;
            if (x - left <= 1) d += 15;  // 左侧高光
            if (right - x <= 1) d -= 20; // 右侧阴影
            Color c = shade(base, d);
            setpix(img, ox + x, oy + y, c);
        }
    }
    // 底部阴影
    for (int y = 13; y < 16; ++y)
        for (int x = 3; x <= 12; ++x)
            shade(*((Color*)img.data+(oy+y)*img.width+ox+x), -25);
}
static void drawItemIronIngot(Image& img, int tx, int ty) { drawItemIngot(img, tx, ty, { 210, 190, 160, 255 }); }
static void drawItemGoldIngot(Image& img, int tx, int ty) { drawItemIngot(img, tx, ty, { 240, 210, 60, 255 }); }

// 镐子图标：按用户提供的精确像素图
// 0=透明, 1=镐头, 2=镐头深色(边缘), 3=镐头高光, 4=手柄
static const int PICKAXE_MAP[16][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // row 0
    {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},  // row 1
    {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},  // row 2
    {0,1,1,1,1,0,0,4,4,0,0,1,1,1,1,0},  // row 3
    {0,1,1,0,0,0,0,4,4,0,0,0,0,1,1,0},  // row 4
    {0,1,0,0,0,0,0,4,4,0,0,0,0,0,1,0},  // row 5
    {0,0,0,0,0,0,0,4,4,0,0,0,0,0,0,0},  // row 6
    {0,0,0,0,0,0,0,4,4,0,0,0,0,0,0,0},  // row 7
    {0,0,0,0,0,0,0,4,4,0,0,0,0,0,0,0},  // row 8
    {0,0,0,0,0,0,0,4,4,0,0,0,0,0,0,0},  // row 9
    {0,0,0,0,0,0,0,4,4,0,0,0,0,0,0,0},  // row 10
    {0,0,0,0,0,0,0,4,4,0,0,0,0,0,0,0},  // row 11
    {0,0,0,0,0,0,0,4,4,0,0,0,0,0,0,0},  // row 12
    {0,0,0,0,0,0,0,4,4,0,0,0,0,0,0,0},  // row 13
    {0,0,0,0,0,0,0,4,4,0,0,0,0,0,0,0},  // row 14
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},  // row 15
};

static void drawPickaxe(Image& img, int tx, int ty, Color handleCol, Color headCol) {
    int ox = tx * 16, oy = ty * 16;
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    Color hD = shade(headCol, -40), hL = shade(headCol, 40);

    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            Color c = { 0, 0, 0, 0 };
            switch (PICKAXE_MAP[y][x]) {
                case 1: c = headCol; break;
                case 2: c = hD; break;
                case 3: c = hL; break;
                case 4: c = handleCol; break;
                default: continue;
            }
            setpix(img, ox + x, oy + y, c);
        }
    }
}

static void drawItemWoodPickaxe(Image& img, int tx, int ty) { drawPickaxe(img, tx, ty, { 120, 85, 50, 255 }, { 150, 110, 70, 255 }); }
static void drawItemStonePickaxe(Image& img, int tx, int ty) { drawPickaxe(img, tx, ty, { 100, 70, 40, 255 }, { 120, 120, 120, 255 }); }
static void drawItemIronPickaxe(Image& img, int tx, int ty) { drawPickaxe(img, tx, ty, { 100, 70, 40, 255 }, { 190, 170, 140, 255 }); }
static void drawItemGoldPickaxe(Image& img, int tx, int ty) { drawPickaxe(img, tx, ty, { 100, 70, 40, 255 }, { 235, 200, 50, 255 }); }
static void drawItemDiamondPickaxe(Image& img, int tx, int ty) { drawPickaxe(img, tx, ty, { 100, 70, 40, 255 }, { 70, 210, 200, 255 }); }

// ---- 食物纹理 ----
static void drawItemApple(Image& img, int tx, int ty) {
    int ox = tx * 16, oy = ty * 16;
    fillTile(img, tx, ty, { 0, 0, 0, 0 }); // 透明背景
    // 苹果主体（红色圆形）
    for (int y = 3; y <= 12; ++y) {
        for (int x = 4; x <= 11; ++x) {
            float dx = (x - 7.5f), dy = (y - 7.5f);
            if (dx*dx + dy*dy <= 20.0f)
                setpix(img, ox + x, oy + y, { 200, 40, 40, 255 });
        }
    }
    // 高光
    for (int y = 5; y <= 7; ++y)
        for (int x = 5; x <= 7; ++x)
            setpix(img, ox + x, oy + y, { 240, 100, 80, 255 });
    // 茎
    for (int y = 1; y <= 3; ++y)
        setpix(img, ox + 8, oy + y, { 80, 60, 30, 255 });
    // 叶子
    setpix(img, ox + 9, oy + 2, { 60, 140, 40, 255 });
    setpix(img, ox + 10, oy + 3, { 60, 140, 40, 255 });
}
static void drawItemBread(Image& img, int tx, int ty) {
    int ox = tx * 16, oy = ty * 16;
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    // 面包主体
    Color crust = { 180, 130, 60, 255 };
    Color inside = { 220, 190, 140, 255 };
    for (int y = 3; y <= 12; ++y) {
        int w = (y < 5 || y > 10) ? 4 : 6;
        for (int x = 8 - w; x <= 8 + w; ++x) {
            setpix(img, ox + x, oy + y, (y <= 4 || y >= 11) ? crust : inside);
        }
    }
    // 面包皮边缘
    for (int y = 5; y <= 10; ++y) {
        setpix(img, ox + 2, oy + y, crust);
        setpix(img, ox + 13, oy + y, crust);
    }
}
static void drawItemRawMeat(Image& img, int tx, int ty, Color base) {
    int ox = tx * 16, oy = ty * 16;
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    // 肉块形状
    for (int y = 3; y <= 12; ++y) {
        int w = (y < 5 || y > 10) ? 3 : 5;
        for (int x = 8 - w; x <= 8 + w; ++x)
            setpix(img, ox + x, oy + y, base);
    }
    // 纹理
    for (int i = 0; i < 6; ++i) {
        int sx = 4 + (int)(rnd(i,0,301) * 8);
        int sy = 4 + (int)(rnd(i,1,301) * 8);
        setpix(img, ox + sx, oy + sy, shade(base, -30));
    }
}
static void drawItemRawBeef(Image& img, int tx, int ty) { drawItemRawMeat(img, tx, ty, { 180, 60, 60, 255 }); }
static void drawItemRawPork(Image& img, int tx, int ty) { drawItemRawMeat(img, tx, ty, { 200, 140, 120, 255 }); }
static void drawItemRawChicken(Image& img, int tx, int ty) {
    int ox = tx * 16, oy = ty * 16;
    fillTile(img, tx, ty, { 0, 0, 0, 0 });
    // 鸡腿形状
    for (int y = 4; y <= 11; ++y) {
        int w = (y >= 5 && y <= 8) ? 2 : 1;
        for (int x = 8 - w; x <= 8 + w; ++x)
            setpix(img, ox + x, oy + y, { 220, 190, 160, 255 });
    }
    // 骨头
    for (int y = 2; y <= 5; ++y) {
        setpix(img, ox + 8, oy + y, { 240, 230, 210, 255 });
        setpix(img, ox + 9, oy + 3, { 240, 230, 210, 255 });
    }
}

// --------- 主函数 ---------
TextureAtlas atlasLoad() {
    Image img = GenImageColor(ATLAS_W, ATLAS_H, BLACK);
    auto S = [](int i,int&c,int&r){ c=i%ATLAS_COLS; r=i/ATLAS_COLS; };
    int c,r;
    S(TEX_GRASS_TOP,c,r);  drawGrassTop(img,c,r);
    S(TEX_GRASS_SIDE,c,r); drawGrassSide(img,c,r);
    S(TEX_DIRT,c,r);       drawDirt(img,c,r);
    S(TEX_STONE,c,r);      drawStone(img,c,r);
    S(TEX_SAND,c,r);       drawSand(img,c,r);
    S(TEX_WOOD_SIDE,c,r);  drawWoodSide(img,c,r);
    S(TEX_WOOD_TOP,c,r);   drawWoodTop(img,c,r);
    S(TEX_LEAVES,c,r);     drawLeaves(img,c,r);
    S(TEX_PLANK,c,r);      drawPlank(img,c,r);
    S(TEX_COBBLE,c,r);     drawCobble(img,c,r);
    S(TEX_BEDROCK,c,r);    drawBedrock(img,c,r);
    S(TEX_WATER,c,r);      drawWater(img,c,r);
    S(TEX_COAL_ORE,c,r);   drawCoalOre(img,c,r);
    S(TEX_IRON_ORE,c,r);   drawIronOre(img,c,r);
    S(TEX_GOLD_ORE,c,r);   drawGoldOre(img,c,r);
    S(TEX_DIAMOND_ORE,c,r);drawDiamondOre(img,c,r);
    S(TEX_CRAFT_TOP,c,r);  drawCraftTop(img,c,r);
    S(TEX_CRAFT_SIDE,c,r); drawCraftSide(img,c,r);
    S(TEX_FURNACE,c,r);    drawFurnace(img,c,r);
	    S(TEX_FURNACE_UNLIT,c,r); drawFurnaceUnlit(img,c,r);
	    S(TEX_CHEST,c,r);      drawChest(img,c,r);
	    S(TEX_CHEST_SIDE,c,r); drawChestSide(img,c,r);
    // 物品纹理
    S(TEX_ITEM_COAL,c,r);           drawItemCoal(img,c,r);
    S(TEX_ITEM_RAW_IRON,c,r);       drawItemRawIron(img,c,r);
    S(TEX_ITEM_RAW_GOLD,c,r);       drawItemRawGold(img,c,r);
    S(TEX_ITEM_DIAMOND,c,r);        drawItemDiamond(img,c,r);
    S(TEX_ITEM_STICK,c,r);          drawItemStick(img,c,r);
    S(TEX_ITEM_IRON_INGOT,c,r);     drawItemIronIngot(img,c,r);
    S(TEX_ITEM_GOLD_INGOT,c,r);     drawItemGoldIngot(img,c,r);
    S(TEX_ITEM_WOOD_PICKAXE,c,r);   drawItemWoodPickaxe(img,c,r);
    S(TEX_ITEM_STONE_PICKAXE,c,r);  drawItemStonePickaxe(img,c,r);
    S(TEX_ITEM_IRON_PICKAXE,c,r);   drawItemIronPickaxe(img,c,r);
    S(TEX_ITEM_GOLD_PICKAXE,c,r);   drawItemGoldPickaxe(img,c,r);
    S(TEX_ITEM_DIAMOND_PICKAXE,c,r);drawItemDiamondPickaxe(img,c,r);
    // 食物纹理
    S(TEX_ITEM_APPLE,c,r);         drawItemApple(img,c,r);
    S(TEX_ITEM_BREAD,c,r);         drawItemBread(img,c,r);
    S(TEX_ITEM_RAW_BEEF,c,r);      drawItemRawBeef(img,c,r);
    S(TEX_ITEM_RAW_PORK,c,r);      drawItemRawPork(img,c,r);
    S(TEX_ITEM_RAW_CHICKEN,c,r);   drawItemRawChicken(img,c,r);

    TextureAtlas atlas{};
	    atlas.texture = LoadTextureFromImage(img);
	    SetTextureFilter(atlas.texture, TEXTURE_FILTER_POINT);
	    SetTextureWrap(atlas.texture, TEXTURE_WRAP_CLAMP);
    UnloadImage(img);

    atlas.w = 1.0f / ATLAS_COLS;
    atlas.h = 1.0f / ATLAS_ROWS;
	    for (int i = 0; i < 64; ++i) {
        atlas.u[i] = (float)(i % ATLAS_COLS) / ATLAS_COLS;
        atlas.v[i] = (float)(i / ATLAS_COLS) / ATLAS_ROWS;
    }
    return atlas;
}
void atlasUnload(TextureAtlas& atlas) { UnloadTexture(atlas.texture); }
