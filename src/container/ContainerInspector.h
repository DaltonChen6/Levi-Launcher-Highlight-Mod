#pragma once

#include "container/ContainerTypes.h"

namespace highlight {

class ContainerInspector {
public:
  // 探测并解析目标方块的容器与实体数据（只读、无锁、零风险）
  static bool inspect(void *region, const int32_t *blockPos,
                      const std::string &typeName, uint16_t blockData,
                      ContainerDisplayData &outData);

  // 当玩家打开容器 GUI 时，传入 ContainerManagerModel* 与最近瞄准的方块坐标，实时同步 GUI 物品数据
  // 返回 true 表示成功捕获并同步了容器物品（非玩家普通背包），并填充 outData
  static bool notifyContainerGuiOpen(void *cm, const int32_t *pos, ContainerDisplayData &outData);
  static bool notifyContainerGuiOpen(void *cm, const int32_t *pos);
};

} // namespace highlight
