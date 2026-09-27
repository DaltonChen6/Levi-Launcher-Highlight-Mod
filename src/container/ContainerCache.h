#pragma once

#include "container/ContainerTypes.h"
#include <mutex>
#include <unordered_map>

namespace highlight {

class ContainerCache {
public:
  static ContainerCache &get();

  bool getContainer(int64_t posKey, ContainerCacheEntry &outEntry);
  void setContainer(int64_t posKey, const ContainerCacheEntry &entry);
  void setDoubleChest(int32_t x, int32_t y, int32_t z, const ContainerCacheEntry &entry);

  bool getFurnace(int64_t posKey, FurnaceSimState &outSim);
  void setFurnace(int64_t posKey, const FurnaceSimState &sim);
  void updateFurnace(int64_t posKey, const std::function<void(FurnaceSimState &)> &updater);

  void clear();

private:
  ContainerCache() = default;

  std::mutex mCacheMutex;
  std::unordered_map<int64_t, ContainerCacheEntry> mContainerCache;

  std::mutex mFurnaceMutex;
  std::unordered_map<int64_t, FurnaceSimState> mFurnaceMap;
};

} // namespace highlight
