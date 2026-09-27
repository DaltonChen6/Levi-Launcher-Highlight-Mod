#pragma once

#include <string>
#include <cstdint>

namespace highlight {

class LocalizationService {
public:
  // 初始化：通过 RTTI 符号 "8I18nImpl" 查找第 15 号槽函数并安装钩子，静默截获全局 I18n 实例指针
  static bool init();

  // 卸载钩子并重置状态
  static void shutdown();

  // 是否已成功捕获到游戏运行时的 I18n 实例
  static bool isReady();

  // 查询当前客户端语言代码（例如 "zh_CN", "en_US", "ja_JP"）
  static std::string getLanguageCode();

  // 核心查询接口：根据方块描述 ID（如 "tile.sand", "tile.bamboo_planks"）或标准类型名（如 "minecraft:bamboo_planks"）
  // 在游戏内存红黑树中检索由玩家当前材质包/语言包决定的真正本地化译名。
  // 若未找到或系统尚未准备好，返回空字符串（此时自动回退到离线字典静态保底）。
  static std::string getTranslation(const std::string &descId, const std::string &typeName = "");

  // 物品本地化查询接口：根据物品名称或 ID 进行翻译
  static std::string getTranslationForItem(const std::string &rawItemName);

  // 精确键查询接口
  static std::string getExactTranslation(const std::string &exactKey);
};

} // namespace highlight
