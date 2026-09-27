#include "discover/LocalizationService.h"
#include "container/ContainerUtils.h"
#include "discover/LocalizationTree.h"

#include <android/log.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <pl/memory/Hook.hpp>
#include <pl/memory/Vtable.hpp>

namespace highlight {
namespace {

std::atomic<void *> gI18nInstance{nullptr};
void *gSlot15Target = nullptr;
void *gOrigSlot15 = nullptr;
bool gHookInstalled = false;

std::atomic<void *> gCachedLocalization{nullptr};
std::string gCachedLanguageCode;

void *HookSlot15Detour(void *self, void *a1, void *a2, void *a3) {
  if (self && !gI18nInstance.load(std::memory_order_relaxed)) {
    gI18nInstance.store(self, std::memory_order_relaxed);
    __android_log_print(ANDROID_LOG_INFO, "VH_I18n", "Captured I18n singleton: %p", self);
  }
  auto orig = reinterpret_cast<void *(*)(void *, void *, void *, void *)>(gOrigSlot15);
  if (orig) {
    return orig(self, a1, a2, a3);
  }
  return nullptr;
}

#if defined(__aarch64__)
void CallGetLocalization(uintptr_t fnSlot24, void *self, const void *pLang, void *pOutSptr) {
  register void *r_out __asm__("x8") = pOutSptr;
  register void *r_self __asm__("x0") = self;
  register const void *r_lang __asm__("x1") = pLang;
  register uintptr_t r_fn __asm__("x9") = fnSlot24;
  asm volatile(
      "blr %3\n"
      : "+r"(r_out), "+r"(r_self), "+r"(r_lang)
      : "r"(r_fn)
      : "x2", "x3", "x4", "x5", "x6", "x7", "x10", "x11", "x12",
        "x13", "x14", "x15", "x16", "x17", "x30", "memory"
  );
}
#endif

void *GetOrUpdateLocalization() {
  void *cached = gCachedLocalization.load(std::memory_order_relaxed);
  if (cached) {
    if (IsValidPointer(reinterpret_cast<uintptr_t>(cached)) &&
        IsMemoryReadable(cached, 0x50)) {
      return cached;
    }
    gCachedLocalization.store(nullptr, std::memory_order_relaxed);
  }

  void *i18n = gI18nInstance.load(std::memory_order_relaxed);
  if (!i18n || !IsValidPointer(reinterpret_cast<uintptr_t>(i18n)) ||
      !IsMemoryReadable(i18n, 8)) {
    return nullptr;
  }

  uintptr_t *vt = *reinterpret_cast<uintptr_t **>(i18n);
  if (!vt || !IsValidPointer(reinterpret_cast<uintptr_t>(vt)) ||
      !IsMemoryReadable(vt, 25 * sizeof(uintptr_t))) {
    return nullptr;
  }

  uintptr_t fnSlot24 = vt[24];
  if (!fnSlot24 || !IsValidPointer(fnSlot24)) {
    return nullptr;
  }

#if defined(__aarch64__)
  alignas(16) uintptr_t sptrBuf[2] = {0, 0};
  std::string fallbackLang = "en_US";
  CallGetLocalization(fnSlot24, i18n, &fallbackLang, sptrBuf);

  uintptr_t locPtr = sptrBuf[0];
  if (locPtr && IsValidPointer(locPtr) &&
      IsMemoryReadable(reinterpret_cast<const void *>(locPtr), 0x50)) {
    void *newLoc = reinterpret_cast<void *>(locPtr);
    gCachedLocalization.store(newLoc, std::memory_order_relaxed);
    gCachedLanguageCode = LocalizationTree::readAsciiString(reinterpret_cast<const uint8_t *>(newLoc) + 0x20);
    return newLoc;
  }
#endif
  return nullptr;
}

} // namespace

bool LocalizationService::init() {
  if (gHookInstalled) return true;

  uintptr_t fnSlot15 = pl::memory::resolveVtableFunction("8I18nImpl", 15, "libminecraftpe.so");
  if (!fnSlot15) {
    __android_log_print(ANDROID_LOG_WARN, "VH_I18n",
                        "resolveVtableFunction(8I18nImpl, 15) returned 0 (will use offline dictionary)");
    return false;
  }

  gSlot15Target = reinterpret_cast<void *>(fnSlot15);
  int res = pl::memory::hook(
      reinterpret_cast<pl::memory::FuncPtr>(gSlot15Target),
      reinterpret_cast<pl::memory::FuncPtr>(HookSlot15Detour),
      reinterpret_cast<pl::memory::FuncPtr *>(&gOrigSlot15));
  if (res == 0) {
    gHookInstalled = true;
    __android_log_print(ANDROID_LOG_INFO, "VH_I18n",
                        "Successfully hooked 8I18nImpl slot 15 at %p", gSlot15Target);
    return true;
  }

  __android_log_print(ANDROID_LOG_WARN, "VH_I18n", "Failed to hook 8I18nImpl slot 15, error: %d", res);
  return false;
}

std::string LocalizationService::getTranslation(const std::string &descId, const std::string &rawId) {
  void *pLoc = GetOrUpdateLocalization();
  if (!pLoc) return {};

  std::vector<std::string> candidates;
  if (!descId.empty()) {
    if (descId.find('.') != std::string::npos) {
      candidates.push_back(descId);
      if (descId.size() > 5 && descId.substr(descId.size() - 5) == ".name") {
        candidates.push_back(descId.substr(0, descId.size() - 5));
      } else {
        candidates.push_back(descId + ".name");
      }
    } else {
      candidates.push_back("tile." + descId + ".name");
      candidates.push_back("item." + descId + ".name");
    }
  }

  if (!rawId.empty()) {
    std::string clean = rawId;
    if (clean.rfind("minecraft:", 0) == 0) {
      clean = clean.substr(10);
    }
    candidates.push_back("tile." + clean + ".name");
    candidates.push_back("item." + clean + ".name");
    candidates.push_back("entity." + clean + ".name");
  }

  for (const auto &key : candidates) {
    std::string result = LocalizationTree::searchTree(pLoc, key);
    if (!result.empty()) return result;
  }
  return {};
}

std::string LocalizationService::getTranslationForItem(const std::string &rawId) {
  void *pLoc = GetOrUpdateLocalization();
  if (!pLoc || rawId.empty()) return {};

  std::string clean = rawId;
  if (clean.rfind("minecraft:", 0) == 0) {
    clean = clean.substr(10);
  }

  std::vector<std::string> candidates = {
      "item." + clean + ".name",
      "tile." + clean + ".name",
      clean + ".name",
      clean
  };

  for (const auto &key : candidates) {
    std::string result = LocalizationTree::searchTree(pLoc, key);
    if (!result.empty()) return result;
  }
  return {};
}

std::string LocalizationService::getExactTranslation(const std::string &exactKey) {
  void *pLoc = GetOrUpdateLocalization();
  if (!pLoc || exactKey.empty()) return {};
  return LocalizationTree::searchTree(pLoc, exactKey);
}

void LocalizationService::shutdown() {
  gHookInstalled = false;
  gI18nInstance.store(nullptr, std::memory_order_relaxed);
  gCachedLocalization.store(nullptr, std::memory_order_relaxed);
}

} // namespace highlight
