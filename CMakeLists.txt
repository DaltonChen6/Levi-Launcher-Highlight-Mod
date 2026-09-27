cmake_minimum_required(VERSION 3.22)

project(visual_highlight_mod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(MOD_ID "visual_highlight" CACHE STRING "LeviLauncher mod package directory name")
set(MOD_NAME "可视化高亮显示" CACHE STRING "LeviLauncher mod display name")
set(MOD_AUTHOR "chrnj" CACHE STRING "LeviLauncher mod author")
set(MOD_VERSION "0.1.2" CACHE STRING "LeviLauncher mod version")
set(MOD_LIBRARY_NAME "visual_highlight" CACHE STRING "Native library name without lib prefix or .so suffix")
set(MOD_MINECRAFT_VERSIONS "[\"1.26.*\"]" CACHE STRING "Raw JSON array for manifest minecraft_versions")
set(MOD_ICON "" CACHE STRING "Optional package-relative icon path, such as icon.png")

include(FetchContent)

FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    # 必须和手机微风启动器内置的 preloader 版本一致（ModuleInfo 结构体含 onKeybind 字段），
    # 否则 HUD 注册会读越界导致崩溃。0.2.3 及以下都没有 onKeybind。
    GIT_TAG a2a7f29a8b690fd83fd8f331ea925d322ab1754c
    SOURCE_SUBDIR cmake/preloader-sdk
)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_Declare(
    boost_pfr
    GIT_REPOSITORY https://github.com/boostorg/pfr.git
    GIT_TAG boost-1.88.0
)
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.7
)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 11.2.0
)
set(FMT_DOC OFF CACHE BOOL "Disable fmt docs" FORCE)
set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Disable fmt install rules" FORCE)
FetchContent_MakeAvailable(preloader_android nlohmann_json boost_pfr magic_enum fmt)

FetchContent_GetProperties(preloader_android)
set(LEVI_PRELOADER_INCLUDE_DIR "${preloader_android_SOURCE_DIR}/include")

if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a" AND
   NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
    message(FATAL_ERROR "Unsupported Android ABI: ${CMAKE_ANDROID_ARCH_ABI}")
endif()

option(LEVI_LINK_PRELOADER
       "Build and link the preloader target instead of leaving SDK symbols for runtime lookup"
       ON)

if(LEVI_LINK_PRELOADER AND NOT TARGET preloader)
    add_subdirectory("${preloader_android_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_preloader_android")
endif()

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/imgui")

