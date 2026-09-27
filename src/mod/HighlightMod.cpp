#include "mod/HighlightMod.h"
#include "render/HudRenderer.h"
#include "discover/LocalizationService.h"

#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

#include <pl/memory/Hook.hpp>
#include <pl/memory/Vtable.hpp>
#include <pl/ModMenu.hpp>

namespace highlight {
namespace {

std::atomic<bool> gModEnabled{true};
std::atomic<HighlightMod *> gMod{nullptr};

uintptr_t GetModuleBase(const char *moduleName) {
  FILE *maps = std::fopen("/proc/self/maps", "r");
  if (!maps) {
    return 0;
  }
  char line[512];
  uintptr_t base = 0;
  while (std::fgets(line, sizeof(line), maps)) {
    if (std::strstr(line, moduleName)) {
      uintptr_t start = 0, end = 0;
      char perms[8] = {};
      if (std::sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %7s", &start, &end, perms) == 3) {
        base = start;
        break;
      }
    }
  }
  std::fclose(maps);
  return base;
}

// ---- Waila 原生钩子体系 (ClientInstance 与 GameMode) ----
using ClientInstanceUpdateFn = void (*)(void *self);
ClientInstanceUpdateFn gOrigClientInstanceUpdate = nullptr;
void *gClientInstanceUpdateTarget = nullptr;

void ClientInstanceUpdateDetour(void *self) {
  if (gOrigClientInstanceUpdate) {
    gOrigClientInstanceUpdate(self);
  }
  HighlightMod *mod = gMod.load(std::memory_order_relaxed);
  if (mod && self) {
    mod->onClientUpdate(self);
  }
}

using StartDestroyBlockFn = bool (*)(void *self, const int32_t *pos, uint8_t face, bool &destroyed);
StartDestroyBlockFn gOrigStartDestroyBlock = nullptr;
void *gStartDestroyTarget = nullptr;

using ContinueDestroyBlockFn = bool (*)(void *self, const int32_t *pos, uint8_t face, const float *playerPos, bool &destroyed);
ContinueDestroyBlockFn gOrigContinueDestroyBlock = nullptr;
void *gContinueDestroyTarget = nullptr;

using StopDestroyBlockFn = void (*)(void *self, const int32_t *pos);
StopDestroyBlockFn gOrigStopDestroyBlock = nullptr;
void *gStopDestroyTarget = nullptr;

bool StartDestroyBlockDetour(void *self, const int32_t *pos, uint8_t face, bool &destroyed) {
  bool ret = gOrigStartDestroyBlock ? gOrigStartDestroyBlock(self, pos, face, destroyed) : false;
  HighlightMod *mod = gMod.load(std::memory_order_relaxed);
  if (mod && self) {
    mod->mDiscovery.onStartDestroyBlock(self, pos, face);
  }
  return ret;
}

bool ContinueDestroyBlockDetour(void *self, const int32_t *pos, uint8_t face, const float *playerPos, bool &destroyed) {
  bool ret = gOrigContinueDestroyBlock ? gOrigContinueDestroyBlock(self, pos, face, playerPos, destroyed) : false;
  HighlightMod *mod = gMod.load(std::memory_order_relaxed);
  if (mod && self) {
    mod->mDiscovery.onContinueDestroyBlock(self, pos, face, playerPos);
  }
  return ret;
}

void StopDestroyBlockDetour(void *self, const int32_t *pos) {
  if (gOrigStopDestroyBlock) {
    gOrigStopDestroyBlock(self, pos);
  }
  HighlightMod *mod = gMod.load(std::memory_order_relaxed);
  if (mod && self) {
    mod->mDiscovery.onStopDestroyBlock(self, pos);
  }
}

} // namespace

HighlightMod &HighlightMod::instance() {
  static HighlightMod instance;
  return instance;
}

HighlightMod::HighlightMod() : mSelf(*ll::mod::NativeMod::current()) {}

bool HighlightMod::load() {
  getSelf().getLogger().info("VisualHighlight load");
  return true;
}

bool HighlightMod::enable() {
  auto &self = getSelf();
  mBase = reinterpret_cast<void *>(GetModuleBase("libminecraftpe.so"));
  if (!mBase) {
    self.getLogger().error("libminecraftpe.so base not found");
    return true;
  }
  self.getLogger().info("mc-core base = {}", fmt::ptr(mBase));
  gMod.store(this, std::memory_order_relaxed);

  // 1. 安装 Waila 原生 ClientInstance 钩子 (每帧驱动准星 HitResult 检测与 HUD 更新)
  uintptr_t ciUpdate = pl::memory::resolveVtableFunction("14ClientInstance", 25, "libminecraftpe.so");
  if (ciUpdate) {
    gClientInstanceUpdateTarget = reinterpret_cast<void *>(ciUpdate);
    int rc = pl::memory::hook(gClientInstanceUpdateTarget,
                              reinterpret_cast<pl::memory::FuncPtr>(ClientInstanceUpdateDetour),
                              reinterpret_cast<pl::memory::FuncPtr *>(&gOrigClientInstanceUpdate));
    __android_log_print(ANDROID_LOG_INFO, "VH", "hooked ClientInstance::update @ %p rc=%d", gClientInstanceUpdateTarget, rc);
  } else {
    __android_log_print(ANDROID_LOG_ERROR, "VH", "failed to resolve ClientInstance::update");
  }

  // 2. 安装 GameMode 破坏事件钩子 (精确获取破坏中方块的挖掘进度)
  uintptr_t startDestroy = pl::memory::resolveVtableFunction("8GameMode", 2, "libminecraftpe.so");
  if (startDestroy) {
    gStartDestroyTarget = reinterpret_cast<void *>(startDestroy);
    pl::memory::hook(gStartDestroyTarget,
                     reinterpret_cast<pl::memory::FuncPtr>(StartDestroyBlockDetour),
                     reinterpret_cast<pl::memory::FuncPtr *>(&gOrigStartDestroyBlock));
    __android_log_print(ANDROID_LOG_INFO, "VH", "hooked GameMode::startDestroyBlock @ %p", gStartDestroyTarget);
  }

  uintptr_t contDestroy = pl::memory::resolveVtableFunction("8GameMode", 4, "libminecraftpe.so");
  if (contDestroy) {
    gContinueDestroyTarget = reinterpret_cast<void *>(contDestroy);
    pl::memory::hook(gContinueDestroyTarget,
                     reinterpret_cast<pl::memory::FuncPtr>(ContinueDestroyBlockDetour),
                     reinterpret_cast<pl::memory::FuncPtr *>(&gOrigContinueDestroyBlock));
    __android_log_print(ANDROID_LOG_INFO, "VH", "hooked GameMode::continueDestroyBlock @ %p", gContinueDestroyTarget);
  }

  uintptr_t stopDestroy = pl::memory::resolveVtableFunction("8GameMode", 5, "libminecraftpe.so");
  if (stopDestroy) {
    gStopDestroyTarget = reinterpret_cast<void *>(stopDestroy);
    pl::memory::hook(gStopDestroyTarget,
                     reinterpret_cast<pl::memory::FuncPtr>(StopDestroyBlockDetour),
                     reinterpret_cast<pl::memory::FuncPtr *>(&gOrigStopDestroyBlock));
    __android_log_print(ANDROID_LOG_INFO, "VH", "hooked GameMode::stopDestroyBlock @ %p", gStopDestroyTarget);
  }

  // 3. 安装设备原生多语言本地化引擎 (动态拦截 8I18nImpl，优先获取玩家自选语言与材质包自定义译名)
  bool locOk = LocalizationService::init();
  __android_log_print(ANDROID_LOG_INFO, "VH", "localization service init = %d", locOk);

  // 4. 初始化 2D ImGui HUD 浮窗渲染器 (挂载到 EGL 交换链)
  bool hudOk = InitHudRenderer();
  __android_log_print(ANDROID_LOG_INFO, "VH", "hud renderer init = %d", hudOk);

  // 5. 注册到微风启动器外部模组管理菜单
  pl::modmenu::ModuleBuilder builder("visual_highlight", "可视化高亮显示");
  builder.modId(self.getId());
  builder.description("准星对准方块时在屏幕顶部居中显示方块信息与挖掘进度");
  builder.defaultEnabled(true);
  builder.hideInHudEditor(true); // 隐藏在 HUD 编辑器中，避免占用或遮挡其他 HUD UI
  builder.onToggle([](std::string_view, bool enabled) {
    gModEnabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
      SetHudText("");
    }
    __android_log_print(ANDROID_LOG_INFO, "VH", "mod enabled toggled: %d", enabled);
  });
  bool regOk = builder.registerModule();
  __android_log_print(ANDROID_LOG_INFO, "VH", "modmenu registerModule = %d", regOk);
  if (regOk) {
    pl::modmenu::setModuleEnabled("visual_highlight", true);
  }

