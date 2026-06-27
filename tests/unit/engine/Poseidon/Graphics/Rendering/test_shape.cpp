#include <catch2/catch_test_macros.hpp>
#include <Poseidon/Graphics/Rendering/Shape/Shape.hpp>
#include <Poseidon/Graphics/Rendering/Shape/ClipShape.hpp>
#include <Poseidon/Graphics/Rendering/Draw/SpecLods.hpp>
#include <Poseidon/Graphics/Core/Engine.hpp>
#include <Poseidon/Graphics/Core/TLVertex.hpp>
#include <Poseidon/Graphics/Dummy/EngineDummy.hpp>
#include <Poseidon/World/Scene/Scene.hpp>
#include <Poseidon/World/Scene/Camera/Camera.hpp>

using namespace Poseidon;

namespace
{
class RecordingEngine : public EngineDummy
{
  public:
    int lastTriSpec = -1;
    int lastMatSpec = -1;
    int triCallCount = 0;
    int matCallCount = 0;

    Poseidon::render::LegacySpec lastMatSpecTyped;
    Poseidon::render::LegacySpec lastTriSpecTyped;

    void SetMaterial(const TLMaterial& /*mat*/, const LightList& /*lights*/,
                     const Poseidon::render::LegacySpec& spec) override
    {
        lastMatSpec = static_cast<int>(Poseidon::render::MergeLegacy(spec));
        lastMatSpecTyped = spec;
        ++matCallCount;
    }

    void PrepareTriangleTL(const MipInfo& /*mip*/, const Poseidon::render::LegacySpec& spec) override
    {
        lastTriSpec = static_cast<int>(Poseidon::render::MergeLegacy(spec));
        lastTriSpecTyped = spec;
        ++triCallCount;
    }
};

struct GEngineGuard
{
    Engine* prev;
    explicit GEngineGuard(Engine* tmp) : prev(GEngine) { GEngine = tmp; }
    ~GEngineGuard() { GEngine = prev; }
};

struct GSceneGuard
{
    Scene* prev;
    explicit GSceneGuard(Scene* tmp) : prev(GScene) { GScene = tmp; }
    ~GSceneGuard() { GScene = prev; }
};

class DummyAnimator : public IAnimator
{
public:
    void DoTransform(TLVertexTable &/*dst*/, const Shape &/*src*/, const Matrix4 &/*posView*/, int /*from*/, int /*to*/) const override {}
    void DoLight(TLVertexTable &/*dst*/, const Shape &/*src*/, const Matrix4 &/*worldToModel*/, const LightList &/*lights*/, int /*spec*/, int /*material*/, int /*from*/, int /*to*/) const override {}
    void GetMaterial(TLMaterial &/*mat*/, int /*index*/) const override {}
    bool GetAnimated(const Shape &/*src*/) const override { return false; }
};
} // namespace

TEST_CASE("Shape: LODShape default construction", "[Shape]")
{
    LODShape shape;
    REQUIRE(shape.NLevels() == 0);
}

TEST_CASE("clipShape.hpp compiles", "[Shape]")
{
    REQUIRE(true);
}

TEST_CASE("LODShape: FindSpecLevel skips dropped null LOD slots", "[Shape][LOD]")
{
    LODShape lod;
    lod.AddShape(new Shape(), 0.0f);
    lod.AddShape(new Shape(), VIEW_PILOT);

    REQUIRE(lod.FindSpecLevel(VIEW_PILOT) == 1);

    lod.ChangeShape(1, nullptr);

    REQUIRE(lod.FindSpecLevel(VIEW_PILOT) == -1);
    REQUIRE(lod.FindLevel(10.0f) == 0);
}

TEST_CASE("FaceArray::Draw fallback sets default section when NSections == 0", "[Shape][FaceArray]")
{
    RecordingEngine rec;
    GEngineGuard engineGuard(&rec);

    Scene sceneObj;
    GSceneGuard sceneGuard(&sceneObj);

    Shape mesh;
    mesh.Init(3);

    Poly poly;
    poly.Init();
    poly.SetN(3);
    poly.Set(0, 0);
    poly.Set(1, 1);
    poly.Set(2, 2);
    mesh.AddFace(poly);

    REQUIRE(mesh.NSections() == 0);
    REQUIRE(mesh.NFaces() == 1);

    DummyAnimator animator;
    LightList lights;
    
    // Call draw, which should trigger the fallback and dynamically create the section
    mesh.Faces().Draw(&animator, lights, mesh, 0, 0, MIdentity, MIdentity);

    // Verify sections were dynamically created and assigned
    REQUIRE(mesh.NSections() == 1);
    REQUIRE(mesh.GetSection(0).beg == mesh.BeginFaces());
    REQUIRE(mesh.GetSection(0).end == mesh.EndFaces());
}

