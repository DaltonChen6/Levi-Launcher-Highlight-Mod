#pragma once

#include "discover/BlockTranslator.h"
#include "discover/LocalizationService.h"

#include <cstdint>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

namespace highlight {

constexpr uintptr_t kTagMask = 0x00FFFFFFFFFFFFFF;

inline bool IsValidPointer(uintptr_t p) {
  uintptr_t masked = p & kTagMask;
  return (masked >= 0x10000ULL && masked <= 0x00007FFFFFFFFFFFULL);
}

inline bool IsAlignedPointer(uintptr_t p, size_t alignment = 8) {
  uintptr_t masked = p & kTagMask;
  return (masked >= 0x10000ULL && masked <= 0x00007FFFFFFFFFFFULL && (masked & (alignment - 1)) == 0);
}

inline bool IsValidFunctionPointer(uintptr_t p) {
  uintptr_t masked = p & kTagMask;
  return (masked >= 0x10000ULL && masked <= 0x00007FFFFFFFFFFFULL && (masked & 3) == 0);
}

inline bool IsMemoryReadable(const void *p, size_t len = 1) {
  if (!p || len == 0) return false;
  uintptr_t start = reinterpret_cast<uintptr_t>(p) & kTagMask;
  if (start < 0x10000ULL || start > 0x00007FFFFFFFFFFFULL) return false;
  uintptr_t end = (reinterpret_cast<uintptr_t>(p) + len - 1) & kTagMask;
  if (end < 0x10000ULL || end > 0x00007FFFFFFFFFFFULL) return false;
  if (end < start) return false;

  static std::mutex sPipeMutex;
  std::lock_guard<std::mutex> lock(sPipeMutex);
  static int sPipe[2] = {-1, -1};
  if (sPipe[0] < 0) {
    if (pipe2(sPipe, O_CLOEXEC | O_NONBLOCK) != 0) {
      return false;
    }
  }

  // Drain any stray bytes in pipe
  char drainBuf[32];
  while (read(sPipe[0], drainBuf, sizeof(drainBuf)) > 0) {}

  auto probeByte = [&](const void *ptr) -> bool {
    ssize_t ret = write(sPipe[1], ptr, 1);
    if (ret == 1) {
      char tmp;
      read(sPipe[0], &tmp, 1);
      return true;
    }
    return false;
  };

  const char *base = reinterpret_cast<const char *>(start);
  if (!probeByte(base)) return false;

  if ((start >> 12) != (end >> 12)) {
    if (!probeByte(base + (len - 1))) return false;
    if (len > 4096) {
      for (uintptr_t page = (start | 0xFFF) + 1; page < end; page += 0x1000) {
        if (!probeByte(base + (page - start))) return false;
      }
    }
  }
  return true;
}

inline std::string TranslateItemSafely(const std::string &rawName) {
  if (rawName.empty()) return {};
  std::string loc = LocalizationService::getTranslationForItem(rawName);
  if (!loc.empty()) return loc;
  return BlockTranslator::translateItem(rawName);
}

// 从 ItemStack 中提取 Item* (全层级只读安全校验，防止空指针与 UAF)
inline void *GetItemFromStack(void *itemStack) {
  if (!itemStack || !IsValidPointer(reinterpret_cast<uintptr_t>(itemStack))) {
    return nullptr;
  }
  if (!IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)itemStack + 8), 8)) {
    return nullptr;
  }
  uintptr_t p1 = *reinterpret_cast<uintptr_t *>((uintptr_t)itemStack + 8);
  if (!p1 || !IsAlignedPointer(p1, 8)) {
    return nullptr;
  }
  if (!IsMemoryReadable(reinterpret_cast<const void *>(p1), 8)) {
    return nullptr;
  }
  uintptr_t p2 = *reinterpret_cast<uintptr_t *>(p1);
  if (p2 && IsAlignedPointer(p2, 8)) {
    return reinterpret_cast<void *>(p2);
  }
  return nullptr;
}

inline std::string ReadLibcppString(const void *p) {
  if (!p || !IsValidPointer(reinterpret_cast<uintptr_t>(p)) || !IsMemoryReadable(p, 24)) {
    return {};
  }
  const uint8_t *bp = static_cast<const uint8_t *>(p);
  auto isValidText = [](const char *str, size_t len) -> bool {
    if (!str || len == 0 || len > 256) return false;
    for (size_t i = 0; i < len; i++) {
      if (static_cast<unsigned char>(str[i]) < 0x20 || static_cast<unsigned char>(str[i]) >= 0x7f) {
        return false;
      }
    }
    return true;
  };

  if ((bp[0] & 1) == 0) {
    size_t len = bp[0] >> 1;
    if (len > 0 && len <= 22) {
      const char *text = reinterpret_cast<const char *>(bp + 1);
      if (isValidText(text, len)) return std::string(text, len);
    }
  } else {
    uint64_t size = 0;
    uintptr_t dataPtr = 0;
    std::memcpy(&size, bp + 8, 8);
    std::memcpy(&dataPtr, bp + 16, 8);
    uintptr_t masked = dataPtr & kTagMask;
    if (size > 0 && size < 256 && masked >= 0x10000ULL && masked <= 0x00007FFFFFFFFFFFULL &&
        IsMemoryReadable(reinterpret_cast<const void *>(dataPtr), size)) {
      const char *text = reinterpret_cast<const char *>(masked);
      if (isValidText(text, size)) return std::string(text, size);
    }
  }
  return {};
}

