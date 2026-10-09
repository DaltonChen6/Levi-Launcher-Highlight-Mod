#include "container/SpecialBlockHandler.h"
#include "container/ContainerUtils.h"

namespace highlight {

bool SpecialBlockHandler::inspectCampfire(void *blockActor, const std::string &typeName,
                                          ContainerDisplayData &outData) {
  if (typeName.find("campfire") == std::string::npos) {
    return false;
  }
  if (!blockActor || !IsValidPointer(reinterpret_cast<uintptr_t>(blockActor)) ||
      !IsMemoryReadable(blockActor, 0x2e0)) {
    return false;
  }

  outData.specialType = SpecialContainerType::Campfire;
  outData.hasContainer = true;
  for (int i = 0; i < 4; i++) {
    void *itemInst = reinterpret_cast<void *>((uintptr_t)blockActor + 0x0c8 + i * 128);
    if (!IsMemoryReadable(itemInst, 0x24)) continue;
    uint8_t count = *reinterpret_cast<const uint8_t *>((uintptr_t)itemInst + 0x22);
    if (count == 0) continue;
    void *item = GetItemFromStack(itemInst);
    if (item) {
      std::string rawName = GetItemName(item);
      if (!rawName.empty()) {
        outData.campfireSlots[i].itemName = TranslateItemSafely(rawName);
        int32_t itemTime = *reinterpret_cast<const int32_t *>((uintptr_t)blockActor + 0x2c8 + i * 4);
        float left = (600.0f - static_cast<float>(itemTime)) / 20.0f;
        if (left < 0.0f) left = 0.0f;
        outData.campfireSlots[i].remainingSeconds = left;
        outData.campfireSlots[i].active = true;
      }
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
  if (!blockActor || !IsValidPointer(reinterpret_cast<uintptr_t>(blockActor)) ||
      !IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)blockActor + 0xc8), 16)) {
    return false;
  }

  outData.specialType = SpecialContainerType::Beehive;
  outData.hasContainer = true;
  uintptr_t begin = *reinterpret_cast<const uintptr_t *>((uintptr_t)blockActor + 0xc8);
  uintptr_t end = *reinterpret_cast<const uintptr_t *>((uintptr_t)blockActor + 0xd0);
  uintptr_t m_begin = begin & kTagMask;
  uintptr_t m_end = end & kTagMask;
  if (IsValidPointer(begin) && IsValidPointer(end) && m_end >= m_begin) {
    size_t count = (m_end - m_begin) / 208;
    if (count > 3) count = 3;
    outData.beeOccupants = static_cast<int>(count);
  }
  outData.honeyLevel = (blockData >> 2) & 7;
  if (outData.honeyLevel > 5) outData.honeyLevel = 5;
  return true;
}

} // namespace highlight
