#include <catch2/catch_test_macros.hpp>

#include <Poseidon/AI/AI.hpp>
#include <Poseidon/World/Scene/Object.hpp>
#include <Poseidon/World/Scene/Object2DMapping.hpp>
#include <Poseidon/World/Terrain/Landscape.hpp>

TEST_CASE("object.hpp compiles", "[scene][object]")
{
    SUCCEED("header included successfully");
}

TEST_CASE("Object2D mapping is independent of face winding", "[scene][object][2d]")
{
    const Vector3 positions[] = {
        Vector3(-1.0f, -1.0f, 0.0f),
        Vector3(-1.0f, 1.0f, 0.0f),
        Vector3(1.0f, 1.0f, 0.0f),
        Vector3(1.0f, -1.0f, 0.0f),
    };

    const int clockwise[] = {0, 1, 2, 3};
    const Poseidon::Object2DQuadCorners clockwiseCorners = Poseidon::SelectObject2DQuadCorners(positions, clockwise);
    CHECK(clockwiseCorners.bottomLeft == 0);
    CHECK(clockwiseCorners.bottomRight == 3);
    CHECK(clockwiseCorners.topLeft == 1);
    CHECK(clockwiseCorners.topRight == 2);

    const int reversed[] = {0, 3, 2, 1};
    const Poseidon::Object2DQuadCorners reversedCorners = Poseidon::SelectObject2DQuadCorners(positions, reversed);
    CHECK(reversedCorners.bottomLeft == 0);
    CHECK(reversedCorners.bottomRight == 3);
    CHECK(reversedCorners.topLeft == 1);
    CHECK(reversedCorners.topRight == 2);
}

TEST_CASE("Object Move when GLOB_LAND is null does not crash", "[scene][object]")
{
    REQUIRE(Poseidon::GLandscape == nullptr);

    Poseidon::ObjectPlain obj(nullptr, 123);

    Matrix4 initialTransform;
    obj.Move(initialTransform);

    obj.Move(Vector3(10.0f, 20.0f, 30.0f));

    CHECK(obj.Position().X() == 10.0f);
    CHECK(obj.Position().Y() == 20.0f);
    CHECK(obj.Position().Z() == 30.0f);
}

