project(GameEditor VERSION 1.0.0)

set(EDITOR_SOURCES
    main.cpp
    GameEditorApp.cpp
    GameEditorApp.hpp
    EditorEntity.cpp
    EditorScene.cpp
    ${CMAKE_SOURCE_DIR}/apps/cwr/Game/GameApplication.cpp
    ${CMAKE_SOURCE_DIR}/apps/cwr/Game/HighPerformanceGpuHint.cpp
    ImGuizmo/ImGuizmo.cpp
)

if(WIN32)
    # We can reuse the Studio icon or create a new one later.
    list(APPEND EDITOR_SOURCES ${CMAKE_SOURCE_DIR}/resources/icon-tool.rc)
    add_executable(GameEditor WIN32 ${EDITOR_SOURCES})
else()
    add_executable(GameEditor ${EDITOR_SOURCES})
endif()

target_compile_definitions(GameEditor PRIVATE
    IMGUI_ENABLE_FREETYPE
    $<$<CONFIG:Debug>:_DEBUG>
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

find_package(SDL3 CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

target_link_libraries(GameEditor PRIVATE
    GameBase
    PoseidonGL33
    Poseidon
    PoseidonOpenAL
    SDL3::SDL3
    Freetype::Freetype
    CLI11::CLI11
    imgui::imgui
)

if(WIN32)
    target_link_libraries(GameEditor PRIVATE
        ws2_32 winmm version comctl32 ole32 shell32
        legacy_stdio_definitions.lib
    )
else()
    target_link_libraries(GameEditor PRIVATE
        -Wl,--start-group
        GameBase PoseidonGL33 Poseidon PoseidonOpenAL
        -Wl,--end-group
        pthread dl
    )
endif()

target_include_directories(GameEditor PRIVATE
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/apps/cwr/Game
)

include(${CMAKE_SOURCE_DIR}/cmake/DistCopy.cmake)
dist_copy(GameEditor)