add_library(${MOD_LIBRARY_NAME} SHARED
    src/main.cpp
    src/mod/HighlightMod.cpp
    src/discover/Discovery.cpp
    src/discover/LocalizationService.cpp
    src/discover/LocalizationTree.cpp
    src/discover/ToolStatusEvaluator.cpp
    src/container/ContainerCache.cpp
    src/container/ContainerModelParser.cpp
    src/container/FurnaceHandler.cpp
    src/container/SpecialBlockHandler.cpp
    src/container/ChestHandler.cpp
    src/container/ContainerInspector.cpp
    src/block/MiningCalculator.cpp
    src/entity/EntityInspector.cpp
    src/render/HudRenderer.cpp
    src/render/FontManager.cpp
    src/render/HudCardRenderer.cpp
    src/render/EmbeddedFont.cpp
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

set_target_properties(${MOD_LIBRARY_NAME} PROPERTIES
    OUTPUT_NAME "${MOD_LIBRARY_NAME}"
)

target_include_directories(${MOD_LIBRARY_NAME}
    PRIVATE
        src
        "${IMGUI_DIR}"
        "${LEVI_PRELOADER_INCLUDE_DIR}"
)

target_compile_options(${MOD_LIBRARY_NAME}
    PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU>:-fvisibility=hidden -fvisibility-inlines-hidden>
)

target_link_options(${MOD_LIBRARY_NAME}
    PRIVATE
        "-Wl,-z,max-page-size=16384" -Wl,--exclude-libs,ALL
)

find_library(log-lib log REQUIRED)
find_library(egl-lib EGL REQUIRED)
find_library(gles-lib GLESv2 REQUIRED)
target_link_libraries(${MOD_LIBRARY_NAME} PRIVATE
    ${log-lib}
    ${egl-lib}
    ${gles-lib}
    nlohmann_json::nlohmann_json
    Boost::pfr
    magic_enum::magic_enum
    fmt::fmt
)

# 静态链接 libc++：游戏内置的旧版 libc++ 缺少新符号（如 __hash_memory），
# 不静态链接会导致 dlopen 失败、模组无法加载。
set(VISUAL_HIGHLIGHT_STATIC_STL)
if(DEFINED CMAKE_ANDROID_NDK)
    set(_VH_STL_DIR
        "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_NDK_HOST_TAG}/sysroot/usr/lib/${CMAKE_ANDROID_ARCH_ABI}")
    if(EXISTS "${_VH_STL_DIR}/libc++_static.a")
        list(APPEND VISUAL_HIGHLIGHT_STATIC_STL "${_VH_STL_DIR}/libc++_static.a")
        if(EXISTS "${_VH_STL_DIR}/libc++abi.a")
            list(APPEND VISUAL_HIGHLIGHT_STATIC_STL "${_VH_STL_DIR}/libc++abi.a")
        endif()
    endif()
endif()
if(NOT VISUAL_HIGHLIGHT_STATIC_STL)
    set(VISUAL_HIGHLIGHT_STATIC_STL -lc++_static -lc++abi)
endif()
target_link_libraries(${MOD_LIBRARY_NAME} PRIVATE ${VISUAL_HIGHLIGHT_STATIC_STL})

if(TARGET preloader)
    target_link_libraries(${MOD_LIBRARY_NAME} PRIVATE preloader)
else()
    target_link_options(${MOD_LIBRARY_NAME} PRIVATE
        "-Wl,--unresolved-symbols=ignore-all")
endif()

if(NOT CMAKE_STRIP)
    message(FATAL_ERROR "CMAKE_STRIP is required to strip the generated library.")
endif()

add_custom_command(TARGET ${MOD_LIBRARY_NAME} POST_BUILD
    COMMAND "${CMAKE_STRIP}" --strip-unneeded "$<TARGET_FILE:${MOD_LIBRARY_NAME}>"
    COMMENT "Stripping symbols from $<TARGET_FILE_NAME:${MOD_LIBRARY_NAME}>"
    VERBATIM
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/manifest.json"
    @ONLY
)

set(LEVI_PACKAGE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/package")
set(LEVI_PACKAGE_MOD_DIR "${LEVI_PACKAGE_ROOT}/${MOD_ID}")
set(LEVI_PACKAGE_FILE "${CMAKE_CURRENT_BINARY_DIR}/${MOD_ID}-${MOD_VERSION}-${CMAKE_ANDROID_ARCH_ABI}.levipack")
set(LEVI_PACKAGE_ICON_COMMAND)

if(NOT MOD_ICON STREQUAL "")
    if(IS_ABSOLUTE "${MOD_ICON}")
        message(FATAL_ERROR "MOD_ICON must be a package-relative path")
    endif()

    if(MOD_ICON MATCHES "(^|/)\\.\\.(/|$)")
        message(FATAL_ERROR "MOD_ICON must not contain ..")
    endif()

    set(MOD_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${MOD_ICON}")
    if(NOT EXISTS "${MOD_ICON_SOURCE}")
        message(FATAL_ERROR "MOD_ICON does not exist: ${MOD_ICON_SOURCE}")
    endif()

    get_filename_component(MOD_ICON_DIR "${MOD_ICON}" DIRECTORY)
    if(NOT MOD_ICON_DIR STREQUAL "")
        list(APPEND LEVI_PACKAGE_ICON_COMMAND
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${LEVI_PACKAGE_MOD_DIR}/${MOD_ICON_DIR}")
    endif()
    list(APPEND LEVI_PACKAGE_ICON_COMMAND
        COMMAND "${CMAKE_COMMAND}" -E copy "${MOD_ICON_SOURCE}" "${LEVI_PACKAGE_MOD_DIR}/${MOD_ICON}")
endif()

add_custom_target(levi_package
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${LEVI_PACKAGE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${LEVI_PACKAGE_MOD_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${MOD_LIBRARY_NAME}>" "${LEVI_PACKAGE_MOD_DIR}/lib${MOD_LIBRARY_NAME}.so"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/manifest.json" "${LEVI_PACKAGE_MOD_DIR}/manifest.json"
    ${LEVI_PACKAGE_ICON_COMMAND}
    COMMAND "${CMAKE_COMMAND}" -E chdir "${LEVI_PACKAGE_ROOT}" "${CMAKE_COMMAND}" -E tar "cf" "${LEVI_PACKAGE_FILE}" --format=zip "${MOD_ID}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS ${MOD_LIBRARY_NAME}
    COMMENT "Packaging ${MOD_ID} for LeviLauncher"
    VERBATIM
)
