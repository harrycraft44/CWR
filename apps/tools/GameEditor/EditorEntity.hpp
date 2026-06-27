#pragma once

#include <string>
#include <Poseidon/Core/Types.hpp>
#include <Poseidon/Foundation/Math/Math3D.hpp>
#include <Poseidon/World/Scene/Object.hpp>

namespace Poseidon
{

class EditorEntity : public RefCount
{
public:
    EditorEntity(const std::string& name, const std::string& modelPath);
    ~EditorEntity();

    bool Load();
    
    // Engine object representation
    Ref<ObjectPlain> GetObject() const { return m_object; }
    
    // Name for Hierarchy panel
    const std::string& GetName() const { return m_name; }
    const std::string& GetModelPath() const { return m_modelPath; }
    void SetName(const std::string& name) { m_name = name; }

    // Transform
    const Matrix4& GetTransform() const { return m_transform; }
    void SetTransform(const Matrix4& transform);

    // Position (shortcut)
    Vector3 GetPosition() const;
    void SetPosition(const Vector3& pos);

    // Call this each frame for per-frame logic
    void Update(float deltaT);

    // Submit this entity into the engine's 3D draw pipeline
    void Draw();

private:
    std::string m_name;
    std::string m_modelPath;
    Ref<ObjectPlain> m_object;
    Matrix4 m_transform;
    bool m_loaded;
};

} // namespace Poseidon
