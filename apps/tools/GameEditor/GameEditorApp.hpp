#pragma once

#include <string>
#include "EditorScene.hpp"
#pragma push_macro("DebugLog")
#undef DebugLog
#include <imgui.h>
#pragma pop_macro("DebugLog")

namespace Poseidon
{

class GameEditorApp
{
  public:
    GameEditorApp();
    void setupTheme();
    void render(const ImVec2& displaySize);

    // Called from main loop to update camera state before 3D draw.
    // Must run before BeginObjects so the scene renders with correct camera.
    void updateCamera();

    // Called from main loop between BeginObjects/EndObjects to submit
    // 3D geometry (grid, axes, entities) into the engine draw pipeline.
    void drawScene();

    // Whether the project is loaded (needed by main loop to know if 3D should draw)
    bool isProjectLoaded() const { return m_projectLoaded && m_sceneInitialized; }

  private:
    void renderMainMenuBar();
    void renderDockspace(const ImVec2& displaySize);
    void renderViewportPanel();
    void renderHierarchyPanel();
    void renderInspectorPanel();
    void renderAssetBrowserPanel();

    // Welcome / Project Setup Dialog
    void renderWelcomeWindow();
    void loadRecentProjects();
    void saveRecentProjects();
    void addRecentProject(const std::string& path);
    void setupProjectDirectory(const std::string& path);
    void showStatus(const std::string& msg, bool isError);

    // DPI / Scaling
    void initDpiScale();
    float sc(float logicalPixels) const { return logicalPixels * m_dpiScale; }

    // Native file/folder pickers (Win32 on Windows, fallback on other)
    static std::string pickFile(const char* filter);    // returns path or ""
    static std::string pickFolder(const char* title);   // returns path or ""

    EditorScene m_scene;
    
    // Project state
    std::string m_projectPath;
    bool m_projectLoaded = false;
    char m_projectPathInput[512];
    char m_newProjectNameInput[128];
    char m_newProjectPathInput[512];
    std::vector<std::string> m_recentProjects;
    
    // OBJ Import Popup state
    bool m_showImportPopup = false;
    char m_importSourcePath[512];
    char m_importTargetName[128];
    
    // Script Editor state
    bool m_showScriptEditor = false;
    std::string m_editingScriptPath;
    std::string m_editingScriptContent;
    char m_newScriptNameInput[128];
    bool m_showNewScriptPopup = false;
    
    // Message notifications
    std::string m_statusMessage;
    float m_statusMessageTime = 0.0f;
    bool m_statusIsError = false;

    // Gizmo state
    int m_gizmoOperation; // ImGuizmo::OPERATION
    int m_gizmoMode;      // ImGuizmo::MODE
    bool m_useSnap;
    float m_snap[3];
    bool m_sceneInitialized = false;

    // DPI / HiDPI scaling
    float m_dpiScale       = 1.0f;
    bool  m_dpiInitialized = false;

    // Camera state (flycam)
    float m_camYaw   = 0.0f;
    float m_camPitch = 0.0f;
    Vector3 m_camPos = Vector3(0.0f, 2.0f, -10.0f);
    float m_camFOV   = 0.85f;
};

} // namespace Poseidon