namespace ContainerEnum {
  constexpr uint8_t Armor = 6;
  constexpr uint8_t LevelEntity = 7; // 箱子 / 陷阱箱 / 漏斗 / 发射器 / 投掷器
  constexpr uint8_t BeaconPayment = 8;
  constexpr uint8_t BrewingStandInput = 9;
  constexpr uint8_t BrewingStandResult = 10;
  constexpr uint8_t BrewingStandFuel = 11;
  constexpr uint8_t CombinedHotbarAndInventory = 12;
  constexpr uint8_t CraftingInput = 13;
  constexpr uint8_t CraftingOutputPreview = 14;
  constexpr uint8_t RecipeConstruction = 15;
  constexpr uint8_t RecipeNature = 16;
  constexpr uint8_t RecipeItems = 17;
  constexpr uint8_t RecipeSearch = 18;
  constexpr uint8_t RecipeSearchBar = 19;
  constexpr uint8_t RecipeEquipment = 20;
  constexpr uint8_t RecipeBook = 21;
  constexpr uint8_t EnchantingInput = 22;
  constexpr uint8_t EnchantingMaterial = 23;
  constexpr uint8_t FurnaceFuel = 24;
  constexpr uint8_t FurnaceIngredient = 25;
  constexpr uint8_t FurnaceResult = 26;
  constexpr uint8_t HorseEquip = 27;
  constexpr uint8_t Hotbar = 28;
  constexpr uint8_t Inventory = 29;
  constexpr uint8_t ShulkerBox = 30;
  constexpr uint8_t Offhand = 34;
  constexpr uint8_t BlastFurnaceIngredient = 45;
  constexpr uint8_t SmokerIngredient = 46;
  constexpr uint8_t Barrel = 58;
  constexpr uint8_t Cursor = 59;
  constexpr uint8_t CrafterLevelEntity = 62;
}

inline bool IsPlayerContainerEnum(uint8_t e) {
  if (e == ContainerEnum::Armor || e == ContainerEnum::CombinedHotbarAndInventory ||
      e == ContainerEnum::Hotbar || e == ContainerEnum::Inventory ||
      e == ContainerEnum::Offhand || e == ContainerEnum::Cursor) {
    return true;
  }
  if (e >= ContainerEnum::CraftingInput && e <= ContainerEnum::RecipeBook) return true;
  if (e >= 64 && e <= 66) return true;
  return false;
}

inline bool IsValidContainerEnum(uint8_t e) {
  if (e == ContainerEnum::LevelEntity || e == ContainerEnum::Barrel ||
      e == ContainerEnum::ShulkerBox || e == ContainerEnum::CrafterLevelEntity ||
      e == ContainerEnum::FurnaceFuel || e == ContainerEnum::FurnaceIngredient ||
      e == ContainerEnum::FurnaceResult || e == ContainerEnum::BlastFurnaceIngredient ||
      e == ContainerEnum::SmokerIngredient || e == ContainerEnum::BeaconPayment ||
      (e >= ContainerEnum::BrewingStandInput && e <= ContainerEnum::BrewingStandFuel)) {
    return true;
  }
  return false;
}

inline bool IsPlayerContainerString(const std::string &s) {
  if (s.empty()) return false;
  std::string lower;
  lower.reserve(s.size());
  for (char c : s) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  if (lower.find("inventory") != std::string::npos) return true;
  if (lower.find("hotbar") != std::string::npos) return true;
  if (lower.find("cursor") != std::string::npos) return true;
  if (lower.find("armor") != std::string::npos) return true;
  if (lower.find("offhand") != std::string::npos) return true;
  if (lower.find("recipe") != std::string::npos) return true;
  if (lower.find("craft") != std::string::npos) return true;
  return false;
}

inline bool IsContainerString(const std::string &s) {
  if (s.empty()) return false;
  std::string lower;
  lower.reserve(s.size());
  for (char c : s) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  if (lower.find("levelentity") != std::string::npos || lower.find("level_entity") != std::string::npos) return true;
  if (lower.find("chest") != std::string::npos) return true;
  if (lower.find("barrel") != std::string::npos) return true;
  if (lower.find("shulker") != std::string::npos) return true;
  if (lower.find("furnace") != std::string::npos) return true;
  if (lower.find("smoker") != std::string::npos) return true;
  if (lower.find("blast") != std::string::npos) return true;
  if (lower.find("hopper") != std::string::npos) return true;
  if (lower.find("dispenser") != std::string::npos || lower.find("dropper") != std::string::npos) return true;
  if (lower.find("crafter") != std::string::npos) return true;
  return false;
}

inline std::string GetItemName(void *item) {
  if (!item || !IsValidPointer(reinterpret_cast<uintptr_t>(item))) {
    return {};
  }
  static const uintptr_t kItemOffsets[] = {0x90, 0x88, 0x60, 0x58, 0xe0, 0xd8, 0xb0, 0xa8};
  std::string fallback;
  for (uintptr_t off : kItemOffsets) {
    std::string s = ReadLibcppString(reinterpret_cast<const void *>((uintptr_t)item + off));
    if (s.empty()) continue;
    if (s.rfind("minecraft:", 0) == 0 || s.rfind("item.", 0) == 0) {
      return s;
    }
    if (s.size() >= 2 && s.find('\xa7') == std::string::npos) {
      if (fallback.empty()) fallback = s;
    }
  }
  return fallback;
}

inline int64_t PosToKey(int32_t x, int32_t y, int32_t z) {
  return (static_cast<int64_t>(x & 0x3FFFFFF) << 38) |
         (static_cast<int64_t>(y & 0xFFF) << 26) |
         (static_cast<int64_t>(z & 0x3FFFFFF));
}

} // namespace highlight
