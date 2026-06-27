#include "MissionEditor.hpp"
#include <Poseidon/IO/ParamFile/ParamFile.hpp>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <filesystem>

namespace Poseidon {

static void LoadMission(const ParamFile& cf, EditorMission& mission) {
    mission = EditorMission(); // Reset

    const ParamClass* missionClass = cf.GetClass("Mission");
    if (!missionClass) return;

    // Read Intel
    const ParamClass* intelClass = missionClass->GetClass("Intel");
    if (intelClass) {
        if (auto* e = intelClass->FindEntryNoInheritance("startWeather")) mission.startWeather = (float)*e;
        if (auto* e = intelClass->FindEntryNoInheritance("forecastWeather")) mission.forecastWeather = (float)*e;
        if (auto* e = intelClass->FindEntryNoInheritance("hour")) mission.hour = (float)*e;
        if (auto* e = intelClass->FindEntryNoInheritance("briefingName")) mission.briefingName = e->GetValue().Data();
    }

    // Read Groups
    const ParamClass* groupsClass = missionClass->GetClass("Groups");
    if (groupsClass) {
        int items = 0;
        if (auto* e = groupsClass->FindEntryNoInheritance("items")) items = (int)*e;
        for (int i = 0; i < items; ++i) {
            std::string itemName = "Item" + std::to_string(i);
            const ParamClass* itemClass = groupsClass->GetClass(itemName.c_str());
            if (!itemClass) continue;

            EditorGroup grp;
            if (auto* e = itemClass->FindEntryNoInheritance("side")) grp.side = e->GetValue().Data();

            const ParamClass* vehiclesClass = itemClass->GetClass("Vehicles");
            if (vehiclesClass) {
                int vitems = 0;
                if (auto* e = vehiclesClass->FindEntryNoInheritance("items")) vitems = (int)*e;
                for (int j = 0; j < vitems; ++j) {
                    std::string vitemName = "Item" + std::to_string(j);
                    const ParamClass* vitemClass = vehiclesClass->GetClass(vitemName.c_str());
                    if (!vitemClass) continue;

                    EditorVehicle v;
                    if (auto* e = vitemClass->FindEntryNoInheritance("vehicle")) v.className = e->GetValue().Data();
                    if (auto* e = vitemClass->FindEntryNoInheritance("side")) v.side = e->GetValue().Data();
                    if (auto* e = vitemClass->FindEntryNoInheritance("player")) v.player = e->GetValue().Data();
                    if (auto* e = vitemClass->FindEntryNoInheritance("special")) v.special = e->GetValue().Data();
                    if (auto* e = vitemClass->FindEntryNoInheritance("id")) v.id = (int)*e;
                    if (auto* e = vitemClass->FindEntryNoInheritance("azimut")) v.azimuth = (float)*e;
                    if (auto* e = vitemClass->FindEntryNoInheritance("skill")) v.skill = (float)*e;
                    if (auto* e = vitemClass->FindEntryNoInheritance("leader")) v.leader = ((int)*e == 1);
                    if (auto* e = vitemClass->FindEntryNoInheritance("init")) v.init = e->GetValue().Data();

                    if (auto* e = vitemClass->FindEntryNoInheritance("position[]")) {
                        if (e->IsArray() && e->GetSize() >= 3) {
                            v.position[0] = (*e)[0].GetFloat();
                            v.position[1] = (*e)[1].GetFloat();
                            v.position[2] = (*e)[2].GetFloat();
                        }
                    }
                    grp.vehicles.push_back(v);
                }
            }
            mission.groups.push_back(grp);
        }
    }

    // Read Empty Vehicles
    const ParamClass* emptyClass = missionClass->GetClass("Vehicles");
    if (emptyClass) {
        int items = 0;
        if (auto* e = emptyClass->FindEntryNoInheritance("items")) items = (int)*e;
        for (int i = 0; i < items; ++i) {
            std::string itemName = "Item" + std::to_string(i);
            const ParamClass* itemClass = emptyClass->GetClass(itemName.c_str());
            if (!itemClass) continue;

            EditorVehicle v;
            v.side = "EMPTY";
            if (auto* e = itemClass->FindEntryNoInheritance("vehicle")) v.className = e->GetValue().Data();
            if (auto* e = itemClass->FindEntryNoInheritance("id")) v.id = (int)*e;
            if (auto* e = itemClass->FindEntryNoInheritance("azimut")) v.azimuth = (float)*e;
            if (auto* e = itemClass->FindEntryNoInheritance("skill")) v.skill = (float)*e;
            if (auto* e = itemClass->FindEntryNoInheritance("init")) v.init = e->GetValue().Data();

            if (auto* e = itemClass->FindEntryNoInheritance("position[]")) {
                if (e->IsArray() && e->GetSize() >= 3) {
                    v.position[0] = (*e)[0].GetFloat();
                    v.position[1] = (*e)[1].GetFloat();
                    v.position[2] = (*e)[2].GetFloat();
                }
            }
            mission.emptyVehicles.push_back(v);
        }
    }

    // Read Markers
    const ParamClass* markersClass = missionClass->GetClass("Markers");
    if (markersClass) {
        int items = 0;
        if (auto* e = markersClass->FindEntryNoInheritance("items")) items = (int)*e;
        for (int i = 0; i < items; ++i) {
            std::string itemName = "Item" + std::to_string(i);
            const ParamClass* itemClass = markersClass->GetClass(itemName.c_str());
            if (!itemClass) continue;

            EditorMarker m;
            if (auto* e = itemClass->FindEntryNoInheritance("name")) m.name = e->GetValue().Data();
            if (auto* e = itemClass->FindEntryNoInheritance("text")) m.text = e->GetValue().Data();
            if (auto* e = itemClass->FindEntryNoInheritance("type")) m.type = e->GetValue().Data();
            if (auto* e = itemClass->FindEntryNoInheritance("colorName")) m.colorName = e->GetValue().Data();

            if (auto* e = itemClass->FindEntryNoInheritance("position[]")) {
                if (e->IsArray() && e->GetSize() >= 3) {
                    m.position[0] = (*e)[0].GetFloat();
                    m.position[1] = (*e)[1].GetFloat();
                    m.position[2] = (*e)[2].GetFloat();
                }
            }
            mission.markers.push_back(m);
        }
    }

    // Read Sensors
    const ParamClass* sensorsClass = missionClass->GetClass("Sensors");
    if (sensorsClass) {
        int items = 0;
        if (auto* e = sensorsClass->FindEntryNoInheritance("items")) items = (int)*e;
        for (int i = 0; i < items; ++i) {
            std::string itemName = "Item" + std::to_string(i);
            const ParamClass* itemClass = sensorsClass->GetClass(itemName.c_str());
            if (!itemClass) continue;

            EditorTrigger t;
            if (auto* e = itemClass->FindEntryNoInheritance("text")) t.name = e->GetValue().Data();
            if (auto* e = itemClass->FindEntryNoInheritance("a")) t.a = (float)*e;
            if (auto* e = itemClass->FindEntryNoInheritance("b")) t.b = (float)*e;
            if (auto* e = itemClass->FindEntryNoInheritance("angle")) t.angle = (float)*e;
            if (auto* e = itemClass->FindEntryNoInheritance("rectangular")) t.rectangular = ((int)*e == 1);
            if (auto* e = itemClass->FindEntryNoInheritance("activationBy")) t.activationBy = e->GetValue().Data();
            if (auto* e = itemClass->FindEntryNoInheritance("activationType")) t.activationType = e->GetValue().Data();
            if (auto* e = itemClass->FindEntryNoInheritance("expCond")) t.expCond = e->GetValue().Data();
            if (auto* e = itemClass->FindEntryNoInheritance("expActiv")) t.expActiv = e->GetValue().Data();
            if (auto* e = itemClass->FindEntryNoInheritance("expDesactiv")) t.expDesactiv = e->GetValue().Data();

            if (auto* e = itemClass->FindEntryNoInheritance("position[]")) {
                if (e->IsArray() && e->GetSize() >= 3) {
                    t.position[0] = (*e)[0].GetFloat();
                    t.position[1] = (*e)[1].GetFloat();
                    t.position[2] = (*e)[2].GetFloat();
                }
            }
            mission.triggers.push_back(t);
        }
    }
}

static void SaveMission(const std::string& path, const EditorMission& mission) {
    std::ofstream out(path);
    if (!out.is_open()) return;

    out << "version=11;\n";
    out << "class Mission\n{\n";
    out << "    randomSeed=9000009;\n";
    out << "    class Intel\n    {\n";
    out << "        startWeather=" << std::fixed << std::setprecision(6) << mission.startWeather << ";\n";
    out << "        forecastWeather=" << mission.forecastWeather << ";\n";
    out << "        hour=" << mission.hour << ";\n";
    if (!mission.briefingName.empty()) {
        out << "        briefingName=\"" << mission.briefingName << "\";\n";
    }
    out << "    };\n";

    // Groups
    out << "    class Groups\n    {\n";
    out << "        items=" << mission.groups.size() << ";\n";
    for (size_t i = 0; i < mission.groups.size(); ++i) {
        const auto& grp = mission.groups[i];
        out << "        class Item" << i << "\n        {\n";
        out << "            side=\"" << grp.side << "\";\n";
        out << "            class Vehicles\n            {\n";
        out << "                items=" << grp.vehicles.size() << ";\n";
        for (size_t j = 0; j < grp.vehicles.size(); ++j) {
            const auto& v = grp.vehicles[j];
            out << "                class Item" << j << "\n                {\n";
            out << "                    position[]={" << std::fixed << std::setprecision(6) << v.position[0] << "," << v.position[1] << "," << v.position[2] << "};\n";
            out << "                    azimut=" << v.azimuth << ";\n";
            if (!v.special.empty()) out << "                    special=\"" << v.special << "\";\n";
            out << "                    id=" << v.id << ";\n";
            out << "                    side=\"" << v.side << "\";\n";
            out << "                    vehicle=\"" << v.className << "\";\n";
            if (v.leader) out << "                    leader=1;\n";
            if (!v.player.empty()) out << "                    player=\"" << v.player << "\";\n";
            out << "                    skill=" << v.skill << ";\n";
            if (!v.init.empty()) {
                std::string escapedInit = v.init;
                size_t pos = 0;
                while ((pos = escapedInit.find('"', pos)) != std::string::npos) {
                    escapedInit.replace(pos, 1, "\"\"");
                    pos += 2;
                }
                out << "                    init=\"" << escapedInit << "\";\n";
            }
            out << "                };\n";
        }
        out << "            };\n";
        out << "        };\n";
    }
    out << "    };\n";

    // Empty Vehicles
    out << "    class Vehicles\n    {\n";
    out << "        items=" << mission.emptyVehicles.size() << ";\n";
    for (size_t i = 0; i < mission.emptyVehicles.size(); ++i) {
        const auto& v = mission.emptyVehicles[i];
        out << "        class Item" << i << "\n        {\n";
        out << "            position[]={" << std::fixed << std::setprecision(6) << v.position[0] << "," << v.position[1] << "," << v.position[2] << "};\n";
        out << "            azimut=" << v.azimuth << ";\n";
        out << "            id=" << v.id << ";\n";
        out << "            side=\"EMPTY\";\n";
        out << "            vehicle=\"" << v.className << "\";\n";
        out << "            skill=" << v.skill << ";\n";
        if (!v.init.empty()) {
            std::string escapedInit = v.init;
            size_t pos = 0;
            while ((pos = escapedInit.find('"', pos)) != std::string::npos) {
                escapedInit.replace(pos, 1, "\"\"");
                pos += 2;
            }
            out << "            init=\"" << escapedInit << "\";\n";
        }
        out << "        };\n";
    }
    out << "    };\n";

    // Markers
    out << "    class Markers\n    {\n";
    out << "        items=" << mission.markers.size() << ";\n";
    for (size_t i = 0; i < mission.markers.size(); ++i) {
        const auto& m = mission.markers[i];
        out << "        class Item" << i << "\n        {\n";
        out << "            position[]={" << std::fixed << std::setprecision(6) << m.position[0] << "," << m.position[1] << "," << m.position[2] << "};\n";
        out << "            name=\"" << m.name << "\";\n";
        if (!m.text.empty()) out << "            text=\"" << m.text << "\";\n";
        out << "            type=\"" << m.type << "\";\n";
        if (!m.colorName.empty()) out << "            colorName=\"" << m.colorName << "\";\n";
        out << "        };\n";
    }
    out << "    };\n";

    // Sensors (Triggers)
    out << "    class Sensors\n    {\n";
    out << "        items=" << mission.triggers.size() << ";\n";
    for (size_t i = 0; i < mission.triggers.size(); ++i) {
        const auto& t = mission.triggers[i];
        out << "        class Item" << i << "\n        {\n";
        out << "            position[]={" << std::fixed << std::setprecision(6) << t.position[0] << "," << t.position[1] << "," << t.position[2] << "};\n";
        out << "            a=" << t.a << ";\n";
        out << "            b=" << t.b << ";\n";
        out << "            activationBy=\"" << t.activationBy << "\";\n";
        out << "            activationType=\"" << t.activationType << "\";\n";
        if (t.rectangular) out << "            rectangular=1;\n";
        if (!t.name.empty()) out << "            text=\"" << t.name << "\";\n";
        if (!t.expCond.empty()) {
            std::string escapedCond = t.expCond;
            size_t pos = 0;
            while ((pos = escapedCond.find('"', pos)) != std::string::npos) {
                escapedCond.replace(pos, 1, "\"\"");
                pos += 2;
            }
            out << "            expCond=\"" << escapedCond << "\";\n";
        } else {
            out << "            expCond=\"true\";\n";
        }
        if (!t.expActiv.empty()) {
            std::string escapedAct = t.expActiv;
            size_t pos = 0;
            while ((pos = escapedAct.find('"', pos)) != std::string::npos) {
                escapedAct.replace(pos, 1, "\"\"");
                pos += 2;
            }
            out << "            expActiv=\"" << escapedAct << "\";\n";
        }
        if (!t.expDesactiv.empty()) {
            std::string escapedDes = t.expDesactiv;
            size_t pos = 0;
            while ((pos = escapedDes.find('"', pos)) != std::string::npos) {
                escapedDes.replace(pos, 1, "\"\"");
                pos += 2;
            }
            out << "            expDesactiv=\"" << escapedDes << "\";\n";
        }
        out << "        };\n";
    }
    out << "    };\n";

    out << "};\n";
    out.close();
}

MissionEditor::MissionEditor() {}
MissionEditor::~MissionEditor() {}

void MissionEditor::init() {
    clear();
}

void MissionEditor::clear() {
    m_mission = EditorMission();
    m_loaded = false;
    m_selectedType = 0;
    m_selectedGroupIdx = -1;
    m_selectedElementIdx = -1;
    m_dragging = false;
    m_toolMode = ToolMode::Select;
}

bool MissionEditor::load(const ParamFile& cf) {
    try {
        LoadMission(cf, m_mission);
        m_loaded = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool MissionEditor::save(const std::string& path) {
    if (!m_loaded) return false;
    try {
        SaveMission(path, m_mission);
        return true;
    } catch (...) {
        return false;
    }
}

ImVec2 MissionEditor::worldToScreen(float wx, float wy, const ImVec2& canvasPos) const {
    return ImVec2(canvasPos.x + m_panOffset.x + wx * m_zoom, canvasPos.y + m_panOffset.y - wy * m_zoom);
}

ImVec2 MissionEditor::screenToWorld(const ImVec2& screenPos, const ImVec2& canvasPos) const {
    return ImVec2((screenPos.x - canvasPos.x - m_panOffset.x) / m_zoom, -(screenPos.y - canvasPos.y - m_panOffset.y) / m_zoom);
}

void MissionEditor::placeAsset(const std::string& modelPathOrClass) {
    if (!m_loaded) return;
    // Spawns an empty vehicle at the map center
    EditorVehicle v;
    v.className = std::filesystem::path(modelPathOrClass).stem().string();
    v.side = "EMPTY";
    v.position[0] = (-m_panOffset.x + 400.0f) / m_zoom; // placing near center
    v.position[1] = 0.0f;
    v.position[2] = (m_panOffset.y - 300.0f) / m_zoom;
    v.azimuth = 0.0f;
    v.id = 100 + (int)m_mission.emptyVehicles.size();
    v.skill = 0.5f;
    m_mission.emptyVehicles.push_back(v);

    m_selectedType = 2; // Empty Vehicle
    m_selectedGroupIdx = -1;
    m_selectedElementIdx = (int)m_mission.emptyVehicles.size() - 1;
}

void MissionEditor::render(float w, float h) {
    if (!m_loaded) {
        ImGui::Text("No mission file loaded. Open a mission.sqm file or click 'Create Presets' above.");
        return;
    }

    renderToolbar();

    float leftSideW = w * 0.25f;
    float rightSideW = w * 0.25f;
    float centerW = w - leftSideW - rightSideW - 12.0f;

    renderHierarchy(leftSideW, h - 35.0f);
    ImGui::SameLine();
    renderViewport(centerW, h - 35.0f);
    ImGui::SameLine();
    renderInspector(rightSideW, h - 35.0f);
}

void MissionEditor::renderToolbar() {
    ImGui::BeginGroup();
    
    // Tools selector
    if (ImGui::RadioButton("Select (Esc)", m_toolMode == ToolMode::Select)) m_toolMode = ToolMode::Select;
    ImGui::SameLine();
    if (ImGui::RadioButton("Place Unit", m_toolMode == ToolMode::PlaceUnit)) m_toolMode = ToolMode::PlaceUnit;
    ImGui::SameLine();
    if (ImGui::RadioButton("Place Prop/Empty", m_toolMode == ToolMode::PlaceEmptyVehicle)) m_toolMode = ToolMode::PlaceEmptyVehicle;
    ImGui::SameLine();
    if (ImGui::RadioButton("Place Trigger", m_toolMode == ToolMode::PlaceTrigger)) m_toolMode = ToolMode::PlaceTrigger;
    ImGui::SameLine();
    if (ImGui::RadioButton("Place Marker", m_toolMode == ToolMode::PlaceMarker)) m_toolMode = ToolMode::PlaceMarker;

    ImGui::SameLine(0, 30);
    ImGui::Checkbox("Grid", &m_showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Labels", &m_showLabels);

    if (m_toolMode == ToolMode::PlaceUnit) {
        ImGui::SameLine(0, 30);
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::BeginCombo("Side##pl", m_placementSide.c_str())) {
            const char* sides[] = {"WEST", "EAST", "GUER", "CIV", "LOGIC"};
            for (int s = 0; s < 5; s++) {
                if (ImGui::Selectable(sides[s], m_placementSide == sides[s])) {
                    m_placementSide = sides[s];
                    if (m_placementSide == "WEST") m_placementClass = "SoldierWB";
                    else if (m_placementSide == "EAST") m_placementClass = "SoldierEB";
                    else if (m_placementSide == "GUER") m_placementClass = "SoldierGB";
                    else if (m_placementSide == "CIV") m_placementClass = "Civ1";
                    else m_placementClass = "Logic";
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("Class##pl", m_placementClass.c_str())) {
            std::vector<std::string> classes;
            if (m_placementSide == "WEST") classes = {"SoldierWB", "HeavyGrenadier", "M113", "M60", "M1A1", "UH60", "A10"};
            else if (m_placementSide == "EAST") classes = {"SoldierEB", "Grenadier", "BMP", "T72", "T80", "Mi24", "Su25"};
            else if (m_placementSide == "GUER") classes = {"SoldierGB", "Grenadier", "T55", "BMP_Res"};
            else if (m_placementSide == "CIV") classes = {"Civ1", "Tractor", "Skoda"};
            else classes = {"Logic", "GameLogic"};
            for (const auto& cls : classes) {
                if (ImGui::Selectable(cls.c_str(), m_placementClass == cls)) m_placementClass = cls;
            }
            ImGui::EndCombo();
        }
    } else if (m_toolMode == ToolMode::PlaceEmptyVehicle) {
        ImGui::SameLine(0, 30);
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::BeginCombo("Prop Class##pl", m_placementClass.c_str())) {
            std::vector<std::string> emptyClasses = {"Jeep", "Truck5t", "UAZ", "WeaponBoxWest", "WeaponBoxEast", "Hedgehog", "BarbedWire", "FlagPoleWest"};
            for (const auto& cls : emptyClasses) {
                if (ImGui::Selectable(cls.c_str(), m_placementClass == cls)) m_placementClass = cls;
            }
            ImGui::EndCombo();
        }
    }

    ImGui::EndGroup();
    ImGui::Separator();
}

void MissionEditor::renderHierarchy(float w, float h) {
    ImGui::BeginChild("HierarchyView", ImVec2(w, h), ImGuiChildFlags_Borders);
    ImGui::Text("Scene Hierarchy");
    ImGui::Separator();

    if (ImGui::TreeNodeEx("Intel Properties", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        if (ImGui::IsItemClicked()) {
            m_selectedType = 5; // Intel
            m_selectedGroupIdx = -1;
            m_selectedElementIdx = -1;
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Groups")) {
        for (int i = 0; i < (int)m_mission.groups.size(); i++) {
            const auto& g = m_mission.groups[i];
            std::string groupLabel = "Group " + std::to_string(i) + " (" + g.side + ")";
            if (ImGui::TreeNode(groupLabel.c_str())) {
                for (int j = 0; j < (int)g.vehicles.size(); j++) {
                    const auto& v = g.vehicles[j];
                    std::string vLabel = v.className + (v.leader ? " [Leader]" : "");
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
                    if (m_selectedType == 1 && m_selectedGroupIdx == i && m_selectedElementIdx == j)
                        flags |= ImGuiTreeNodeFlags_Selected;
                    
                    if (ImGui::TreeNodeEx(vLabel.c_str(), flags)) {
                        if (ImGui::IsItemClicked()) {
                            m_selectedType = 1;
                            m_selectedGroupIdx = i;
                            m_selectedElementIdx = j;
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Empty Vehicles / Props")) {
        for (int i = 0; i < (int)m_mission.emptyVehicles.size(); i++) {
            const auto& v = m_mission.emptyVehicles[i];
            std::string label = v.className + " (ID: " + std::to_string(v.id) + ")";
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (m_selectedType == 2 && m_selectedElementIdx == i)
                flags |= ImGuiTreeNodeFlags_Selected;

            if (ImGui::TreeNodeEx(label.c_str(), flags)) {
                if (ImGui::IsItemClicked()) {
                    m_selectedType = 2;
                    m_selectedGroupIdx = -1;
                    m_selectedElementIdx = i;
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Markers")) {
        for (int i = 0; i < (int)m_mission.markers.size(); i++) {
            const auto& m = m_mission.markers[i];
            std::string label = m.name + " (\"" + m.text + "\")";
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (m_selectedType == 3 && m_selectedElementIdx == i)
                flags |= ImGuiTreeNodeFlags_Selected;

            if (ImGui::TreeNodeEx(label.c_str(), flags)) {
                if (ImGui::IsItemClicked()) {
                    m_selectedType = 3;
                    m_selectedGroupIdx = -1;
                    m_selectedElementIdx = i;
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Triggers (Sensors)")) {
        for (int i = 0; i < (int)m_mission.triggers.size(); i++) {
            const auto& t = m_mission.triggers[i];
            std::string label = (t.name.empty() ? "Trigger " + std::to_string(i) : t.name);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (m_selectedType == 4 && m_selectedElementIdx == i)
                flags |= ImGuiTreeNodeFlags_Selected;

            if (ImGui::TreeNodeEx(label.c_str(), flags)) {
                if (ImGui::IsItemClicked()) {
                    m_selectedType = 4;
                    m_selectedGroupIdx = -1;
                    m_selectedElementIdx = i;
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    ImGui::EndChild();
}

void MissionEditor::renderViewport(float w, float h) {
    ImGui::BeginChild("ViewportView", ImVec2(w, h), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
    if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(24, 24, 28, 255));

    // Handle view Panning & Zooming
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePosInCanvas = ImVec2(io.MousePos.x - canvasPos.x, io.MousePos.y - canvasPos.y);

    if (ImGui::IsWindowHovered()) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            m_panOffset.x += io.MouseDelta.x;
            m_panOffset.y += io.MouseDelta.y;
            m_panning = true;
        } else {
            m_panning = false;
        }

        if (io.MouseWheel != 0.0f) {
            float oldZoom = m_zoom;
            m_zoom += io.MouseWheel * 0.1f * m_zoom;
            m_zoom = std::clamp(m_zoom, 0.05f, 15.0f);

            m_panOffset.x = mousePosInCanvas.x - (mousePosInCanvas.x - m_panOffset.x) * (m_zoom / oldZoom);
            m_panOffset.y = mousePosInCanvas.y - (mousePosInCanvas.y - m_panOffset.y) * (m_zoom / oldZoom);
        }
    }

    drawGrid(drawList, canvasPos, canvasSize);
    drawEntities(drawList, canvasPos, canvasSize);
    handleSelectionAndDrag(canvasPos, canvasSize);

    // Render viewport bounds overlay
    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(80, 80, 90, 255), 0.0f, 0, 1.5f);

    // Zoom indicator in viewport corner
    char zoomBuf[32];
    snprintf(zoomBuf, sizeof(zoomBuf), "Zoom: %.1f%%", m_zoom * 100.0f);
    drawList->AddText(ImVec2(canvasPos.x + 10.0f, canvasPos.y + 10.0f), IM_COL32(200, 200, 210, 180), zoomBuf);

    ImGui::EndChild();
}

void MissionEditor::drawGrid(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
    if (!m_showGrid) return;

    // Grid spacing adapts dynamically to zoom level
    float gridMeters = 100.0f;
    if (m_zoom > 2.0f) gridMeters = 10.0f;
    else if (m_zoom < 0.2f) gridMeters = 1000.0f;

    float gridSpacingPixels = gridMeters * m_zoom;

    // Calculate start world position
    ImVec2 startWorld = screenToWorld(canvasPos, canvasPos);
    ImVec2 endWorld = screenToWorld(ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), canvasPos);

    float startX = std::floor(startWorld.x / gridMeters) * gridMeters;
    float startY = std::floor(endWorld.y / gridMeters) * gridMeters; // screen Y is down, so world Y is inverted
    float endX = std::ceil(endWorld.x / gridMeters) * gridMeters;
    float endY = std::ceil(startWorld.y / gridMeters) * gridMeters;

    // Draw Vertical lines
    for (float wx = startX; wx <= endX; wx += gridMeters) {
        ImVec2 screenStart = worldToScreen(wx, startY, canvasPos);
        ImVec2 screenEnd = worldToScreen(wx, endY, canvasPos);
        
        // check canvas boundaries
        screenStart.y = std::max(canvasPos.y, screenStart.y);
        screenEnd.y = std::min(canvasPos.y + canvasSize.y, screenEnd.y);

        ImU32 color = (wx == 0.0f) ? IM_COL32(100, 100, 255, 100) : IM_COL32(60, 60, 65, 80);
        drawList->AddLine(screenStart, screenEnd, color, 1.0f);

        if (m_showLabels && wx >= startWorld.x && wx <= endWorld.x) {
            char label[32];
            snprintf(label, sizeof(label), "%d", (int)wx);
            drawList->AddText(ImVec2(screenStart.x + 2.0f, canvasPos.y + canvasSize.y - 18.0f), IM_COL32(120, 120, 130, 120), label);
        }
    }

    // Draw Horizontal lines
    for (float wy = startY; wy <= endY; wy += gridMeters) {
        ImVec2 screenStart = worldToScreen(startX, wy, canvasPos);
        ImVec2 screenEnd = worldToScreen(endX, wy, canvasPos);

        screenStart.x = std::max(canvasPos.x, screenStart.x);
        screenEnd.x = std::min(canvasPos.x + canvasSize.x, screenEnd.x);

        ImU32 color = (wy == 0.0f) ? IM_COL32(100, 100, 255, 100) : IM_COL32(60, 60, 65, 80);
        drawList->AddLine(screenStart, screenEnd, color, 1.0f);

        if (m_showLabels && wy >= endWorld.y && wy <= startWorld.y) {
            char label[32];
            snprintf(label, sizeof(label), "%d", (int)wy);
            drawList->AddText(ImVec2(canvasPos.x + 5.0f, screenStart.y - 14.0f), IM_COL32(120, 120, 130, 120), label);
        }
    }
}

void MissionEditor::drawEntities(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
    // Colors helper
    auto GetSideColor = [](const std::string& side) -> ImU32 {
        if (side == "WEST") return IM_COL32(0, 229, 255, 220); // Neon Blue
        if (side == "EAST") return IM_COL32(255, 23, 68, 220);  // Neon Red
        if (side == "GUER") return IM_COL32(0, 230, 118, 220); // Neon Green
        if (side == "CIV") return IM_COL32(255, 234, 0, 220);  // Golden Yellow
        if (side == "LOGIC") return IM_COL32(213, 0, 249, 220); // Purple
        return IM_COL32(176, 190, 197, 220); // Empty Gray
    };

    // Draw Triggers
    for (int i = 0; i < (int)m_mission.triggers.size(); i++) {
        const auto& t = m_mission.triggers[i];
        ImVec2 screenCenter = worldToScreen(t.position[0], t.position[2], canvasPos);

        // check if inside canvas roughly
        if (screenCenter.x < canvasPos.x - 2000.0f || screenCenter.x > canvasPos.x + canvasSize.x + 2000.0f) continue;

        ImU32 triggerColor = IM_COL32(0, 229, 255, 30);
        ImU32 borderColor = IM_COL32(0, 229, 255, 100);
        if (m_selectedType == 4 && m_selectedElementIdx == i) {
            triggerColor = IM_COL32(255, 234, 0, 45);
            borderColor = IM_COL32(255, 234, 0, 200);
        }

        // Draw sensory radius (ellipse)
        float sa = t.a * m_zoom;
        float sb = t.b * m_zoom;
        if (t.rectangular) {
            // Rectangular trigger
            // Draw simple axis-aligned rect for simplicity, rotating if angle set
            drawList->AddRectFilled(ImVec2(screenCenter.x - sa, screenCenter.y - sb), ImVec2(screenCenter.x + sa, screenCenter.y + sb), triggerColor);
            drawList->AddRect(ImVec2(screenCenter.x - sa, screenCenter.y - sb), ImVec2(screenCenter.x + sa, screenCenter.y + sb), borderColor, 0.0f, 0, 1.5f);
        } else {
            // Circle/Ellipse trigger
            drawList->AddEllipseFilled(screenCenter, ImVec2(sa, sb), triggerColor);
            drawList->AddEllipse(screenCenter, ImVec2(sa, sb), borderColor, 1.5f);
        }
        
        // Trigger center indicator
        drawList->AddCircleFilled(screenCenter, 3.0f, borderColor);
        if (!t.name.empty()) {
            drawList->AddText(ImVec2(screenCenter.x + 6.0f, screenCenter.y - 12.0f), borderColor, t.name.c_str());
        }
    }

    // Draw Group lines connecting members to leaders
    for (int i = 0; i < (int)m_mission.groups.size(); i++) {
        const auto& grp = m_mission.groups[i];
        if (grp.vehicles.empty()) continue;

        // Leader is typically vehicles[0] or vehicles marked leader=true
        const EditorVehicle* leader = nullptr;
        for (const auto& v : grp.vehicles) {
            if (v.leader) {
                leader = &v;
                break;
            }
        }
        if (!leader && !grp.vehicles.empty()) leader = &grp.vehicles[0];

        if (!leader) continue;

        ImVec2 leaderScreen = worldToScreen(leader->position[0], leader->position[2], canvasPos);

        for (const auto& v : grp.vehicles) {
            if (&v == leader) continue;
            ImVec2 screenPos = worldToScreen(v.position[0], v.position[2], canvasPos);
            drawList->AddLine(screenPos, leaderScreen, IM_COL32(0, 229, 255, 120), 1.0f); // dashed cyan line
        }
    }

    // Draw Grouped vehicles/soldiers
    for (int i = 0; i < (int)m_mission.groups.size(); i++) {
        const auto& grp = m_mission.groups[i];
        for (int j = 0; j < (int)grp.vehicles.size(); j++) {
            const auto& v = grp.vehicles[j];
            ImVec2 screenPos = worldToScreen(v.position[0], v.position[2], canvasPos);

            ImU32 fill = GetSideColor(grp.side);
            bool isSelected = (m_selectedType == 1 && m_selectedGroupIdx == i && m_selectedElementIdx == j);

            // Draw unit circle
            drawList->AddCircleFilled(screenPos, 8.0f, fill);
            
            // Draw Azimuth direction arrow
            float rad = (v.azimuth) * (3.14159265f / 180.0f);
            float dx = sin(rad);
            float dy = cos(rad);
            ImVec2 dirEnd = ImVec2(screenPos.x + dx * 16.0f, screenPos.y - dy * 16.0f);
            drawList->AddLine(screenPos, dirEnd, IM_COL32(255, 255, 255, 200), 1.5f);

            // Glowing outline if selected
            if (isSelected) {
                drawList->AddCircle(screenPos, 11.0f, IM_COL32(255, 234, 0, 255), 24, 2.0f);
            } else {
                drawList->AddCircle(screenPos, 8.0f, IM_COL32(255, 255, 255, 220), 24, 1.0f);
            }

            // Draw label
            if (m_showLabels) {
                drawList->AddText(ImVec2(screenPos.x + 10.0f, screenPos.y - 12.0f), IM_COL32(240, 240, 245, 200), v.className.c_str());
            }
        }
    }

    // Draw Empty vehicles/props
    for (int i = 0; i < (int)m_mission.emptyVehicles.size(); i++) {
        const auto& v = m_mission.emptyVehicles[i];
        ImVec2 screenPos = worldToScreen(v.position[0], v.position[2], canvasPos);

        ImU32 fill = IM_COL32(130, 140, 150, 180);
        bool isSelected = (m_selectedType == 2 && m_selectedElementIdx == i);

        // Empty vehicles are drawn as squares
        float size = 7.0f;
        drawList->AddRectFilled(ImVec2(screenPos.x - size, screenPos.y - size), ImVec2(screenPos.x + size, screenPos.y + size), fill);
        
        // Draw azimuth heading line
        float rad = (v.azimuth) * (3.14159265f / 180.0f);
        float dx = sin(rad);
        float dy = cos(rad);
        ImVec2 dirEnd = ImVec2(screenPos.x + dx * 14.0f, screenPos.y - dy * 14.0f);
        drawList->AddLine(screenPos, dirEnd, IM_COL32(220, 220, 225, 200), 1.5f);

        if (isSelected) {
            drawList->AddRect(ImVec2(screenPos.x - size - 3.0f, screenPos.y - size - 3.0f), 
                             ImVec2(screenPos.x + size + 3.0f, screenPos.y + size + 3.0f), IM_COL32(255, 234, 0, 255), 0.0f, 0, 2.0f);
        } else {
            drawList->AddRect(ImVec2(screenPos.x - size, screenPos.y - size), 
                             ImVec2(screenPos.x + size, screenPos.y + size), IM_COL32(255, 255, 255, 220), 0.0f, 0, 1.0f);
        }

        if (m_showLabels) {
            drawList->AddText(ImVec2(screenPos.x + 10.0f, screenPos.y - 12.0f), IM_COL32(200, 200, 210, 180), v.className.c_str());
        }
    }

    // Draw Markers
    for (int i = 0; i < (int)m_mission.markers.size(); i++) {
        const auto& m = m_mission.markers[i];
        ImVec2 screenPos = worldToScreen(m.position[0], m.position[2], canvasPos);

        ImU32 markerColor = IM_COL32(0, 229, 255, 255);
        if (m.colorName == "ColorRed") markerColor = IM_COL32(255, 23, 68, 255);
        else if (m.colorName == "ColorGreen") markerColor = IM_COL32(0, 230, 118, 255);
        else if (m.colorName == "ColorYellow") markerColor = IM_COL32(255, 234, 0, 255);

        bool isSelected = (m_selectedType == 3 && m_selectedElementIdx == i);

        // Draw crosshair or flag icon
        drawList->AddLine(ImVec2(screenPos.x - 8.0f, screenPos.y), ImVec2(screenPos.x + 8.0f, screenPos.y), markerColor, 1.5f);
        drawList->AddLine(ImVec2(screenPos.x, screenPos.y - 8.0f), ImVec2(screenPos.x, screenPos.y + 8.0f), markerColor, 1.5f);

        if (isSelected) {
            drawList->AddCircle(screenPos, 12.0f, IM_COL32(255, 234, 0, 255), 16, 2.0f);
        }

        std::string markerName = m.name + (m.text.empty() ? "" : " (\"" + m.text + "\")");
        drawList->AddText(ImVec2(screenPos.x + 10.0f, screenPos.y - 12.0f), markerColor, markerName.c_str());
    }
}

void MissionEditor::handleSelectionAndDrag(const ImVec2& canvasPos, const ImVec2& canvasSize) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    if (m_toolMode == ToolMode::Select) {
        if (ImGui::IsWindowHovered()) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // Clicking left mouse button checks for hit test
                m_dragging = false;

                // 1. Check Grouped units
                for (int i = 0; i < (int)m_mission.groups.size(); i++) {
                    const auto& grp = m_mission.groups[i];
                    for (int j = 0; j < (int)grp.vehicles.size(); j++) {
                        const auto& v = grp.vehicles[j];
                        ImVec2 screenPos = worldToScreen(v.position[0], v.position[2], canvasPos);
                        float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + 
                                       (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
                        if (distSq <= 100.0f) { // 10px radius hit
                            m_selectedType = 1;
                            m_selectedGroupIdx = i;
                            m_selectedElementIdx = j;
                            m_dragging = true;
                            return;
                        }
                    }
                }

                // 2. Check Empty vehicles
                for (int i = 0; i < (int)m_mission.emptyVehicles.size(); i++) {
                    const auto& v = m_mission.emptyVehicles[i];
                    ImVec2 screenPos = worldToScreen(v.position[0], v.position[2], canvasPos);
                    float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + 
                                   (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
                    if (distSq <= 100.0f) {
                        m_selectedType = 2;
                        m_selectedGroupIdx = -1;
                        m_selectedElementIdx = i;
                        m_dragging = true;
                        return;
                    }
                }

                // 3. Check Markers
                for (int i = 0; i < (int)m_mission.markers.size(); i++) {
                    const auto& m = m_mission.markers[i];
                    ImVec2 screenPos = worldToScreen(m.position[0], m.position[2], canvasPos);
                    float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + 
                                   (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
                    if (distSq <= 100.0f) {
                        m_selectedType = 3;
                        m_selectedGroupIdx = -1;
                        m_selectedElementIdx = i;
                        m_dragging = true;
                        return;
                    }
                }

                // 4. Check Triggers
                for (int i = 0; i < (int)m_mission.triggers.size(); i++) {
                    const auto& t = m_mission.triggers[i];
                    ImVec2 screenPos = worldToScreen(t.position[0], t.position[2], canvasPos);
                    float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + 
                                   (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
                    if (distSq <= 64.0f) {
                        m_selectedType = 4;
                        m_selectedGroupIdx = -1;
                        m_selectedElementIdx = i;
                        m_dragging = true;
                        return;
                    }
                }

                // Clicked blank area: clear selection
                m_selectedType = 0;
                m_selectedGroupIdx = -1;
                m_selectedElementIdx = -1;
            }

            if (m_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                // Update selected position based on mouse drag movement
                ImVec2 worldPos = screenToWorld(mousePos, canvasPos);

                if (m_selectedType == 1) { // Grouped Vehicle
                    auto& v = m_mission.groups[m_selectedGroupIdx].vehicles[m_selectedElementIdx];
                    v.position[0] = worldPos.x;
                    v.position[2] = worldPos.y;
                } else if (m_selectedType == 2) { // Empty Prop
                    auto& v = m_mission.emptyVehicles[m_selectedElementIdx];
                    v.position[0] = worldPos.x;
                    v.position[2] = worldPos.y;
                } else if (m_selectedType == 3) { // Marker
                    auto& m = m_mission.markers[m_selectedElementIdx];
                    m.position[0] = worldPos.x;
                    m.position[2] = worldPos.y;
                } else if (m_selectedType == 4) { // Trigger
                    auto& t = m_mission.triggers[m_selectedElementIdx];
                    t.position[0] = worldPos.x;
                    t.position[2] = worldPos.y;
                }
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                m_dragging = false;
            }
        }
    } else {
        // Placement Mode: click on map to spawn new objects
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 worldPos = screenToWorld(mousePos, canvasPos);
            createNewEntityAt(worldPos.x, worldPos.y);
            m_toolMode = ToolMode::Select; // Automatically revert to select mode
        }
    }
}

void MissionEditor::createNewEntityAt(float wx, float wy) {
    if (m_toolMode == ToolMode::PlaceUnit) {
        // Creates a new group with 1 unit
        EditorGroup grp;
        grp.side = m_placementSide;
        
        EditorVehicle v;
        v.className = m_placementClass;
        v.side = m_placementSide;
        v.position[0] = wx;
        v.position[1] = 0.0f;
        v.position[2] = wy;
        v.azimuth = 0.0f;
        v.id = 10 + (int)m_mission.groups.size() * 5; // generate basic distinct id
        v.leader = true;
        v.skill = 0.5f;

        grp.vehicles.push_back(v);
        m_mission.groups.push_back(grp);

        m_selectedType = 1;
        m_selectedGroupIdx = (int)m_mission.groups.size() - 1;
        m_selectedElementIdx = 0;
    } else if (m_toolMode == ToolMode::PlaceEmptyVehicle) {
        EditorVehicle v;
        v.className = m_placementClass;
        v.side = "EMPTY";
        v.position[0] = wx;
        v.position[1] = 0.0f;
        v.position[2] = wy;
        v.azimuth = 0.0f;
        v.id = 50 + (int)m_mission.emptyVehicles.size();
        v.skill = 0.5f;

        m_mission.emptyVehicles.push_back(v);

        m_selectedType = 2;
        m_selectedGroupIdx = -1;
        m_selectedElementIdx = (int)m_mission.emptyVehicles.size() - 1;
    } else if (m_toolMode == ToolMode::PlaceTrigger) {
        EditorTrigger t;
        t.position[0] = wx;
        t.position[1] = 0.0f;
        t.position[2] = wy;
        t.a = 50.0f;
        t.b = 50.0f;
        t.activationBy = "WEST";
        t.activationType = "PRESENT";
        t.expCond = "true";

        m_mission.triggers.push_back(t);

        m_selectedType = 4;
        m_selectedGroupIdx = -1;
        m_selectedElementIdx = (int)m_mission.triggers.size() - 1;
    } else if (m_toolMode == ToolMode::PlaceMarker) {
        EditorMarker m;
        m.name = "marker_" + std::to_string(m_mission.markers.size());
        m.text = "Marker Point";
        m.position[0] = wx;
        m.position[1] = 0.0f;
        m.position[2] = wy;
        m.type = "Flag";
        m.colorName = "ColorBlue";

        m_mission.markers.push_back(m);

        m_selectedType = 3;
        m_selectedGroupIdx = -1;
        m_selectedElementIdx = (int)m_mission.markers.size() - 1;
    }
}

void MissionEditor::renderInspector(float w, float h) {
    ImGui::BeginChild("InspectorView", ImVec2(w, h), ImGuiChildFlags_Borders);
    ImGui::Text("Properties Inspector");
    ImGui::Separator();

    if (m_selectedType == 0) {
        ImGui::TextDisabled("Select an entity to inspect");
        ImGui::EndChild();
        return;
    }

    if (ImGui::Button("Delete Selected")) {
        if (m_selectedType == 1) { // Grouped Vehicle
            auto& grp = m_mission.groups[m_selectedGroupIdx];
            grp.vehicles.erase(grp.vehicles.begin() + m_selectedElementIdx);
            if (grp.vehicles.empty()) {
                m_mission.groups.erase(m_mission.groups.begin() + m_selectedGroupIdx);
            }
        } else if (m_selectedType == 2) { // Empty Prop
            m_mission.emptyVehicles.erase(m_mission.emptyVehicles.begin() + m_selectedElementIdx);
        } else if (m_selectedType == 3) { // Marker
            m_mission.markers.erase(m_mission.markers.begin() + m_selectedElementIdx);
        } else if (m_selectedType == 4) { // Trigger
            m_mission.triggers.erase(m_mission.triggers.begin() + m_selectedElementIdx);
        }
        m_selectedType = 0;
        m_selectedGroupIdx = -1;
        m_selectedElementIdx = -1;
        ImGui::EndChild();
        return;
    }
    ImGui::Separator();

    if (m_selectedType == 5) {
        // Intel properties
        ImGui::Text("Intel Settings");
        ImGui::InputFloat("Hour (Time)", &m_mission.hour, 0.5f, 1.0f, "%.2f");
        m_mission.hour = std::clamp(m_mission.hour, 0.0f, 24.0f);
        ImGui::SliderFloat("Start Weather", &m_mission.startWeather, 0.0f, 1.0f);
        ImGui::SliderFloat("Forecast Weather", &m_mission.forecastWeather, 0.0f, 1.0f);
        ImGui::InputFloat("View Distance (m)", &m_mission.viewDistance, 100.0f, 500.0f, "%.0f");
        m_mission.viewDistance = std::clamp(m_mission.viewDistance, 100.0f, 5000.0f);
        
        char nameBuf[256];
        strncpy(nameBuf, m_mission.briefingName.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("Briefing Name", nameBuf, sizeof(nameBuf))) {
            m_mission.briefingName = nameBuf;
        }
    } else if (m_selectedType == 1 || m_selectedType == 2) {
        // Vehicle/Unit properties
        EditorVehicle& v = (m_selectedType == 1) ? 
            m_mission.groups[m_selectedGroupIdx].vehicles[m_selectedElementIdx] : 
            m_mission.emptyVehicles[m_selectedElementIdx];

        ImGui::Text("%s Properties", (m_selectedType == 1) ? "Grouped Unit" : "Empty Prop/Vehicle");
        
        char classBuf[128];
        strncpy(classBuf, v.className.c_str(), sizeof(classBuf));
        if (ImGui::InputText("Class Name", classBuf, sizeof(classBuf))) {
            v.className = classBuf;
        }

        ImGui::InputInt("Unique ID", &v.id);
        
        if (m_selectedType == 1) {
            ImGui::Checkbox("Group Leader", &v.leader);
            
            char playBuf[128];
            strncpy(playBuf, v.player.c_str(), sizeof(playBuf));
            if (ImGui::InputText("Player Role", playBuf, sizeof(playBuf))) {
                v.player = playBuf;
            }
        }

        ImGui::Separator();
        ImGui::Text("Transform");
        ImGui::DragFloat("Pos X", &v.position[0], 1.0f);
        ImGui::DragFloat("Altitude", &v.position[1], 0.1f);
        ImGui::DragFloat("Pos Y", &v.position[2], 1.0f);

        ImGui::Separator();
        // Azimuth Rotation Dial Widget
        ImGui::Text("Azimuth Heading: %.0f deg", v.azimuth);
        
        // Interactive azimuth slider/dial
        ImGui::SliderFloat("Azimut##sl", &v.azimuth, 0.0f, 360.0f, "%.0f");
        v.azimuth = std::fmod(v.azimuth + 360.0f, 360.0f);

        ImGui::Separator();
        ImGui::SliderFloat("Skill Level", &v.skill, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Initialization script");
        char initBuf[1024];
        strncpy(initBuf, v.init.c_str(), sizeof(initBuf));
        if (ImGui::InputTextMultiline("##init", initBuf, sizeof(initBuf), ImVec2(-1, 80))) {
            v.init = initBuf;
        }
    } else if (m_selectedType == 3) {
        // Marker
        EditorMarker& m = m_mission.markers[m_selectedElementIdx];
        ImGui::Text("Marker Settings");

        char nameBuf[128];
        strncpy(nameBuf, m.name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("Marker ID/Name", nameBuf, sizeof(nameBuf))) {
            m.name = nameBuf;
        }

        char textBuf[128];
        strncpy(textBuf, m.text.c_str(), sizeof(textBuf));
        if (ImGui::InputText("Display Text", textBuf, sizeof(textBuf))) {
            m.text = textBuf;
        }

        ImGui::DragFloat("Pos X", &m.position[0], 1.0f);
        ImGui::DragFloat("Pos Y", &m.position[2], 1.0f);

        if (ImGui::BeginCombo("Marker Type", m.type.c_str())) {
            const char* types[] = {"Flag", "Empty", "Dot", "Arrow", "Warning"};
            for (int i = 0; i < 5; i++) {
                if (ImGui::Selectable(types[i], m.type == types[i])) m.type = types[i];
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("Color", m.colorName.c_str())) {
            const char* colors[] = {"ColorBlue", "ColorRed", "ColorGreen", "ColorYellow", "ColorWhite"};
            for (int i = 0; i < 5; i++) {
                if (ImGui::Selectable(colors[i], m.colorName == colors[i])) m.colorName = colors[i];
            }
            ImGui::EndCombo();
        }
    } else if (m_selectedType == 4) {
        // Trigger
        EditorTrigger& t = m_mission.triggers[m_selectedElementIdx];
        ImGui::Text("Trigger Settings");

        char nameBuf[128];
        strncpy(nameBuf, t.name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("Trigger Name", nameBuf, sizeof(nameBuf))) {
            t.name = nameBuf;
        }

        ImGui::DragFloat("Pos X", &t.position[0], 1.0f);
        ImGui::DragFloat("Pos Y", &t.position[2], 1.0f);
        
        ImGui::InputFloat("Axis A (Radius)", &t.a, 5.0f);
        ImGui::InputFloat("Axis B (Radius)", &t.b, 5.0f);
        ImGui::Checkbox("Rectangular Shape", &t.rectangular);

        if (ImGui::BeginCombo("Activation By", t.activationBy.c_str())) {
            const char* acts[] = {"WEST", "EAST", "GUER", "CIV", "ANYBODY"};
            for (int i = 0; i < 5; i++) {
                if (ImGui::Selectable(acts[i], t.activationBy == acts[i])) t.activationBy = acts[i];
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("Activation Type", t.activationType.c_str())) {
            const char* types[] = {"PRESENT", "NOT PRESENT", "DETECTED BY WEST", "DETECTED BY EAST"};
            for (int i = 0; i < 4; i++) {
                if (ImGui::Selectable(types[i], t.activationType == types[i])) t.activationType = types[i];
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Condition Expression");
        char condBuf[512];
        strncpy(condBuf, t.expCond.c_str(), sizeof(condBuf));
        if (ImGui::InputText("##cond", condBuf, sizeof(condBuf))) {
            t.expCond = condBuf;
        }

        ImGui::Text("Activation Statement");
        char actBuf[1024];
        strncpy(actBuf, t.expActiv.c_str(), sizeof(actBuf));
        if (ImGui::InputTextMultiline("##activ", actBuf, sizeof(actBuf), ImVec2(-1, 60))) {
            t.expActiv = actBuf;
        }

        ImGui::Text("Deactivation Statement");
        char deBuf[1024];
        strncpy(deBuf, t.expDesactiv.c_str(), sizeof(deBuf));
        if (ImGui::InputTextMultiline("##deactiv", deBuf, sizeof(deBuf), ImVec2(-1, 60))) {
            t.expDesactiv = deBuf;
        }
    }

    ImGui::EndChild();
}

} // namespace Poseidon
