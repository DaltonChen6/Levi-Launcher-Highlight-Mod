#include "render/HudCardRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace highlight {

ImVec4 HudCardRenderer::getQualityColor(const std::string &name, float timeSec) {
  // 神话 (Mythic): 下界合金 / 潮涌 / 重锤 / 不死图腾 -> 全光谱平滑彩虹流光
  if (name.find("下界合金") != std::string::npos || name.find("netherite") != std::string::npos ||
      name.find("重锤") != std::string::npos || name.find("mace") != std::string::npos ||
      name.find("不死图腾") != std::string::npos || name.find("totem") != std::string::npos) {
    float hue = std::fmod(timeSec * 0.4f, 1.0f);
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(hue, 0.85f, 1.0f, r, g, b);
    return ImVec4(r, g, b, 1.0f);
  }
  // 传奇 (Legendary): 附魔金苹果 / 远古残骸 / 鞘翅 / 龙蛋 -> 灿金色
  if (name.find("附魔金苹果") != std::string::npos || name.find("远古残骸") != std::string::npos ||
      name.find("鞘翅") != std::string::npos || name.find("elytra") != std::string::npos) {
    return ImVec4(1.0f, 0.72f, 0.12f, 1.0f);
  }
  // 史诗 (Epic): 钻石系列 / 紫水晶 / 回响碎片 -> 霓虹紫
  if (name.find("钻石") != std::string::npos || name.find("diamond") != std::string::npos ||
      name.find("紫水晶") != std::string::npos || name.find("amethyst") != std::string::npos) {
    return ImVec4(0.95f, 0.38f, 0.98f, 1.0f);
  }
  // 稀有 (Rare): 青金石 / 红石 / 绿宝石 / 金锭 -> 水青海蓝
  if (name.find("金") != std::string::npos || name.find("gold") != std::string::npos ||
      name.find("绿宝石") != std::string::npos || name.find("emerald") != std::string::npos ||
      name.find("红石") != std::string::npos || name.find("redstone") != std::string::npos) {
    return ImVec4(0.35f, 0.85f, 1.0f, 1.0f);
  }
  // 罕见 (Uncommon): 铁 / 铜 / 煤炭 -> 翠绿荧光
  if (name.find("铁") != std::string::npos || name.find("iron") != std::string::npos ||
      name.find("铜") != std::string::npos || name.find("copper") != std::string::npos ||
      name.find("煤") != std::string::npos || name.find("coal") != std::string::npos) {
    return ImVec4(0.45f, 0.95f, 0.45f, 1.0f);
  }
  return ImVec4(0.92f, 0.93f, 0.95f, 1.0f);
}

