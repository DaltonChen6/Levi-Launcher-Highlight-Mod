#pragma once

#include "container/ContainerTypes.h"
#include <cstdint>
#include <string>

namespace highlight {

class FurnaceHandler {
public:
  static int32_t getFuelBurnDuration(const std::string &rawId, int32_t fallback = 1600);

  static bool inspect(void *blockActor, const std::string &typeName, int64_t posKey,
                      ContainerDisplayData &outData);
};

} // namespace highlight
