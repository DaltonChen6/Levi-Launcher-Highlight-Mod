#pragma once

#include "container/ContainerTypes.h"
#include "entity/EntityInspector.h"
#include "imgui.h"

#include <string>
#include <vector>

namespace highlight {

struct PickupRenderInfo {
  std::string name;
  int count = 0;
  float age = 0.0f;
};

class HudCardRenderer {
public:
  static ImVec4 getQualityColor(const std::string &name, float timeSec);

  static void renderMainCard(const std::string &text,
                             const ContainerDisplayData &containerData,
                             const EntityDisplayData &entityData,
                             float timeSec);

  static void renderPickups(const std::vector<PickupRenderInfo> &pickups,
                            float displayWidth, float displayHeight,
                            float timeSec);
};

} // namespace highlight
