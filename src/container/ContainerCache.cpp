#include "container/ContainerCache.h"
#include "container/ContainerUtils.h"

namespace highlight {

ContainerCache &ContainerCache::get() {
  static ContainerCache sInstance;
  return sInstance;
}

bool ContainerCache::getContainer(int64_t posKey, ContainerCacheEntry &outEntry) {
  std::lock_guard<std::mutex> lock(mCacheMutex);
  auto it = mContainerCache.find(posKey);
  if (it != mContainerCache.end()) {
    outEntry = it->second;
    return true;
  }
  return false;
}

void ContainerCache::setContainer(int64_t posKey, const ContainerCacheEntry &entry) {
  std::lock_guard<std::mutex> lock(mCacheMutex);
  mContainerCache[posKey] = entry;
}

void ContainerCache::setDoubleChest(int32_t x, int32_t y, int32_t z, const ContainerCacheEntry &entry) {
  std::lock_guard<std::mutex> lock(mCacheMutex);
  // 只写入中心坐标，坚决不向周围 4 邻域扩散污染
  int64_t centerKey = PosToKey(x, y, z);
  mContainerCache[centerKey] = entry;
}

bool ContainerCache::getFurnace(int64_t posKey, FurnaceSimState &outSim) {
  std::lock_guard<std::mutex> lock(mFurnaceMutex);
  auto it = mFurnaceMap.find(posKey);
  if (it != mFurnaceMap.end()) {
    outSim = it->second;
    return true;
  }
  return false;
}

void ContainerCache::setFurnace(int64_t posKey, const FurnaceSimState &sim) {
  std::lock_guard<std::mutex> lock(mFurnaceMutex);
  mFurnaceMap[posKey] = sim;
}

void ContainerCache::updateFurnace(int64_t posKey, const std::function<void(FurnaceSimState &)> &updater) {
  std::lock_guard<std::mutex> lock(mFurnaceMutex);
  updater(mFurnaceMap[posKey]);
}

void ContainerCache::clear() {
  {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mContainerCache.clear();
  }
  {
    std::lock_guard<std::mutex> lock(mFurnaceMutex);
    mFurnaceMap.clear();
  }
}

} // namespace highlight
