#include "discover/LocalizationTree.h"
#include "container/ContainerUtils.h"

#include <cctype>
#include <cstring>

namespace highlight {

std::string LocalizationTree::readAsciiString(const void *p) {
  if (!p || !IsValidPointer(reinterpret_cast<uintptr_t>(p)) || !IsMemoryReadable(p, 24)) {
    return {};
  }
  const uint8_t *bp = static_cast<const uint8_t *>(p);

  auto isValidAscii = [](const char *str, size_t len) -> bool {
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
      if (isValidAscii(text, len)) return std::string(text, len);
    }
  } else {
    uint64_t size = 0;
    uintptr_t dataPtr = 0;
    std::memcpy(&size, bp + 8, 8);
    std::memcpy(&dataPtr, bp + 16, 8);
    if (size > 0 && size < 256 && IsValidPointer(dataPtr) &&
        IsMemoryReadable(reinterpret_cast<const void *>(dataPtr), size)) {
      const char *text = reinterpret_cast<const char *>(dataPtr);
      if (isValidAscii(text, size)) return std::string(text, size);
    }
  }
  return {};
}

std::string LocalizationTree::readUtf8String(const void *p) {
  if (!p || !IsValidPointer(reinterpret_cast<uintptr_t>(p)) || !IsMemoryReadable(p, 24)) {
    return {};
  }
  const uint8_t *bp = static_cast<const uint8_t *>(p);

  auto isValidUtf8 = [](const char *str, size_t len) -> bool {
    if (!str || len == 0 || len > 512) return false;
    for (size_t i = 0; i < len; i++) {
      unsigned char c = static_cast<unsigned char>(str[i]);
      if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
        return false;
      }
    }
    return true;
  };

  if ((bp[0] & 1) == 0) {
    size_t len = bp[0] >> 1;
    if (len > 0 && len <= 22) {
      const char *text = reinterpret_cast<const char *>(bp + 1);
      if (isValidUtf8(text, len)) return std::string(text, len);
    }
  } else {
    uint64_t size = 0;
    uintptr_t dataPtr = 0;
    std::memcpy(&size, bp + 8, 8);
    std::memcpy(&dataPtr, bp + 16, 8);
    if (size > 0 && size <= 512 && IsValidPointer(dataPtr) &&
        IsMemoryReadable(reinterpret_cast<const void *>(dataPtr), size)) {
      const char *text = reinterpret_cast<const char *>(dataPtr);
      if (isValidUtf8(text, size)) return std::string(text, size);
    }
  }
  return {};
}

std::string LocalizationTree::stripMinecraftColorCodes(const std::string &text) {
  std::string result;
  result.reserve(text.size());
  for (size_t i = 0; i < text.size(); i++) {
    if (static_cast<unsigned char>(text[i]) == 0xC2 &&
        i + 1 < text.size() &&
        static_cast<unsigned char>(text[i + 1]) == 0xA7) {
      i += 2;
      continue;
    }
    result.push_back(text[i]);
  }
  return result;
}

std::string LocalizationTree::snakeToCamel(const std::string &s) {
  std::string result;
  result.reserve(s.size());
  bool toUpper = false;
  for (char c : s) {
    if (c == '_') {
      toUpper = true;
    } else {
      result.push_back(toUpper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c);
      toUpper = false;
    }
  }
  return result;
}

std::string LocalizationTree::searchTree(void *pLoc, const std::string &targetKey) {
  if (!pLoc || targetKey.empty()) return {};

  const uint8_t *locBytes = static_cast<const uint8_t *>(pLoc);
  if (!IsMemoryReadable(locBytes + 0x40, 8)) return {};

  uintptr_t rootAddr = *reinterpret_cast<const uintptr_t *>(locBytes + 0x40);
  const uint8_t *curr = reinterpret_cast<const uint8_t *>(rootAddr);

  int maxDepth = 64;
  while (curr && maxDepth-- > 0) {
    uintptr_t currAddr = reinterpret_cast<uintptr_t>(curr);
    if (!IsValidPointer(currAddr) || !IsMemoryReadable(curr, 0x50)) {
      break;
    }

    std::string nodeKey = readAsciiString(curr + 0x20);
    if (nodeKey.empty()) {
      break;
    }

    if (nodeKey == targetKey) {
      std::string val = readUtf8String(curr + 0x38);
      return stripMinecraftColorCodes(val);
    }

    if (targetKey < nodeKey) {
      curr = *reinterpret_cast<const uint8_t *const *>(curr + 0x00);
    } else {
      curr = *reinterpret_cast<const uint8_t *const *>(curr + 0x08);
    }
  }

  return {};
}

} // namespace highlight
