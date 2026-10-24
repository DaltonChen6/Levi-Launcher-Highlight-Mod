# Levi-Launcher-Highlight-Mod (VisualHighlight)

Minecraft 基岩版（Bedrock Edition 1.21.x ~ 1.26.x）可视化高亮显示模组，基于 LeviLaunchroid `preload-native` 原生模组（`.levipack`）架构开发。

## 项目特点

1. **纯原生内存探针**：直接 Hook 客户端渲染与发现管线，不碰玩家背包，彻底根除 Script API 模组临时占用背包导致物品丢失的毁灭性 Bug。
2. **0 遮挡 GUI 交互**：严格对齐 WAILA `PauseManager` 工业级规范，玩家打开任何界面（背包、箱子、熔炉、工作台）期间前台彻底隐藏 HUD，绝不遮挡操作。
3. **白名单容器透视**：底层拦截 `ContainerManagerModel` 单字节白名单（`LevelEntity`、`Barrel`、`ShulkerBox`、`Furnace` 等），绝不混入玩家自身背包物品。
4. **动力学熔炉模拟**：20 ticks/sec 动力学时间流逝推算，熄灭防幽灵燃烧，关箱后物品持续准确显示。
5. **真实物理挖掘预测**：综合方块硬度、工具材质、效率附魔、急迫/疲劳药水、水中/滞空惩罚实时推算真实秒数。
6. **设备动态多语言**：动态挂钩 `8I18nImpl` 提取系统语言包与材质包译名，内置 7000+ 条高频中文词典保底。

## 环境要求

- Android NDK（NDK 30.0.15729638 / API 28+ / ARM64-v8a）
- CMake 3.22+ 与 Ninja
- 必须纯静态链接 libc++（`_analysis/elf_undef.py` 严格校验 `libc++ related: 0`）

## 构建命令

```powershell
powershell -ExecutionPolicy Bypass -File .\构建模组.ps1
```

## 目录结构

```text
highlight-mod/
├── CMakeLists.txt        # CMake 工程定义（纯静态 libc++ 编译配置）
├── manifest.json.in      # .levipack 安装包清单模板
├── VERSION.txt           # 自动化版本控制（当前 0.1.80）
├── scripts/package.ps1   # 打包辅助脚本
├── extern/imgui          # ImGui 1.91+ 渲染核心（含 Android assert 保护）
└── src/
    ├── main.cpp          # PL_REGISTER_MOD 模组注册入口
    ├── mod/              # 模组生命周期管理与配置
    ├── discover/         # 准星碰撞检测、挖掘进度与多语言服务
    ├── container/        # 容器模型解析、箱子/熔炉/特种方块透视
    ├── entity/           # 实体属性全维透视（HP/马匹物理/村民等）
    ├── block/            # 物理挖掘耗时预测公式
    └── render/           # EGL 交换链挂钩、ImGui 现代卡片渲染与内嵌字体
```
