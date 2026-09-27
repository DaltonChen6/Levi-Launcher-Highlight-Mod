#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace highlight {

struct ContainerSlot {
  std::string itemName;  // 中文翻译名称，如 "钻石"
  std::string rawId;     // 原生 ID，如 "minecraft:diamond"
  uint16_t aux = 0;      // 附加值/变种
  uint32_t count = 0;    // 堆叠数量（同类合并后总数）
};

struct CampfireSlotInfo {
  std::string itemName;
  float remainingSeconds = 0.0f;
  bool active = false;
};

enum class SpecialContainerType {
  None,
  Furnace,
  Campfire,
  Beehive
};

struct ContainerDisplayData {
  bool hasContainer = false;
  bool isUnsynced = false;    // 是否为未开启同步的容器（尚未对准打开记录过）
  std::string containerTitle; // 如 "大箱子", "木桶", "潜影盒"
  int totalSlots = 0;         // 总格子数 (如 54, 27, 9, 5)
  int filledSlots = 0;        // 有物品的格子数
  std::vector<ContainerSlot> mergedItems; // 合并同类后的物品列表

  SpecialContainerType specialType = SpecialContainerType::None;

  // 熔炉/烟熏炉/高炉专用数据
  float cookProgress = 0.0f;    // 0.0f ~ 1.0f
  float cookSecondsLeft = 0.0f; // 烧炼剩余秒数
  float fuelProgress = 0.0f;    // 0.0f ~ 1.0f
  float fuelSecondsLeft = 0.0f; // 燃料剩余燃烧秒数
  bool isCooking = false;
  bool isBurning = false;
  std::string furnaceInput;
  uint32_t furnaceInputCount = 0;
  std::string furnaceFuel;
  uint32_t furnaceFuelCount = 0;
  std::string furnaceResult;
  uint32_t furnaceResultCount = 0;

  // 营火专用数据 (4 个独立槽位)
  CampfireSlotInfo campfireSlots[4];

  // 蜂箱/蜂巢专用数据
  int beeOccupants = 0; // 蜜蜂数 (0~3)
  int honeyLevel = 0;   // 蜂蜜等级 (0~5)

  void clear() {
    hasContainer = false;
    isUnsynced = false;
    containerTitle.clear();
    totalSlots = 0;
    filledSlots = 0;
    mergedItems.clear();
    specialType = SpecialContainerType::None;
    cookProgress = 0.0f;
    cookSecondsLeft = 0.0f;
    fuelProgress = 0.0f;
    fuelSecondsLeft = 0.0f;
    isCooking = false;
    isBurning = false;
    furnaceInput.clear();
    furnaceInputCount = 0;
    furnaceFuel.clear();
    furnaceFuelCount = 0;
    furnaceResult.clear();
    furnaceResultCount = 0;
    for (int i = 0; i < 4; i++) {
      campfireSlots[i] = CampfireSlotInfo{};
    }
    beeOccupants = 0;
    honeyLevel = 0;
  }
};

struct FurnaceSimState {
  int32_t litTime = 0;
  int32_t litDuration = 1600;
  int32_t cookTicks = 0;
  int32_t maxCookTime = 200;
  std::chrono::steady_clock::time_point lastUpdate;
  std::string inputName;
  std::string inputRawId;
  uint32_t inputCount = 0;
  std::string fuelName;
  std::string fuelRawId;
  uint32_t fuelCount = 0;
  std::string resultName;
  std::string resultRawId;
  uint32_t resultCount = 0;
  bool isInitialized = false;
};

struct ContainerCacheEntry {
  std::chrono::steady_clock::time_point lastUpdated;
  int totalSlots = 0;
  int filledSlots = 0;
  std::vector<ContainerSlot> mergedItems;
  bool isSynced = false; // 是否经由玩家打开 GUI 真实物理同步过
};

} // namespace highlight
