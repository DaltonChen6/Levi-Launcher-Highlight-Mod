#include "block/MiningCalculator.h"
#include <cmath>

namespace highlight {

float MiningCalculator::calculateMiningTime(const std::string &typeName, const std::string &heldItem,
                                            bool canHarvest, bool requiresTool,
                                            bool isAirborne, bool isUnderwater) {
  // 1. 获取方块硬度 (Hardness)
  float hardness = 1.5f; // 默认石质方块
  if (typeName.find("obsidian") != std::string::npos) hardness = 50.0f;
  else if (typeName.find("debris") != std::string::npos) hardness = 30.0f;
  else if (typeName.find("ender_chest") != std::string::npos) hardness = 22.5f;
  else if (typeName.find("anvil") != std::string::npos) hardness = 5.0f;
  else if (typeName.find("iron_block") != std::string::npos) hardness = 5.0f;
  else if (typeName.find("deepslate") != std::string::npos) hardness = 3.0f;
  else if (typeName.find("ore") != std::string::npos) hardness = 3.0f;
  else if (typeName.find("log") != std::string::npos || typeName.find("wood") != std::string::npos ||
           typeName.find("planks") != std::string::npos) hardness = 2.0f;
  else if (typeName.find("stone") != std::string::npos || typeName.find("cobble") != std::string::npos) hardness = 1.5f;
  else if (typeName.find("wool") != std::string::npos) hardness = 0.8f;
  else if (typeName.find("dirt") != std::string::npos || typeName.find("sand") != std::string::npos ||
           typeName.find("gravel") != std::string::npos || typeName.find("clay") != std::string::npos) hardness = 0.5f;
  else if (typeName.find("glass") != std::string::npos) hardness = 0.3f;
  else if (typeName.find("leaves") != std::string::npos) hardness = 0.2f;
  else if (typeName.find("bedrock") != std::string::npos || typeName.find("barrier") != std::string::npos) return -1.0f;

  // 2. 基础工具速率倍率 (Tool Multiplier)
  float speedMultiplier = 1.0f;
  if (heldItem.find("下界合金") != std::string::npos || heldItem.find("netherite") != std::string::npos) speedMultiplier = 9.0f;
  else if (heldItem.find("钻石") != std::string::npos || heldItem.find("diamond") != std::string::npos) speedMultiplier = 8.0f;
  else if (heldItem.find("铁") != std::string::npos || heldItem.find("iron") != std::string::npos) speedMultiplier = 6.0f;
  else if (heldItem.find("石") != std::string::npos || heldItem.find("stone") != std::string::npos) speedMultiplier = 4.0f;
  else if (heldItem.find("木") != std::string::npos || heldItem.find("wood") != std::string::npos) speedMultiplier = 2.0f;
  else if (heldItem.find("金") != std::string::npos || heldItem.find("gold") != std::string::npos) speedMultiplier = 12.0f;
  else if (heldItem.find("剪刀") != std::string::npos || heldItem.find("shears") != std::string::npos) {
    if (typeName.find("leaves") != std::string::npos || typeName.find("cobweb") != std::string::npos) speedMultiplier = 15.0f;
    else if (typeName.find("wool") != std::string::npos) speedMultiplier = 5.0f;
  }

  // 3. 状态与环境修正 (滞空与水下除以 5)
  if (isAirborne) speedMultiplier /= 5.0f;
  if (isUnderwater) speedMultiplier /= 5.0f;

  // 4. 除数规则: 匹配掉落工具除以 30, 非掉落工具除以 100
  float divisor = (canHarvest || !requiresTool) ? 30.0f : 100.0f;
  float damagePerTick = speedMultiplier / (hardness * divisor);
  if (damagePerTick >= 1.0f) {
    return 0.05f; // 瞬间破坏: 1 tick = 0.05s
  }
  float ticks = std::ceil(1.0f / damagePerTick);
  return ticks / 20.0f;
}

bool MiningCalculator::isToolMatching(const std::string &heldItem, const std::string &recommendTool) {
  if (recommendTool.empty() || recommendTool.find("空手") != std::string::npos) return true;
  if (recommendTool.find("镐") != std::string::npos && (heldItem.find("镐") != std::string::npos || heldItem.find("pickaxe") != std::string::npos)) return true;
  if (recommendTool.find("斧") != std::string::npos && (heldItem.find("斧") != std::string::npos || heldItem.find("axe") != std::string::npos)) return true;
  if (recommendTool.find("锹") != std::string::npos && (heldItem.find("锹") != std::string::npos || heldItem.find("shovel") != std::string::npos)) return true;
  if (recommendTool.find("锄") != std::string::npos && (heldItem.find("锄") != std::string::npos || heldItem.find("hoe") != std::string::npos)) return true;
  if (recommendTool.find("剪刀") != std::string::npos && (heldItem.find("剪刀") != std::string::npos || heldItem.find("shears") != std::string::npos)) return true;
  if (recommendTool.find("剑") != std::string::npos && (heldItem.find("剑") != std::string::npos || heldItem.find("sword") != std::string::npos)) return true;
  return false;
}

} // namespace highlight