  return true;
}

void HighlightMod::onClientUpdate(void *ci) {
  if (!gModEnabled.load(std::memory_order_relaxed)) {
    SetHudText("");
    return;
  }
  mDiscovery.onClientUpdate(ci);
}

bool HighlightMod::disable() {
  pl::modmenu::unregisterModule("visual_highlight");
  ShutdownHudRenderer();
  if (gClientInstanceUpdateTarget && gOrigClientInstanceUpdate) {
    pl::memory::unhook(gClientInstanceUpdateTarget, reinterpret_cast<pl::memory::FuncPtr>(&ClientInstanceUpdateDetour));
    gClientInstanceUpdateTarget = nullptr;
    gOrigClientInstanceUpdate = nullptr;
  }
  if (gStartDestroyTarget && gOrigStartDestroyBlock) {
    pl::memory::unhook(gStartDestroyTarget, reinterpret_cast<pl::memory::FuncPtr>(&StartDestroyBlockDetour));
    gStartDestroyTarget = nullptr;
    gOrigStartDestroyBlock = nullptr;
  }
  if (gContinueDestroyTarget && gOrigContinueDestroyBlock) {
    pl::memory::unhook(gContinueDestroyTarget, reinterpret_cast<pl::memory::FuncPtr>(&ContinueDestroyBlockDetour));
    gContinueDestroyTarget = nullptr;
    gOrigContinueDestroyBlock = nullptr;
  }
  if (gStopDestroyTarget && gOrigStopDestroyBlock) {
    pl::memory::unhook(gStopDestroyTarget, reinterpret_cast<pl::memory::FuncPtr>(&StopDestroyBlockDetour));
    gStopDestroyTarget = nullptr;
    gOrigStopDestroyBlock = nullptr;
  }

  LocalizationService::shutdown();

  gMod.store(nullptr, std::memory_order_relaxed);
  return true;
}

bool HighlightMod::unload() {
  getSelf().getLogger().info("VisualHighlight unload");
  return true;
}

} // namespace highlight
