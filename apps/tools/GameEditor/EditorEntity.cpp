#include "EditorEntity.hpp"
#include <filesystem>
#include <Poseidon/Graphics/Rendering/Shape/Shape.hpp>
#include <Poseidon/IO/ParamFileExt.hpp>
#include <Poseidon/Foundation/Framework/Log.hpp>
#include <Poseidon/World/Scene/Scene.hpp>

namespace Poseidon
{

// FindShape is defined in ParamFileExt.cpp
extern RString FindShape(RString name);

EditorEntity::EditorEntity(const std::string& name, const std::string& modelPath)
    : m_name(name), m_modelPath(modelPath), m_loaded(false)
{
    // Matrix4 default constructor is likely identity, but we can set it explicitly if needed.
    // For now, let's leave it as is since it doesn't have Identity()
}

EditorEntity::~EditorEntity()
{
}

bool EditorEntity::Load()
{
    RString foundPath;

    // If the stored path already exists on disk (absolute project path), use it directly.
    // FindShape only searches engine-internal directories and will always fail for paths
    // outside the game data tree (e.g. C:\Users\...\project\models\foo.p3d).
    if (std::filesystem::exists(m_modelPath))
    {
        foundPath = RString(m_modelPath.c_str());
    }
    else
    {
        // Fall back to the engine's search (relative / bank names)
        foundPath = FindShape(RString(m_modelPath.c_str()));
        if (foundPath.GetLength() == 0)
        {
            LOG_ERROR(Core, "EditorEntity: cannot find model {}", m_modelPath.c_str());
            return false;
        }
    }

    Ref<LODShapeWithShadow> shape = Shapes.New(foundPath, false, false);
    if (!shape || shape->NLevels() <= 0)
    {
        LOG_ERROR(Core, "EditorEntity: shape {} failed to load", (const char*)foundPath);
        return false;
    }

    // Allocate the ObjectPlain instance.
    // 0 is the ID, in a real system we would track unique IDs
    m_object = new ObjectPlain(shape, 0);
    
    // Set default initial position
    m_object->Move(m_transform);
    
    m_loaded = true;
    LOG_INFO(Core, "EditorEntity: loaded {} as '{}'", m_modelPath.c_str(), m_name.c_str());
    return true;
}

void EditorEntity::SetTransform(const Matrix4& transform)
{
    m_transform = transform;
    if (m_object)
    {
        m_object->Move(m_transform);
    }
}

Vector3 EditorEntity::GetPosition() const
{
    return m_transform.Position();
}

void EditorEntity::SetPosition(const Vector3& pos)
{
    m_transform.SetPosition(pos);
    if (m_object)
    {
        m_object->Move(m_transform);
    }
}

void EditorEntity::Update(float deltaT)
{
    // Per-frame logic (animation, physics, etc.) would go here.
    // 3D rendering is handled by Draw(), called from the main loop's
    // 3D pipeline between BeginObjects and EndObjects.
}

void EditorEntity::Draw()
{
    if (!m_loaded || !m_object || !Poseidon::GScene)
        return;

    // Call Object::Draw to insert it into the scene's draw queue for this frame
    // ClipAll is required to make it render normally in the 3D world
    m_object->Draw(0, ClipAll | ClipUser0, *m_object);
}

} // namespace Poseidon
