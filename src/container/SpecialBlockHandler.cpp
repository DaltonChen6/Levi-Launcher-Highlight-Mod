#include "container/SpecialBlockHandler.h"
#include "container/ContainerUtils.h"

namespace highlight {

bool SpecialBlockHandler::inspectCampfire(void *blockActor, const std::string &typeName,
                                          ContainerDisplayData &outData) {
  if (typeName.find("campfire") == std::string::npos) {
    return false;
  }

  outData.specialType = SpecialContainerType::Campfire;
  outData.hasContainer = true;
  for (int i = 0; i < 4; i++) {
    void *itemInst = reinterpret_cast<void *>((uintptr_t)blockActor + 0x0c8 + i * 128);
    void *item = GetItemFromStack(itemInst);
    if (item) {
      std::string rawName = GetItemName(item);
      outData.campfireSlots[i].itemName = TranslateItemSafely(rawName);
      int32_t itemTime = *reinterpret_cast<const int32_t *>((uintptr_t)blockActor + 0x2c8 + i * 4);
      float left = (600.0f - static_cast<float>(itemTime)) / 20.0f;
      if (left < 0.0f) left = 0.0f;
      outData.campfireSlots[i].remainingSeconds = left;
      outData.campfireSlots[i].active = true;
    }
  }
  return true;
}

bool SpecialBlockHandler::inspectBeehive(void *blockActor, uint16_t blockData,
                                        const std::string &typeName,
                                        ContainerDisplayData &outData) {
  if (typeName.find("beehive") == std::string::npos && typeName.find("bee_nest") == std::string::npos) {
    return false;
  }

  outData.specialType = SpecialContainerType::Beehive;
  outData.hasContainer = true;
  uintptr_t begin = *reinterpret_cast<const uintptr_t *>((uintptr_t)blockActor + 0xc8);
  uintptr_t end = *reinterpret_cast<const uintptr_t *>((uintptr_t)blockActor + 0xd0);
  if (begin && end && end >= begin && IsValidPointer(begin) && IsValidPointer(end)) {
    size_t count = (end - begin) / 208;
    if (count > 3) count = 3;
    outData.beeOccupants = static_cast<int>(count);
  }
  outData.honeyLevel = (blockData >> 2) & 7;
  if (outData.honeyLevel > 5) outData.honeyLevel = 5;
  return true;
}

} // namespace highlight
