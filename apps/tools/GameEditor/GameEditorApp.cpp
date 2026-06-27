#include "GameEditorApp.hpp"

#pragma push_macro("DebugLog")
#undef DebugLog
#include "EditorEntity.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <Poseidon/Graphics/Core/Engine.hpp>
#include <Poseidon/World/Scene/Scene.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include "ImGuizmo/ImGuizmo.h"
#pragma pop_macro("DebugLog")

// Win32 native file / folder pickers — included AFTER engine headers to avoid macro pollution
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <commdlg.h>   // GetOpenFileNameA
#  include <shlobj.h>    // SHBrowseForFolderA / SHGetPathFromIDListA
#endif

namespace Poseidon
{

// ─────────────────────────────────────────────────────────────────────────────
// Matrix / Camera helpers  (unchanged)
// ─────────────────────────────────────────────────────────────────────────────

static void Matrix4ToImGuizmo(const Matrix4& in, float* out)
{
    const Matrix3P& orient = in.Orientation();
    const Vector3P& pos    = in.Position();
    out[0]  = orient(0,0); out[4]  = orient(0,1); out[8]  = orient(0,2); out[12] = pos[0];
    out[1]  = orient(1,0); out[5]  = orient(1,1); out[9]  = orient(1,2); out[13] = pos[1];
    out[2]  = orient(2,0); out[6]  = orient(2,1); out[10] = orient(2,2); out[14] = pos[2];
    out[3]  = 0.0f;        out[7]  = 0.0f;        out[11] = 0.0f;        out[15] = 1.0f;
}

static void ImGuizmoToMatrix4(const float* in, Matrix4& out)
{
    Matrix3P orient;
    orient(0,0) = in[0]; orient(0,1) = in[4]; orient(0,2) = in[8];
    orient(1,0) = in[1]; orient(1,1) = in[5]; orient(1,2) = in[9];
    orient(2,0) = in[2]; orient(2,1) = in[6]; orient(2,2) = in[10];
    out.SetOrientation(orient);
    Vector3P pos(in[12], in[13], in[14]);
    out.SetPosition(pos);
}

static void BuildProjectionMatrix(Poseidon::Camera* cam, float* proj)
{
    float n = cam->Near(), f = cam->Far();
    float r = cam->Left(), l = -r;
    float t = cam->Top(),  b = -t;
    proj[0]  = (2.0f*n)/(r-l); proj[1]  = 0.0f;           proj[2]  = 0.0f;            proj[3]  = 0.0f;
    proj[4]  = 0.0f;            proj[5]  = (2.0f*n)/(t-b); proj[6]  = 0.0f;            proj[7]  = 0.0f;
    proj[8]  = 0.0f;            proj[9]  = 0.0f;            proj[10] = -(f+n)/(f-n);   proj[11] = -1.0f;
    proj[12] = 0.0f;            proj[13] = 0.0f;            proj[14] = -(2.0f*f*n)/(f-n); proj[15] = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Win32 native file / folder pickers
// ─────────────────────────────────────────────────────────────────────────────

/*static*/ std::string GameEditorApp::pickFile(const char* filter)
{
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.lpstrFilter  = filter ? filter : "All Files\0*.*\0";
    ofn.lpstrFile    = buf;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return buf;
#endif
    return "";
}

/*static*/ std::string GameEditorApp::pickFolder(const char* title)
{
#ifdef _WIN32
    BROWSEINFOA bi = {};
    bi.lpszTitle   = title ? title : "Select Folder";
    bi.ulFlags     = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pid = SHBrowseForFolderA(&bi);
    if (pid)
    {
        char path[MAX_PATH] = {};
        SHGetPathFromIDListA(pid, path);
        CoTaskMemFree(pid);
        return path;
    }
#endif
    return "";
}

// ─────────────────────────────────────────────────────────────────────────────
// DPI scale  (read-only — never mutate ImGui font/style after init)
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::initDpiScale()
{
    if (m_dpiInitialized) return;
    m_dpiInitialized = true;
#ifdef _WIN32
    UINT dpi   = GetDpiForSystem();         // 96 = 100 %, 144 = 150 %, 192 = 200 %
    m_dpiScale = static_cast<float>(dpi) / 96.0f;
#else
    m_dpiScale = 1.0f;
#endif
    // We intentionally do NOT call ScaleAllSizes or rebuild the font atlas.
    // Doing so mid-session invalidates the GPU texture → black screen.
    // All pixel values below go through sc() which multiplies by m_dpiScale.
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

GameEditorApp::GameEditorApp()
{
    m_gizmoOperation = ImGuizmo::TRANSLATE;
    m_gizmoMode      = ImGuizmo::WORLD;
    m_useSnap        = false;
    m_snap[0] = m_snap[1] = m_snap[2] = 1.0f;
    m_projectPathInput[0]    = '\0';
    m_newProjectNameInput[0] = '\0';
    m_newProjectPathInput[0] = '\0';
    m_importSourcePath[0]    = '\0';
    m_importTargetName[0]    = '\0';
    m_newScriptNameInput[0]  = '\0';
    loadRecentProjects();
}

// ─────────────────────────────────────────────────────────────────────────────
// Recent Projects helpers
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::loadRecentProjects()
{
    m_recentProjects.clear();
    std::ifstream f("recent_projects.txt");
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line))
        if (!line.empty()) m_recentProjects.push_back(line);
}

void GameEditorApp::saveRecentProjects()
{
    std::ofstream f("recent_projects.txt");
    for (const auto& p : m_recentProjects) f << p << "\n";
}

void GameEditorApp::addRecentProject(const std::string& path)
{
    m_recentProjects.erase(
        std::remove(m_recentProjects.begin(), m_recentProjects.end(), path),
        m_recentProjects.end());
    m_recentProjects.insert(m_recentProjects.begin(), path);
    if (m_recentProjects.size() > 10) m_recentProjects.resize(10);
    saveRecentProjects();
}

void GameEditorApp::setupProjectDirectory(const std::string& path)
{
    std::filesystem::create_directories(path + "/models");
    std::filesystem::create_directories(path + "/scripts");
}

void GameEditorApp::showStatus(const std::string& msg, bool isError)
{
    m_statusMessage     = msg;
    m_statusMessageTime = 4.0f;
    m_statusIsError     = isError;
}

// ─────────────────────────────────────────────────────────────────────────────
// Theme
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::setupTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 6.0f;
    s.FrameRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 4.0f;
    s.FramePadding      = ImVec2(8, 4);
    s.ItemSpacing       = ImVec2(8, 6);
    s.IndentSpacing     = 16.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    c[ImGuiCol_ChildBg]          = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
    c[ImGuiCol_PopupBg]          = ImVec4(0.12f, 0.12f, 0.14f, 0.98f);
    c[ImGuiCol_Border]           = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    c[ImGuiCol_FrameBg]          = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_TitleBg]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    c[ImGuiCol_MenuBarBg]        = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    c[ImGuiCol_ScrollbarBg]      = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    c[ImGuiCol_Header]           = ImVec4(0.20f, 0.45f, 0.80f, 0.50f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(0.20f, 0.45f, 0.80f, 0.75f);
    c[ImGuiCol_HeaderActive]     = ImVec4(0.20f, 0.45f, 0.80f, 1.00f);
    c[ImGuiCol_Button]           = ImVec4(0.20f, 0.45f, 0.80f, 0.65f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(0.24f, 0.52f, 0.90f, 1.00f);
    c[ImGuiCol_ButtonActive]     = ImVec4(0.16f, 0.38f, 0.70f, 1.00f);
    c[ImGuiCol_Tab]              = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    c[ImGuiCol_TabHovered]       = ImVec4(0.20f, 0.45f, 0.80f, 0.80f);
    c[ImGuiCol_TabActive]        = ImVec4(0.18f, 0.40f, 0.72f, 1.00f);
    c[ImGuiCol_Text]             = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
    c[ImGuiCol_CheckMark]        = ImVec4(0.30f, 0.60f, 1.00f, 1.00f);
    c[ImGuiCol_DockingPreview]   = ImVec4(0.20f, 0.45f, 0.80f, 0.50f);
    c[ImGuiCol_Separator]        = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main render entry
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::render(const ImVec2& displaySize)
{
    static bool themeSetup = false;
    if (!themeSetup) { setupTheme(); themeSetup = true; }

    initDpiScale();

    ImGuiIO& io = ImGui::GetIO();
    if (m_statusMessageTime > 0.0f)
        m_statusMessageTime -= io.DeltaTime;

    if (!m_projectLoaded)
    {
        renderWelcomeWindow();
        return;
    }

    if (!m_sceneInitialized) { m_scene.Init(); m_sceneInitialized = true; }
    m_scene.Update(0.016f);

    renderMainMenuBar();
    renderDockspace(displaySize);
    renderViewportPanel();
    renderHierarchyPanel();
    renderInspectorPanel();
    renderAssetBrowserPanel();
}

// ─────────────────────────────────────────────────────────────────────────────
// Welcome / Project Setup  — two separate top-level windows, NOT nested
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::renderWelcomeWindow()
{
    ImGuiIO& io  = ImGui::GetIO();
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    // ── 1. Full-screen dark background ──────────────────────────────────────
    // Drawn as a separate, independent window so it never blocks the card.
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.09f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##WelcomeBG", nullptr,
        ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoResize  |
        ImGuiWindowFlags_NoMove        | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs      | ImGuiWindowFlags_NoNav     |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);
    ImGui::End();           // background has no content — close it immediately
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // ── 2. Centered dialog card ──────────────────────────────────────────────
    const float cardW = sc(640.0f);
    const float cardH = sc(540.0f);
    ImGui::SetNextWindowPos(ImVec2((W - cardW) * 0.5f, (H - cardH) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   sc(8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(sc(22), sc(18)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.28f, 0.48f, 0.88f, 0.70f));

    ImGui::Begin("##WelcomeCard", nullptr,
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    // Title
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.72f, 1.00f, 1.0f));
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("  Poseidon Level Editor");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Recent Projects ──────────────────────────────────────────────────────
    if (!m_recentProjects.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.68f, 0.72f, 1.0f));
        ImGui::Text("Recent Projects");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        float listH = std::min((float)m_recentProjects.size() * sc(24.0f), sc(108.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
        ImGui::BeginChild("##RecentList", ImVec2(-1, listH), true);
        for (const auto& rp : m_recentProjects)
        {
            bool exists = std::filesystem::exists(rp);
            if (!exists) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            if (ImGui::Selectable(rp.c_str()) && exists)
            {
                m_projectPath   = rp;
                m_projectLoaded = true;
                addRecentProject(rp);
            }
            if (!exists) ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // ── Open Existing ────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.68f, 0.72f, 1.0f));
    ImGui::Text("Open Existing Project");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    const float btnW = sc(78.0f);
    ImGui::SetNextItemWidth(-(btnW * 2 + ImGui::GetStyle().ItemSpacing.x * 2 + 4));
    ImGui::InputText("##LoadPath", m_projectPathInput, sizeof(m_projectPathInput));
    ImGui::SameLine();
    if (ImGui::Button("Browse##lp", ImVec2(btnW, 0)))
    {
        std::string chosen = pickFolder("Select Project Folder");
        if (!chosen.empty())
            strncpy(m_projectPathInput, chosen.c_str(), sizeof(m_projectPathInput) - 1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Open", ImVec2(btnW, 0)))
    {
        std::string p = m_projectPathInput;
        if (std::filesystem::is_directory(p)) { m_projectPath = p; m_projectLoaded = true; addRecentProject(p); }
        else showStatus("Directory not found: " + p, true);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Create New ───────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.68f, 0.68f, 0.72f, 1.0f));
    ImGui::Text("Create New Project");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    const float browseW = sc(78.0f);
    ImGui::SetNextItemWidth(-(browseW + ImGui::GetStyle().ItemSpacing.x + 2));
    ImGui::InputText("Parent Folder##np", m_newProjectPathInput, sizeof(m_newProjectPathInput));
    ImGui::SameLine();
    if (ImGui::Button("Browse##np", ImVec2(browseW, 0)))
    {
        std::string chosen = pickFolder("Select Parent Folder");
        if (!chosen.empty())
            strncpy(m_newProjectPathInput, chosen.c_str(), sizeof(m_newProjectPathInput) - 1);
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("Project Name##nn", m_newProjectNameInput, sizeof(m_newProjectNameInput));
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f, 0.48f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.60f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.36f, 0.18f, 1.0f));
    if (ImGui::Button("Create Project", ImVec2(-1, sc(30.0f))))
    {
        std::string base = m_newProjectPathInput, name = m_newProjectNameInput;
        if (!base.empty() && !name.empty())
        {
            std::string full = base + "/" + name;
            setupProjectDirectory(full);
            m_projectPath   = full;
            m_projectLoaded = true;
            addRecentProject(full);
        }
        else showStatus("Fill in both fields.", true);
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Sandbox quick-launch ─────────────────────────────────────────────────
    const float sbW = sc(150.0f);
    ImGui::SetCursorPosX((cardW - sbW) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.22f, 0.27f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.36f, 1.0f));
    if (ImGui::Button("Open Sandbox", ImVec2(sbW, 0)))
    {
        std::string sb = "./sandbox";
        setupProjectDirectory(sb);
        m_projectPath   = sb;
        m_projectLoaded = true;
        showStatus("Opened in sandbox mode.", false);
    }
    ImGui::PopStyleColor(2);

    // Status message
    if (m_statusMessageTime > 0.0f)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text,
            m_statusIsError ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                            : ImVec4(0.4f, 1.0f, 0.6f, 1.0f));
        ImGui::TextWrapped("%s", m_statusMessage.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End(); // ##WelcomeCard
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Menu Bar
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::renderMainMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene"))  m_scene.Init();
            if (ImGui::MenuItem("Open Scene")) m_scene.Load(m_projectPath + "/myscene.edscene");
            if (ImGui::MenuItem("Save Scene")) m_scene.Save(m_projectPath + "/myscene.edscene");
            ImGui::Separator();
            if (ImGui::MenuItem("Close Project")) { m_projectLoaded = false; m_sceneInitialized = false; loadRecentProjects(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
            if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}
            ImGui::EndMenu();
        }

        // Project label
        {
            std::string label = "  Project: " + m_projectPath + "  ";
            float tw = ImGui::CalcTextSize(label.c_str()).x;
            float cx = (ImGui::GetWindowWidth() - tw) * 0.5f;
            if (cx > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(cx);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.60f, 1.0f));
            ImGui::Text("%s", label.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - sc(360.0f));
        if (ImGui::RadioButton("Trans", m_gizmoOperation == ImGuizmo::TRANSLATE)) m_gizmoOperation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rot",   m_gizmoOperation == ImGuizmo::ROTATE))    m_gizmoOperation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", m_gizmoOperation == ImGuizmo::SCALE))     m_gizmoOperation = ImGuizmo::SCALE;
        ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();
        if (ImGui::RadioButton("Local", m_gizmoMode == ImGuizmo::LOCAL)) m_gizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", m_gizmoMode == ImGuizmo::WORLD)) m_gizmoMode = ImGuizmo::WORLD;
        ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();
        ImGui::Checkbox("Snap", &m_useSnap);

        if (m_statusMessageTime > 0.0f)
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text,
                m_statusIsError ? ImVec4(1.0f, 0.5f, 0.5f, 1.0f) : ImVec4(0.5f, 1.0f, 0.6f, 1.0f));
            ImGui::Text("  %s", m_statusMessage.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::EndMainMenuBar();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Dockspace
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::renderDockspace(const ImVec2& displaySize)
{
    ImGuiWindowFlags wf =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize  |
        ImGuiWindowFlags_NoMove    | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0, 0));
    ImGui::Begin("EditorDockSpace", nullptr, wf);
    ImGui::PopStyleVar(3);

    ImGuiID dsId = ImGui::GetID("MyDockSpace");
    ImGuiDockNodeFlags dfl = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(dsId, ImVec2(0, 0), dfl);

    static bool firstTime = true;
    if (firstTime)
    {
        firstTime = false;
        ImGui::DockBuilderRemoveNode(dsId);
        ImGui::DockBuilderAddNode(dsId, dfl | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dsId, vp->WorkSize);

        ImGuiID main        = dsId;
        ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(main, ImGuiDir_Down,  0.25f, nullptr, &main);
        ImGuiID dock_left   = ImGui::DockBuilderSplitNode(main, ImGuiDir_Left,  0.20f, nullptr, &main);
        ImGuiID dock_right  = ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.25f, nullptr, &main);

        ImGui::DockBuilderDockWindow("Hierarchy",     dock_left);
        ImGui::DockBuilderDockWindow("Inspector",     dock_right);
        ImGui::DockBuilderDockWindow("Asset Browser", dock_bottom);
        ImGui::DockBuilderDockWindow("Viewport",      main);
        ImGui::DockBuilderFinish(dsId);
    }
    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Viewport  (camera + gizmo)
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::renderViewportPanel()
{
    if (!Poseidon::GScene || !Poseidon::GScene->GetCamera()) return;
    Poseidon::Camera* cam = Poseidon::GScene->GetCamera();

    ImGuiIO& io = ImGui::GetIO();

    // Render a transparent Viewport window docked in the dockspace
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleColor();

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    if (viewportSize.x < 50.0f) viewportSize.x = 50.0f;
    if (viewportSize.y < 50.0f) viewportSize.y = 50.0f;

    ImVec2 contentMin = ImGui::GetWindowPos();
    contentMin.x += ImGui::GetWindowContentRegionMin().x;
    contentMin.y += ImGui::GetWindowContentRegionMin().y;

    // Calculate fractions of display size for GEngine's AspectSettings, guarding against division-by-zero
    float left = 0.0f;
    float top = 0.0f;
    float right = 1.0f;
    float bottom = 1.0f;
    if (io.DisplaySize.x > 0.0f)
    {
        left  = contentMin.x / io.DisplaySize.x;
        right = (contentMin.x + viewportSize.x) / io.DisplaySize.x;
    }
    if (io.DisplaySize.y > 0.0f)
    {
        top    = contentMin.y / io.DisplaySize.y;
        bottom = (contentMin.y + viewportSize.y) / io.DisplaySize.y;
    }

    left   = std::max(0.0f, std::min(1.0f, left));
    top    = std::max(0.0f, std::min(1.0f, top));
    right  = std::max(0.0f, std::min(1.0f, right));
    bottom = std::max(0.0f, std::min(1.0f, bottom));

    // Update AspectSettings in GEngine to render 3D inside this rect
    if (Poseidon::GEngine)
    {
        Poseidon::AspectSettings aspect;
        Poseidon::GEngine->GetAspectSettings(aspect);
        aspect.worldLeft = left;
        aspect.worldTop = top;
        aspect.worldRight = right;
        aspect.worldBottom = bottom;
        aspect.leftFOV = 0.75f * (viewportSize.y > 0.0f ? (viewportSize.x / viewportSize.y) : 1.0f);
        aspect.topFOV = 0.75f;
        Poseidon::GEngine->SetAspectSettings(aspect);

        // Deep math logging: Viewport parameters
        static int s_vpLogCounter = 0;
        if (++s_vpLogCounter % 120 == 0)
        {
            LOG_INFO(Core, "[RENDER_MATH] Viewport Size: ({:.1f}, {:.1f}), Display Size: ({:.1f}, {:.1f})", 
                     viewportSize.x, viewportSize.y, io.DisplaySize.x, io.DisplaySize.y);
            LOG_INFO(Core, "[RENDER_MATH] Aspect Frac Rect -> Left: {:.4f}, Top: {:.4f}, Right: {:.4f}, Bottom: {:.4f}", 
                     left, top, right, bottom);
            LOG_INFO(Core, "[RENDER_MATH] Aspect FOVs -> Left: {:.4f}, Top: {:.4f}", 
                     aspect.leftFOV, aspect.topFOV);
        }
    }

    // Camera Navigation: Flycam controls when dragging in the viewport window
    // (Camera position/orientation applied to engine camera in updateCamera())
    bool isHovered = ImGui::IsWindowHovered();
    if (isHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        ImGui::SetWindowFocus(); // Focus viewport to capture keyboard inputs (W/A/S/D)
        
        m_camYaw   -= io.MouseDelta.x * 0.005f;
        m_camPitch -= io.MouseDelta.y * 0.005f;
        m_camPitch  = std::max(-1.5f, std::min(1.5f, m_camPitch));

        float speed = 10.0f * io.DeltaTime;
        if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) speed *= 3.0f;

        Vector3 fwd(std::sin(m_camYaw)*std::cos(m_camPitch), -std::sin(m_camPitch), std::cos(m_camYaw)*std::cos(m_camPitch));
        Vector3 rgt(std::cos(m_camYaw), 0.0f, -std::sin(m_camYaw));

        if (ImGui::IsKeyDown(ImGuiKey_W)) m_camPos += fwd * speed;
        if (ImGui::IsKeyDown(ImGuiKey_S)) m_camPos -= fwd * speed;
        if (ImGui::IsKeyDown(ImGuiKey_D)) m_camPos += rgt * speed;
        if (ImGui::IsKeyDown(ImGuiKey_A)) m_camPos -= rgt * speed;
    }

    ImGuizmo::BeginFrame();
    static ImGuizmo::OPERATION mOp(ImGuizmo::TRANSLATE);
    static ImGuizmo::MODE      mMode(ImGuizmo::WORLD);

    if (ImGui::IsWindowFocused() && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) mOp = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) mOp = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) mOp = ImGuizmo::SCALE;
    }

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(contentMin.x, contentMin.y, viewportSize.x, viewportSize.y);

    float cameraView[16], cameraProj[16];
    Matrix4ToImGuizmo(cam->InverseGeneral(), cameraView);
    BuildProjectionMatrix(cam, cameraProj);

    auto* sel = m_scene.GetSelectedEntity();
    if (sel)
    {
        float objMat[16];
        Matrix4ToImGuizmo(sel->GetTransform(), objMat);
        ImGuizmo::Manipulate(cameraView, cameraProj, mOp, mMode, objMat,
                             nullptr, m_useSnap ? m_snap : nullptr);
        if (ImGuizmo::IsUsing())
        {
            Matrix4 newT;
            ImGuizmoToMatrix4(objMat, newT);
            sel->SetTransform(newT);
        }
    }

    // Floating camera and grid settings overlay inside viewport
    ImGui::SetCursorPos(ImVec2(10, 30));
    ImGui::BeginChild("ViewportSettingsOverlay", ImVec2(210, 220), true);
    
    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushItemWidth(90);
        ImGui::SliderFloat("FOV", &m_camFOV, 0.4f, 1.5f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            m_camPos = Vector3(0.0f, 2.0f, -10.0f);
            m_camYaw = 0.0f;
            m_camPitch = 0.0f;
            m_camFOV = 0.85f;
        }
    }
    
    if (ImGui::CollapsingHeader("Grid Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Show Grid", &m_scene.m_showGrid);
        ImGui::PushItemWidth(90);
        ImGui::SliderInt("Size", &m_scene.m_gridSize, 10, 150);
        ImGui::SliderFloat("Step", &m_scene.m_gridStep, 0.5f, 10.0f, "%.1f");
        ImGui::ColorEdit3("Color", m_scene.m_gridColor, ImGuiColorEditFlags_NoInputs);
        ImGui::PopItemWidth();
    }
    
    ImGui::EndChild();

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Hierarchy
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::renderHierarchyPanel()
{
    ImGui::Begin("Hierarchy");
    if (ImGui::TreeNodeEx("Scene Root", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (EditorEntity* e : m_scene.GetEntities())
        {
            ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (m_scene.GetSelectedEntity() == e) fl |= ImGuiTreeNodeFlags_Selected;
            ImGui::TreeNodeEx((void*)e, fl, "%s", e->GetName().c_str());
            if (ImGui::IsItemClicked()) m_scene.SetSelectedEntity(e);
        }
        ImGui::TreePop();
    }
    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Inspector
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::renderInspectorPanel()
{
    ImGui::Begin("Inspector");
    EditorEntity* sel = m_scene.GetSelectedEntity();
    if (sel)
    {
        ImGui::Text("Name: %s", sel->GetName().c_str());
        ImGui::Separator();
        Vector3 pos = sel->GetPosition();
        float pa[3] = { pos.X(), pos.Y(), pos.Z() };
        if (ImGui::DragFloat3("Position", pa, 0.1f))
            sel->SetPosition(Vector3(pa[0], pa[1], pa[2]));
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(-1, 0))) m_scene.SetSelectedEntity(nullptr);
        ImGui::PopStyleColor();
    }
    else ImGui::TextDisabled("No object selected.");
    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Asset Browser
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::renderAssetBrowserPanel()
{
    ImGui::Begin("Asset Browser");

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.55f, 1.0f));
    ImGui::Text("Project: %s", m_projectPath.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();

    // Right-click context menu
    if (ImGui::BeginPopupContextWindow("##AssetCtx", ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem("Import OBJ Model..."))  { m_showImportPopup = true; m_importSourcePath[0] = '\0'; m_importTargetName[0] = '\0'; }
        if (ImGui::MenuItem("Create New Script...")) { m_showNewScriptPopup = true; m_newScriptNameInput[0] = '\0'; }
        ImGui::EndPopup();
    }

    if (ImGui::BeginTabBar("##AssetTabs"))
    {
        // ── Models ──────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Models"))
        {
            std::string dir = m_projectPath + "/models";
            if (std::filesystem::exists(dir))
            {
                for (const auto& e : std::filesystem::directory_iterator(dir))
                {
                    if (e.path().extension() != ".p3d") continue;
                    std::string name = e.path().filename().string();
                    std::string full = e.path().string();
                    ImGui::PushID(full.c_str());
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.80f, 1.0f));
                    if (ImGui::Button("Spawn"))
                    {
                        EditorEntity* ent = m_scene.AddEntity(name, full);
                        if (ent) { ent->SetPosition(Vector3(0,0,0)); m_scene.SetSelectedEntity(ent); }
                        else showStatus("Failed to spawn: " + name, true);
                    }
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.90f, 1.0f));
                    ImGui::Text("[M] %s", name.c_str());
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
            }
            else
            {
                ImGui::TextDisabled("No models/ folder in project.");
                if (ImGui::Button("Create models/ folder")) std::filesystem::create_directories(dir);
            }
            ImGui::EndTabItem();
        }

        // ── Scripts ──────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Scripts"))
        {
            std::string dir = m_projectPath + "/scripts";
            if (std::filesystem::exists(dir))
            {
                for (const auto& e : std::filesystem::directory_iterator(dir))
                {
                    std::string name = e.path().filename().string();
                    std::string full = e.path().string();
                    ImGui::PushID(full.c_str());
                    if (ImGui::Button("Edit"))
                    {
                        m_editingScriptPath = full;
                        std::ifstream f(full);
                        m_editingScriptContent = { (std::istreambuf_iterator<char>(f)), {} };
                        m_showScriptEditor = true;
                    }
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.12f, 0.12f, 0.85f));
                    if (ImGui::Button("Del"))
                    {
                        std::filesystem::remove(full);
                        showStatus("Deleted: " + name, false);
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.90f, 0.70f, 1.0f));
                    ImGui::Text("[S] %s", name.c_str());
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
                ImGui::Spacing();
                if (ImGui::Button("+ New Script")) { m_showNewScriptPopup = true; m_newScriptNameInput[0] = '\0'; }
            }
            else
            {
                ImGui::TextDisabled("No scripts/ folder in project.");
                if (ImGui::Button("Create scripts/ folder")) std::filesystem::create_directories(dir);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End(); // Asset Browser

    // ── OBJ Import popup ─────────────────────────────────────────────────────
    if (m_showImportPopup) ImGui::OpenPopup("Import OBJ##modal");
    if (ImGui::BeginPopupModal("Import OBJ##modal", &m_showImportPopup, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Convert an OBJ file into a P3D model.");
        ImGui::Spacing();

        const float bw = sc(80.0f);
        ImGui::SetNextItemWidth(sc(380.0f) - bw - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputText("Source OBJ##src", m_importSourcePath, sizeof(m_importSourcePath));
        ImGui::SameLine();
        if (ImGui::Button("Browse##obj", ImVec2(bw, 0)))
        {
            std::string c = pickFile("OBJ Files\0*.obj\0All Files\0*.*\0");
            if (!c.empty()) strncpy(m_importSourcePath, c.c_str(), sizeof(m_importSourcePath) - 1);
        }
        ImGui::SetNextItemWidth(sc(260.0f));
        ImGui::InputText("Output Name (no ext)", m_importTargetName, sizeof(m_importTargetName));
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.48f, 0.24f, 1.0f));
        if (ImGui::Button("Convert & Import", ImVec2(sc(190.0f), sc(30.0f))))
        {
            std::string src = m_importSourcePath, name = m_importTargetName;
            if (!src.empty() && !name.empty())
            {
                std::string dst    = m_projectPath + "/models/" + name + ".p3d";
                std::string script = std::string(PROJECT_SOURCE_DIR) + "/apps/tools/GameEditor/scripts/obj2p3d.py";
                std::string cmd    = "python \"" + script + "\" \"" + src + "\" \"" + dst + "\"";
                int ret = std::system(cmd.c_str());
                showStatus(ret == 0 ? "Imported: " + name + ".p3d"
                                    : "Import failed (exit " + std::to_string(ret) + ")", ret != 0);
            }
            else showStatus("Fill in both fields.", true);
            ImGui::CloseCurrentPopup(); m_showImportPopup = false;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(sc(90.0f), sc(30.0f)))) { ImGui::CloseCurrentPopup(); m_showImportPopup = false; }
        ImGui::EndPopup();
    }

    // ── New Script popup ─────────────────────────────────────────────────────
    if (m_showNewScriptPopup) ImGui::OpenPopup("New Script##modal");
    if (ImGui::BeginPopupModal("New Script##modal", &m_showNewScriptPopup, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Enter a filename for the new script:");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(sc(300.0f));
        ImGui::InputText("##nsname", m_newScriptNameInput, sizeof(m_newScriptNameInput));
        ImGui::Spacing();
        if (ImGui::Button("Create", ImVec2(sc(110.0f), sc(28.0f))))
        {
            std::string nm = m_newScriptNameInput;
            if (!nm.empty())
            {
                std::string path = m_projectPath + "/scripts/" + nm;
                { std::ofstream f(path); f << "// " << nm << "\n\n"; }
                showStatus("Created: " + nm, false);
                m_editingScriptPath    = path;
                m_editingScriptContent = "// " + nm + "\n\n";
                m_showScriptEditor     = true;
            }
            ImGui::CloseCurrentPopup(); m_showNewScriptPopup = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(sc(80.0f), sc(28.0f)))) { ImGui::CloseCurrentPopup(); m_showNewScriptPopup = false; }
        ImGui::EndPopup();
    }

    // ── Script Editor modal ───────────────────────────────────────────────────
    if (m_showScriptEditor) ImGui::OpenPopup("Script Editor##modal");
    if (ImGui::BeginPopupModal("Script Editor##modal", &m_showScriptEditor, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Editing: %s", m_editingScriptPath.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        static char scriptBuf[65536];
        if (ImGui::IsWindowAppearing())
        {
            size_t len = std::min(m_editingScriptContent.size(), sizeof(scriptBuf) - 1);
            std::memcpy(scriptBuf, m_editingScriptContent.c_str(), len);
            scriptBuf[len] = '\0';
        }

        ImGui::InputTextMultiline("##scripteditor", scriptBuf, sizeof(scriptBuf),
                                  ImVec2(sc(680.0f), sc(400.0f)), ImGuiInputTextFlags_AllowTabInput);
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.48f, 0.24f, 1.0f));
        if (ImGui::Button("Save", ImVec2(sc(110.0f), sc(28.0f))))
        {
            { std::ofstream f(m_editingScriptPath); f << scriptBuf; }
            showStatus("Saved: " + m_editingScriptPath, false);
            ImGui::CloseCurrentPopup(); m_showScriptEditor = false;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(sc(80.0f), sc(28.0f)))) { ImGui::CloseCurrentPopup(); m_showScriptEditor = false; }
        ImGui::EndPopup();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// updateCamera — apply camera state to engine before 3D draw pipeline
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::updateCamera()
{
    if (!Poseidon::GScene || !Poseidon::GScene->GetCamera() || !Poseidon::GEngine)
        return;

    Poseidon::Camera* cam = Poseidon::GScene->GetCamera();

    // Compute direction vectors from yaw/pitch
    Vector3 fwd(std::sin(m_camYaw)*std::cos(m_camPitch), -std::sin(m_camPitch), std::cos(m_camYaw)*std::cos(m_camPitch));
    Vector3 rgt(std::cos(m_camYaw), 0.0f, -std::sin(m_camYaw));
    Vector3 up = fwd.CrossProduct(rgt);

    cam->SetPosition(m_camPos);
    cam->SetDirectionAndUp(fwd, up);

    // Set up perspective projection and adjust camera for this frame with explicit, valid defaults
    cam->SetPerspectiveForView(Poseidon::GEngine, 0.1f, 1000.0f, m_camFOV);
    cam->Adjust(Poseidon::GEngine);

    // Deep math logging: Camera properties and transforms
    static int s_camLogCounter = 0;
    if (++s_camLogCounter % 120 == 0)
    {
        LOG_INFO(Core, "[RENDER_MATH] Camera Pos: ({:.3f}, {:.3f}, {:.3f}), Yaw: {:.3f}, Pitch: {:.3f}, FOV: {:.3f}", 
                 m_camPos.X(), m_camPos.Y(), m_camPos.Z(), m_camYaw, m_camPitch, m_camFOV);
        LOG_INFO(Core, "[RENDER_MATH] Camera Vectors -> Fwd: ({:.3f}, {:.3f}, {:.3f}), Rgt: ({:.3f}, {:.3f}, {:.3f}), Up: ({:.3f}, {:.3f}, {:.3f})",
                 fwd.X(), fwd.Y(), fwd.Z(), rgt.X(), rgt.Y(), rgt.Z(), up.X(), up.Y(), up.Z());
        
        Matrix4 camInv = cam->InverseScaled();
        LOG_INFO(Core, "[RENDER_MATH] View Matrix (InvScaled) Translation: ({:.3f}, {:.3f}, {:.3f})", 
                 camInv.Position().X(), camInv.Position().Y(), camInv.Position().Z());
        
        const Matrix4& scaledInv = Poseidon::GScene->ScaledInvTransform();
        LOG_INFO(Core, "[RENDER_MATH] ScaledInvTransform Translation: ({:.3f}, {:.3f}, {:.3f})", 
                 scaledInv.Position().X(), scaledInv.Position().Y(), scaledInv.Position().Z());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// drawScene — submit 3D geometry into the engine pipeline
// ─────────────────────────────────────────────────────────────────────────────

void GameEditorApp::drawScene()
{
    m_scene.Draw();
}

} // namespace Poseidon
