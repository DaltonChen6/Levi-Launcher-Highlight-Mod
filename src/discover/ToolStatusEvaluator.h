#pragma once

#include <cstdint>
#include <string>

namespace highlight {

struct ToolEvaluationResult {
  std::string heldItemName = "空手 / 其他物品";
  std::string toolStatus;
  std::string recommendTool;
  bool requiresTool = false;
  bool canHarvest = false;
  bool isFishing = false;
};

class ToolStatusEvaluator {
public:
  static ToolEvaluationResult evaluate(void *player, void *block, uintptr_t legacy,
                                      const std::string &typeName);
};

} // namespace highlight
