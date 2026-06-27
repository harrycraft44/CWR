#pragma once

#include "EditorEntity.hpp"
#include <vector>
#include <memory>
#include <Poseidon/Foundation/Math/Math3D.hpp>

namespace Poseidon
{

class EditorScene
{
public:
    EditorScene();
    ~EditorScene();

    void Init();

    // Add a new entity to the scene
    EditorEntity* AddEntity(const std::string& name, const std::string& modelPath);

    // Get all entities
    const std::vector<EditorEntity*>& GetEntities() const { return m_entities; }

    // Selection
    EditorEntity* GetSelectedEntity() const { return m_selectedEntity; }
    void SetSelectedEntity(EditorEntity* entity) { m_selectedEntity = entity; }

    // Raycast against all entities
    EditorEntity* Raycast(const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance);

    // Serialization
    bool Save(const std::string& filepath);
    bool Load(const std::string& filepath);

    // Call this each frame to update entity logic
    void Update(float deltaT);

    // Submit 3D geometry (grid, axes, entities) into the engine draw pipeline.
    // Must be called between Scene::BeginObjects and Scene::EndObjects.
    void Draw();

    // Grid settings
    bool m_showGrid = true;
    int m_gridSize = 50;
    float m_gridStep = 1.0f;
    float m_gridColor[3] = {0.5f, 0.5f, 0.5f};

private:
    std::vector<EditorEntity*> m_entities;
    EditorEntity* m_selectedEntity;
};

} // namespace Poseidon
