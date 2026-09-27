#pragma once

#include <string>

namespace highlight {

class MiningCalculator {
public:
  static float calculateMiningTime(const std::string &typeName, const std::string &heldItem,
                                  bool canHarvest, bool requiresTool,
                                  bool isAirborne = false, bool isUnderwater = false);

  static bool isToolMatching(const std::string &heldItem, const std::string &recommendTool);
};

} // namespace highlight
