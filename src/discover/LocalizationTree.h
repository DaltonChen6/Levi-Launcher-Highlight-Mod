#pragma once

#include <cstdint>
#include <string>

namespace highlight {

class LocalizationTree {
public:
  // 读取 libc++ std::string 键名 (纯 ASCII 标识符，如 "tile.dirt.name")
  static std::string readAsciiString(const void *p);

  // 读取 libc++ std::string 本地化译文（支持 UTF-8 中文/多语言字符）
  static std::string readUtf8String(const void *p);

  // 剔除 Minecraft 样式与颜色代码（如 "§6", "§r", "§l"）
  static std::string stripMinecraftColorCodes(const std::string &text);

  // 下划线命名转大驼峰命名 (如 "iron_block" -> "IronBlock")
  static std::string snakeToCamel(const std::string &s);

  // 在 Localization 红黑树中检索目标键对应的值
  static std::string searchTree(void *pLoc, const std::string &targetKey);
};

} // namespace highlight