void HudCardRenderer::renderMainCard(const std::string &text,
                                     const ContainerDisplayData &containerData,
                                     const EntityDisplayData &entityData,
                                     float timeSec) {
  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.14f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.0f));
  ImGui::SetNextWindowBgAlpha(0.75f);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.12f, 0.85f));
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.55f, 0.95f, 0.60f));

  ImGui::Begin("VH_HUD", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoFocusOnAppearing);

  ImGui::SetWindowFontScale(1.0f);

  if (entityData.hasEntity) {
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "【%s】", entityData.name.c_str());
    ImGui::Text("ID: %s | 距离: %.1f米", entityData.rawId.c_str(), entityData.distance);

    float maxHp = (entityData.maxHp > 0.0f) ? entityData.maxHp : 20.0f;
    float hp = std::clamp(entityData.hp, 0.0f, maxHp);
    float hpRatio = hp / maxHp;

    ImGui::Text("生命: %.1f / %.1f ❤", hp, maxHp);
    ImVec4 hpCol = (hpRatio > 0.5f) ? ImVec4(0.2f, 0.85f, 0.3f, 1.0f)
                 : (hpRatio > 0.2f) ? ImVec4(0.95f, 0.85f, 0.2f, 1.0f)
                                    : ImVec4(0.95f, 0.25f, 0.25f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, hpCol);
    ImGui::ProgressBar(hpRatio, ImVec2(240.0f, 16.0f), "");
    ImGui::PopStyleColor();

    if (entityData.armor > 0) {
      ImGui::Text("护甲: %d 🛡", entityData.armor);
    }
    if (entityData.isHorse) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.45f, 1.0f),
                         "马匹属性: 速度 %.2f m/s | 跳跃 %.2f 格",
                         entityData.horseSpeed, entityData.horseJump);
    }
    if (entityData.isWolf) {
      ImGui::Spacing();
      if (!entityData.collarColor.empty()) {
        ImGui::BulletText("项圈颜色: %s", entityData.collarColor.c_str());
      }
      if (entityData.wolfArmor > 0.0f) {
        ImGui::BulletText("狼铠耐久: %.0f%%", entityData.wolfArmor * 100.0f);
      }
    }
    if (entityData.isVillager) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                         "职业: %s | 等级: %d 阶",
                         entityData.profession.c_str(), entityData.villagerLevel);
    }
    if (entityData.isIronGolem) {
      static const char *kCracks[] = {"完好无损", "轻微裂纹", "中度裂纹", "严重破损"};
      int idx = std::clamp(entityData.crackLevel, 0, 3);
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.9f, 1.0f), "身体状态: %s", kCracks[idx]);
    }
    if (entityData.isWarden) {
      ImGui::Spacing();
      ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.9f, 1.0f), "怒气感知: %d / 150", entityData.angerLevel);
    }
  } else if (!text.empty()) {
    ImGui::TextUnformatted(text.c_str());
  }

  // 容器透视展示
  if (containerData.hasContainer) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (containerData.specialType == SpecialContainerType::Furnace) {
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "【熔炉状态】");
      if (containerData.isCooking) {
        ImGui::Text("烧炼进度: %.0f%% (剩余 %.1f秒)",
                    containerData.cookProgress * 100.0f, containerData.cookSecondsLeft);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.95f, 0.65f, 0.2f, 1.0f));
        ImGui::ProgressBar(containerData.cookProgress, ImVec2(240.0f, 16.0f), "");
        ImGui::PopStyleColor();
      } else {
        ImGui::TextDisabled("烧炼待机中 (0%%)");
      }

      if (containerData.isBurning) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.15f, 1.0f), "燃料燃烧: 剩余 %.1f秒",
                           containerData.fuelSecondsLeft);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.4f, 0.1f, 1.0f));
        ImGui::ProgressBar(containerData.fuelProgress, ImVec2(240.0f, 12.0f), "");
        ImGui::PopStyleColor();
      }

      ImGui::Spacing();
      if (!containerData.furnaceInput.empty()) {
        ImVec4 col = getQualityColor(containerData.furnaceInput, timeSec);
        ImGui::TextColored(col, "• 原料: %s x%d", containerData.furnaceInput.c_str(), containerData.furnaceInputCount);
      }
      if (!containerData.furnaceFuel.empty()) {
        ImVec4 col = getQualityColor(containerData.furnaceFuel, timeSec);
        ImGui::TextColored(col, "• 燃料: %s x%d", containerData.furnaceFuel.c_str(), containerData.furnaceFuelCount);
      }
      if (!containerData.furnaceResult.empty()) {
        ImVec4 col = getQualityColor(containerData.furnaceResult, timeSec);
        ImGui::TextColored(col, "• 产物: %s x%d", containerData.furnaceResult.c_str(), containerData.furnaceResultCount);
      }
    } else if (containerData.specialType == SpecialContainerType::Campfire) {
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "【营火烤肉剩余时间】");
      int activeCount = 0;
      for (int i = 0; i < 4; i++) {
        const auto &slot = containerData.campfireSlots[i];
        if (slot.active) {
          activeCount++;
          if (slot.remainingSeconds > 0.1f) {
            ImGui::BulletText("[%d] %s: 剩余 %.1f秒", i + 1, slot.itemName.c_str(), slot.remainingSeconds);
          } else {
            ImGui::BulletText("[%d] %s: 已烤熟 √", i + 1, slot.itemName.c_str());
          }
        }
      }
      if (activeCount == 0) {
        ImGui::TextDisabled("  (当前无食材)");
      }
    } else if (containerData.specialType == SpecialContainerType::Beehive) {
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "【蜂箱状态】");
      ImGui::BulletText("蜜蜂: %d/3 只", containerData.beeOccupants);
      char honeyText[64];
      std::snprintf(honeyText, sizeof(honeyText), "等级 %d/5", containerData.honeyLevel);
      ImGui::ProgressBar(containerData.honeyLevel / 5.0f, ImVec2(-1.0f, 12.0f), honeyText);
    } else {
      if (containerData.isUnsynced) {
        ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "【%s】(%d 格)",
                           containerData.containerTitle.c_str(), containerData.totalSlots);
        ImGui::TextDisabled("  (未同步 - 对准打开后自动记录)");
      } else {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "【%s】(已用 %d/%d 格)",
                           containerData.containerTitle.c_str(), containerData.filledSlots, containerData.totalSlots);

        if (containerData.mergedItems.empty()) {
          ImGui::TextDisabled("  (空容器)");
        } else {
          // 自适应流式折行排布，最多展示 12 种不同物品，超出部分智能折叠
          size_t displayCount = std::min(containerData.mergedItems.size(), static_cast<size_t>(12));
          int col = 0;
          for (size_t i = 0; i < displayCount; i++) {
            if (col > 0) ImGui::SameLine();
            const auto &it = containerData.mergedItems[i];
            ImVec4 colItem = getQualityColor(it.itemName, timeSec);
            ImGui::TextColored(colItem, "[%s x%d]", it.itemName.c_str(), it.count);
            col++;
            if (col >= 3) col = 0;
          }
          if (containerData.mergedItems.size() > 12) {
            ImGui::TextDisabled("  ...等共 %zu 种物品", containerData.mergedItems.size());
          }
        }
      }
    }
  }

  ImGui::End();
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar(2);
}

