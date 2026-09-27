#include "container/ChestHandler.h"
#include "container/ContainerCache.h"
#include "container/ContainerUtils.h"

#include <chrono>

namespace highlight {

bool ChestHandler::inspect(void *region, void *blockActor, const int32_t *blockPos,
                           const std::string &typeName, int64_t posKey,
                           ContainerDisplayData &outData) {
  bool isHopper = (typeName.find("hopper") != std::string::npos);
  bool isChest = (typeName.find("chest") != std::string::npos && typeName.find("ender_chest") == std::string::npos);
  bool isBarrel = (typeName.find("barrel") != std::string::npos);
  bool isShulker = (typeName.find("shulker") != std::string::npos);
  bool isDispenserDropper = (typeName.find("dispenser") != std::string::npos ||
                             typeName.find("dropper") != std::string::npos);

  if (!isChest && !isBarrel && !isShulker && !isHopper && !isDispenserDropper) {
    return false;
  }

  // 1. 获取容器槽位总数 (totalSlots) 与大箱子配对坐标
  // 基岩版原生物理真理：
  // ChestBlockActor + 0x278 为 mLargeChestPaired 指针 (非空且合法则为双箱 54 格)
  // ChestBlockActor + 0x280 为 mLargeChestPairedPosition (int32_t px, py, pz)
  int totalSlots = 27;
  bool isDoubleChest = false;
  int32_t pairedPos[3] = {0, 0, 0};
  bool hasPairedPos = false;

  if (isHopper) {
    totalSlots = 5;
  } else if (isDispenserDropper) {
    totalSlots = 9;
  } else if (blockActor && IsValidPointer(reinterpret_cast<uintptr_t>(blockActor))) {
    if (isChest && IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)blockActor + 0x278), 24)) {
      uintptr_t pairedPtr = *reinterpret_cast<const uintptr_t *>((uintptr_t)blockActor + 0x278);
      if (pairedPtr != 0 && IsValidPointer(pairedPtr) &&
          IsMemoryReadable(reinterpret_cast<const void *>(pairedPtr), 0x20)) {
        isDoubleChest = true;
        totalSlots = 54;
        pairedPos[0] = *reinterpret_cast<const int32_t *>((uintptr_t)blockActor + 0x280);
        pairedPos[1] = *reinterpret_cast<const int32_t *>((uintptr_t)blockActor + 0x284);
        pairedPos[2] = *reinterpret_cast<const int32_t *>((uintptr_t)blockActor + 0x288);
        if (pairedPos[1] >= -64 && pairedPos[1] <= 320 &&
            std::abs(pairedPos[0]) < 30000000 && std::abs(pairedPos[2]) < 30000000) {
          hasPairedPos = true;
        }
      }
    }

    if (!isDoubleChest) {
      uintptr_t *vt = *reinterpret_cast<uintptr_t **>(blockActor);
      if (vt && IsValidPointer(reinterpret_cast<uintptr_t>(vt)) && vt[75] && IsValidPointer(vt[75])) {
        auto getSize = reinterpret_cast<int (*)(void *)>(vt[75]);
        int sz = getSize(blockActor);
        if (sz >= 1 && sz <= 108) {
          totalSlots = sz;
          if (sz > 27) isDoubleChest = true;
        }
      }
    }
  }

  auto getTitle = [&](int slots) -> std::string {
    if (isChest) return (slots > 27) ? "大箱子" : "箱子";
    if (isBarrel) return "木桶";
    if (isShulker) return "潜影盒";
    if (isHopper) return "漏斗";
    if (isDispenserDropper) return (typeName.find("dispenser") != std::string::npos) ? "发射器" : "投掷器";
    return (slots > 27) ? "大箱子" : "箱子";
  };

  // 2. 优先读取已缓存的同步数据（经由玩家打开 GUI 实时同步过）
  ContainerCacheEntry cachedEntry;
  bool hasCache = ContainerCache::get().getContainer(posKey, cachedEntry);
  if (!hasCache && isDoubleChest && hasPairedPos) {
    // 精确双箱对侧坐标检索：直接查询配对箱坐标，零误判、零污染邻近独立箱子
    int64_t pairedKey = PosToKey(pairedPos[0], pairedPos[1], pairedPos[2]);
    if (ContainerCache::get().getContainer(pairedKey, cachedEntry) && cachedEntry.totalSlots > 27) {
      hasCache = true;
      // 将配对缓存精准赋予当前这半边箱子，后续查询 O(1) 直达
      ContainerCache::get().setContainer(posKey, cachedEntry);
    }
  }

  if (hasCache) {
    outData.hasContainer = true;
    outData.totalSlots = cachedEntry.totalSlots > 0 ? cachedEntry.totalSlots : totalSlots;
    outData.filledSlots = cachedEntry.filledSlots;
    outData.mergedItems = cachedEntry.mergedItems;
    outData.isUnsynced = !cachedEntry.isSynced;
    outData.containerTitle = getTitle(outData.totalSlots);
    return true;
  }

  // 3. 无缓存时，尝试直接读取 BlockActor（对本地放置的潜影盒等带 NBT 方块有效）
  if (blockActor && IsValidPointer(reinterpret_cast<uintptr_t>(blockActor))) {
    uintptr_t *vt = *reinterpret_cast<uintptr_t **>(blockActor);
    if (vt && IsValidPointer(reinterpret_cast<uintptr_t>(vt))) {
      auto readItemFromSlot = [&](int slot) -> void * {
        if (isChest || isBarrel || isShulker) {
          if (vt[77] && IsValidPointer(vt[77])) {
            auto getItem = reinterpret_cast<void *(*)(void *, int)>(vt[77]);
            return getItem(blockActor, slot);
          }
        } else if (isHopper) {
          return reinterpret_cast<void *>((uintptr_t)blockActor + 0x218 + slot * 152);
        } else if (isDispenserDropper) {
          return reinterpret_cast<void *>((uintptr_t)blockActor + 0x228 + slot * 152);
        }
        return nullptr;
      };

      int filledSlots = 0;
      std::vector<ContainerSlot> mergedItems;
      for (int slot = 0; slot < totalSlots; slot++) {
        void *itemStack = readItemFromSlot(slot);
        if (!itemStack || !IsValidPointer(reinterpret_cast<uintptr_t>(itemStack))) continue;
        if (!IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)itemStack + 0x20), 4)) continue;
        uint8_t count = *reinterpret_cast<const uint8_t *>((uintptr_t)itemStack + 0x22);
        if (count == 0) continue;

        void *item = GetItemFromStack(itemStack);
        if (!item || !IsValidPointer(reinterpret_cast<uintptr_t>(item))) continue;

        uint16_t aux = *reinterpret_cast<const uint16_t *>((uintptr_t)itemStack + 0x20);
        std::string rawId = GetItemName(item);
        if (rawId.empty()) continue;

        std::string name = TranslateItemSafely(rawId);
        if (name.empty()) name = rawId;

        filledSlots++;

        bool merged = false;
        for (auto &slotItem : mergedItems) {
          if (slotItem.rawId == rawId && slotItem.aux == aux) {
            slotItem.count += count;
            merged = true;
            break;
          }
        }
        if (!merged) {
          mergedItems.push_back(ContainerSlot{name, rawId, aux, count});
        }
      }

      if (filledSlots > 0) {
        // 本地确实读到了有效物品，更新单坐标缓存并展示
        outData.hasContainer = true;
        outData.totalSlots = totalSlots;
        outData.filledSlots = filledSlots;
        outData.mergedItems = mergedItems;
        outData.isUnsynced = false;
        outData.containerTitle = getTitle(totalSlots);

        ContainerCacheEntry entry;
        entry.lastUpdated = std::chrono::steady_clock::now();
        entry.totalSlots = totalSlots;
        entry.filledSlots = filledSlots;
        entry.mergedItems = mergedItems;
        entry.isSynced = true;
        ContainerCache::get().setContainer(posKey, entry);
        return true;
      }
    }
  }

  // 4. 既无缓存、BlockActor 本地也未记录（基岩版瘦客户端未开箱常态）：
  // 诚实展示容器标题与格数，标记为未开启同步，坚决不向缓存写入空数据覆盖破坏！
  outData.hasContainer = true;
  outData.totalSlots = totalSlots;
  outData.filledSlots = 0;
  outData.mergedItems.clear();
  outData.isUnsynced = true;
  outData.containerTitle = getTitle(totalSlots);

  return true;
}

} // namespace highlight
