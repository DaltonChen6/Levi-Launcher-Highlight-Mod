#include "container/ContainerInspector.h"
#include "container/ChestHandler.h"
#include "container/ContainerCache.h"
#include "container/ContainerModelParser.h"
#include "container/ContainerUtils.h"
#include "container/FurnaceHandler.h"
#include "container/SpecialBlockHandler.h"

namespace highlight {

bool ContainerInspector::notifyContainerGuiOpen(void *cm, const int32_t *lookedPos,
                                                ContainerDisplayData &outData) {
  outData.clear();
  if (!cm || !IsValidPointer(reinterpret_cast<uintptr_t>(cm)) ||
      !IsMemoryReadable(cm, 0x150)) {
    return false;
  }

  // 1. 获取被打开容器的世界坐标 (LevelContainerManagerModel + 0x130 为 mBlockPos)
  int32_t cx = *reinterpret_cast<const int32_t *>((uintptr_t)cm + 0x130);
  int32_t cy = *reinterpret_cast<const int32_t *>((uintptr_t)cm + 0x134);
  int32_t cz = *reinterpret_cast<const int32_t *>((uintptr_t)cm + 0x138);

  int32_t targetPos[3] = {cx, cy, cz};
  bool hasValidPos = (cy >= -64 && cy <= 320 && std::abs(cx) < 30000000 && std::abs(cz) < 30000000);
  if (!hasValidPos && lookedPos && IsMemoryReadable(lookedPos, 12)) {
    targetPos[0] = lookedPos[0];
    targetPos[1] = lookedPos[1];
    targetPos[2] = lookedPos[2];
    hasValidPos = true;
  }

  // 2. 委托专用解析模块遍历 mContainers 并过滤玩家背包
  ParsedContainerResult parsed;
  if (!ContainerModelParser::parse(cm, parsed) || !parsed.foundContainer) {
    return false;
  }

  // 3. 特种熔炉实时数据装配与物理状态同步
  if (parsed.isFurnace) {
    std::string title = parsed.foundTitle.empty() ? "熔炉" : parsed.foundTitle;
    int32_t memLitTime = 0, memLitDuration = 1600, memCookTicks = 0;
    if (IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)cm + 0x13c), 12)) {
      memCookTicks = *reinterpret_cast<const int32_t *>((uintptr_t)cm + 0x13c);
      memLitTime = *reinterpret_cast<const int32_t *>((uintptr_t)cm + 0x140);
      memLitDuration = *reinterpret_cast<const int32_t *>((uintptr_t)cm + 0x144);
    }
    bool isSmokerOrBlast = (title.find("烟熏") != std::string::npos ||
                            title.find("高炉") != std::string::npos);
    int32_t maxCook = isSmokerOrBlast ? 100 : 200;

    FurnaceSimState sim;
    sim.inputRawId = parsed.inRaw; sim.inputName = parsed.inName; sim.inputCount = parsed.inCount;
    sim.fuelRawId = parsed.fuelRaw; sim.fuelName = parsed.fuelName; sim.fuelCount = parsed.fuelCount;
    sim.resultRawId = parsed.resRaw; sim.resultName = parsed.resName; sim.resultCount = parsed.resCount;
    sim.litTime = (memLitTime > 0) ? memLitTime : 0;
    sim.litDuration = (memLitDuration > 0) ? memLitDuration : 1600;
    sim.cookTicks = (memCookTicks > 0) ? memCookTicks : 0;
    sim.maxCookTime = maxCook;
    sim.lastUpdate = std::chrono::steady_clock::now();
    sim.isInitialized = true;

    if (hasValidPos) {
      int64_t posKey = PosToKey(targetPos[0], targetPos[1], targetPos[2]);
      ContainerCache::get().setFurnace(posKey, sim);
    }

    outData.hasContainer = true;
    outData.specialType = SpecialContainerType::Furnace;
    outData.containerTitle = title;
    outData.totalSlots = 3;
    outData.filledSlots = (parsed.inCount > 0 ? 1 : 0) + (parsed.fuelCount > 0 ? 1 : 0) + (parsed.resCount > 0 ? 1 : 0);
    outData.isBurning = (sim.litTime > 0);
    outData.isCooking = (sim.litTime > 0 && sim.inputCount > 0);
    outData.furnaceInput = sim.inputName;
    outData.furnaceInputCount = sim.inputCount;
    outData.furnaceFuel = sim.fuelName;
    outData.furnaceFuelCount = sim.fuelCount;
    outData.furnaceResult = sim.resultName;
    outData.furnaceResultCount = sim.resultCount;
    outData.cookProgress = (sim.maxCookTime > 0) ? std::min(1.0f, static_cast<float>(sim.cookTicks) / static_cast<float>(sim.maxCookTime)) : 0.0f;
    outData.cookSecondsLeft = (sim.maxCookTime > sim.cookTicks) ? static_cast<float>(sim.maxCookTime - sim.cookTicks) / 20.0f : 0.0f;
    outData.fuelProgress = (sim.litDuration > 0) ? std::min(1.0f, static_cast<float>(sim.litTime) / static_cast<float>(sim.litDuration)) : 0.0f;
    outData.fuelSecondsLeft = static_cast<float>(sim.litTime) / 20.0f;
    outData.isUnsynced = false;
    return true;
  }

  // 4. 标准容器 (箱子、木桶、潜影盒、漏斗) 数据装配
  std::string title = parsed.foundTitle;
  if (title.empty()) {
    title = (parsed.totalSlots > 27) ? "大箱子" : "箱子";
  }

  outData.hasContainer = true;
  outData.containerTitle = title;
  outData.totalSlots = parsed.totalSlots;
  outData.filledSlots = parsed.filledSlots;
  outData.mergedItems = parsed.mergedItems;
  outData.isUnsynced = false;

  // 单坐标精准写入 ContainerCache 供关闭后高亮保持
  if (hasValidPos) {
    int64_t posKey = PosToKey(targetPos[0], targetPos[1], targetPos[2]);
    ContainerCacheEntry entry;
    entry.lastUpdated = std::chrono::steady_clock::now();
    entry.totalSlots = parsed.totalSlots;
    entry.filledSlots = parsed.filledSlots;
    entry.mergedItems = parsed.mergedItems;
    entry.isSynced = true;
    ContainerCache::get().setContainer(posKey, entry);
  }

  return true;
}

