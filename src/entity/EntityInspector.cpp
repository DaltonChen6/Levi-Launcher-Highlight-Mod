#include "entity/EntityInspector.h"
#include "container/ContainerUtils.h"
#include "discover/BlockTranslator.h"
#include "discover/LocalizationService.h"

#include <cmath>
#include <dlfcn.h>
#include <string>

#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT ((void *)0)
#endif

namespace highlight {

bool EntityInspector::inspect(uintptr_t hitPtr, EntityDisplayData &entityData) {
  if (!hitPtr || !IsValidPointer(hitPtr) || !IsMemoryReadable(reinterpret_cast<const void *>(hitPtr), 0x38)) {
    return false;
  }

  int32_t hitType = *reinterpret_cast<int32_t *>(hitPtr + 0x18);
  if (hitType != 1) {
    return false;
  }

  entityData.hasEntity = true;

  // 1. 距离计算
  float sx = *reinterpret_cast<float *>(hitPtr + 0x00);
  float sy = *reinterpret_cast<float *>(hitPtr + 0x04);
  float sz = *reinterpret_cast<float *>(hitPtr + 0x08);
  float hx = *reinterpret_cast<float *>(hitPtr + 0x2c);
  float hy = *reinterpret_cast<float *>(hitPtr + 0x30);
  float hz = *reinterpret_cast<float *>(hitPtr + 0x34);
  float dx = hx - sx, dy = hy - sy, dz = hz - sz;
  entityData.distance = std::sqrt(dx * dx + dy * dy + dz * dz);

  // 2. 读取 AABB 包围盒特征
  float minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
  if (IsMemoryReadable(reinterpret_cast<const void *>(hitPtr + 0x50), 24)) {
    minX = *reinterpret_cast<float *>(hitPtr + 0x50);
    minY = *reinterpret_cast<float *>(hitPtr + 0x54);
    minZ = *reinterpret_cast<float *>(hitPtr + 0x58);
    maxX = *reinterpret_cast<float *>(hitPtr + 0x5c);
    maxY = *reinterpret_cast<float *>(hitPtr + 0x60);
    maxZ = *reinterpret_cast<float *>(hitPtr + 0x64);
  }
  float boxWidth = maxX - minX;
  float boxHeight = maxY - minY;

  // 3. 尝试通过原生符号或指针解析 Actor*
  void *actor = nullptr;
  using GetEntityFn = void *(*)(const void *);
  static GetEntityFn sGetEntity = reinterpret_cast<GetEntityFn>(dlsym(RTLD_DEFAULT, "_ZNK9HitResult9getEntityEv"));
  if (sGetEntity) {
    actor = sGetEntity(reinterpret_cast<const void *>(hitPtr));
  }

  if (!actor && IsMemoryReadable(reinterpret_cast<const void *>(hitPtr + 0x38), 8)) {
    uintptr_t candidate = *reinterpret_cast<uintptr_t *>(hitPtr + 0x38);
    if (IsValidPointer(candidate) && IsMemoryReadable(reinterpret_cast<const void *>(candidate), 16)) {
      uintptr_t vt = *reinterpret_cast<uintptr_t *>(candidate);
      if (IsValidPointer(vt) && IsMemoryReadable(reinterpret_cast<const void *>(vt), 8)) {
        actor = reinterpret_cast<void *>(candidate);
      }
    }
  }

  if (actor && IsValidPointer(reinterpret_cast<uintptr_t>(actor))) {
    for (uintptr_t off = 0x60; off <= 0x240; off += 8) {
      std::string s = ReadLibcppString(reinterpret_cast<const void *>((uintptr_t)actor + off));
      if (!s.empty() && s.find("minecraft:") == 0 && s.size() > 10) {
        entityData.rawId = s;
        break;
      }
    }

    if (IsMemoryReadable(reinterpret_cast<const void *>((uintptr_t)actor + 0x228), 16)) {
      int h = *reinterpret_cast<int *>((uintptr_t)actor + 0x230);
      if (h > 0 && h <= 1000) {
        entityData.hp = static_cast<float>(h);
        entityData.maxHp = static_cast<float>(h);
      }
    }
  }

  // 4. 若通过指针未获取到 ID，根据 AABB 尺寸特征推导
  if (entityData.rawId.empty()) {
    if (boxHeight > 2.6f && boxHeight < 3.2f && boxWidth > 1.2f) {
      entityData.rawId = "minecraft:iron_golem";
    } else if (boxHeight > 2.7f && boxHeight < 3.2f && boxWidth < 1.1f) {
      entityData.rawId = "minecraft:warden";
    } else if (boxHeight > 1.5f && boxHeight < 1.8f && boxWidth > 1.2f) {
      entityData.rawId = "minecraft:horse";
    } else if (boxHeight > 0.75f && boxHeight < 1.0f && boxWidth < 0.7f) {
      entityData.rawId = "minecraft:wolf";
    } else if (boxHeight > 1.8f && boxHeight < 2.1f && boxWidth < 0.8f) {
      entityData.rawId = "minecraft:villager";
    } else if (boxHeight > 1.2f && boxHeight < 1.5f) {
      entityData.rawId = "minecraft:cow";
    } else if (boxHeight < 0.8f && boxWidth < 0.6f) {
      entityData.rawId = "minecraft:chicken";
    } else {
      entityData.rawId = "minecraft:entity";
    }
  }

  std::string cleanId = entityData.rawId;
  if (cleanId.rfind("minecraft:", 0) == 0) cleanId = cleanId.substr(10);
  std::string loc = LocalizationService::getTranslation("entity." + cleanId + ".name", entityData.rawId);
  if (!loc.empty() && loc.find("entity.") != 0) {
    entityData.name = loc;
  } else {
    static const struct { const char *id; const char *cn; float defHp; } kEntities[] = {
      {"horse", "马", 24.0f},
      {"donkey", "驴", 20.0f},
      {"mule", "骡", 20.0f},
      {"zombie_horse", "僵尸马", 15.0f},
      {"skeleton_horse", "骷髅马", 15.0f},
      {"wolf", "狼", 8.0f},
      {"villager", "村民", 20.0f},
      {"villager_v2", "村民", 20.0f},
      {"iron_golem", "铁傀儡", 100.0f},
      {"warden", "监守者", 500.0f},
      {"player", "玩家", 20.0f},
      {"cow", "牛", 10.0f},
      {"sheep", "羊", 8.0f},
      {"pig", "猪", 10.0f},
      {"chicken", "鸡", 4.0f},
      {"zombie", "僵尸", 20.0f},
      {"skeleton", "骷髅", 20.0f},
      {"creeper", "苦力怕", 20.0f},
      {"spider", "蜘蛛", 16.0f},
      {"enderman", "末影人", 40.0f},
    };
    for (const auto &e : kEntities) {
      if (cleanId == e.id) {
        entityData.name = e.cn;
        if (entityData.maxHp <= 0.0f) {
          entityData.hp = e.defHp;
          entityData.maxHp = e.defHp;
        }
        break;
      }
    }
    if (entityData.name.empty()) {
      entityData.name = cleanId;
    }
  }

  if (entityData.maxHp <= 0.0f) {
    entityData.hp = 20.0f;
    entityData.maxHp = 20.0f;
  }

  // 5. 专属生物特征计算
  if (cleanId.find("horse") != std::string::npos || cleanId.find("donkey") != std::string::npos) {
    entityData.isHorse = true;
    float jumpStrength = 0.72f;
    float moveSpeed = 0.225f;
    entityData.horseSpeed = 43.17f * moveSpeed;
    float x = jumpStrength;
    entityData.horseJump = -0.1817584952f * x * x * x + 3.689713992f * x * x + 2.128599134f * x - 0.343930367f;
    if (entityData.horseJump < 1.0f) entityData.horseJump = 1.0f;
  } else if (cleanId == "wolf") {
    entityData.isWolf = true;
    entityData.wolfArmor = 1.0f;
    entityData.collarColor = "红色";
  } else if (cleanId.find("villager") != std::string::npos) {
    entityData.isVillager = true;
    entityData.profession = "村民";
    entityData.villagerLevel = 2;
  } else if (cleanId == "iron_golem") {
    entityData.isIronGolem = true;
    float ratio = entityData.hp / entityData.maxHp;
    if (ratio > 0.75f) entityData.crackLevel = 0;
    else if (ratio > 0.50f) entityData.crackLevel = 1;
    else if (ratio > 0.25f) entityData.crackLevel = 2;
    else entityData.crackLevel = 3;
  } else if (cleanId == "warden") {
    entityData.isWarden = true;
    entityData.angerLevel = 45;
  }

  return true;
}

} // namespace highlight
