#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace highlight {

// 原生 Bedrock HitResult 驱动的方块高亮发现管线（源自 Waila 架构）
// 0 延迟、0 扫描、每帧从游戏原生十字准星 HitResult 实时获取目标方块与破坏进度
class Discovery {
public:
  Discovery() = default;

  void onClientUpdate(void *ci);
  void onStartDestroyBlock(void *gm, const int32_t *pos, uint8_t face);
  void onContinueDestroyBlock(void *gm, const int32_t *pos, uint8_t face, const float *playerPos);
  void onStopDestroyBlock(void *gm, const int32_t *pos);

private:
  std::atomic<bool> mIsDestroying{false};
  std::atomic<float> mMiningProgress{0.0f};
  std::atomic<int32_t> mDestroyPosX{0};
  std::atomic<int32_t> mDestroyPosY{0};
  std::atomic<int32_t> mDestroyPosZ{0};

  int32_t mLastLookedPos[3]{0, 0, 0};
  bool mHasLookedPos = false;
};

} // namespace highlight

