#include "container/FurnaceHandler.h"
#include "container/ContainerCache.h"
#include "container/ContainerUtils.h"

#include <algorithm>
#include <chrono>

namespace highlight {

int32_t FurnaceHandler::getFuelBurnDuration(const std::string &rawId, int32_t fallback) {
  if (rawId.empty()) return fallback;
  if (rawId.find("lava_bucket") != std::string::npos) return 20000;
  if (rawId.find("coal_block") != std::string::npos) return 16000;
  if (rawId.find("dried_kelp_block") != std::string::npos) return 4000;
  if (rawId.find("blaze_rod") != std::string::npos) return 2400;
  if (rawId.find("coal") != std::string::npos || rawId.find("charcoal") != std::string::npos) return 1600;
  if (rawId.find("log") != std::string::npos || rawId.find("wood") != std::string::npos ||
      rawId.find("planks") != std::string::npos) return 300;
  if (rawId.find("fence") != std::string::npos || rawId.find("gate") != std::string::npos ||
      rawId.find("stairs") != std::string::npos || rawId.find("slab") != std::string::npos ||
      rawId.find("trapdoor") != std::string::npos || rawId.find("door") != std::string::npos) return 300;
  if (rawId.find("stick") != std::string::npos || rawId.find("sapling") != std::string::npos) return 100;
  return fallback;
}

bool FurnaceHandler::inspect(void *blockActor, const std::string &typeName, int64_t posKey,
                             ContainerDisplayData &outData) {
  if (typeName.find("furnace") == std::string::npos && typeName.find("smoker") == std::string::npos) {
    return false;
  }

  if (!blockActor || !IsValidPointer(reinterpret_cast<uintptr_t>(blockActor)) ||
      !IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)blockActor + 0x204), 16)) {
    return false;
  }

  outData.specialType = SpecialContainerType::Furnace;
  outData.hasContainer = true;
  bool isSmokerOrBlast = (typeName.find("smoker") != std::string::npos ||
                          typeName.find("blast") != std::string::npos);
  if (typeName.find("smoker") != std::string::npos) {
    outData.containerTitle = "烟熏炉";
  } else if (typeName.find("blast") != std::string::npos) {
    outData.containerTitle = "高炉";
  } else {
    outData.containerTitle = "熔炉";
  }
  bool isBlockLit = (typeName.find("lit_") != std::string::npos);
  int32_t defaultMaxCookTime = isSmokerOrBlast ? 100 : 200;

  // 严格对齐基岩版 FurnaceBlockActor 逆向内存真理：
  // 0x204: int32_t mLitTime (燃烧剩余刻)
  // 0x208: int32_t mLitDuration (燃料总刻数)
  // 0x20c: int32_t mCookTime (当前烧炼进度刻)
  // 0x210: int32_t mMaxCookTime (单件烧炼所需总刻数)
  int32_t memLitTime = *reinterpret_cast<const int32_t *>((uintptr_t)blockActor + 0x204);
  int32_t memLitDuration = *reinterpret_cast<const int32_t *>((uintptr_t)blockActor + 0x208);
  int32_t memCookTicks = *reinterpret_cast<const int32_t *>((uintptr_t)blockActor + 0x20c);
  int32_t memMaxCook = *reinterpret_cast<const int32_t *>((uintptr_t)blockActor + 0x210);
  int32_t maxCookTime = (memMaxCook > 0 && memMaxCook <= 1000) ? memMaxCook : defaultMaxCookTime;

  void *inStack = reinterpret_cast<void *>((uintptr_t)blockActor + 0x218);
  void *fuelStack = reinterpret_cast<void *>((uintptr_t)blockActor + 0x2b0);
  void *outStack = reinterpret_cast<void *>((uintptr_t)blockActor + 0x348);

  // 安全槽位读取器：仅当读到实际物品且 count > 0 时才判定为成功，绝不把空内存判定为有效读出
  auto readSlot = [](void *stack, std::string &outRaw, std::string &outName, uint32_t &outCount) -> bool {
    outRaw.clear(); outName.clear(); outCount = 0;
    if (!stack || !IsValidPointer(reinterpret_cast<uintptr_t>(stack))) return false;
    if (!IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)stack + 0x20), 4)) return false;
    uint8_t count = *reinterpret_cast<const uint8_t *>((uintptr_t)stack + 0x22);
    if (count == 0) return false; // 槽位为空或瘦客户端未同步，返回 false 保护已缓存物品
    void *item = GetItemFromStack(stack);
    if (!item || !IsValidPointer(reinterpret_cast<uintptr_t>(item))) return false;
    outRaw = GetItemName(item);
    if (outRaw.empty()) return false;
    outName = TranslateItemSafely(outRaw);
    if (outName.empty()) outName = outRaw;
    outCount = count;
    return true;
  };

  FurnaceSimState sim;
  bool cached = ContainerCache::get().getFurnace(posKey, sim);

  if (!cached || !sim.isInitialized) {
    sim.litTime = (isBlockLit && memLitTime > 0) ? memLitTime : 0;
    sim.litDuration = (memLitDuration > 0) ? memLitDuration : 1600;
    sim.cookTicks = (memCookTicks > 0) ? memCookTicks : 0;
    sim.maxCookTime = maxCookTime;

    readSlot(inStack, sim.inputRawId, sim.inputName, sim.inputCount);
    readSlot(fuelStack, sim.fuelRawId, sim.fuelName, sim.fuelCount);
    readSlot(outStack, sim.resultRawId, sim.resultName, sim.resultCount);

    sim.lastUpdate = std::chrono::steady_clock::now();
    sim.isInitialized = true;
  } else {
    // 内存物品直读同步：仅当瘦客户端内存真正读到有效非空物品时才覆盖，坚决防止关闭 GUI 后空内存刷空已缓存物品
    std::string newInRaw, newInName; uint32_t newInCount = 0;
    if (readSlot(inStack, newInRaw, newInName, newInCount) && newInCount > 0) {
      sim.inputRawId = newInRaw;
      sim.inputName = newInName;
      sim.inputCount = newInCount;
    }

    std::string newFuelRaw, newFuelName; uint32_t newFuelCount = 0;
    if (readSlot(fuelStack, newFuelRaw, newFuelName, newFuelCount) && newFuelCount > 0) {
      sim.fuelRawId = newFuelRaw;
      sim.fuelName = newFuelName;
      sim.fuelCount = newFuelCount;
    }

    std::string newOutRaw, newOutName; uint32_t newOutCount = 0;
    if (readSlot(outStack, newOutRaw, newOutName, newOutCount) && newOutCount > 0) {
      sim.resultRawId = newOutRaw;
      sim.resultName = newOutName;
      sim.resultCount = newOutCount;
    }

    // 状态数据校准：熄灭状态强制清空 litTime，防止残留值触发假燃烧
    if (!isBlockLit) {
      sim.litTime = 0;
    } else if (memLitTime > 0) {
      sim.litTime = memLitTime;
      if (memLitDuration > 0) sim.litDuration = memLitDuration;
    }
    if (memCookTicks > 0) {
      sim.cookTicks = memCookTicks;
    }

    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - sim.lastUpdate).count();
    sim.lastUpdate = now;

    int deltaTicks = static_cast<int>(dt * 20.0f);
    if (deltaTicks > 0) {
      if (sim.litTime > 0) {
        sim.litTime -= deltaTicks;
        if (sim.litTime < 0) sim.litTime = 0;

        if (sim.inputCount > 0) {
          sim.cookTicks += deltaTicks;
          while (sim.cookTicks >= sim.maxCookTime && sim.inputCount > 0) {
            sim.cookTicks -= sim.maxCookTime;
            sim.inputCount--;
            sim.resultCount++;
            if (sim.resultName.empty() && !sim.inputRawId.empty()) {
              const auto &id = sim.inputRawId;
              if (id.find("beef") != std::string::npos) sim.resultName = "熟牛肉";
              else if (id.find("porkchop") != std::string::npos) sim.resultName = "熟猪排";
              else if (id.find("mutton") != std::string::npos) sim.resultName = "熟羊肉";
              else if (id.find("chicken") != std::string::npos) sim.resultName = "熟鸡肉";
              else if (id.find("rabbit") != std::string::npos) sim.resultName = "熟兔肉";
              else if (id.find("salmon") != std::string::npos) sim.resultName = "熟鲑鱼";
              else if (id.find("cod") != std::string::npos) sim.resultName = "熟鳕鱼";
              else if (id.find("iron") != std::string::npos) sim.resultName = "铁锭";
              else if (id.find("gold") != std::string::npos) sim.resultName = "金锭";
              else if (id.find("copper") != std::string::npos) sim.resultName = "铜锭";
              else if (id.find("debris") != std::string::npos) sim.resultName = "下界合金碎片";
              else if (id.find("cobbled_deepslate") != std::string::npos) sim.resultName = "深板岩";
              else if (id.find("cobble") != std::string::npos) sim.resultName = "石头";
              else if (id.find("stone") != std::string::npos) sim.resultName = "平滑石头";
              else if (id.find("sand") != std::string::npos) sim.resultName = "玻璃";
              else if (id.find("potato") != std::string::npos) sim.resultName = "烤马铃薯";
              else if (id.find("kelp") != std::string::npos) sim.resultName = "干海带";
              else if (id.find("clay") != std::string::npos) sim.resultName = "红砖";
              else if (id.find("log") != std::string::npos || id.find("wood") != std::string::npos) sim.resultName = "木炭";
            }
            if (sim.inputCount == 0) {
              sim.cookTicks = 0;
              break;
            }
          }
        } else {
          sim.cookTicks = 0;
        }

        if (sim.litTime <= 0 && sim.inputCount > 0 && sim.fuelCount > 0 && isBlockLit) {
          sim.fuelCount--;
          sim.litDuration = getFuelBurnDuration(sim.fuelRawId, 1600);
          sim.litTime = sim.litDuration;
        }
      } else {
        if (sim.cookTicks > 0) {
          sim.cookTicks -= deltaTicks * 2;
          if (sim.cookTicks < 0) sim.cookTicks = 0;
        }
        if (isBlockLit && sim.inputCount > 0 && sim.fuelCount > 0) {
          sim.fuelCount--;
          sim.litDuration = getFuelBurnDuration(sim.fuelRawId, 1600);
          sim.litTime = sim.litDuration;
        }
      }
    }
  }

  // 物理方块点亮状态兜底：若世界中方块本身已熄灭（非 lit_*），强制将燃烧时间归零，彻底根除幽灵燃烧倒计时
  if (isBlockLit && sim.litTime <= 0) {
    sim.litTime = sim.litDuration > 0 ? sim.litDuration : 1600;
  } else if (!isBlockLit) {
    sim.litTime = 0;
  }

  ContainerCache::get().setFurnace(posKey, sim);

  outData.isBurning = (sim.litTime > 0 || isBlockLit);
  outData.isCooking = (outData.isBurning && (sim.inputCount > 0 || isBlockLit));

  if (sim.maxCookTime > 0) {
    outData.cookProgress = static_cast<float>(sim.cookTicks) / static_cast<float>(sim.maxCookTime);
    if (outData.cookProgress > 1.0f) outData.cookProgress = 1.0f;
    float leftTicks = static_cast<float>(sim.maxCookTime - sim.cookTicks);
    if (leftTicks < 0.0f) leftTicks = 0.0f;
    outData.cookSecondsLeft = leftTicks / 20.0f;
  }

  if (sim.litDuration > 0) {
    outData.fuelSecondsLeft = static_cast<float>(sim.litTime) / 20.0f;
    outData.fuelProgress = static_cast<float>(sim.litTime) / static_cast<float>(sim.litDuration);
    if (outData.fuelProgress > 1.0f) outData.fuelProgress = 1.0f;
  } else {
    outData.fuelProgress = outData.isBurning ? 1.0f : 0.0f;
  }

  if (sim.inputCount > 0) {
    outData.furnaceInput = sim.inputName;
    outData.furnaceInputCount = sim.inputCount;
  } else if (isBlockLit) {
    outData.furnaceInput = "(进行中 - 打开后同步)";
    outData.furnaceInputCount = 1;
  } else {
    outData.furnaceInput = "";
    outData.furnaceInputCount = 0;
  }

  outData.furnaceFuel = (sim.fuelCount > 0) ? sim.fuelName : "";
  outData.furnaceFuelCount = sim.fuelCount;
  outData.furnaceResult = (sim.resultCount > 0) ? sim.resultName : "";
  outData.furnaceResultCount = sim.resultCount;

  return true;
}

} // namespace highlight
