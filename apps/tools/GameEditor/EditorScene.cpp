#include "EditorScene.hpp"
#include <fstream>
#include <sstream>
#include <Poseidon/Graphics/Core/Engine.hpp>

namespace Poseidon
{

EditorScene::EditorScene() : m_selectedEntity(nullptr)
{
}

EditorScene::~EditorScene()
{
    for (auto* entity : m_entities)
    {
        delete entity;
    }
    m_entities.clear();
}

void EditorScene::Init()
{
    // Add some default objects so the editor is not empty

    
    // We can add more defaults if we know their paths
    // e.g. AddEntity("Jeep", "jeep.p3d");
}

EditorEntity* EditorScene::AddEntity(const std::string& name, const std::string& modelPath)
{
    EditorEntity* entity = new EditorEntity(name, modelPath);
    if (entity->Load())
    {
        m_entities.push_back(entity);
        return entity;
    }
    else
    {
        delete entity;
        return nullptr;
    }
}

EditorEntity* EditorScene::Raycast(const Vector3& rayOrigin, const Vector3& rayDir, float maxDistance)
{
    Vector3 rayEnd = rayOrigin + (rayDir * maxDistance);
    
    EditorEntity* closestEntity = nullptr;
    float closestDistance = maxDistance * maxDistance;

    for (EditorEntity* entity : m_entities)
    {
        if (auto obj = entity->GetObject())
        {
            CollisionBuffer result;
            obj->Intersect(result, rayOrigin, rayEnd, 0.0f, ObjIntersectGeom);
            
            if (result.Size() > 0)
            {
                // Find the closest collision point
                for (int i = 0; i < result.Size(); ++i)
                {
                    float distSq = (result[i].pos - rayOrigin).SquareSize();
                    if (distSq < closestDistance)
                    {
                        closestDistance = distSq;
                        closestEntity = entity;
                    }
                }
            }
        }
    }

    return closestEntity;
}

// Update loop

#include <glad/gl.h>

void EditorScene::Update(float deltaT)
{
    // Per-frame entity logic (animation, physics, etc.)
    for (auto* entity : m_entities)
    {
        entity->Update(deltaT);
    }
}

void EditorScene::Draw()
{
    // Draw 3D Grid
    if (Poseidon::GEngine)
    {
        if (m_showGrid)
        {
            Poseidon::Color gridColor(m_gridColor[0], m_gridColor[1], m_gridColor[2], 1.0f);
            Poseidon::PackedColor packedGridColor(gridColor);
            
            for (int i = -m_gridSize; i <= m_gridSize; i++)
            {
                // Lines along Z
                Poseidon::GEngine->DrawLine3D(Vector3(i * m_gridStep, 0.0f, -m_gridSize * m_gridStep), 
                                              Vector3(i * m_gridStep, 0.0f, m_gridSize * m_gridStep), 
                                              packedGridColor, 0);
                // Lines along X
                Poseidon::GEngine->DrawLine3D(Vector3(-m_gridSize * m_gridStep, 0.0f, i * m_gridStep), 
                                              Vector3(m_gridSize * m_gridStep, 0.0f, i * m_gridStep), 
                                              packedGridColor, 0);
            }
        }
        
        // Draw Origin axes
        Poseidon::GEngine->DrawLine3D(Vector3(0.0f, 0.0f, 0.0f), Vector3(5.0f, 0.0f, 0.0f), Poseidon::PackedColor(Poseidon::Color(1.0f, 0.0f, 0.0f, 1.0f)), 0); // Red X
        Poseidon::GEngine->DrawLine3D(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 5.0f, 0.0f), Poseidon::PackedColor(Poseidon::Color(0.0f, 1.0f, 0.0f, 1.0f)), 0); // Green Y
        Poseidon::GEngine->DrawLine3D(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 5.0f), Poseidon::PackedColor(Poseidon::Color(0.0f, 0.0f, 1.0f, 1.0f)), 0); // Blue Z
    }

    // Submit entities into the engine's draw pipeline
    for (auto* entity : m_entities)
    {
        entity->Draw();
    }
}

bool EditorScene::Save(const std::string& filepath)
{
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    
    out << m_entities.size() << "\n";
    for (auto* entity : m_entities)
    {
        out << entity->GetName() << "\n";
        out << entity->GetModelPath() << "\n";
        Vector3 pos = entity->GetPosition();
        out << pos.X() << " " << pos.Y() << " " << pos.Z() << "\n";
    }
    
    out.close();
    return true;
}

bool EditorScene::Load(const std::string& filepath)
{
    std::ifstream in(filepath);
    if (!in.is_open()) return false;
    
    // Clear current scene
    m_selectedEntity = nullptr;
    for (auto* entity : m_entities)
    {
        delete entity;
    }
    m_entities.clear();
    
    size_t count = 0;
    in >> count;
    std::string line;
    std::getline(in, line); // consume newline
    
    for (size_t i = 0; i < count; ++i)
    {
        std::string name, path;
        std::getline(in, name);
        std::getline(in, path);
        
        float x, y, z;
        in >> x >> y >> z;
        std::getline(in, line); // consume newline
        
        EditorEntity* entity = AddEntity(name, path);
        if (entity)
        {
            entity->SetPosition(Vector3(x, y, z));
        }
    }
    
    in.close();
    return true;
}

} // namespace Poseidon
