#include "container/ContainerModelParser.h"
#include "container/ContainerUtils.h"

namespace highlight {

bool ContainerModelParser::parse(void *cm, ParsedContainerResult &out) {
  out = ParsedContainerResult{};
  if (!cm || !IsValidPointer(reinterpret_cast<uintptr_t>(cm)) ||
      !IsMemoryReadable(cm, 0x150)) {
    return false;
  }

  uintptr_t mContainers = (uintptr_t)cm + 0xb0;
  if (!IsMemoryReadable(reinterpret_cast<const void *>(mContainers), 0x18)) {
    return false;
  }

  uintptr_t node = *reinterpret_cast<const uintptr_t *>(mContainers + 0x10);
  int nodeCount = 0;

  while (node && IsValidPointer(node) && nodeCount++ < 32) {
    if (!IsMemoryReadable(reinterpret_cast<const void *>(node), 0x38)) break;

    std::string key = ReadLibcppString(reinterpret_cast<const void *>(node + 0x10));
    uintptr_t cmModel = *reinterpret_cast<const uintptr_t *>(node + 0x28);
    if (!cmModel || !IsValidPointer(cmModel) ||
        !IsMemoryReadable(reinterpret_cast<const void *>(cmModel), 0x38)) {
      node = *reinterpret_cast<const uintptr_t *>(node);
      continue;
    }

    uint8_t enumVal = *reinterpret_cast<const uint8_t *>(cmModel + 0x28);
    std::string modelName = ReadLibcppString(reinterpret_cast<const void *>(cmModel + 0x10));

    // 1. 严格过滤玩家自身背包、快捷栏、光标、合成配方等
    if (IsPlayerContainerEnum(enumVal) || IsPlayerContainerString(key) || IsPlayerContainerString(modelName)) {
      node = *reinterpret_cast<const uintptr_t *>(node);
      continue;
    }

    // 2. 严格白名单判定合法世界方块容器类型
    bool isLevelEntity = (enumVal == ContainerEnum::LevelEntity ||
                          key.find("LevelEntity") != std::string::npos ||
                          modelName.find("LevelEntity") != std::string::npos);
    bool isBarrelModel = (enumVal == ContainerEnum::Barrel ||
                          key.find("Barrel") != std::string::npos ||
                          modelName.find("Barrel") != std::string::npos);
    bool isShulkerModel = (enumVal == ContainerEnum::ShulkerBox ||
                           key.find("Shulker") != std::string::npos ||
                           modelName.find("Shulker") != std::string::npos);
    bool isCrafterModel = (enumVal == ContainerEnum::CrafterLevelEntity ||
                           key.find("Crafter") != std::string::npos ||
                           modelName.find("Crafter") != std::string::npos);

    bool isFurnaceSlot = (enumVal == ContainerEnum::FurnaceFuel ||
                          enumVal == ContainerEnum::FurnaceIngredient ||
                          enumVal == ContainerEnum::FurnaceResult ||
                          enumVal == ContainerEnum::BlastFurnaceIngredient ||
                          enumVal == ContainerEnum::SmokerIngredient ||
                          key.find("Furnace") != std::string::npos ||
                          key.find("Smoker") != std::string::npos ||
                          key.find("Blast") != std::string::npos ||
                          modelName.find("Furnace") != std::string::npos ||
                          modelName.find("Smoker") != std::string::npos ||
                          modelName.find("Blast") != std::string::npos);

    if (!isLevelEntity && !isBarrelModel && !isShulkerModel && !isCrafterModel && !isFurnaceSlot) {
      node = *reinterpret_cast<const uintptr_t *>(node);
      continue;
    }

    // 3. 读取该模型内挂载的槽位物品 (cmModel + 0xd8, 0xe0)
    if (IsMemoryReadable(reinterpret_cast<const void *>(cmModel + 0xd8), 16)) {
      uintptr_t begin = *reinterpret_cast<const uintptr_t *>(cmModel + 0xd8);
      uintptr_t end = *reinterpret_cast<const uintptr_t *>(cmModel + 0xe0);

      if (IsValidPointer(begin) && IsValidPointer(end) && end >= begin) {
        size_t count = (end - begin) / 152;
        if (count >= 1 && count <= 108) {
          out.foundContainer = true;

          if (isFurnaceSlot) {
            out.isFurnace = true;
            if (out.foundTitle.empty()) {
              if (enumVal == ContainerEnum::SmokerIngredient ||
                  key.find("Smoker") != std::string::npos ||
                  modelName.find("Smoker") != std::string::npos) {
                out.foundTitle = "烟熏炉";
              } else if (enumVal == ContainerEnum::BlastFurnaceIngredient ||
                         key.find("Blast") != std::string::npos ||
                         modelName.find("Blast") != std::string::npos) {
                out.foundTitle = "高炉";
              } else {
                out.foundTitle = "熔炉";
              }
            }
          } else {
            out.totalSlots += static_cast<int>(count);
            if (out.foundTitle.empty()) {
              if (isBarrelModel) {
                out.foundTitle = "木桶";
              } else if (isShulkerModel) {
                out.foundTitle = "潜影盒";
              } else if (isCrafterModel) {
                out.foundTitle = "合成器";
              } else if (count == 5) {
                out.foundTitle = "漏斗";
              } else if (count == 9) {
                out.foundTitle = "发射器/投掷器";
              } else if (count > 27) {
                out.foundTitle = "大箱子";
              } else {
                out.foundTitle = "箱子";
              }
            }
          }

          for (size_t i = 0; i < count; i++) {
            uintptr_t itemStack = begin + i * 152;
            if (!IsMemoryReadable(reinterpret_cast<const void *>(itemStack + 0x20), 4)) continue;
            uint8_t countByte = *reinterpret_cast<const uint8_t *>(itemStack + 0x22);
            if (countByte == 0) continue;

            void *item = GetItemFromStack(reinterpret_cast<void *>(itemStack));
            if (!item || !IsValidPointer(reinterpret_cast<uintptr_t>(item))) continue;

            uint16_t aux = *reinterpret_cast<const uint16_t *>(itemStack + 0x20);
            std::string rawId = GetItemName(item);
            if (rawId.empty()) continue;

            std::string name = TranslateItemSafely(rawId);
            if (name.empty()) name = rawId;

            out.filledSlots++;

            if (isFurnaceSlot) {
              bool isIng = (enumVal == ContainerEnum::FurnaceIngredient ||
                            enumVal == ContainerEnum::BlastFurnaceIngredient ||
                            enumVal == ContainerEnum::SmokerIngredient ||
                            key.find("Ingredient") != std::string::npos ||
                            modelName.find("Ingredient") != std::string::npos);
              bool isFuel = (enumVal == ContainerEnum::FurnaceFuel ||
                             key.find("Fuel") != std::string::npos ||
                             modelName.find("Fuel") != std::string::npos);
              bool isRes = (enumVal == ContainerEnum::FurnaceResult ||
                            key.find("Result") != std::string::npos ||
                            modelName.find("Result") != std::string::npos);
              if (isIng) {
                out.inRaw = rawId; out.inName = name; out.inCount = countByte;
              } else if (isFuel) {
                out.fuelRaw = rawId; out.fuelName = name; out.fuelCount = countByte;
              } else if (isRes) {
                out.resRaw = rawId; out.resName = name; out.resCount = countByte;
              }
            } else {
              bool merged = false;
              for (auto &slotItem : out.mergedItems) {
                if (slotItem.rawId == rawId && slotItem.aux == aux) {
                  slotItem.count += countByte;
                  merged = true;
                  break;
                }
              }
              if (!merged) {
                out.mergedItems.push_back(ContainerSlot{name, rawId, aux, countByte});
              }
            }
          }
        }
      }
    }

    node = *reinterpret_cast<const uintptr_t *>(node);
  }

  return out.foundContainer;
}

} // namespace highlight
