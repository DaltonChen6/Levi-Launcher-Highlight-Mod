#pragma once

#include "container/ContainerTypes.h"
#include <cstdint>
#include <string>

namespace highlight {

class SpecialBlockHandler {
public:
  // 营火 / 灵魂营火 (4 槽位烤肉及秒数倒计时)
  static bool inspectCampfire(void *blockActor, const std::string &typeName,
                              ContainerDisplayData &outData);

  // 蜂箱 / 蜂巢 (蜜蜂数量与蜂蜜等级)
  static bool inspectBeehive(void *blockActor, uint16_t blockData,
                             const std::string &typeName,
                             ContainerDisplayData &outData);
};

} // namespace highlight
