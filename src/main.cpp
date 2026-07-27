// main.cpp - 程序入口与主循环（生存模式、背包合成、新菜单）
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "block.h"
#include "world.h"
#include "player.h"
#include "renderer.h"
#include "crafting.h"
#include "mob.h"
#include "drop.h"
#include "chest.h"
#include <cmath>

enum class GameState {
    MAIN_MENU,
    PLAYING,
    PAUSED,
};

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Voxel Sandbox - Survival");
    SetTargetFPS(60);
    SetExitKey(0);
    loadGameFont();

    unsigned int seed = 20260711u;
    World world;
    worldInit(world, seed, 6);

    Player player;
    playerInit(player, world, 8, 8);

    for (int i = 0; i < 60; ++i) worldUpdate(world, player.pos, 4);
    int gy = worldFindGroundY(world, 8, 8);
    player.pos.y = (float)gy + 0.1f;
    for (int i = 0; i < 200; ++i) worldBuildDirtyMeshes(world, 20);

    GameTime gt;
    gt.dayLength = 2400.0f;
    gt.time = 0.30f * gt.dayLength;

    GameState state = GameState::MAIN_MENU;
    bool showDebug = false;
    bool showInventory = false;  // ← 替代 INVENTORY 状态
    ChestGUI chestGUI = { false, 0, 0, 0, { 0, 0 } };
    bool showChest = false;
    FurnaceGUI furnaceGUI = { false, 0, 0, 0, { 0, 0 } };
    bool showFurnace = false;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

	        if (IsKeyPressed(KEY_F1)) showDebug = !showDebug;

	        // Ctrl+Shift+Alt+K：血量设为半颗心（调试用）
	        if (IsKeyPressed(KEY_K) && IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_LEFT_ALT)) {
	            player.hp = 0.5f; // 半颗心，再受伤即死
	        }

        // Ctrl+Shift+Alt+G：给予调试物品
        if (IsKeyPressed(KEY_G) && IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_LEFT_ALT)) {
            // 给予全套工具和材料方便测试
            int giveItems[] = {
                ITEM_WOOD_PICKAXE, ITEM_STONE_PICKAXE, ITEM_IRON_PICKAXE,
                ITEM_GOLD_PICKAXE, ITEM_DIAMOND_PICKAXE,
                ITEM_COAL, ITEM_RAW_IRON, ITEM_RAW_GOLD, ITEM_DIAMOND,
                ITEM_STICK, ITEM_IRON_INGOT, ITEM_GOLD_INGOT,
                ITEM_CHEST, ITEM_FURNACE, ITEM_CRAFTING_TABLE,
                ITEM_APPLE, ITEM_BREAD, ITEM_RAW_BEEF, ITEM_RAW_PORK, ITEM_RAW_CHICKEN
            };
            for (int gi : giveItems) {
                player.inventory.addItem(gi, 8);
            }
            // 给一些常见方块
            player.inventory.addItem(ITEM_STONE, 64);
            player.inventory.addItem(ITEM_DIRT, 64);
            player.inventory.addItem(ITEM_WOOD, 64);
            player.inventory.addItem(ITEM_COBBLE, 64);
        }

        // ====== 主菜单 ======
        if (state == GameState::MAIN_MENU) {
            gt.time += dt;
            if (gt.time >= gt.dayLength) gt.time -= gt.dayLength;
            BeginDrawing();
            int r = drawTitleScreen(gt, true);
            EndDrawing();
            if (r == 1) {
                state = GameState::PLAYING;
                DisableCursor();
                player.miningTimer = 0.0f;
                player.miningTarget = 0;
            }
            else if (r == 2 || IsKeyPressed(KEY_ESCAPE)) break;
            continue;
        }

        // ====== 暂停 ======
	        if (state == GameState::PAUSED) {
	            BeginDrawing(); ClearBackground(BLACK); drawSky(gt, player.camera);
	            BeginMode3D(player.camera); worldDraw(world); EndMode3D();
	            drawCrosshair(); drawHUD(player, world, gt, showDebug, world.atlas);
	            int pauseR = drawPausedOverlay();
	            EndDrawing();
	            if (pauseR == 1) { state = GameState::PLAYING; DisableCursor(); continue; }
	            if (pauseR == 2) break;
	            continue;
	        }

        // ====== 游玩状态 ======
        gt.time += dt;
        if (gt.time >= gt.dayLength) gt.time -= gt.dayLength;

        // E 键切换背包 / 关闭箱子
        if (IsKeyPressed(KEY_E)) {
            if (showChest) {
                // 回收手持物品
                if (chestGUI.grabbed.item > 0) {
                    player.inventory.addItem(chestGUI.grabbed.item, chestGUI.grabbed.count);
                    chestGUI.grabbed = { 0, 0 };
                }
                showChest = false;
                chestGUI.open = false;
                DisableCursor();
            } else if (showFurnace) {
                if (furnaceGUI.grabbed.item > 0) {
                    player.inventory.addItem(furnaceGUI.grabbed.item, furnaceGUI.grabbed.count);
                    furnaceGUI.grabbed = { 0, 0 };
                }
                showFurnace = false;
                furnaceGUI.open = false;
                DisableCursor();
            } else {
                showInventory = !showInventory;
                if (showInventory) {
                    EnableCursor();
                    player.craftGrid.clear();
                    player.craftGrid.is3x3 = false;  // 仅右键工作台才能用 3x3
                } else {
                    DisableCursor();
                    // 关闭背包时回收合成网格物品
                    for (int r = 0; r < GRID_SIZE; ++r)
                        for (int c = 0; c < GRID_SIZE; ++c)
                            if (player.craftGrid.slots[r][c].item > 0) {
                                player.inventory.addItem(player.craftGrid.slots[r][c].item, player.craftGrid.slots[r][c].count);
                                player.craftGrid.slots[r][c] = { 0, 0 };
                            }
                    // 手持物品也回收
                    if (player.grabbed.item > 0) {
                        player.inventory.addItem(player.grabbed.item, player.grabbed.count);
                        player.grabbed = { 0, 0 };
                    }
                }
            }
        }
	        if (IsKeyPressed(KEY_ESCAPE)) {
	            if (showChest) {
	                if (chestGUI.grabbed.item > 0) {
	                    player.inventory.addItem(chestGUI.grabbed.item, chestGUI.grabbed.count);
	                    chestGUI.grabbed = { 0, 0 };
	                }
	                showChest = false;
	                chestGUI.open = false;
	                DisableCursor();
	            } else if (showFurnace) {
	                if (furnaceGUI.grabbed.item > 0) {
	                    player.inventory.addItem(furnaceGUI.grabbed.item, furnaceGUI.grabbed.count);
	                    furnaceGUI.grabbed = { 0, 0 };
	                }
	                showFurnace = false;
	                furnaceGUI.open = false;
	                DisableCursor();
	            } else if (showInventory) {
	                // 关闭背包（回收合成格和手持物品）
	                for (int r = 0; r < GRID_SIZE; ++r)
	                    for (int c = 0; c < GRID_SIZE; ++c)
	                        if (player.craftGrid.slots[r][c].item > 0) {
	                            player.inventory.addItem(player.craftGrid.slots[r][c].item, player.craftGrid.slots[r][c].count);
	                            player.craftGrid.slots[r][c] = { 0, 0 };
	                        }
	                if (player.grabbed.item > 0) {
	                    player.inventory.addItem(player.grabbed.item, player.grabbed.count);
	                    player.grabbed = { 0, 0 };
	                }
	                showInventory = false;
	                DisableCursor();
	            } else {
	                state = GameState::PAUSED; EnableCursor();
	            }
            continue;
	        }

	        // 熔炉冶炼（每帧更新，无论界面状态）
	        world.furnaceSys.updateAll(dt);

			        if (!showInventory && !showChest && !showFurnace) {
			            playerUpdate(player, world, dt);
		            worldUpdate(world, player.pos, 1);
		            dropUpdate(world.drops, dt, world);
		            worldBuildDirtyMeshes(world, 2);

		            // 快捷栏
	            for (int i = 0; i < 8; ++i)
	                if (IsKeyPressed((KeyboardKey)(KEY_ONE + i))) player.inventory.selectedSlot = i;
	            float wh = GetMouseWheelMove();
	            if (wh != 0) player.inventory.selectedSlot = (player.inventory.selectedSlot + (wh > 0 ? -1 : 1) + 8) % 8;

		            // ---- Q 键丢弃物品 ----
	                    if (IsKeyPressed(KEY_Q)) {
			                int sel = player.inventory.selectedItem();
			                int cnt = player.inventory.selectedCount();
			                if (sel > 0 && cnt > 0) {
			                    int dropCount = IsKeyDown(KEY_LEFT_CONTROL) ? cnt : 1;
			                    Vector3 fwd = playerForward(player);
			                    // 从相机眼睛位置向前投掷
			                    Vector3 dropPos = {
			                        player.camera.position.x + fwd.x * 0.8f,
			                        player.camera.position.y + fwd.y * 0.8f,
			                        player.camera.position.z + fwd.z * 0.8f
			                    };
			                    // 若起始点在实体方块内，逐步回退到玩家侧（避免穿模）
			                    int dcx = (int)floorf(dropPos.x);
			                    int dcy = (int)floorf(dropPos.y);
			                    int dcz = (int)floorf(dropPos.z);
			                    if (isSolid(worldGetBlock(world, dcx, dcy, dcz))) {
			                        // 回退到相机位置前方 0.2 单位
			                        dropPos = {
			                            player.camera.position.x + fwd.x * 0.2f,
			                            player.camera.position.y + fwd.y * 0.2f,
			                            player.camera.position.z + fwd.z * 0.2f
			                        };
			                    }
			                    dropSpawn(world.drops, dropPos, sel, dropCount, false, fwd);
			                    player.inventory.removeItem(sel, dropCount);
			                }
			            }

	            // ---- 右键交互：放置方块 / 进食 / 交互 ----
	            RaycastHit hit = playerRaycast(player, world);
	            bool rdNow = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
	            if (rdNow) {
	                int sel = player.inventory.selectedItem();
	                // 如果手持方块且瞄准了方块：放置
	                if (hit.hit && isPlaceableItem(sel)) {
	                    int px = hit.bx + hit.nx, py = hit.by + hit.ny, pz = hit.bz + hit.nz;
	                    Vector3 pmn = { pminX(player), pminY(player), pminZ(player) };
	                    Vector3 pmx = { pmaxX(player), pmaxY(player), pmaxZ(player) };
		                    if (!(px + 1 > pmn.x && px < pmx.x && py + 1 > pmn.y && py < pmx.y && pz + 1 > pmn.z && pz < pmx.z)
		                        && worldGetBlock(world, px, py, pz) == BLOCK_AIR) {
		                        BlockType bt = itemToBlock(sel);
		                        worldSetBlock(world, px, py, pz, bt);
		                        // 箱子：记录朝向（根据玩家 yaw 计算）
		                        if (bt == BLOCK_CHEST) {
		                            float yaw = fmodf(player.yaw, 2.0f * PI);
		                            if (yaw < 0) yaw += 2.0f * PI;
                            int dir = ((int)((yaw + PI / 4.0f) / (PI / 2.0f))) % 4;
                            auto& cd = world.chestSys.getOrCreate(px, py, pz);
                            cd.facing = (dir + 2) % 4; // 旋转180°，锁扣朝向玩家
		                        }
		                        player.inventory.consumeSelected();
		                    }
		                } else if (hit.hit && worldGetBlock(world, hit.bx, hit.by, hit.bz) == BLOCK_FURNACE) {
		                    // 右键熔炉：打开熔炉界面
		                    showFurnace = true;
		                    furnaceGUI.open = true;
		                    furnaceGUI.fx = hit.bx;
		                    furnaceGUI.fy = hit.by;
		                    furnaceGUI.fz = hit.bz;
		                    furnaceGUI.grabbed = { 0, 0 };
		                    world.furnaceSys.getOrCreate(hit.bx, hit.by, hit.bz);
		                    EnableCursor();
		                } else if (hit.hit && worldGetBlock(world, hit.bx, hit.by, hit.bz) == BLOCK_CRAFTING_TABLE) {
		                    // 右键工作台：打开 3x3 合成（替代靠近检测）
		                    showInventory = true;
		                    player.craftGrid.clear();
		                    player.craftGrid.is3x3 = true;
		                    EnableCursor();
	                } else if (hit.hit && worldGetBlock(world, hit.bx, hit.by, hit.bz) == BLOCK_CHEST) {
	                    // 右键箱子：打开箱子界面
	                    showChest = true;
	                    chestGUI.open = true;
	                    chestGUI.cx = hit.bx;
	                    chestGUI.cy = hit.by;
	                    chestGUI.cz = hit.bz;
	                    chestGUI.grabbed = { 0, 0 };
	                    world.chestSys.getOrCreate(hit.bx, hit.by, hit.bz);
	                    EnableCursor();
	                } else if (isFood(sel) && player.hunger < 20.0f) {
	                    // 非瞄准方块 / 手持食物：进食
	                    player.hunger += (float)foodHunger(sel);
	                    if (player.hunger > 20.0f) player.hunger = 20.0f;
	                    player.inventory.consumeSelected();
	                }
	            }

		        } else {
            // ====== 箱子界面 ======
            if (showChest) {
                if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ESCAPE)) {
                    showChest = false;
                    chestGUI.open = false;
                    // 回收手持物品
                    if (chestGUI.grabbed.item > 0) {
                        player.inventory.addItem(chestGUI.grabbed.item, chestGUI.grabbed.count);
                        chestGUI.grabbed = { 0, 0 };
                    }
                    DisableCursor();
                    goto drawn;
                }

                const int S = 42, G = 4, sd = S + G;
                Vector2 m = GetMousePosition();
                int sw = GetScreenWidth();
                // 箱子 27 格（3 行 x 9 列）
                int chestCols = 9, chestRows = 3;
                int totalW = chestCols * sd;
                int chestBX = sw / 2 - totalW / 2, chestBY = GetScreenHeight() / 2 - 30 - chestRows * sd;
                // 玩家背包区域（在箱子下方）
                int playerBX = sw / 2 - 6 * sd / 2, playerBY = chestBY + chestRows * sd + 30;
                int hby = playerBY + 3 * sd + 14;

                auto slotRect = [&](int x, int y) -> Rectangle {
                    return { (float)x, (float)y, S, S };
                };

                bool lc = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                bool rc = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

                { // 新作用域
                ChestData& chest = world.chestSys.getOrCreate(chestGUI.cx, chestGUI.cy, chestGUI.cz);

                // ---- 箱子格子 ----
                for (int r = 0; r < chestRows; ++r) {
                    for (int c = 0; c < chestCols; ++c) {
                        int idx = r * chestCols + c;
                        int sx = chestBX + c * sd, sy = chestBY + r * sd;
                        if (!CheckCollisionPointRec(m, slotRect(sx, sy))) continue;

	                        if (lc) {
	                            // 左键：同类合并 / 不同类交换
	                            if (chestGUI.grabbed.item == chest.slots[idx].item && chestGUI.grabbed.item > 0
	                                && chest.slots[idx].count < 64) {
	                                int space = 64 - chest.slots[idx].count;
	                                int take = (chestGUI.grabbed.count < space) ? chestGUI.grabbed.count : space;
	                                chest.slots[idx].count += take;
	                                chestGUI.grabbed.count -= take;
	                                if (chestGUI.grabbed.count <= 0) chestGUI.grabbed = { 0, 0 };
	                            } else {
	                                InventorySlot tmp = chestGUI.grabbed;
	                                chestGUI.grabbed = chest.slots[idx];
	                                chest.slots[idx] = tmp;
	                            }
                        } else if (rc) {
                            if (chestGUI.grabbed.item == 0 && chest.slots[idx].item > 0) {
                                int half = (chest.slots[idx].count + 1) / 2;
                                chestGUI.grabbed = { chest.slots[idx].item, half };
                                chest.slots[idx].count -= half;
                                if (chest.slots[idx].count <= 0) chest.slots[idx] = { 0, 0 };
                            } else if (chestGUI.grabbed.item > 0 && chestGUI.grabbed.count > 0) {
                                int& cnt = chest.slots[idx].count;
                                int& itm = chest.slots[idx].item;
                                if (itm == 0) { itm = chestGUI.grabbed.item; cnt = 1; }
                                else if (itm == chestGUI.grabbed.item && cnt < 64) cnt++;
                                else continue;
                                chestGUI.grabbed.count--;
                                if (chestGUI.grabbed.count <= 0) chestGUI.grabbed = { 0, 0 };
                            }
                        }
                        goto chestEnd;
                    }
                }

                // ---- 快捷栏 + 背包格子 ----
                for (int i = 0; i < TOTAL_SLOTS; ++i) {
                    bool isHot = i < HOTBAR_SLOTS;
                    int sx = isHot ? (playerBX + i * sd) : (playerBX + ((i - HOTBAR_SLOTS) % 6) * sd);
                    int sy = isHot ? hby : (playerBY + ((i - HOTBAR_SLOTS) / 6) * sd);
                    if (!CheckCollisionPointRec(m, slotRect(sx, sy))) continue;

	                    if (lc) {
	                        // 左键：同类合并 / 不同类交换
	                        if (chestGUI.grabbed.item == player.inventory.slots[i].item && chestGUI.grabbed.item > 0
	                            && player.inventory.slots[i].count < 64) {
	                            int space = 64 - player.inventory.slots[i].count;
	                            int take = (chestGUI.grabbed.count < space) ? chestGUI.grabbed.count : space;
	                            player.inventory.slots[i].count += take;
	                            chestGUI.grabbed.count -= take;
	                            if (chestGUI.grabbed.count <= 0) chestGUI.grabbed = { 0, 0 };
	                        } else {
	                            InventorySlot tmp = chestGUI.grabbed;
	                            chestGUI.grabbed = player.inventory.slots[i];
	                            player.inventory.slots[i] = tmp;
	                        }
                    } else if (rc) {
                        if (chestGUI.grabbed.item == 0 && player.inventory.slots[i].item > 0) {
                            int half = (player.inventory.slots[i].count + 1) / 2;
                            chestGUI.grabbed = { player.inventory.slots[i].item, half };
                            player.inventory.slots[i].count -= half;
                            if (player.inventory.slots[i].count <= 0) player.inventory.slots[i] = { 0, 0 };
                        } else if (chestGUI.grabbed.item > 0 && chestGUI.grabbed.count > 0) {
                            int& cnt = player.inventory.slots[i].count;
                            int& itm = player.inventory.slots[i].item;
                            if (itm == 0) { itm = chestGUI.grabbed.item; cnt = 1; }
                            else if (itm == chestGUI.grabbed.item && cnt < 64) cnt++;
                            else continue;
                            chestGUI.grabbed.count--;
                            if (chestGUI.grabbed.count <= 0) chestGUI.grabbed = { 0, 0 };
                        }
                    }
                    break;
                }

                chestEnd:;
                } // end chest interaction block scope
                goto drawn;
            }

            // ====== 熔炉界面 ======
            if (showFurnace) {
                if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ESCAPE)) {
                    if (furnaceGUI.grabbed.item > 0) {
                        player.inventory.addItem(furnaceGUI.grabbed.item, furnaceGUI.grabbed.count);
                        furnaceGUI.grabbed = { 0, 0 };
                    }
                    showFurnace = false;
                    furnaceGUI.open = false;
                    DisableCursor();
                    goto drawn;
                }

                const int S = 42, G = 4, sd = S + G;
                Vector2 m = GetMousePosition();
                int sw = GetScreenWidth(), sh = GetScreenHeight();
                int cx = sw / 2, cy = sh / 2;
                // 熔炉面板居中
                int bx = cx - 70, by = cy - 55;
                FurnaceData& fur = world.furnaceSys.getOrCreate(furnaceGUI.fx, furnaceGUI.fy, furnaceGUI.fz);

                auto slotRect = [&](int x, int y) -> Rectangle {
                    return { (float)x, (float)y, S, S };
                };
                bool lc = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                bool rc = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

                // 点击输入槽（上方）
                auto handleClick = [&](InventorySlot& slot) {
                    if (lc) {
                        // 左键：同类合并 / 不同类交换
                        if (furnaceGUI.grabbed.item == slot.item && furnaceGUI.grabbed.item > 0
                            && slot.count < 64) {
                            int space = 64 - slot.count;
                            int take = (furnaceGUI.grabbed.count < space) ? furnaceGUI.grabbed.count : space;
                            slot.count += take;
                            furnaceGUI.grabbed.count -= take;
                            if (furnaceGUI.grabbed.count <= 0) furnaceGUI.grabbed = { 0, 0 };
                        } else {
                            InventorySlot tmp = furnaceGUI.grabbed;
                            furnaceGUI.grabbed = slot;
                            slot = tmp;
                        }
                    } else if (rc) {
                        if (furnaceGUI.grabbed.item == 0 && slot.item > 0) {
                            int half = (slot.count + 1) / 2;
                            furnaceGUI.grabbed = { slot.item, half };
                            slot.count -= half;
                            if (slot.count <= 0) slot = { 0, 0 };
                        } else if (furnaceGUI.grabbed.item > 0 && furnaceGUI.grabbed.count > 0) {
                            if (slot.item == 0) { slot = { furnaceGUI.grabbed.item, 1 }; }
                            else if (slot.item == furnaceGUI.grabbed.item && slot.count < 64) slot.count++;
                            else return;
                            furnaceGUI.grabbed.count--;
                            if (furnaceGUI.grabbed.count <= 0) furnaceGUI.grabbed = { 0, 0 };
                        }
                    }
                };

                // 输入槽
                int inputX = bx, inputY = by;
                if (CheckCollisionPointRec(m, slotRect(inputX, inputY))) handleClick(fur.input);
                // 燃料槽
                int fuelX = bx, fuelY = by + sd + 8;
                if (CheckCollisionPointRec(m, slotRect(fuelX, fuelY))) handleClick(fur.fuel);
                // 输出槽
                int outX = bx + sd * 2 + 20, outY = by + sd / 2;
                if (CheckCollisionPointRec(m, slotRect(outX, outY))) handleClick(fur.output);

                // 玩家背包（下方）
                int playerBX = cx - 6 * sd / 2, playerBY = by + sd * 2 + 45;
                int hby = playerBY + 3 * sd + 14;
                // 背包
                for (int i = 0; i < BACKPACK_SLOTS; ++i) {
                    int r = i / 6, c = i % 6;
                    int sx = playerBX + c * sd, sy = playerBY + r * sd;
                    if (CheckCollisionPointRec(m, slotRect(sx, sy)))
                        handleClick(player.inventory.slots[HOTBAR_SLOTS + i]);
                }
                // 快捷栏
                for (int i = 0; i < HOTBAR_SLOTS; ++i) {
                    int sx = playerBX + i * sd, sy = hby;
                    if (CheckCollisionPointRec(m, slotRect(sx, sy)))
                        handleClick(player.inventory.slots[i]);
                }

                goto drawn;
            }

            // ====== 背包/合成界面 ======
            // ---- 右键拖拽标记 ----
            static bool dragActive = false;
            static int dragLastSlot = -1;
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                if (!dragActive) { dragActive = true; dragLastSlot = -1; }
            } else { dragActive = false; dragLastSlot = -1; }

            // ---- 背包打开：MC 风格拖拽合成 ----
            const int S = 42, G = 4;
            Vector2 m = GetMousePosition();
            int sw = GetScreenWidth();
            int bx = sw / 2 - 6 * (S + G) / 2, by = GetScreenHeight() / 2 - 20;
            int hby = by + 3 * (S + G) + 14;
            int gx = bx + 6 * (S + G) + 40, gy = by;
            int gs = (player.craftGrid.is3x3 ? 3 : 2);
            int ox = gx + gs * (S + G) + 20, oy = gy + 8 + (gs - 1) * (S + G) / 2;
            auto slotR = [&](int x, int y) -> Rectangle { return { (float)x, (float)y, S, S }; };

            bool lc = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            bool rc = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

            // 工作台 3x3 模式由打开方式决定（右键工作台 = 3x3，否则 2x2）
            gs = (player.craftGrid.is3x3 ? 3 : 2);
            ox = gx + gs * (S + G) + 20;
            oy = gy + 8 + (gs - 1) * (S + G) / 2;

            // ---- 输出槽：点击合成 ----
            {
                int ri = matchRecipe(player.craftGrid);
                if (ri >= 0 && CheckCollisionPointRec(m, slotR(ox, oy)) && lc) {
                    InventorySlot out = { SHAPED_RECIPES[ri].resultItem, SHAPED_RECIPES[ri].resultCount };
                    if (IsKeyDown(KEY_LEFT_SHIFT)) {
                        // Shift+左键：批量合成最大数量
                        while (true) {
                            if (matchRecipe(player.craftGrid) != ri) break;
                            int outI = out.item, outC = out.count;
                            if (player.grabbed.item == 0) {
                                executeRecipe(player.craftGrid, ri);
                                player.grabbed = { outI, outC };
                            } else if (player.grabbed.item == outI && player.grabbed.count + outC <= 64) {
                                executeRecipe(player.craftGrid, ri);
                                player.grabbed.count += outC;
                            } else break;
                        }
                    } else {
                        // 单次合成
                        if (player.grabbed.item == 0 || (player.grabbed.item == out.item && player.grabbed.count + out.count <= 64)) {
                            executeRecipe(player.craftGrid, ri);
                            if (player.grabbed.item == 0) player.grabbed = out;
                            else player.grabbed.count += out.count;
                        }
                    }
                    goto craftDone;
                }
            }

	            // ---- 合成网格格 ----
	            for (int r = 0; r < gs; ++r) {
	                for (int c = 0; c < gs; ++c) {
	                    int x = gx + c * (S + G), y = gy + 8 + r * (S + G);
	                    if (!CheckCollisionPointRec(m, slotR(x, y))) continue;

		                    InventorySlot& cell = player.craftGrid.slots[r][c];
		                    // 合成格也支持右键拖拽
			                    bool gridDrag = dragActive && !rc;
			                    int gridIdx = r * GRID_SIZE + c;
			                    if (gridDrag && gridIdx == dragLastSlot) break;
			                    if (gridDrag && player.grabbed.item > 0 && cell.item == player.grabbed.item && cell.count >= 64) continue;
			                    if (gridDrag && player.grabbed.item > 0 && cell.item > 0 && cell.item != player.grabbed.item) continue;

			                    if (lc) {
			                        // 左键：同类合并 / 不同类交换
			                        if (player.grabbed.item == cell.item && player.grabbed.item > 0
			                            && cell.count < 64) {
			                            int space = 64 - cell.count;
			                            int take = (player.grabbed.count < space) ? player.grabbed.count : space;
			                            cell.count += take;
			                            player.grabbed.count -= take;
			                            if (player.grabbed.count <= 0) player.grabbed = { 0, 0 };
			                        } else {
			                            InventorySlot tmp = cell; cell = player.grabbed; player.grabbed = tmp;
			                        }
			                        gridDrag = false;
		                    } else if (rc || gridDrag) {
		                        if (player.grabbed.item == 0 && cell.item > 0 && cell.count > 0 && !gridDrag) {
		                            // 右手空：取一半（仅单次点击）
		                            int half = (cell.count + 1) / 2;
		                            player.grabbed = { cell.item, half };
		                            cell.count -= half;
		                            if (cell.count <= 0) cell = { 0, 0 };
		                        } else if (player.grabbed.item > 0 && player.grabbed.count > 0) {
		                            if (cell.item == 0 || (cell.item == player.grabbed.item && cell.count < 64)) {
			                                if (cell.item == 0) { cell.item = player.grabbed.item; cell.count = 1; }
			                                else cell.count++;
			                                player.grabbed.count--;
			                                if (player.grabbed.count <= 0) player.grabbed = { 0, 0 };
			                                dragLastSlot = gridIdx;
			                                if (gridDrag) continue; else break;
		                            }
		                        }
		                    }
		            // ---- 右键拖拽标记（跨帧保持）----
			            if (!gridDrag) goto craftDone;
			            } // end inner for
			            } // end outer for
			            // 拖拽模式：没有合成格被处理时，继续到背包栏处理

		    // ---- 背包 + 快捷栏 ----
	            {
	                for (int i = 0; i < TOTAL_SLOTS; ++i) {
	                    bool isHot = i < HOTBAR_SLOTS;
	                    int x = isHot ? (bx + i * (S + G)) : (bx + ((i - HOTBAR_SLOTS) % 6) * (S + G));
	                    int y = isHot ? hby : (by + ((i - HOTBAR_SLOTS) / 6) * (S + G));
	                    if (!CheckCollisionPointRec(m, slotR(x, y))) continue;

	                    if (lc) {
	                        dragActive = false; // 左键打断拖拽
	                        // 左键：同类合并 / 不同类交换
	                        if (player.grabbed.item == player.inventory.slots[i].item && player.grabbed.item > 0
	                            && player.inventory.slots[i].count < 64) {
	                            int space = 64 - player.inventory.slots[i].count;
	                            int take = (player.grabbed.count < space) ? player.grabbed.count : space;
	                            player.inventory.slots[i].count += take;
	                            player.grabbed.count -= take;
	                            if (player.grabbed.count <= 0) player.grabbed = { 0, 0 };
	                        } else {
	                            InventorySlot tmp = player.grabbed;
	                            player.grabbed = player.inventory.slots[i];
	                            player.inventory.slots[i] = tmp;
	                        }
	                        break;
	                    } else if (rc || (dragActive && IsMouseButtonDown(MOUSE_BUTTON_RIGHT))) {
	                        // 右键处理
	                        bool isDrag = dragActive && !rc;

	                        // 拖拽时，如果这个格子上次已经放过，跳过
	                        if (isDrag && i == dragLastSlot) break;

	                        // 手持空：取一半（仅限单次点击）
	                        if (player.grabbed.item == 0 && player.inventory.slots[i].item > 0) {
	                            if (isDrag) break; // 拖拽时不能取东西
	                            int half = (player.inventory.slots[i].count + 1) / 2;
	                            player.grabbed = { player.inventory.slots[i].item, half };
	                            player.inventory.slots[i].count -= half;
	                            if (player.inventory.slots[i].count <= 0) player.inventory.slots[i] = { 0, 0 };
	                            break;
	                        }

	                        // 手持有物品：放到格子（单次点击放1个，拖拽时每个不同格子放1个）
	                        if (player.grabbed.item > 0 && player.grabbed.count > 0) {
	                            InventorySlot& slot = player.inventory.slots[i];
	                            if (slot.item == 0 || (slot.item == player.grabbed.item && slot.count < 64)) {
	                                if (slot.item == 0) { slot.item = player.grabbed.item; slot.count = 1; }
	                                else slot.count++;
	                                player.grabbed.count--;
	                                if (player.grabbed.count <= 0) player.grabbed = { 0, 0 };
	                                dragLastSlot = i; // 记录此格已放过
	                                if (isDrag) continue; // 拖拽：继续检查其他格子
	                                break; // 单次：退出
	                            } else {
	                                // 不同类型或满格：不能放
	                                if (isDrag) continue;
	                                break;
	                            }
	                        }
	                        break;
	                    }
	                } // end for
	            } // close block scope
	
		            craftDone:;
		        } // close else body
	
			drawn:  // 箱子界面跳转到绘制
	
	        // ---- 绘制 ----
	        BeginDrawing();
	        ClearBackground(BLACK);
	        drawSky(gt, player.camera);
	        BeginMode3D(player.camera);
	        worldDraw(world);
	        // 绘制掉落物
	        dropDraw(world.drops, player.camera, world.atlas);
		        if (!showInventory && !showChest && !showFurnace) {
	            RaycastHit hh = playerRaycast(player, world);
	            drawBlockHighlight(hh);
	        }
	        EndMode3D();
	        drawCrosshair();
	        drawHUD(player, world, gt, showDebug, world.atlas);
	        if (showInventory) drawInventoryScreen(player, world, world.atlas);
	        if (showChest) drawChestScreen(player, world, chestGUI, world.atlas);
	        if (showFurnace) drawFurnaceScreen(player, world, furnaceGUI, world.atlas);
	        EndDrawing();
	    } // while
	
	    unloadGameFont();
    worldUnload(world);
    CloseWindow();
    return 0;
}
