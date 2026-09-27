#pragma once

#include "container/ContainerTypes.h"
#include <cstdint>
#include <string>

namespace highlight {

class ChestHandler {
public:
  static bool inspect(void *region, void *blockActor, const int32_t *blockPos,
                      const std::string &typeName, int64_t posKey,
                      ContainerDisplayData &outData);
};

} // namespace highlight
