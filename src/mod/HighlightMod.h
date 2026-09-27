#pragma once

#include <atomic>
#include <vector>

#include <pl/Mod.hpp>
#include <pl/memory/Hook.hpp>

#include "discover/Discovery.h"

namespace highlight {

class HighlightMod {
public:
  static HighlightMod &instance();

  HighlightMod();

  [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

  bool load();
  bool enable();
  bool disable();
  bool unload();

  void onClientUpdate(void *ci);

  void *mBase = nullptr;
  Discovery mDiscovery;

private:
  ll::mod::NativeMod &mSelf;
};

} // namespace highlight
