#include "discover/Discovery.h"
#include "block/MiningCalculator.h"
#include "container/ContainerCache.h"
#include "container/ContainerInspector.h"
#include "container/ContainerUtils.h"
#include "discover/BlockTranslator.h"
#include "discover/LocalizationService.h"
#include "discover/ToolStatusEvaluator.h"
#include "entity/EntityInspector.h"
#include "render/HudRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace highlight {
namespace {

inline void ClearHud() {
  SetHudText("");
  SetHudContainerData(ContainerDisplayData{});
  SetHudEntityData(EntityDisplayData{});
}

static std::string resolveBlockTypeName(uintptr_t legacy, const std::string &descId) {
  std::string fullName = ReadLibcppString(reinterpret_cast<const void *>(legacy + 0xe0));
  std::string rawName = ReadLibcppString(reinterpret_cast<const void *>(legacy + 0x90));
  std::string typeName;
  if (fullName.rfind("minecraft:", 0) == 0 && fullName.size() > 10) {
    typeName = fullName;
  } else if (!descId.empty()) {
    std::string clean = (descId.rfind("tile.", 0) == 0) ? descId.substr(5) : descId;
    if (clean.size() > 5 && clean.substr(clean.size() - 5) == ".name") clean.erase(clean.size() - 5);
    typeName = "minecraft:" + clean;
  } else if (!rawName.empty() && rawName != "minecraft" && rawName.rfind("tile.", 0) != 0) {
    typeName = "minecraft:" + rawName;
  }
  if (typeName.size() > 8) {
    if (typeName.compare(typeName.size() - 8, 8, " default") == 0 ||
        typeName.compare(typeName.size() - 8, 8, ".default") == 0) {
      typeName.erase(typeName.size() - 8);
    }
  }
  return typeName;
}

static void syncDestroyProgress(void *gm, const int32_t *pos, std::atomic<bool> &isDest,
                                std::atomic<int32_t> &px, std::atomic<int32_t> &py, std::atomic<int32_t> &pz,
                                std::atomic<float> &prog, bool isStart) {
  if (!gm || !pos || !IsMemoryReadable(pos, 12)) return;
  isDest.store(true, std::memory_order_relaxed);
  px.store(pos[0], std::memory_order_relaxed);
  py.store(pos[1], std::memory_order_relaxed);
  pz.store(pos[2], std::memory_order_relaxed);
  if (IsValidPointer(reinterpret_cast<uintptr_t>(gm)) &&
      IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)gm + 0x24), 4)) {
    float val = *reinterpret_cast<const float *>((uintptr_t)gm + 0x24);
    if (val >= 0.0f && val <= 1.0f) {
      prog.store(val, std::memory_order_relaxed);
      return;
    }
  }
  if (isStart) prog.store(0.01f, std::memory_order_relaxed);
}

} // namespace

