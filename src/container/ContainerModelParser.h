#pragma once

#include "container/ContainerTypes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace highlight {

struct ParsedContainerResult {
  bool foundContainer = false;
  bool isFurnace = false;
  std::string foundTitle;
  int totalSlots = 0;
  int filledSlots = 0;
  std::vector<ContainerSlot> mergedItems;

  // 熔炉专属解析结果
  std::string inRaw, inName;
  uint32_t inCount = 0;
  std::string fuelRaw, fuelName;
  uint32_t fuelCount = 0;
  std::string resRaw, resName;
  uint32_t resCount = 0;
};

class ContainerModelParser {
public:
  // 遍历并解析 ContainerManagerModel + 0xb0 处挂载的所有 ContainerModel
  // 严格执行合法方块容器白名单，绝对拦截玩家背包/快捷栏/光标
  static bool parse(void *cm, ParsedContainerResult &out);
};

} // namespace highlight
