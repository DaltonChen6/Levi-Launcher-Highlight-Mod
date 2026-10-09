#include "render/HudRenderer.h"
#include "render/FontManager.h"
#include "render/HudCardRenderer.h"

#include <android/log.h>
#include <dlfcn.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include <pl/memory/Hook.hpp>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"

namespace highlight {
namespace {

using EglSwapBuffersFn = EGLBoolean (*)(EGLDisplay, EGLSurface);
using EglSwapBuffersDamageFn = EGLBoolean (*)(EGLDisplay, EGLSurface,
                                              const EGLint *, EGLint);

EglSwapBuffersFn gOrigSwapBuffers = nullptr;
void *gSwapBuffersTarget = nullptr;
EglSwapBuffersDamageFn gOrigSwapBuffersDamage = nullptr;
void *gSwapBuffersDamageTarget = nullptr;
bool gHooked = false;
bool gImguiReady = false;

std::mutex gRenderMutex;
std::mutex gTextMutex;
std::string gHudText;
ContainerDisplayData gContainerData;
EntityDisplayData gEntityData;

std::mutex gPickupMutex;
std::vector<PickupRenderInfo> gPickupItems;

void RenderHudFrame() {
  std::unique_lock<std::mutex> renderLock(gRenderMutex, std::try_to_lock);
  if (!renderLock.owns_lock()) {
    return;
  }

  std::string text;
  ContainerDisplayData containerData;
  EntityDisplayData entityData;
  {
    std::lock_guard<std::mutex> lock(gTextMutex);
    text = gHudText;
    containerData = gContainerData;
    entityData = gEntityData;
  }

  std::vector<PickupRenderInfo> pickups;
  {
    std::lock_guard<std::mutex> lock(gPickupMutex);
    static auto sLastPickupTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - sLastPickupTime).count();
    sLastPickupTime = now;
    if (dt > 0.1f) dt = 0.016f;

    for (auto it = gPickupItems.begin(); it != gPickupItems.end();) {
      it->age += dt;
      if (it->age >= 3.5f) {
        it = gPickupItems.erase(it);
      } else {
        ++it;
      }
    }
    pickups = gPickupItems;
  }

  if (text.empty() && !containerData.hasContainer && !entityData.hasEntity && pickups.empty()) {
    return;
  }

  if (!gImguiReady) {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    FontManager::setupFont(io);

    if (ImGui_ImplOpenGL3_Init()) {
      gImguiReady = true;
      __android_log_print(ANDROID_LOG_INFO, "VH", "imgui inited with font");
    } else {
      __android_log_print(ANDROID_LOG_ERROR, "VH", "imgui init failed");
    }
  }
  if (!gImguiReady) {
    return;
  }

  ImGuiIO &io = ImGui::GetIO();
  EGLDisplay d = eglGetCurrentDisplay();
  EGLSurface s = eglGetCurrentSurface(EGL_DRAW);
  EGLint w = 0, h = 0;
  if (d != EGL_NO_DISPLAY && s != EGL_NO_SURFACE) {
    eglQuerySurface(d, s, EGL_WIDTH, &w);
    eglQuerySurface(d, s, EGL_HEIGHT, &h);
  }
  if (w <= 0 || h <= 0) {
    w = 1080;
    h = 2400;
  }
  io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
  io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

  GLint lastFbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &lastFbo);
  if (lastFbo != 0) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();

  static auto sAppStartTime = std::chrono::steady_clock::now();
  float timeSec = std::chrono::duration<float>(std::chrono::steady_clock::now() - sAppStartTime).count();

  // 渲染主 HUD 卡片
  HudCardRenderer::renderMainCard(text, containerData, entityData, timeSec);

  // 渲染拾取动画卡片
  HudCardRenderer::renderPickups(pickups, io.DisplaySize.x, io.DisplaySize.y, timeSec);

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  if (lastFbo != 0) {
    glBindFramebuffer(GL_FRAMEBUFFER, lastFbo);
  }
}

EGLBoolean SwapBuffersDetour(EGLDisplay dpy, EGLSurface surface) {
  RenderHudFrame();
  if (gOrigSwapBuffers) {
    return gOrigSwapBuffers(dpy, surface);
  }
  return EGL_FALSE;
}

