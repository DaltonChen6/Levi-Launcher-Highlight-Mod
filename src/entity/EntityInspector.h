#pragma once

#include "render/HudRenderer.h"
#include <cstdint>

namespace highlight {

class EntityInspector {
public:
  static bool inspect(uintptr_t hitPtr, EntityDisplayData &outData);
};

} // namespace highlight