bool ContainerInspector::notifyContainerGuiOpen(void *cm, const int32_t *pos) {
  ContainerDisplayData dummy;
  return notifyContainerGuiOpen(cm, pos, dummy);
}

bool ContainerInspector::inspect(void *region, const int32_t *blockPos,
                                 const std::string &typeName, uint16_t blockData,
                                 ContainerDisplayData &outData) {
  outData.clear();
  if (!region || !blockPos || !IsValidPointer(reinterpret_cast<uintptr_t>(region))) {
    return false;
  }

  int64_t posKey = PosToKey(blockPos[0], blockPos[1], blockPos[2]);

  uintptr_t *bsVt = *reinterpret_cast<uintptr_t **>(region);
  if (!bsVt || !IsValidPointer(reinterpret_cast<uintptr_t>(bsVt)) || !bsVt[4]) {
    return false;
  }

  // bsVt[4] = BlockSource::getBlockEntity(const BlockPos&)
  auto getBlockEntity = reinterpret_cast<void *(*)(void *, const int32_t *)>(bsVt[4]);
  void *blockActor = getBlockEntity(region, blockPos);
  if (!blockActor || !IsValidPointer(reinterpret_cast<uintptr_t>(blockActor))) {
    return false;
  }

  // 1. 熔炉 / 高炉 / 烟熏炉 (物理级平滑实时时间模拟与槽位直读)
  if (FurnaceHandler::inspect(blockActor, typeName, posKey, outData)) {
    return true;
  }

  // 2. 营火 / 灵魂营火 (4 槽位烤肉及秒数倒计时)
  if (SpecialBlockHandler::inspectCampfire(blockActor, typeName, outData)) {
    return true;
  }

  // 3. 蜂箱 / 蜂巢 (蜜蜂数量与蜂蜜等级)
  if (SpecialBlockHandler::inspectBeehive(blockActor, blockData, typeName, outData)) {
    return true;
  }

  // 4. 标准容器 (箱子/大箱子、木桶、潜影盒、漏斗、投掷器、发射器)
  if (ChestHandler::inspect(region, blockActor, blockPos, typeName, posKey, outData)) {
    return true;
  }

  return false;
}

} // namespace highlight