EGLBoolean SwapBuffersWithDamageDetour(EGLDisplay dpy, EGLSurface surface,
                                      const EGLint *rects, EGLint n_rects) {
  RenderHudFrame();
  if (gOrigSwapBuffersDamage) {
    return gOrigSwapBuffersDamage(dpy, surface, rects, n_rects);
  }
  return EGL_FALSE;
}

} // namespace

void SetHudText(const std::string &text) {
  std::lock_guard<std::mutex> lock(gTextMutex);
  gHudText = text;
}

void SetHudContainerData(const ContainerDisplayData &data) {
  std::lock_guard<std::mutex> lock(gTextMutex);
  gContainerData = data;
}

void SetHudEntityData(const EntityDisplayData &data) {
  std::lock_guard<std::mutex> lock(gTextMutex);
  gEntityData = data;
}

void PushPickupItem(const std::string &name, int count) {
  if (name.empty() || count <= 0) return;
  std::lock_guard<std::mutex> lock(gPickupMutex);
  for (auto &it : gPickupItems) {
    if (it.name == name) {
      it.count += count;
      it.age = 0.0f;
      return;
    }
  }
  gPickupItems.push_back(PickupRenderInfo{name, count, 0.0f});
  if (gPickupItems.size() > 6) {
    gPickupItems.erase(gPickupItems.begin());
  }
}

bool InitHudRenderer() {
  if (gHooked) {
    return true;
  }

  void *libEgl = dlopen("libEGL.so", RTLD_NOW | RTLD_NOLOAD);
  if (!libEgl) {
    libEgl = dlopen("libEGL.so", RTLD_NOW);
  }
  if (!libEgl) {
    __android_log_print(ANDROID_LOG_ERROR, "VH", "dlopen(libEGL.so) failed");
    return false;
  }

  void *sym = dlsym(libEgl, "eglSwapBuffers");
  if (!sym) {
    __android_log_print(ANDROID_LOG_ERROR, "VH", "dlsym(eglSwapBuffers) failed");
    return false;
  }
  gSwapBuffersTarget = sym;

  int res = pl::memory::hook(
      reinterpret_cast<pl::memory::FuncPtr>(gSwapBuffersTarget),
      reinterpret_cast<pl::memory::FuncPtr>(SwapBuffersDetour),
      reinterpret_cast<pl::memory::FuncPtr *>(&gOrigSwapBuffers));
  if (res != 0) {
    __android_log_print(ANDROID_LOG_ERROR, "VH", "pl::memory::hook(eglSwapBuffers) failed: %d", res);
    return false;
  }
  __android_log_print(ANDROID_LOG_INFO, "VH", "eglSwapBuffers hooked successfully");

  void *damageSym = dlsym(libEgl, "eglSwapBuffersWithDamageKHR");
  if (damageSym) {
    gSwapBuffersDamageTarget = damageSym;
    int damageRes = pl::memory::hook(
        reinterpret_cast<pl::memory::FuncPtr>(gSwapBuffersDamageTarget),
        reinterpret_cast<pl::memory::FuncPtr>(SwapBuffersWithDamageDetour),
        reinterpret_cast<pl::memory::FuncPtr *>(&gOrigSwapBuffersDamage));
    if (damageRes == 0) {
      __android_log_print(ANDROID_LOG_INFO, "VH", "eglSwapBuffersWithDamageKHR hooked successfully");
    }
  }

  gHooked = true;
  return true;
}

void ShutdownHudRenderer() {
  if (gHooked) {
    if (gSwapBuffersTarget && gOrigSwapBuffers) {
      pl::memory::unhook(gSwapBuffersTarget, reinterpret_cast<pl::memory::FuncPtr>(SwapBuffersDetour));
      gSwapBuffersTarget = nullptr;
      gOrigSwapBuffers = nullptr;
    }
    if (gSwapBuffersDamageTarget && gOrigSwapBuffersDamage) {
      pl::memory::unhook(gSwapBuffersDamageTarget, reinterpret_cast<pl::memory::FuncPtr>(SwapBuffersWithDamageDetour));
      gSwapBuffersDamageTarget = nullptr;
      gOrigSwapBuffersDamage = nullptr;
    }
    gHooked = false;
  }
  if (gImguiReady) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    gImguiReady = false;
  }
}

} // namespace highlight