void HudCardRenderer::renderPickups(const std::vector<PickupRenderInfo> &pickups,
                                    float displayWidth, float displayHeight,
                                    float timeSec) {
  if (pickups.empty()) return;

  float posX = displayWidth - 280.0f;
  float startY = displayHeight * 0.40f;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));

  for (size_t i = 0; i < pickups.size(); ++i) {
    const auto &item = pickups[i];
    float alpha = 1.0f;
    if (item.age < 0.2f) {
      alpha = item.age / 0.2f;
    } else if (item.age > 2.5f) {
      alpha = std::clamp(1.0f - (item.age - 2.5f) / 1.0f, 0.0f, 1.0f);
    }
    if (alpha <= 0.01f) continue;

    float animOffset = 0.0f;
    if (item.age < 0.25f) {
      float t = item.age / 0.25f;
      animOffset = 30.0f * (1.0f - std::sin(t * 1.57079f));
    }

    ImGui::SetNextWindowPos(ImVec2(posX + animOffset, startY + i * 42.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f * alpha);

    char winId[32];
    std::snprintf(winId, sizeof(winId), "VH_Pickup_%zu", i);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.75f * alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.45f, 0.85f, 0.5f * alpha));

    ImGui::Begin(winId, nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing);

    ImVec4 nameCol = getQualityColor(item.name, timeSec);
    nameCol.w *= alpha;

    ImGui::TextColored(nameCol, "+%d %s", item.count, item.name.c_str());
    ImGui::End();

    ImGui::PopStyleColor(2);
  }

  ImGui::PopStyleVar(2);
}

} // namespace highlight
