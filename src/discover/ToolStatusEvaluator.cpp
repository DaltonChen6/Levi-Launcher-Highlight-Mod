#include "discover/ToolStatusEvaluator.h"
#include "block/MiningCalculator.h"
#include "container/ContainerUtils.h"
#include "discover/BlockTranslator.h"
#include "discover/LocalizationService.h"

#include <pl/memory/Vtable.hpp>

namespace highlight {

ToolEvaluationResult ToolStatusEvaluator::evaluate(void *player, void *block,
                                                   uintptr_t legacy,
                                                   const std::string &typeName) {
  ToolEvaluationResult res;
  res.recommendTool = BlockTranslator::getRecommendedTool(typeName);

  // 1. 读取方块是否强制要求工具采集 (legacy + 0x135)
  uint8_t flags = 0;
  if (IsMemoryReadable(reinterpret_cast<const void *>(legacy + 0x135), 1)) {
    flags = *reinterpret_cast<const uint8_t *>(legacy + 0x135);
  }
  res.requiresTool = (flags & 4) != 0;

  static const char *const kStrictRequiresToolKeywords[] = {
      "ore", "stone", "deepslate", "granite", "diorite", "andesite",
      "calcite", "tuff", "basalt", "blackstone", "brick", "cobble",
      "obsidian", "concrete", "terracotta", "ice", "furnace", "anvil",
      "copper_bulb", "copper_grate", "trial_spawner", "vault", "heavy_core"
  };
  if (!res.requiresTool) {
    for (const char *kw : kStrictRequiresToolKeywords) {
      if (typeName.find(kw) != std::string::npos &&
          typeName.find("concrete_powder") == std::string::npos &&
          typeName.find("moss") == std::string::npos) {
        res.requiresTool = true;
        break;
      }
    }
  }

  // 2. LocalPlayer 手持工具识别与采集能力判定
  if (player && IsValidPointer(reinterpret_cast<uintptr_t>(player))) {
    static auto sGetCarriedItem = reinterpret_cast<void *(*)(void *)>(
        pl::memory::resolveVtableFunction("6Player", 78, "libminecraftpe.so"));
    if (sGetCarriedItem) {
      void *itemStack = sGetCarriedItem(player);
      if (itemStack && IsValidPointer(reinterpret_cast<uintptr_t>(itemStack))) {
        void *item = GetItemFromStack(itemStack);
        if (item && IsValidPointer(reinterpret_cast<uintptr_t>(item))) {
          std::string rawId = GetItemName(item);
          if (!rawId.empty()) {
            if (rawId.find("fishing_rod") != std::string::npos) {
              res.isFishing = true;
            }
            std::string locItemName = LocalizationService::getTranslationForItem(rawId);
            if (!locItemName.empty()) {
              res.heldItemName = locItemName;
            } else {
              res.heldItemName = BlockTranslator::translateItem(rawId);
            }
          }

          // Item::canDestroySpecial (slot 34 @ 0x110)
          uintptr_t *itemVt = *reinterpret_cast<uintptr_t **>(item);
          if (itemVt && IsValidPointer(reinterpret_cast<uintptr_t>(itemVt)) && itemVt[34]) {
            auto canDestroy = reinterpret_cast<bool (*)(void *, void *)>(itemVt[34]);
            res.canHarvest = canDestroy(item, block);
          }

          if (!res.requiresTool && !res.canHarvest) {
            res.canHarvest = MiningCalculator::isToolMatching(res.heldItemName, res.recommendTool);
          }

          if (res.isFishing) {
            res.toolStatus = "钓鱼模式 (手持钓鱼竿)";
          } else if (res.canHarvest) {
            res.toolStatus = "有效采集工具 √";
          } else if (!res.requiresTool) {
            if (res.recommendTool.empty() || res.recommendTool.find("任何") != std::string::npos ||
                res.recommendTool.find("空手") != std::string::npos) {
              res.toolStatus = "可徒手采集 √";
            } else {
              res.toolStatus = "可徒手采集 √ (建议: " + res.recommendTool + ")";
            }
          } else {
            res.toolStatus = "工具不匹配 × (建议: " + res.recommendTool + ")";
          }
        }
      }
    }
  }

  if (res.toolStatus.empty()) {
    if (res.isFishing) {
      res.toolStatus = "钓鱼模式 (手持钓鱼竿)";
    } else if (!res.requiresTool) {
      if (res.recommendTool.empty() || res.recommendTool.find("任何") != std::string::npos ||
          res.recommendTool.find("空手") != std::string::npos) {
        res.toolStatus = "可徒手采集 √";
      } else {
        res.toolStatus = "可徒手采集 √ (建议: " + res.recommendTool + ")";
      }
    } else {
      res.toolStatus = "工具不匹配 × (建议: " + res.recommendTool + ")";
    }
  }

  return res;
}

} // namespace highlight
