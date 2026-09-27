#pragma once

#include <string>

namespace highlight {

struct ContainerDisplayData;

// 实体与生物高亮透视数据结构
struct EntityDisplayData {
  bool hasEntity = false;
  std::string name;             // 生物名称，如 "马", "村民", "铁傀儡", "监守者"
  std::string rawId;            // 原生 ID，如 "minecraft:horse"
  float distance = 0.0f;        // 准星距离 (米)
  float hp = 0.0f;              // 当前血量 (半心为 1.0)
  float maxHp = 0.0f;           // 最大血量
  int armor = 0;                // 护甲值 (0~30)
  bool isPlayer = false;

  // 马匹物理属性（严格对齐 Minecraft Wiki 官方公式）
  bool isHorse = false;
  float horseSpeed = 0.0f;      // 实际移动速度 (m/s)
  float horseJump = 0.0f;       // 跳跃高度 (格)

  // 狼属性
  bool isWolf = false;
  float wolfArmor = 0.0f;       // 狼铠耐久 (0.0f ~ 1.0f)
  std::string collarColor;      // 项圈颜色

  // 村民属性
  bool isVillager = false;
  std::string profession;       // 职业名称 (如 "武器匠")
  int villagerLevel = 0;        // 交易等级 (1~5)

  // 铁傀儡
  bool isIronGolem = false;
  int crackLevel = 0;           // 裂纹程度 (0 无, 1 轻微, 2 中度, 3 严重)

  // 监守者
  bool isWarden = false;
  int angerLevel = 0;           // 怒气等级 (0~150)

  void clear() {
    hasEntity = false;
    name.clear();
    rawId.clear();
    distance = 0.0f;
    hp = 0.0f;
    maxHp = 0.0f;
    armor = 0;
    isPlayer = false;
    isHorse = false;
    horseSpeed = 0.0f;
    horseJump = 0.0f;
    isWolf = false;
    wolfArmor = 0.0f;
    collarColor.clear();
    isVillager = false;
    profession.clear();
    villagerLevel = 0;
    isIronGolem = false;
    crackLevel = 0;
    isWarden = false;
    angerLevel = 0;
  }
};

// 初始化：hook eglSwapBuffers，每帧在屏幕中间绘制 HUD。
// 返回是否成功。
bool InitHudRenderer();
void ShutdownHudRenderer();

// 设置要显示的方块文本（渲染线程每帧读取）
void SetHudText(const std::string &text);

// 设置容器透视数据
void SetHudContainerData(const ContainerDisplayData &data);

// 设置实体与生物高亮透视数据
void SetHudEntityData(const EntityDisplayData &data);

// 推送右侧物品拾取弹簧动画提示
void PushPickupItem(const std::string &itemName, int count);

} // namespace highlight
