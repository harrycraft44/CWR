#pragma once

#include <string>
#include <vector>
#include <memory>
#pragma push_macro("DebugLog")
#undef DebugLog
#include <imgui.h>
#pragma pop_macro("DebugLog")

namespace Poseidon {

class ParamFile;

struct EditorVehicle {
    std::string className;
    float position[3] = {0.0f, 0.0f, 0.0f}; // X, Alt, Y
    float azimuth = 0.0f;
    int id = 0;
    std::string side;
    std::string player;
    bool leader = false;
    float skill = 0.5f;
    std::string init;
    std::string special;
};

struct EditorGroup {
    std::string side;
    std::vector<EditorVehicle> vehicles;
};

struct EditorMarker {
    std::string name;
    std::string text;
    float position[3] = {0.0f, 0.0f, 0.0f};
    std::string type = "Flag";
    std::string colorName = "ColorBlue";
};

struct EditorTrigger {
    std::string name;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float a = 100.0f;
    float b = 100.0f;
    float angle = 0.0f;
    bool rectangular = false;
    std::string activationBy = "WEST";
    std::string activationType = "PRESENT";
    std::string expCond = "true";
    std::string expActiv;
    std::string expDesactiv;
};

struct EditorMission {
    float startWeather = 0.3f;
    float forecastWeather = 0.3f;
    float hour = 12.0f;
    std::string briefingName = "Custom Editor Mission";
    float viewDistance = 1000.0f;

    std::vector<EditorGroup> groups;
    std::vector<EditorVehicle> emptyVehicles;
    std::vector<EditorMarker> markers;
    std::vector<EditorTrigger> triggers;
};

class MissionEditor {
public:
    MissionEditor();
    ~MissionEditor();

    void init();
    bool load(const ParamFile& cf);
    bool save(const std::string& path);
    void render(float w, float h);

    bool isLoaded() const { return m_loaded; }
    void clear();

    // Asset Browser placement helper
    void placeAsset(const std::string& modelPathOrClass);

private:
    void renderViewport(float w, float h);
    void renderHierarchy(float w, float h);
    void renderInspector(float w, float h);
    void renderToolbar();

    void drawGrid(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
    void drawEntities(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
    void handleSelectionAndDrag(const ImVec2& canvasPos, const ImVec2& canvasSize);
    void createNewEntityAt(float wx, float wy);

    // Helpers
    ImVec2 worldToScreen(float wx, float wy, const ImVec2& canvasPos) const;
    ImVec2 screenToWorld(const ImVec2& screenPos, const ImVec2& canvasPos) const;

    EditorMission m_mission;
    bool m_loaded = false;

    // Viewport state
    ImVec2 m_panOffset = {400.0f, 300.0f};
    float m_zoom = 1.0f;
    bool m_panning = false;

    // Selection & Drag state
    int m_selectedType = 0; // 0=None, 1=Vehicle (Grouped), 2=Vehicle (Empty), 3=Marker, 4=Trigger
    int m_selectedGroupIdx = -1;
    int m_selectedElementIdx = -1;
    bool m_dragging = false;

    // Placement Tool state
    enum class ToolMode {
        Select,
        PlaceUnit,
        PlaceEmptyVehicle,
        PlaceTrigger,
        PlaceMarker
    };
    ToolMode m_toolMode = ToolMode::Select;
    std::string m_placementClass = "SoldierWB";
    std::string m_placementSide = "WEST";

    // Presets and options
    bool m_showGrid = true;
    bool m_showLabels = true;
};

} // namespace Poseidon