void Discovery::onClientUpdate(void *ci) {
  if (!ci || !IsValidPointer(reinterpret_cast<uintptr_t>(ci))) { ClearHud(); return; }
  uintptr_t *ciVt = *reinterpret_cast<uintptr_t **>(ci);
  if (!ciVt || !IsValidPointer(reinterpret_cast<uintptr_t>(ciVt))) { ClearHud(); return; }

  // 0. 获取 LocalPlayer 并检测背包/容器界面是否打开 (ciVt[32] = getLocalPlayer)
  auto getPlayer = reinterpret_cast<void *(*)(void *)>(ciVt[32]);
  void *player = getPlayer ? getPlayer(ci) : nullptr;

  // 逆向深度验证: Player + 0x558 是 std::shared_ptr<ContainerManagerModel> mContainerManager
  // 当玩家打开容器 GUI 时，底层实时抓取容器物品并同步显示于屏幕顶端；若为玩家自身背包则彻底清空隐藏
  if (player && IsValidPointer(reinterpret_cast<uintptr_t>(player)) &&
      IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)player + 0x558), 8)) {
    uintptr_t containerManager = *reinterpret_cast<uintptr_t *>((uintptr_t)player + 0x558);
    if (containerManager != 0) {
      // 当玩家打开任何 GUI（自身背包、箱子、熔炉、合成台等）时：
      // 1. 若打开的是世界方块容器，静默提取物品同步到本地缓存，供关闭后高亮保持；
      // 2. 界面打开期间彻底清空并隐藏 HUD（严格对齐 WAILA PauseManager 规范，绝不遮挡游戏 GUI）；
      ContainerInspector::notifyContainerGuiOpen(reinterpret_cast<void *>(containerManager),
                                                 mHasLookedPos ? mLastLookedPos : nullptr);
      ClearHud();
      return;
    }
  }

  // 1. ci->slot(31) (offset 0xf8): BlockSource*
  auto getRegion = reinterpret_cast<void *(*)(void *)>(ciVt[31]);
  if (!getRegion) { ClearHud(); return; }
  void *region = getRegion(ci);
  if (!region || !IsValidPointer(reinterpret_cast<uintptr_t>(region))) { ClearHud(); return; }

  // 2. region->slot(43) (offset 0x158): Level*
  uintptr_t *bsVt = *reinterpret_cast<uintptr_t **>(region);
  if (!bsVt || !IsValidPointer(reinterpret_cast<uintptr_t>(bsVt))) { ClearHud(); return; }
  auto getLevel = reinterpret_cast<void *(*)(void *)>(bsVt[43]);
  if (!getLevel) { ClearHud(); return; }
  void *level = getLevel(region);
  if (!level || !IsValidPointer(reinterpret_cast<uintptr_t>(level))) { ClearHud(); return; }

  // 世界/维度变更检测：防止跨世界或死亡重生物品缓存污染
  static void *sLastLevel = nullptr;
  if (level != sLastLevel) {
    sLastLevel = level;
    ContainerCache::get().clear();
  }

  // 3. Level + 0x1d0: HitResult*
  if (!IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)level + 0x1d0), 8)) { ClearHud(); return; }
  uintptr_t hitPtr = *reinterpret_cast<uintptr_t *>((uintptr_t)level + 0x1d0);
  if (!hitPtr || !IsValidPointer(hitPtr) || !IsMemoryReadable(reinterpret_cast<const void *>(hitPtr), 0x38)) {
    ClearHud(); return;
  }

  // 4. HitResult::mType at offset +0x18 (0 = Tile/Block, 1 = Entity). 对准空气时立即清空 HUD
  int32_t hitType = *reinterpret_cast<int32_t *>(hitPtr + 0x18);
  if (hitType != 0 && hitType != 1) { ClearHud(); return; }

  // 模块化解耦：实体透视独立由 EntityInspector 处理
  if (hitType == 1) {
    SetHudText("");
    SetHudContainerData(ContainerDisplayData{});
    EntityDisplayData entityData;
    if (EntityInspector::inspect(hitPtr, entityData)) {
      SetHudEntityData(entityData);
    } else {
      SetHudEntityData(EntityDisplayData{});
    }
    return;
  }

  // 准星指向方块，清空实体透视数据
  SetHudEntityData(EntityDisplayData{});

  // 5. 方块世界坐标 (hit + 0x20, 0x24, 0x28)
  int32_t blockPos[3] = {
      *reinterpret_cast<int32_t *>(hitPtr + 0x20),
      *reinterpret_cast<int32_t *>(hitPtr + 0x24),
      *reinterpret_cast<int32_t *>(hitPtr + 0x28)
  };
  mLastLookedPos[0] = blockPos[0];
  mLastLookedPos[1] = blockPos[1];
  mLastLookedPos[2] = blockPos[2];
  mHasLookedPos = true;

  // 6. BlockSource::getBlock(blockPos) via slot 2 (offset 0x10)
  auto getBlock = reinterpret_cast<void *(*)(void *, const int32_t *)>(bsVt[2]);
  if (!getBlock) { ClearHud(); return; }
  void *block = getBlock(region, blockPos);
  if (!block || !IsValidPointer(reinterpret_cast<uintptr_t>(block))) { ClearHud(); return; }

  // 7. BlockLegacy at block + 0x68
  if (!IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)block + 0x68), 8)) { ClearHud(); return; }
  uintptr_t legacy = *reinterpret_cast<uintptr_t *>((uintptr_t)block + 0x68);
  if (!legacy || !IsValidPointer(legacy)) { ClearHud(); return; }

  // 8. 读取方块标准名称与描述 ID
  std::string descId = ReadLibcppString(reinterpret_cast<const void *>(legacy + 0x8));
  std::string typeName = resolveBlockTypeName(legacy, descId);

  if (typeName.empty() || typeName == "minecraft" || typeName == "minecraft:" ||
      typeName == "minecraft:air" || typeName == "tile.air.name" || typeName == "tile.air") {
    ClearHud();
    return;
  }

  // 9. 距离计算
  float sx = *reinterpret_cast<float *>(hitPtr + 0x00);
  float sy = *reinterpret_cast<float *>(hitPtr + 0x04);
  float sz = *reinterpret_cast<float *>(hitPtr + 0x08);
  float hx = *reinterpret_cast<float *>(hitPtr + 0x2c);
  float hy = *reinterpret_cast<float *>(hitPtr + 0x30);
  float hz = *reinterpret_cast<float *>(hitPtr + 0x34);
  float dx = hx - sx, dy = hy - sy, dz = hz - sz;
  float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (dist < 0.01f) {
    float fbx = static_cast<float>(blockPos[0]) + 0.5f - sx;
    float fby = static_cast<float>(blockPos[1]) + 0.5f - sy;
    float fbz = static_cast<float>(blockPos[2]) + 0.5f - sz;
    dist = std::sqrt(fbx * fbx + fby * fby + fbz * fbz);
  }

  // 10. 模块化解耦：手持工具与适宜度判定
  ToolEvaluationResult toolRes = ToolStatusEvaluator::evaluate(player, block, legacy, typeName);

  // 11. 挖掘进度
  float miningProg = 0.0f;
  if (mIsDestroying.load(std::memory_order_relaxed)) {
    if (blockPos[0] == mDestroyPosX.load(std::memory_order_relaxed) &&
        blockPos[1] == mDestroyPosY.load(std::memory_order_relaxed) &&
        blockPos[2] == mDestroyPosZ.load(std::memory_order_relaxed)) {
      miningProg = mMiningProgress.load(std::memory_order_relaxed);
    }
  }

  // 12. 格式化 HUD 卡片文本
  std::string localizedName = LocalizationService::getTranslation(descId, typeName);
  if (localizedName.empty()) {
    localizedName = BlockTranslator::toChinese(typeName);
  }
  char buf[1024];
  int n = std::snprintf(buf, sizeof(buf),
                        "【%s】\nID: %s\n距离: %.1f米 | 坐标: (%d, %d, %d)\n手持: %s | %s",
                        localizedName.c_str(), typeName.c_str(), dist,
                        blockPos[0], blockPos[1], blockPos[2],
                        toolRes.heldItemName.c_str(), toolRes.toolStatus.c_str());

  // 物理挖掘公式计算耗时
  float predSec = MiningCalculator::calculateMiningTime(typeName, toolRes.heldItemName, toolRes.canHarvest, toolRes.requiresTool, false, false);
  if (predSec > 0.0f && n > 0 && static_cast<size_t>(n) < sizeof(buf)) {
    n += std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n),
                       predSec <= 0.051f ? " | 预计: 瞬破 √" : " | 预计: %.2f秒", predSec);
  }

  if (n > 0 && static_cast<size_t>(n) < sizeof(buf) && miningProg > 0.001f) {
    if (predSec > 0.051f) {
      float remainSec = (1.0f - miningProg) * predSec;
      std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "\n挖掘进度: %.1f%% (剩余 %.1f秒)",
                    miningProg * 100.0f, remainSec);
    } else {
      std::snprintf(buf + n, sizeof(buf) - static_cast<size_t>(n), "\n挖掘进度: %.1f%%", miningProg * 100.0f);
    }
  }

  // 13. 容器与特种方块全景透视探测
  uint16_t blockData = 0;
  if (IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)block + 0x10), 2)) {
    blockData = *reinterpret_cast<const uint16_t *>((uintptr_t)block + 0x10);
  }

  ContainerDisplayData containerData;
  ContainerInspector::inspect(region, blockPos, typeName, blockData, containerData);
  SetHudContainerData(containerData);

  SetHudText(std::string(buf));
}

void Discovery::onStartDestroyBlock(void *gm, const int32_t *pos, uint8_t face) {
  syncDestroyProgress(gm, pos, mIsDestroying, mDestroyPosX, mDestroyPosY, mDestroyPosZ, mMiningProgress, true);
}

void Discovery::onContinueDestroyBlock(void *gm, const int32_t *pos, uint8_t face, const float *playerPos) {
  syncDestroyProgress(gm, pos, mIsDestroying, mDestroyPosX, mDestroyPosY, mDestroyPosZ, mMiningProgress, false);
}

void Discovery::onStopDestroyBlock(void *gm, const int32_t *pos) {
  mIsDestroying.store(false, std::memory_order_relaxed);
  mMiningProgress.store(0.0f, std::memory_order_relaxed);
}

} // namespace highlight
