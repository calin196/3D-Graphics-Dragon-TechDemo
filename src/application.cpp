#include "mesh.h"
#include "texture.h"

#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui/imgui.h>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/window.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <limits>
#include <unordered_set>
#include <vector>

Mesh createDragonRevolvedMesh(const std::vector<glm::vec2>& profile, const glm::vec3& color,
    const glm::vec2& textureRepeat = glm::vec2(1.0f), float vOffset = 0.0f)
{

    Mesh mesh {};
    mesh.material.kd = color;
    constexpr unsigned int sides = 8;
    auto addTriangle = [&mesh](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec2& uvA,
                           const glm::vec2& uvB, const glm::vec2& uvC) {
        const glm::vec3 cross = glm::cross(b - a, c - a);
        if (glm::dot(cross, cross) < 1e-12f)
            return;
        const glm::vec3 normal = glm::normalize(cross);
        const unsigned int first = static_cast<unsigned int>(mesh.vertices.size());
        mesh.vertices.push_back({ a, normal, uvA });
        mesh.vertices.push_back({ b, normal, uvB });
        mesh.vertices.push_back({ c, normal, uvC });
        mesh.triangles.push_back({ first, first + 1, first + 2 });
    };
    for (std::size_t ring = 0; ring + 1 < profile.size(); ++ring) {
        for (unsigned int side = 0; side < sides; ++side) {
            const float angle = glm::radians(360.0f * static_cast<float>(side) / sides);
            const float nextAngle = glm::radians(360.0f * static_cast<float>((side + 1) % sides) / sides);
            const glm::vec3 a(profile[ring].x * std::cos(angle), profile[ring].x * std::sin(angle), profile[ring].y);
            const glm::vec3 b(
                profile[ring].x * std::cos(nextAngle), profile[ring].x * std::sin(nextAngle), profile[ring].y);
            const glm::vec3 c(profile[ring + 1].x * std::cos(nextAngle), profile[ring + 1].x * std::sin(nextAngle),
                profile[ring + 1].y);
            const glm::vec3 d(
                profile[ring + 1].x * std::cos(angle), profile[ring + 1].x * std::sin(angle), profile[ring + 1].y);

            const float u0 = static_cast<float>((side + 2) % sides) / sides * textureRepeat.x;
            const float u1 = u0 + textureRepeat.x / sides;
            const float midU = (u0 + u1) * 0.5f;
            const float v0 = profile[ring].y * textureRepeat.y + vOffset;
            const float v1 = profile[ring + 1].y * textureRepeat.y + vOffset;
            float startU0 = u0;
            float startU1 = u1;
            if (profile[ring].x == 0) {
                startU0 = midU;
                startU1 = midU;
            }
            float endU0 = u0;
            float endU1 = u1;
            if (profile[ring + 1].x == 0) {
                endU0 = midU;
                endU1 = midU;
            }
            const glm::vec2 uvA(startU0, v0);
            const glm::vec2 uvB(startU1, v0);
            const glm::vec2 uvC(endU1, v1);
            const glm::vec2 uvD(endU0, v1);
            addTriangle(a, b, c, uvA, uvB, uvC);
            addTriangle(a, c, d, uvA, uvC, uvD);
        }
    }
    return mesh;
}

Mesh createDragonRoundedMesh(const glm::vec3& color)
{
    Mesh mesh {};
    mesh.material.kd = color;
    constexpr unsigned int rings = 16, sides = 32;
    for (unsigned int ring = 0; ring <= rings; ++ring) {
        const float latitude = glm::radians(180.0f) * static_cast<float>(ring) / rings;
        float radius;
        if (ring == 0 || ring == rings) {
            radius = 0.0f;
        } else {
            radius = std::sin(latitude);
        }
        for (unsigned int side = 0; side <= sides; ++side) {
            unsigned int wrappedSide = side;
            if (side == sides) {
                wrappedSide = 0;
            }
            const float angle = glm::radians(360.0f) * static_cast<float>(wrappedSide) / sides;
            const glm::vec3 position(radius * std::cos(angle), radius * std::sin(angle), -std::cos(latitude));
            mesh.vertices.push_back({ position, glm::normalize(position),
                { static_cast<float>(side) / sides, static_cast<float>(ring) / rings } });
        }
    }
    for (unsigned int ring = 0; ring < rings; ++ring) {
        for (unsigned int side = 0; side < sides; ++side) {
            const unsigned int a = ring * (sides + 1) + side, b = a + 1, d = a + sides + 1, c = d + 1;
            if (ring > 0)
                mesh.triangles.push_back({ a, b, c });
            if (ring + 1 < rings)
                mesh.triangles.push_back({ a, c, d });
        }
    }
    return mesh;
}

glm::vec3 dragonTendrilPoint(float t, float bend)
{
    return glm::vec3(0, bend * t * t, t);
}

Mesh createDragonTendrilMesh(float rootRadius, float bend, const glm::vec3& color)
{
    Mesh mesh {};
    mesh.material.kd = color;
    constexpr unsigned int rings = 24, sides = 16;
    for (unsigned int ring = 0; ring <= rings; ++ring) {
        const float t = static_cast<float>(ring) / rings;
        const glm::vec3 tangent = glm::normalize(glm::vec3(0, 2.0f * bend * t, 1));
        const glm::vec3 up = glm::cross(tangent, glm::vec3(1, 0, 0));
        const float radius = rootRadius * (1.0f - t);
        for (unsigned int side = 0; side <= sides; ++side) {
            unsigned int wrappedSide = side;
            if (side == sides) {
                wrappedSide = 0;
            }
            const float angle = glm::radians(360.0f) * static_cast<float>(wrappedSide) / sides;
            const glm::vec3 radial = std::cos(angle) * glm::vec3(1, 0, 0) + std::sin(angle) * up;
            const glm::vec3 centerlineDerivative(0, 2.0f * bend * t, 1);
            const float radialSlope = rootRadius / glm::length(centerlineDerivative);
            const glm::vec3 normal = glm::normalize(radial + radialSlope * tangent);
            mesh.vertices.push_back(
                { dragonTendrilPoint(t, bend) + radial * radius, normal, { static_cast<float>(side) / sides, t } });
        }
    }
    for (unsigned int ring = 0; ring < rings; ++ring) {
        for (unsigned int side = 0; side < sides; ++side) {
            const unsigned int a = ring * (sides + 1) + side, b = a + 1, d = a + sides + 1, c = d + 1;
            mesh.triangles.push_back({ a, b, c });
            if (ring + 1 < rings)
                mesh.triangles.push_back({ a, c, d });
        }
    }
    return mesh;
}

Mesh createDragonFaceProfile(const std::vector<glm::vec4>& profile, const glm::vec3& color,
    float crossSectionPower = 1.0f, float topFraction = 1.0f)
{

    Mesh mesh {};
    mesh.material.kd = color;
    constexpr unsigned int sides = 32, steps = 4;
    const unsigned int intervals = static_cast<unsigned int>(profile.size() - 1) * steps;
    for (unsigned int ring = 0; ring <= intervals; ++ring) {
        const std::size_t span = std::min<std::size_t>(ring / steps, profile.size() - 2);
        const float t = static_cast<float>(ring - span * steps) / steps;
        std::size_t previousSpan = 0;
        if (span != 0) {
            previousSpan = span - 1;
        }
        const glm::vec4 a = profile[previousSpan];
        const glm::vec4 b = profile[span];
        const glm::vec4 c = profile[span + 1], d = profile[std::min(span + 2, profile.size() - 1)];
        const glm::vec4 constantTerm = 2.0f * b;
        const glm::vec4 linearTerm = (-a + c) * t;
        const glm::vec4 quadraticTerm = (2.0f * a - 5.0f * b + 4.0f * c - d) * t * t;
        const glm::vec4 cubicTerm = (-a + 3.0f * b - 3.0f * c + d) * t * t * t;
        const glm::vec4 shape = 0.5f * (constantTerm + linearTerm + quadraticTerm + cubicTerm);
        for (unsigned int side = 0; side <= sides; ++side) {
            unsigned int wrappedSide = side;
            if (side == sides) {
                wrappedSide = 0;
            }
            const float angle = glm::radians(360.0f) * static_cast<float>(wrappedSide) / sides;
            const bool pole = ring == 0 || ring == intervals;
            float horizontalRadius = 0.0f;
            float verticalRadius = 0.0f;
            if (!pole) {
                float horizontalShape = std::cos(angle);
                float verticalShape = std::sin(angle);
                if (crossSectionPower != 1.0f) {
                    horizontalShape = std::copysign(std::pow(std::abs(horizontalShape), crossSectionPower), horizontalShape);
                    verticalShape = std::copysign(std::pow(std::abs(verticalShape), crossSectionPower), verticalShape);
                }
                horizontalRadius = glm::max(shape.x, 0.0f) * horizontalShape;
                verticalRadius = glm::max(shape.y, 0.0f) * verticalShape;
                verticalRadius = glm::min(verticalRadius, glm::max(shape.y, 0.0f) * topFraction);
            }
            const glm::vec3 position(horizontalRadius, shape.z + verticalRadius, shape.w);
            mesh.vertices.push_back(
                { position, glm::vec3(0), { static_cast<float>(side) / sides, static_cast<float>(ring) / intervals } });
        }
    }
    auto triangle = [&mesh](unsigned int a, unsigned int b, unsigned int c) {
        mesh.triangles.push_back({ a, b, c });
        const glm::vec3 normal = glm::cross(mesh.vertices[b].position - mesh.vertices[a].position,
            mesh.vertices[c].position - mesh.vertices[a].position);
        for (unsigned int index : { a, b, c })
            mesh.vertices[index].normal += normal;
    };
    for (unsigned int ring = 0; ring < intervals; ++ring) {
        for (unsigned int side = 0; side < sides; ++side) {
            const unsigned int a = ring * (sides + 1) + side, b = a + 1, d = a + sides + 1, c = d + 1;
            if (ring > 0)
                triangle(a, b, c);
            if (ring + 1 < intervals)
                triangle(a, c, d);
        }
    }
    for (unsigned int ring = 0; ring <= intervals; ++ring) {
        auto& first = mesh.vertices[ring * (sides + 1)].normal;
        auto& last = mesh.vertices[ring * (sides + 1) + sides].normal;
        first = last = first + last;
        for (unsigned int side = 0; side <= sides; ++side) {
            auto& normal = mesh.vertices[ring * (sides + 1) + side].normal;
            if (ring == 0) {
                normal = glm::vec3(0, 0, -1);
            } else if (ring == intervals) {
                normal = glm::vec3(0, 0, 1);
            } else {
                normal = glm::normalize(normal);
            }
        }
    }
    return mesh;
}

// The open target is authored once; live jaw motion is vertex interpolation in GLSL.
const glm::mat4& dragonJawOpenTransform()
{
    static const glm::mat4 transform = [] {
        const glm::vec3 hinge(0, -0.024f, 0.075f);
        glm::mat4 target = glm::translate(glm::mat4(1), hinge);
        target = glm::rotate(target, glm::radians(-40.0f), glm::vec3(1, 0, 0));
        return glm::translate(target, -hinge);
    }();
    return transform;
}

glm::vec3 dragonJawMorphOffset(const glm::vec3& closedPosition)
{
    const glm::vec3 openPosition(dragonJawOpenTransform() * glm::vec4(closedPosition, 1));
    return openPosition - closedPosition;
}

Mesh createDragonSnoutMesh()
{
    // Visual polish: refined head proportions for a narrower dragon face.
    return createDragonFaceProfile(
        { { 0, 0, 0.010f, -0.340f }, { 0.053f, 0.044f, 0.010f, -0.331f },
            { 0.068f, 0.047f, 0.020f, -0.298f }, { 0.063f, 0.050f, 0.028f, -0.263f },
            { 0.043f, 0.038f, 0.025f, -0.156f }, { 0.060f, 0.064f, 0.035f, -0.04f },
            { 0.070f, 0.066f, 0.030f, 0.07f }, { 0, 0, 0.020f, 0.13f } },
        glm::vec3(0.72f, 0.11f, 0.055f), 0.90f, 0.85f);
}

enum class DragonTeethRow { Upper, Lower };

Mesh createDragonTeethMesh(DragonTeethRow row = DragonTeethRow::Upper)
{
    Mesh teeth {};
    teeth.material.kd = glm::vec3(0.96f, 0.86f, 0.65f);
    const Mesh tooth = createDragonRevolvedMesh(
        { { 0, 0 }, { 1, 0 }, { 0.85f, 0.22f }, { 0.40f, 0.70f }, { 0, 1 } }, teeth.material.kd);
    float toothPitchDegrees = 90.0f;
    if (row == DragonTeethRow::Lower) {
        toothPitchDegrees = -90.0f;
    }
    auto addTooth = [&](const glm::vec3& root, float radius, float length, float outwardAngle) {
        glm::mat4 transform = glm::translate(glm::mat4(1), root);
        transform = glm::rotate(transform, glm::radians(outwardAngle), glm::vec3(0, 0, 1));
        transform = glm::rotate(transform, glm::radians(toothPitchDegrees), glm::vec3(1, 0, 0));
        transform = glm::scale(transform, glm::vec3(radius, radius, length));
        const glm::mat3 normalTransform = glm::inverseTranspose(glm::mat3(transform));
        const unsigned int firstVertex = static_cast<unsigned int>(teeth.vertices.size());
        for (Vertex vertex : tooth.vertices) {
            vertex.position = glm::vec3(transform * glm::vec4(vertex.position, 1));
            vertex.normal = glm::normalize(normalTransform * vertex.normal);
            teeth.vertices.push_back(vertex);
        }
        for (const glm::uvec3& triangle : tooth.triangles) {
            teeth.triangles.push_back(triangle + glm::uvec3(firstVertex));
        }
    };
    // Each tooth row is batched, with mirrored left/right placements.
    for (float side : { -1.0f, 1.0f }) {
        if (row == DragonTeethRow::Upper) {
            addTooth(glm::vec3(side * 0.064f, -0.003f, -0.286f), 0.016f, 0.100f, side * 12.0f);
            const std::array<float, 4> rowWidths { 0.053f, 0.044f, 0.038f, 0.043f };
            for (int toothIndex = 0; toothIndex < 4; ++toothIndex) {
                const float z = -0.230f + toothIndex * 0.039f;
                const float length = 0.050f - toothIndex * 0.004f;
                const glm::vec3 root(side * rowWidths[toothIndex], 0.008f, z);
                addTooth(root, 0.011f, length, side * 8.0f);
            }
        } else {
            // Visual polish: added lower jaw teeth, staggered between the upper teeth.
            addTooth(glm::vec3(side * 0.051f, -0.008f, -0.270f), 0.013f, 0.065f, side * -10.0f);
            const std::array<float, 4> rowWidths { 0.050f, 0.043f, 0.038f, 0.045f };
            for (int toothIndex = 0; toothIndex < 4; ++toothIndex) {
                const float z = -0.218f + toothIndex * 0.038f;
                const float length = 0.050f - toothIndex * 0.004f;
                const glm::vec3 root(side * rowWidths[toothIndex], 0.004f, z);
                addTooth(root, 0.010f, length, side * -8.0f);
            }
        }
    }
    return teeth;
}

Mesh createDragonJawMesh()
{
    // Match the snout's underside so the closed lip has no permanent mouth gap.
    Mesh jaw = createDragonSnoutMesh();
    jaw.material.kd = glm::vec3(0.82f, 0.57f, 0.36f);
    constexpr std::size_t sides = 32;
    constexpr std::size_t columns = sides + 1;
    for (std::size_t first = 0; first < jaw.vertices.size(); first += columns) {
        std::array<float, columns> snoutHeights {};
        for (std::size_t side = 0; side <= sides; ++side) {
            snoutHeights[side] = jaw.vertices[first + side].position.y;
        }
        const float width = std::abs(jaw.vertices[first].position.x);
        const float thickness = 0.030f * glm::min(1.0f, width / 0.05f);
        for (std::size_t side = 0; side <= sides; ++side) {
            auto& vertex = jaw.vertices[first + side];
            const float angle = glm::two_pi<float>() * static_cast<float>(side) / sides;
            const float verticalWeight = std::sin(angle);
            float jawY;
            if (side <= sides / 2) {
                jawY = snoutHeights[sides - side] + 0.001f * verticalWeight;
            } else {
                jawY = snoutHeights[side] - thickness * (-verticalWeight);
            }
            vertex.position.y = jawY;
            vertex.normal = glm::vec3(0);
        }
    }
    for (const auto& triangle : jaw.triangles) {
        const glm::vec3 edgeA = jaw.vertices[triangle.y].position - jaw.vertices[triangle.x].position;
        const glm::vec3 edgeB = jaw.vertices[triangle.z].position - jaw.vertices[triangle.x].position;
        const glm::vec3 faceNormal = glm::cross(edgeA, edgeB);
        for (unsigned int index : { triangle.x, triangle.y, triangle.z }) {
            jaw.vertices[index].normal += faceNormal;
        }
    }
    for (std::size_t first = 0; first < jaw.vertices.size(); first += columns) {
        const glm::vec3 seamNormal = jaw.vertices[first].normal + jaw.vertices[first + sides].normal;
        jaw.vertices[first].normal = seamNormal;
        jaw.vertices[first + sides].normal = seamNormal;
    }
    for (auto& vertex : jaw.vertices) {
        if (glm::dot(vertex.normal, vertex.normal) > 1e-16f) {
            vertex.normal = glm::normalize(vertex.normal);
        } else {
            vertex.normal = glm::vec3(0, -1, 0);
        }
    }
    return jaw;
}

constexpr glm::vec2 waterPondCenter(-4.0f, -2.5f);
constexpr glm::vec2 waterPondRadii(1.6f, 1.15f);
constexpr glm::vec2 mountainCenter(0.0f, 1.8f);
constexpr float mountainWidthScale = 1.15f;
constexpr float mountainHeightScale = 1.20f;
constexpr float mountainHalfExtent = 1.65f * mountainWidthScale;

std::uint32_t landscapeHash(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

float landscapeNoise(glm::vec2 p)
{
    const glm::ivec2 cell(glm::floor(p));
    const glm::vec2 f = glm::fract(p), blend = f * f * (3.0f - 2.0f * f);
    auto sample = [](int x, int y) {
        const std::uint32_t horizontalSeed = static_cast<std::uint32_t>(x) * 73856093u;
        const std::uint32_t verticalSeed = static_cast<std::uint32_t>(y) * 19349663u;
        const std::uint32_t hash = landscapeHash(horizontalSeed ^ verticalSeed ^ 4515u);
        return (hash & 0xffffffu) / 16777215.0f;
    };
    const float lowerRow = glm::mix(sample(cell.x, cell.y), sample(cell.x + 1, cell.y), blend.x);
    const float upperRow = glm::mix(sample(cell.x, cell.y + 1), sample(cell.x + 1, cell.y + 1), blend.x);
    return glm::mix(lowerRow, upperRow, blend.y);
}

float mountainHeight(glm::vec2 world)
{
    const glm::vec2 p = (world - mountainCenter) / mountainWidthScale;
    const float angle = std::atan2(p.y, p.x);
    const float radius = glm::length(p / glm::vec2(1.30f, 1.14f));
    const float primaryOutlineWave = 0.10f * std::sin(5.0f * angle + 0.5f);
    const float secondaryOutlineWave = 0.065f * std::cos(3.0f * angle - 0.8f);
    const float outline = 1.0f + primaryOutlineWave + secondaryOutlineWave;
    const float envelope = glm::max(0.0f, 1.0f - (radius / outline) * (radius / outline));

    const glm::vec2 warped = p + glm::vec2(0.11f, -0.07f);
    const float coarseNoise = 0.34f * (landscapeNoise(warped * 3.0f) - 0.5f);
    const float mediumNoise = 0.18f * (landscapeNoise(warped * 7.0f) - 0.5f);
    const float fineNoise = 0.07f * (landscapeNoise(warped * 15.0f) - 0.5f);
    const float layered = coarseNoise + mediumNoise + fineNoise;

    auto crest = [p](glm::vec2 center, glm::vec2 extent, float height) {
        const glm::vec2 normalizedOffset = (p - center) / extent;
        const float crestFalloff = glm::max(0.0f, 1.0f - glm::length(normalizedOffset));
        return height * std::pow(crestFalloff, 1.10f);
    };
    const float main = crest({ -.32f, -.06f }, { .54f, .64f }, 2.15f);
    const float shoulder = crest({ .32f, .17f }, { .46f, .56f }, 1.65f);
    const float spur = crest({ -.27f, .34f }, { .46f, .51f }, 1.15f);
    const float rock = glm::max(main, glm::max(shoulder, spur));
    const float broadErosion = 0.20f * landscapeNoise(warped * 6.0f);
    const float detailErosion = 0.12f * (1.0f - std::abs(2.0f * landscapeNoise(warped * 14.0f) - 1.0f));
    const float erosion = 0.86f + broadErosion + detailErosion;
    const float heightAboveBase = 0.48f * envelope * envelope + rock * erosion + layered * envelope * envelope;
    return -1.003f + mountainHeightScale * heightAboveBase;
}

glm::vec3 mountainNormal(glm::vec2 world)
{
    constexpr float e = 0.005f;
    const float heightDifferenceX = mountainHeight(world - glm::vec2(e, 0)) - mountainHeight(world + glm::vec2(e, 0));
    const float heightDifferenceZ = mountainHeight(world - glm::vec2(0, e)) - mountainHeight(world + glm::vec2(0, e));
    const glm::vec3 gradientNormal(heightDifferenceX, 2.0f * e, heightDifferenceZ);
    return glm::normalize(gradientNormal);
}

Mesh createMountainMesh()
{
    Mesh mesh {};
    mesh.material.kd = glm::vec3(0.48f, 0.46f, 0.42f);
    constexpr unsigned int intervals = 112;
    for (unsigned int z = 0; z <= intervals; ++z)
        for (unsigned int x = 0; x <= intervals; ++x) {
            const glm::vec2 uv(static_cast<float>(x) / intervals, static_cast<float>(z) / intervals);
            const glm::vec2 world = mountainCenter + (uv * 2.0f - 1.0f) * mountainHalfExtent;
            mesh.vertices.push_back({ { world.x, mountainHeight(world), world.y }, glm::vec3(0), uv });
        }
    auto triangle = [&](unsigned int a, unsigned int b, unsigned int c) {
        const auto pa = mesh.vertices[a].position, pb = mesh.vertices[b].position, pc = mesh.vertices[c].position;
        if (glm::max(pa.y, glm::max(pb.y, pc.y)) < -0.999f)
            return;
        mesh.triangles.push_back({ a, b, c });
        const glm::vec3 normal = glm::cross(pb - pa, pc - pa);
        for (auto i : { a, b, c })
            mesh.vertices[i].normal += normal;
    };
    for (unsigned int z = 0; z < intervals; ++z)
        for (unsigned int x = 0; x < intervals; ++x) {
            const unsigned int a = z * (intervals + 1) + x, b = a + 1, c = a + intervals + 1, d = c + 1;
            triangle(a, c, b);
            triangle(b, c, d);
        }
    for (auto& vertex : mesh.vertices) {
        if (glm::dot(vertex.normal, vertex.normal) > 1e-16f) {
            vertex.normal = glm::normalize(vertex.normal);
        } else {
            vertex.normal = glm::vec3(0, 1, 0);
        }
    }
    return mesh;
}

struct MountainSurface {
    Mesh mesh = createMountainMesh();
    std::array<std::array<int, 2>, 112 * 112> cells {};
    glm::vec2 minimum { 1e10f }, maximum { -1e10f };

    MountainSurface()
    {
        for (auto& cell : cells)
            cell = { -1, -1 };
        for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
            const auto t = mesh.triangles[i];
            for (auto index : { t.x, t.y, t.z }) {
                const auto p = mesh.vertices[index].position;
                minimum = glm::min(minimum, glm::vec2(p.x, p.z));
                maximum = glm::max(maximum, glm::vec2(p.x, p.z));
            }
            const unsigned int x = std::min({ t.x % 113, t.y % 113, t.z % 113 });
            const unsigned int z = std::min({ t.x / 113, t.y / 113, t.z / 113 });
            int triangleInCell = 1;
            if (t.x == z * 113 + x) {
                triangleInCell = 0;
            }
            cells[z * 112 + x][triangleInCell] = static_cast<int>(i);
        }
    }
};

const MountainSurface& mountainSurface()
{
    static const MountainSurface surface;
    return surface;
}

float terrainHeightAt(float worldX, float worldZ)
{
    const auto& surface = mountainSurface();
    if (worldX < surface.minimum.x || worldZ < surface.minimum.y || worldX > surface.maximum.x
        || worldZ > surface.maximum.y)
        return -1.0f;
    const float gridScale = 112.0f / (2.0f * mountainHalfExtent);
    const float gridX = (worldX - mountainCenter.x + mountainHalfExtent) * gridScale;
    const float gridZ = (worldZ - mountainCenter.y + mountainHalfExtent) * gridScale;
    if (gridX < 0 || gridZ < 0 || gridX > 112 || gridZ > 112)
        return -1.0f;
    const int cellX = std::min(static_cast<int>(gridX), 111), cellZ = std::min(static_cast<int>(gridZ), 111);
    const float cellOffsetX = gridX - cellX, cellOffsetZ = gridZ - cellZ;
    const bool firstTriangle = cellOffsetX + cellOffsetZ <= 1.0f;
    int triangleInCell;
    if (firstTriangle) {
        triangleInCell = 0;
    } else {
        triangleInCell = 1;
    }
    const int triangle = surface.cells[cellZ * 112 + cellX][triangleInCell];
    if (triangle < 0)
        return -1.0f;
    const auto indices = surface.mesh.triangles[triangle];
    const float heightA = surface.mesh.vertices[indices.x].position.y,
                heightB = surface.mesh.vertices[indices.y].position.y,
                heightC = surface.mesh.vertices[indices.z].position.y;
    float interpolatedHeight;
    if (firstTriangle) {
        const float contributionA = (1 - cellOffsetX - cellOffsetZ) * heightA;
        const float contributionB = cellOffsetZ * heightB;
        const float contributionC = cellOffsetX * heightC;
        interpolatedHeight = contributionA + contributionB + contributionC;
    } else {
        const float contributionA = (1 - cellOffsetZ) * heightA;
        const float contributionB = (1 - cellOffsetX) * heightB;
        const float contributionC = (cellOffsetX + cellOffsetZ - 1) * heightC;
        interpolatedHeight = contributionA + contributionB + contributionC;
    }
    return std::max(-1.0f, interpolatedHeight);
}

Mesh createDragonFacialDetail(int kind)
{
    Mesh mesh {};
    if (kind == 0 || kind == 3) {
        mesh.material.kd = glm::vec3(.82f, .50f, .18f);
    } else if (kind == 1) {
        mesh.material.kd = glm::vec3(.43f, .035f, .025f);
    } else {
        mesh.material.kd = glm::vec3(.72f, .10f, .055f);
    }
    auto sweep = [&](const std::function<glm::vec3(float)>& center, const std::function<float(float)>& radius,
                     bool closed, float flatten) {
        unsigned int segments = 40;
        unsigned int sides = 12;
        if (kind == 3) {
            segments = 24;
            sides = 8;
        }
        const auto first = static_cast<unsigned int>(mesh.vertices.size());
        for (unsigned int ring = 0; ring <= segments; ++ring) {
            const float t = ring / static_cast<float>(segments);
            const glm::vec3 p = center(t);
            float nextParameter;
            float previousParameter;
            if (closed) {
                nextParameter = t + .0001f;
                previousParameter = t - .0001f;
            } else {
                nextParameter = glm::min(1.f, t + .0001f);
                previousParameter = glm::max(0.f, t - .0001f);
            }
            const glm::vec3 tangentDifference = center(nextParameter) - center(previousParameter);
            const glm::vec3 tangent = glm::normalize(tangentDifference);
            glm::vec3 reference;
            if (std::abs(tangent.y) > .9f) {
                reference = glm::vec3(1, 0, 0);
            } else {
                reference = glm::vec3(0, 1, 0);
            }
            const glm::vec3 right = glm::normalize(glm::cross(tangent, reference)), up = glm::cross(right, tangent);
            for (unsigned int side = 0; side <= sides; ++side) {
                unsigned int wrappedSide = side;
                if (side == sides) {
                    wrappedSide = 0;
                }
                const float angle = glm::two_pi<float>() * wrappedSide / sides;
                const glm::vec3 radial = right * std::cos(angle) + up * std::sin(angle) * flatten;
                mesh.vertices.push_back(
                    { p + radius(t) * radial, glm::normalize(radial), { t, side / static_cast<float>(sides) } });
            }
        }
        auto triangle = [&](unsigned int a, unsigned int b, unsigned int c) {
            auto& va = mesh.vertices[a];
            auto& vb = mesh.vertices[b];
            auto& vc = mesh.vertices[c];
            const glm::vec3 faceNormal = glm::cross(vb.position - va.position, vc.position - va.position);
            const glm::vec3 expectedNormal = va.normal + vb.normal + vc.normal;
            if (glm::dot(faceNormal, expectedNormal) < 0) {
                std::swap(b, c);
            }
            mesh.triangles.push_back({ a, b, c });
        };
        for (unsigned int ring = 0; ring < segments; ++ring)
            for (unsigned int side = 0; side < sides; ++side) {
                const auto a = first + ring * (sides + 1) + side, b = a + 1, c = a + sides + 1, d = c + 1;
                triangle(a, b, c);
                triangle(b, d, c);
            }
        for (unsigned int i = first; i < mesh.vertices.size(); ++i)
            mesh.vertices[i].normal = glm::vec3(0);
        for (const auto tri : mesh.triangles)
            if (tri.x >= first) {
                const auto normal = glm::cross(mesh.vertices[tri.y].position - mesh.vertices[tri.x].position,
                    mesh.vertices[tri.z].position - mesh.vertices[tri.x].position);
                for (auto i : { tri.x, tri.y, tri.z })
                    mesh.vertices[i].normal += normal;
            }
        for (unsigned int ring = 0; ring <= segments; ++ring) {
            const auto a = first + ring * (sides + 1), b = a + sides;
            const auto normal = mesh.vertices[a].normal + mesh.vertices[b].normal;
            mesh.vertices[a].normal = mesh.vertices[b].normal = normal;
        }
        if (closed)
            for (unsigned int side = 0; side <= sides; ++side) {
                const auto a = first + side, b = first + segments * (sides + 1) + side;
                const auto normal = mesh.vertices[a].normal + mesh.vertices[b].normal;
                mesh.vertices[a].normal = mesh.vertices[b].normal = normal;
            }
        for (unsigned int i = first; i < mesh.vertices.size(); ++i)
            mesh.vertices[i].normal = glm::normalize(mesh.vertices[i].normal);
    };
    auto bezier = [](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, float t) {
        const float u = 1 - t;
        const glm::vec3 startContribution = u * u * u * a;
        const glm::vec3 firstControlContribution = 3 * u * u * t * b;
        const glm::vec3 secondControlContribution = 3 * u * t * t * c;
        const glm::vec3 endContribution = t * t * t * d;
        return startContribution + firstControlContribution + secondControlContribution + endContribution;
    };
    if (kind == 0)
        for (float side : { -1.f, 1.f }) {
            sweep(
                [=](float t) {
                    return bezier({ side * .050f, .090f, -.26f }, { side * .110f, .145f, -.23f },
                        { side * .148f, .125f, -.12f }, { side * .100f, .085f, -.005f }, t);
                },
                [](float t) { return .005f + .015f * std::sin(glm::pi<float>() * (.15f + .85f * t)); }, false, .65f);
        }
    else if (kind == 1)
        for (float side : { -1.f, 1.f }) {
            const glm::mat4 eye = glm::rotate(glm::translate(glm::mat4(1), { side * .125f, .062f, -.16f }),
                glm::radians(side * -12.f), glm::vec3(0, 1, 0));
            sweep(
                [=](float t) {
                    const float angle = glm::two_pi<float>() * t;
                    return glm::vec3(
                        eye * glm::vec4(side * .009f, .022f * std::sin(angle), .043f * std::cos(angle), 1));
                },
                [](float) { return .0038f; }, true, 1.f);
        }
    else if (kind == 2)
        for (int ridge = -1; ridge <= 1; ++ridge) {
            const float x = ridge * .031f, lower = std::abs(ridge) * .008f;
            sweep(
                [=](float t) {
                    return bezier({ x, .112f - lower, -.16f }, { x, .153f - lower, -.06f }, { x, .147f - lower, .02f },
                        { x, .093f - lower, .14f }, t);
                },
                [=](float t) {
                    float ridgeScale;
                    if (ridge == 0) {
                        ridgeScale = 1.f;
                    } else {
                        ridgeScale = .68f;
                    }
                    const float radius = .003f + .009f * std::sin(glm::pi<float>() * t);
                    return radius * ridgeScale;
                },
                false, .60f);
        }
    else if (kind == 3) {
        // Visual polish: layered curled moustaches frame the muzzle without crossing the fire opening.
        for (float side : { -1.0f, 1.0f }) {
            for (int layer = 0; layer < 3; ++layer) {
                const float rootZ = -0.268f + layer * 0.080f;
                const float rootY = 0.032f + layer * 0.008f;
                float rootX = 0.044f;
                if (layer == 0) {
                    rootX = 0.051f;
                } else if (layer == 2) {
                    rootX = 0.048f;
                }
                sweep(
                    [=](float t) {
                        const glm::vec3 root(side * rootX, rootY, rootZ);
                        const glm::vec3 outward(side * 0.092f, rootY + 0.032f, rootZ + 0.018f);
                        const glm::vec3 curlBase(side * 0.130f, rootY - 0.010f, rootZ + 0.078f);
                        const glm::vec3 curlTip(side * 0.153f, rootY + 0.043f, rootZ + 0.048f);
                        return bezier(root, outward, curlBase, curlTip, t);
                    },
                    [](float t) {
                        const float taper = std::pow(1.0f - t, 1.3f);
                        return 0.001f + 0.009f * taper;
                    },
                    false, 0.80f);
            }
        }
        // Raised bridge ridges fill the bare area between the nostrils and forehead.
        for (int ridge = -1; ridge <= 1; ++ridge) {
            const float x = ridge * 0.021f;
            sweep(
                [=](float t) {
                    return bezier({ x, 0.075f, -0.270f }, { x, 0.055f, -0.205f },
                        { x, 0.070f, -0.105f }, { x, 0.096f, -0.025f }, t);
                },
                [](float t) {
                    const float raisedCenter = std::sin(glm::pi<float>() * t);
                    return 0.0018f + 0.0022f * raisedCenter;
                },
                false, 0.65f);
        }
    }
    return mesh;
}

Mesh createLandscapeRock(unsigned int variant)
{
    Mesh mesh {};
    constexpr unsigned int sides = 9, rings = 6;
    std::array<glm::vec3, 2 + (rings - 1) * sides> points {};
    points[0] = { 0, -0.65f, 0 };
    points.back() = { 0.08f, 0.85f, -0.10f };
    for (unsigned int ring = 1; ring < rings; ++ring)
        for (unsigned int side = 0; side < sides; ++side) {
            const float latitude = glm::pi<float>() * ring / rings, angle = glm::two_pi<float>() * side / sides;
            const float irregular =
                0.80f + 0.30f * ((landscapeHash(variant * 3571u + ring * 31u + side) & 65535u) / 65535.0f);
            points[1 + (ring - 1) * sides + side] = glm::vec3(std::sin(latitude) * std::cos(angle) * irregular,
                -0.75f * std::cos(latitude) + 0.05f * std::sin(angle * 3 + variant),
                std::sin(latitude) * std::sin(angle) * irregular);
        }
    auto tri = [&](unsigned int a, unsigned int b, unsigned int c) {
        glm::vec3 pa = points[a], pb = points[b], pc = points[c];
        if (glm::dot(glm::cross(pb - pa, pc - pa), (pa + pb + pc) / 3.0f) < 0)
            std::swap(pb, pc);
        const glm::vec3 n = glm::normalize(glm::cross(pb - pa, pc - pa));
        const auto first = static_cast<unsigned int>(mesh.vertices.size());
        for (auto p : { pa, pb, pc })
            mesh.vertices.push_back({ p, n, { 0, 0 } });
        mesh.triangles.push_back({ first, first + 1, first + 2 });
    };
    for (unsigned int side = 0; side < sides; ++side) {
        const auto next = (side + 1) % sides;
        tri(0, 1 + next, 1 + side);
        for (unsigned int r = 0; r < rings - 2; ++r) {
            const auto a = 1 + r * sides + side, b = 1 + r * sides + next;
            tri(a, b, a + sides);
            tri(b, b + sides, a + sides);
        }
        tri(1 + (rings - 2) * sides + side, 1 + (rings - 2) * sides + next,
            static_cast<unsigned int>(points.size() - 1));
    }
    return mesh;
}

std::array<Mesh, 5> createLandscapeDetails()
{
    std::array<Mesh, 5> result {};
    result[0].material.kd = { 0.44f, 0.43f, 0.40f };
    result[1].material.kd = { 0.48f, 0.40f, 0.31f };
    result[2].material.kd = { 0.54f, 0.51f, 0.44f };
    result[3].material.kd = { 0.31f, 0.36f, 0.18f };
    result[4].material.kd = { 0.39f, 0.43f, 0.24f };
    std::uint32_t state = 4515u;
    auto random = [&]() {
        state = 1664525u * state + 1013904223u;
        return (state >> 8) / 16777216.0f;
    };
    const std::array<Mesh, 3> shapes { createLandscapeRock(0), createLandscapeRock(1), createLandscapeRock(2) };
    std::vector<glm::vec3> occupied;
    auto awayFromRoute = [](glm::vec2 p) { return p.x < -5.1f || p.x > 5.1f || p.y < -2.6f || p.y > 6.35f; };
    for (int rock = 0; rock < 158; ++rock) {
        float size;
        if (rock < 120) {
            size = glm::mix(0.045f, 0.13f, random());
        } else if (rock < 150) {
            size = glm::mix(0.17f, 0.29f, random());
        } else {
            size = glm::mix(0.43f, 0.65f, random());
        }
        glm::vec2 location(0);
        bool placed = false;
        for (int attempt = 0; attempt < 300 && !placed; ++attempt) {
            location = glm::vec2(random(), random()) * 18.0f - 9.0f;
            if (glm::length(location - mountainCenter) < mountainHalfExtent || (rock >= 150 && !awayFromRoute(location)))
                continue;
            placed = true;
            for (auto other : occupied)
                if (glm::length(location - glm::vec2(other)) < size + other.z + 0.06f) {
                    placed = false;
                    break;
                }
        }
        if (!placed)
            continue;
        occupied.push_back({ location, size });
        const int variant = rock % 3;
        const glm::mat4 rotation =
            glm::rotate(glm::rotate(glm::mat4(1), random() * glm::two_pi<float>(), glm::vec3(0, 1, 0)),
                (random() - 0.5f) * 0.35f, glm::vec3(1, 0, 0));
        const glm::mat4 local =
            glm::scale(rotation, glm::vec3(size * (0.8f + random() * 0.5f), size * (0.65f + random() * 0.5f), size));
        float bottom = 1e10f;
        for (const auto& v : shapes[variant].vertices)
            bottom = glm::min(bottom, glm::vec3(local * glm::vec4(v.position, 1)).y);
        const glm::mat4 transform =
            glm::translate(glm::mat4(1), glm::vec3(location.x, -1.0f - bottom, location.y)) * local;
        const auto normal = glm::inverseTranspose(glm::mat3(transform));
        Mesh& mesh = result[variant];
        const auto first = static_cast<unsigned int>(mesh.vertices.size());
        for (auto v : shapes[variant].vertices) {
            v.position = glm::vec3(transform * glm::vec4(v.position, 1));
            v.normal = glm::normalize(normal * v.normal);
            mesh.vertices.push_back(v);
        }
        for (auto t : shapes[variant].triangles)
            mesh.triangles.push_back(t + glm::uvec3(first));
    }
    for (int patch = 0; patch < 32; ++patch) {
        const glm::vec2 center = glm::vec2(random(), random()) * 17.6f - 8.8f;
        if (glm::length(center - mountainCenter) < mountainHalfExtent + 0.15f)
            continue;
        for (int clump = 0; clump < 4; ++clump) {
            const glm::vec2 root = center + (glm::vec2(random(), random()) - 0.5f) * 0.65f;
            bool clear = glm::length(root - mountainCenter) >= mountainHalfExtent;
            const glm::vec2 pondDistance = (root - waterPondCenter) / waterPondRadii;
            if (glm::length(pondDistance) < 1.12f)
                clear = false;
            for (auto rock : occupied)
                if (glm::length(root - glm::vec2(rock)) < rock.z + 0.12f)
                    clear = false;
            if (!clear)
                continue;
            Mesh& mesh = result[3 + clump % 2];
            for (int blade = 0; blade < 7; ++blade) {
                const float angle = random() * glm::two_pi<float>(), height = 0.10f + random() * 0.14f,
                            width = 0.010f + random() * 0.009f;
                const glm::vec3 across(std::cos(angle), 0, std::sin(angle)), bend(-std::sin(angle), 0, std::cos(angle));
                const glm::vec3 origin(root.x + (random() - 0.5f) * 0.08f, -0.998f, root.y + (random() - 0.5f) * 0.08f);
                const std::array<glm::vec3, 5> points { origin - across * width, origin + across * width,
                    origin + glm::vec3(0, height * .55f, 0) + bend * height * .12f - across * width * .55f,
                    origin + glm::vec3(0, height * .55f, 0) + bend * height * .12f + across * width * .55f,
                    origin + glm::vec3(0, height, 0) + bend * height * .35f };
                for (auto t : { glm::uvec3(0, 1, 2), glm::uvec3(1, 3, 2), glm::uvec3(2, 3, 4) }) {
                    const glm::vec3 n =
                        glm::normalize(glm::cross(points[t.y] - points[t.x], points[t.z] - points[t.x]));
                    const auto first = static_cast<unsigned int>(mesh.vertices.size());
                    for (int face = 0; face < 2; ++face) {
                        glm::vec3 faceNormal;
                        glm::uvec3 faceIndices;
                        if (face) {
                            faceNormal = -n;
                            faceIndices = glm::uvec3(first + 3, first + 5, first + 4);
                        } else {
                            faceNormal = n;
                            faceIndices = glm::uvec3(first, first + 1, first + 2);
                        }
                        for (auto i : { t.x, t.y, t.z }) {
                            mesh.vertices.push_back({ points[i], faceNormal, { 0, 0 } });
                        }
                        mesh.triangles.push_back(faceIndices);
                    }
                }
            }
        }
    }
    return result;
}

constexpr std::size_t dragonBoneCount = 8;
constexpr float dragonJointSpacing = 0.11f * 1.35f;
constexpr float dragonNeckZ = -0.34f;
constexpr float dragonBodyEndZ = -0.32f + 8.0f * dragonJointSpacing;
constexpr float dragonTailTipZ = dragonNeckZ + (0.725f - dragonNeckZ) * 1.35f + 0.15f;

SkinInfluence dragonSkinInfluence(float z)
{

    const float firstJointCenter = -0.32f + dragonJointSpacing * 0.5f;
    const float jointCoordinate = (z - firstJointCenter) / dragonJointSpacing;
    const float coordinate = glm::clamp(jointCoordinate, 0.0f, 7.0f);
    const int first = std::min(static_cast<int>(coordinate), 6);
    const float blend = glm::smoothstep(0.0f, 1.0f, coordinate - static_cast<float>(first));
    return { glm::ivec2(first, first + 1), glm::vec2(1.0f - blend, blend) };
}

float dragonBodyRadius(float z)
{
    const float slope = (0.057f - 0.12f) / (dragonBodyEndZ - dragonNeckZ);
    if (z <= dragonBodyEndZ) {

        const float t = (z - dragonNeckZ) / (dragonBodyEndZ - dragonNeckZ);
        const float envelope = std::sin(glm::radians(180.0f) * t);
        const float taperedRadius = 0.12f + slope * (z - dragonNeckZ);
        const float radiusWave = 1.0f + 0.12f * std::sin(glm::radians(360.0f) * t) * envelope * envelope;
        return taperedRadius * radiusWave;
    }

    const float length = dragonTailTipZ - dragonBodyEndZ;
    const float t = glm::clamp((z - dragonBodyEndZ) / length, 0.0f, 1.0f);
    const float radiusContribution = (2 * t * t * t - 3 * t * t + 1) * 0.057f;
    const float startSlopeContribution = (t * t * t - 2 * t * t + t) * length * slope;
    const float tipSlopeContribution = (t * t * t - t * t) * length * -0.3f;
    return radiusContribution + startSlopeContribution + tipSlopeContribution;
}

Mesh createDragonBodyMesh()
{
    Mesh mesh {};
    mesh.material.kd = glm::vec3(0.68f, 0.075f, 0.055f);
    constexpr unsigned int intervals = 128, sides = 32;
    for (unsigned int ring = 0; ring <= intervals; ++ring) {
        const float z = glm::mix(dragonNeckZ, dragonTailTipZ, static_cast<float>(ring) / intervals);
        const float radius = dragonBodyRadius(z);
        constexpr float epsilon = 0.0001f;
        const float slope = (dragonBodyRadius(z + epsilon) - dragonBodyRadius(z - epsilon)) / (2.0f * epsilon);
        for (unsigned int side = 0; side <= sides; ++side) {

            const float u = static_cast<float>(side) / sides;
            float wrappedU = u;
            if (side == sides) {
                wrappedU = 0.0f;
            }
            const float angle = glm::radians(360.0f) * wrappedU - glm::radians(90.0f);
            const glm::vec3 radial(std::cos(angle), std::sin(angle), 0);
            mesh.vertices.push_back({ glm::vec3(radius * radial.x, radius * radial.y, z),
                glm::normalize(glm::vec3(radial.x, radial.y, -slope)),
                glm::vec2(4.0f * u, (z - dragonNeckZ) / 0.11f) });
        }
    }
    for (unsigned int ring = 0; ring < intervals; ++ring) {
        for (unsigned int side = 0; side < sides; ++side) {
            const unsigned int a = ring * (sides + 1) + side, b = a + 1;
            const unsigned int d = a + sides + 1, c = d + 1;
            mesh.triangles.push_back({ a, b, c });
            if (ring + 1 < intervals)
                mesh.triangles.push_back({ a, c, d });
        }
    }

    const unsigned int center = static_cast<unsigned int>(mesh.vertices.size());
    mesh.vertices.push_back({ { 0, 0, dragonNeckZ }, { 0, 0, -1 }, { 0, 0 } });
    for (unsigned int side = 0; side < sides; ++side)
        mesh.triangles.push_back({ center, side + 1, side });
    return mesh;
}

GPUMesh createSkinnedDragonBody()
{
    const Mesh mesh = createDragonBodyMesh();
    std::vector<SkinInfluence> influences;
    influences.reserve(mesh.vertices.size());
    for (const Vertex& vertex : mesh.vertices)
        influences.push_back(dragonSkinInfluence(vertex.position.z));
    return GPUMesh(mesh, influences);
}

Mesh createGroundMesh()
{

    Mesh ground {};
    ground.vertices = { { { -10.0f, -1.0f, -10.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
        { { -10.0f, -1.0f, 10.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 10.0f } },
        { { 10.0f, -1.0f, 10.0f }, { 0.0f, 1.0f, 0.0f }, { 10.0f, 10.0f } },
        { { 10.0f, -1.0f, -10.0f }, { 0.0f, 1.0f, 0.0f }, { 10.0f, 0.0f } } };
    ground.triangles = { { 0, 1, 2 }, { 0, 2, 3 } };
    ground.material.kd = glm::vec3(1.0f);

    ground.material.kdTexture = std::make_shared<Image>(RESOURCE_ROOT "resources/checkerboard.png");
    return ground;
}

Mesh createWaterPondMesh()
{
    Mesh pond {};
    pond.material.kd = glm::vec3(0.025f, 0.24f, 0.27f);
    constexpr unsigned int rings = 24;
    constexpr unsigned int sections = 96;
    pond.vertices.push_back({ { waterPondCenter.x, -0.94f, waterPondCenter.y }, { 0, 1, 0 }, { 0, 0 } });
    for (unsigned int ring = 1; ring <= rings; ++ring) {
        const float radius = static_cast<float>(ring) / rings;
        for (unsigned int section = 0; section <= sections; ++section) {
            const float angle = glm::two_pi<float>() * static_cast<float>(section) / sections;
            const float shoreline = 1.0f + 0.05f * std::sin(3 * angle) + 0.03f * std::cos(5 * angle);
            const glm::vec2 direction(std::cos(angle), std::sin(angle));
            const glm::vec2 point = waterPondCenter + direction * waterPondRadii * radius * shoreline;
            pond.vertices.push_back({ { point.x, -0.94f, point.y }, { 0, 1, 0 }, { radius, angle } });
        }
    }
    for (unsigned int section = 0; section < sections; ++section)
        pond.triangles.push_back({ 0, section + 2, section + 1 });
    for (unsigned int ring = 1; ring < rings; ++ring) {
        for (unsigned int section = 0; section < sections; ++section) {
            const unsigned int inner = 1 + (ring - 1) * (sections + 1) + section;
            const unsigned int outer = inner + sections + 1;
            pond.triangles.push_back({ inner, inner + 1, outer });
            pond.triangles.push_back({ inner + 1, outer + 1, outer });
        }
    }
    return pond;
}

Mesh createLavaLakeMesh()
{
    Mesh lake {};
    constexpr unsigned int sections = 128;
    for (unsigned int section = 0; section <= sections; ++section) {
        const float angle = glm::two_pi<float>() * static_cast<float>(section) / sections;
        const glm::vec2 radial(std::cos(angle), std::sin(angle));
        const float shoreline = 2.65f + 0.14f * std::sin(5 * angle) + 0.10f * std::cos(3 * angle);
        for (int ring = 0; ring < 2; ++ring) {
            float radius = 0.65f;
            if (ring == 1)
                radius = shoreline;
            const glm::vec2 world = mountainCenter + radial * radius;
            const glm::vec2 uv = (world - mountainCenter) * 0.6f;
            lake.vertices.push_back({ { world.x, -0.985f, world.y }, { 0, 1, 0 }, uv });
        }
    }
    for (unsigned int section = 0; section < sections; ++section) {
        const unsigned int inner = section * 2, outer = inner + 1;
        lake.triangles.push_back({ inner, inner + 2, outer });
        lake.triangles.push_back({ outer, inner + 2, outer + 2 });
    }
    lake.material.kd = glm::vec3(1);
    return lake;
}

Mesh createMountainLavaStreams()
{
    Mesh streams {};
    constexpr unsigned int steps = 96;
    const glm::vec2 summit = mountainCenter + mountainWidthScale * glm::vec2(-0.32f, -0.06f);
    const std::array<glm::vec2, 3> outlets {
        mountainCenter + mountainWidthScale * glm::vec2(1.65f, 0.8f),
        mountainCenter + mountainWidthScale * glm::vec2(-1.5f, -0.95f),
        mountainCenter + mountainWidthScale * glm::vec2(-0.25f, 1.65f)
    };
    for (const glm::vec2 outlet : outlets) {
        const unsigned int first = static_cast<unsigned int>(streams.vertices.size());
        const glm::vec2 direction = glm::normalize(outlet - summit);
        const glm::vec2 sideways(-direction.y, direction.x);
        for (unsigned int step = 0; step <= steps; ++step) {
            const float progress = static_cast<float>(step) / steps;
            const float bend = mountainWidthScale * 0.09f * std::sin(progress * glm::two_pi<float>());
            const glm::vec2 center = glm::mix(summit, outlet, progress) + sideways * bend;
            const float halfWidth = mountainWidthScale * glm::mix(0.055f, 0.105f, progress);
            for (int edge = 0; edge < 2; ++edge) {
                const float side = static_cast<float>(edge) * 2.0f - 1.0f;
                const glm::vec2 point = center + sideways * halfWidth * side;
                const float surface = glm::max(terrainHeightAt(point.x, point.y), -0.985f);
                streams.vertices.push_back({ { point.x, surface + 0.025f, point.y },
                    { 0, 1, 0 }, { static_cast<float>(edge), progress * 2.0f } });
            }
        }
        for (unsigned int step = 0; step < steps; ++step) {
            const unsigned int left = first + step * 2, right = left + 1;
            streams.triangles.push_back({ left, right, left + 2 });
            streams.triangles.push_back({ right, right + 2, left + 2 });
        }
    }
    streams.material.kd = glm::vec3(1);
    return streams;
}

struct CubicBezierSegment {
    glm::vec3 p0;
    glm::vec3 p1;
    glm::vec3 p2;
    glm::vec3 p3;
};

glm::vec3 evaluateCubicBezier(const CubicBezierSegment& segment, float t)
{
    const float u = 1.0f - t;
    const glm::vec3 startContribution = u * u * u * segment.p0;
    const glm::vec3 firstControlContribution = 3.0f * u * u * t * segment.p1;
    const glm::vec3 secondControlContribution = 3.0f * u * t * t * segment.p2;
    const glm::vec3 endContribution = t * t * t * segment.p3;
    return startContribution + firstControlContribution + secondControlContribution + endContribution;
}

glm::vec3 evaluateCubicBezierTangent(const CubicBezierSegment& segment, float t)
{
    const float u = 1.0f - t;
    const glm::vec3 startTangent = 3.0f * u * u * (segment.p1 - segment.p0);
    const glm::vec3 middleTangent = 6.0f * u * t * (segment.p2 - segment.p1);
    const glm::vec3 endTangent = 3.0f * t * t * (segment.p3 - segment.p2);
    return startTangent + middleTangent + endTangent;
}

struct ParticlePassState {
    GLint drawFramebuffer, readFramebuffer, viewport[4], program, vao, activeTexture;
    GLint textures[2], blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha, blendEquationRgb, blendEquationAlpha;
    GLboolean depthTest, depthWrite, blend, cull, scissor;

    ParticlePassState()
    {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        for (int unit = 0; unit < 2; ++unit) {
            glActiveTexture(GL_TEXTURE6 + unit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[unit]);
        }
        depthTest = glIsEnabled(GL_DEPTH_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
        blend = glIsEnabled(GL_BLEND);
        cull = glIsEnabled(GL_CULL_FACE);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
    }

    ~ParticlePassState()
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glUseProgram(program);
        glBindVertexArray(vao);
        for (int unit = 0; unit < 2; ++unit) {
            glActiveTexture(GL_TEXTURE6 + unit);
            glBindTexture(GL_TEXTURE_2D, textures[unit]);
        }
        glActiveTexture(activeTexture);
        auto restore = [](GLenum capability, GLboolean enabled) {
            if (enabled)
                glEnable(capability);
            else
                glDisable(capability);
        };
        restore(GL_DEPTH_TEST, depthTest);
        glDepthMask(depthWrite);
        restore(GL_BLEND, blend);
        restore(GL_CULL_FACE, cull);
        restore(GL_SCISSOR_TEST, scissor);
        glBlendFuncSeparate(blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha);
        glBlendEquationSeparate(blendEquationRgb, blendEquationAlpha);
    }
};

struct BloomPassState {
    ParticlePassState common;
    GLint textures[2], renderbuffer;

    BloomPassState()
    {
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
        for (int i = 0; i < 2; ++i) {
            glActiveTexture(GL_TEXTURE8 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[i]);
        }
    }

    ~BloomPassState()
    {
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        for (int i = 0; i < 2; ++i) {
            glActiveTexture(GL_TEXTURE8 + i);
            glBindTexture(GL_TEXTURE_2D, textures[i]);
        }
    }
};

class Application {
public:
    Application()
        : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41)
        , m_groundMesh(createGroundMesh())
        , m_waterMesh(createWaterPondMesh())
        , m_lavaMesh(createLavaLakeMesh())
        , m_lavaStreamsMesh(createMountainLavaStreams())
        , m_mountainMesh(mountainSurface().mesh)
        , m_bodyMesh(makeCollisionBody())
        , m_headMesh(makeCollisionMesh(
              createDragonFaceProfile(
                  { { 0, 0, 0.020f, -0.32f }, { 0.054f, 0.045f, 0.035f, -0.275f },
                      { 0.092f, 0.080f, 0.050f, -0.21f }, { 0.116f, 0.093f, 0.050f, -0.12f },
                      { 0.125f, 0.095f, 0.035f, 0.0f }, { 0.102f, 0.095f, 0.010f, 0.10f },
                      { 0.082f, 0.090f, -0.005f, 0.19f }, { 0, 0, 0, 0.25f } },
                  glm::vec3(0.62f, 0.065f, 0.045f), 0.84f, 0.78f),
              1, 33))
        , m_snoutMesh(makeCollisionMesh(createDragonSnoutMesh(), 2, 33))
        , m_jawMesh(makeCollisionMouthMesh(createDragonJawMesh(), 3, 33))
        , m_throatMesh(makeCollisionMesh(
              createDragonFaceProfile(
                  { { 0, 0, -0.066f, -0.59f }, { 0.055f, 0.030f, -0.065f, -0.56f }, { 0.074f, 0.046f, -0.060f, -0.53f },
                      { 0.092f, 0.061f, -0.050f, -0.46f }, { 0.108f, 0.082f, -0.027f, -0.38f },
                      { 0.112f, 0.105f, -0.008f, -0.29f }, { 0.09f, 0.085f, 0, -0.23f }, { 0, 0, 0, -0.18f } },
                  glm::vec3(0.82f, 0.57f, 0.36f)),
              4, 33))
        , m_faceDetailMesh(makeCollisionMesh(createDragonRoundedMesh(glm::vec3(0.62f, 0.065f, 0.045f)), 5, 33))

        , m_goldDetailMesh(makeCollisionMesh(
              createDragonRevolvedMesh({ { 0, 0 }, { 1, 0 }, { 0, 1 } }, glm::vec3(0.95f, 0.74f, 0.3f)), 6, 0))
        , m_lightDetailMesh(makeCollisionMesh(createDragonRoundedMesh(glm::vec3(0.82f, 0.57f, 0.36f)), 7, 33))
        , m_eyeMesh(makeCollisionMesh(createDragonRoundedMesh(glm::vec3(0.025f, 0.04f, 0.03f)), 8, 33))
        , m_hornMesh(makeCollisionMesh(createDragonTendrilMesh(0.12f, -0.45f, glm::vec3(0.95f, 0.74f, 0.3f)), 9, 17))
        , m_whiskerMesh(
              makeCollisionMesh(createDragonTendrilMesh(0.014f, 0.35f, glm::vec3(0.82f, 0.57f, 0.36f)), 10, 17))
        , m_browMesh(makeCollisionMesh(createDragonFacialDetail(0), 11, 13))
        , m_eyelidMesh(makeCollisionMesh(createDragonFacialDetail(1), 12, 13))
        , m_foreheadMesh(makeCollisionMesh(createDragonFacialDetail(2), 13, 13))
        , m_teethMesh(makeCollisionMesh(createDragonTeethMesh(), 14, 3))
        , m_lowerTeethMesh(makeCollisionMouthMesh(createDragonTeethMesh(DragonTeethRow::Lower), 15, 3))
        , m_muzzleDetailsMesh(makeCollisionMesh(createDragonFacialDetail(3), 16, 9))
        , m_texture(RESOURCE_ROOT "resources/checkerboard.png")
    {
        m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                onKeyPressed(key, mods);
            else if (action == GLFW_RELEASE)
                onKeyReleased(key, mods);
        });
        m_window.registerScrollCallback([this](const glm::vec2& offset) { m_pendingCameraScroll += offset.y; });
        m_window.registerMouseMoveCallback(std::bind(&Application::onMouseMove, this, std::placeholders::_1));
        m_window.registerMouseButtonCallback([this](int button, int action, int mods) {
            if (action == GLFW_PRESS)
                onMouseClicked(button, mods);
            else if (action == GLFW_RELEASE)
                onMouseReleased(button, mods);
        });

        initializeDragonSkeleton();
        initializeCollisionSamples();
        initializeFlightArcLengths();
        initializeDragonScaleTexture();
        initializeGroundSurface();
        initializeLavaFrames();
        initializeLandscape();
        initializeEnvironmentMap();
        initializeShadowMap();
        initializeShadowMap(true);

        m_meshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/dragon.obj");

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_depth_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shadow_depth_frag.glsl");
            m_shadowShader = shadowBuilder.build();

            ShaderBuilder bezierBuilder;
            bezierBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            bezierBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/bezier_frag.glsl");
            m_bezierShader = bezierBuilder.build();
            initializeBezierLines();

            ShaderBuilder particleUpdateBuilder;
            particleUpdateBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/particle_update_vert.glsl");
            particleUpdateBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/particle_update_frag.glsl");
            m_particleUpdateShader = particleUpdateBuilder.build();
            ShaderBuilder particleRenderBuilder;
            particleRenderBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/particle_render_vert.glsl");
            particleRenderBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/particle_render_frag.glsl");
            m_particleRenderShader = particleRenderBuilder.build();
            initializeDragonParticles();

            ShaderBuilder skyBuilder;
            skyBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/sky_vert.glsl");
            skyBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/sky_frag.glsl");
            m_skyShader = skyBuilder.build();
            glGenVertexArrays(1, &m_skyVao);
            m_skyReady = true;

            ShaderBuilder waterBuilder;
            waterBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/water_vert.glsl");
            waterBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/water_frag.glsl");
            m_waterShader = waterBuilder.build();
            m_waterReady = true;

            auto bloomShader = [](const char* fragment) {
                ShaderBuilder builder;
                builder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/bloom_vert.glsl");
                builder.addStage(GL_FRAGMENT_SHADER, fragment);
                return builder.build();
            };
            m_bloomExtractShader = bloomShader(RESOURCE_ROOT "shaders/bloom_extract_frag.glsl");
            m_bloomBlurShader = bloomShader(RESOURCE_ROOT "shaders/bloom_blur_frag.glsl");
            m_bloomCombineShader = bloomShader(RESOURCE_ROOT "shaders/bloom_combine_frag.glsl");
            glGenVertexArrays(1, &m_bloomVao);
            m_bloomShadersReady = true;

        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }
    }

    ~Application()
    {

        freeBloomTargets();
        glDeleteVertexArrays(1, &m_bloomVao);
        glDeleteTextures(1, &m_mountainTexture);
        glDeleteTextures(1, &m_groundSurfaceTexture);
        glDeleteTextures(static_cast<GLsizei>(m_lavaFrames.size()), m_lavaFrames.data());
        glDeleteVertexArrays(1, &m_skyVao);
        glDeleteFramebuffers(2, m_particleFramebuffers.data());
        glDeleteTextures(2, m_particlePositions.data());
        glDeleteTextures(2, m_particleVelocities.data());
        glDeleteVertexArrays(1, &m_particleVao);
        glDeleteFramebuffers(1, &m_shadowFramebuffer);
        glDeleteTextures(1, &m_shadowDepthTexture);
        glDeleteFramebuffers(1, &m_fireShadowFramebuffer);
        glDeleteTextures(1, &m_fireShadowDepthTexture);
        glDeleteTextures(1, &m_environmentMap);
        glDeleteTextures(1, &m_dragonRoughnessTexture);
        glDeleteTextures(1, &m_dragonMetallicTexture);
        glDeleteTextures(1, &m_dragonNormalTexture);
        glDeleteTextures(1, &m_dragonScaleTexture);
        glDeleteBuffers(1, &m_bezierVbo);
        glDeleteVertexArrays(1, &m_bezierVao);
    }

    void update()
    {
        auto previousFrameTime = std::chrono::steady_clock::now();
        while (!m_window.shouldClose()) {
            const auto currentFrameTime = std::chrono::steady_clock::now();
            m_deltaTime = std::chrono::duration<float>(currentFrameTime - previousFrameTime).count();
            previousFrameTime = currentFrameTime;

            m_window.updateInput();

            for (int shortcut = 0; shortcut < 3; ++shortcut) {
                const bool down = m_window.isKeyPressed(GLFW_KEY_F1 + shortcut);
                if (down && !m_cameraShortcutWasDown[shortcut] && !ImGui::GetIO().WantCaptureKeyboard) {
                    const auto mode = static_cast<CameraMode>(shortcut);
                    if (mode != m_cameraMode) {
                        m_cameraMode = mode;
                        m_cameraDragging = false;
                    }
                }
                m_cameraShortcutWasDown[shortcut] = down;
            }

            const bool fireKeyDown = m_window.isKeyPressed(GLFW_KEY_1);
            if (fireKeyDown && !m_fireKeyWasDown && !ImGui::GetIO().WantCaptureKeyboard) {
                m_fireBreathing = !m_fireBreathing;
            }
            m_fireKeyWasDown = fireKeyDown;

            bool flightModeChanged = false;
            bool constraintsChanged = false;
            bool resetDragon = false;
            const ImVec2 screen = ImGui::GetIO().DisplaySize;
            const float maximumHeight = std::min(760.0f, std::max(300.0f, screen.y - 28.0f));
            ImGui::SetNextWindowSize(ImVec2(420, std::min(680.0f, maximumHeight)), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(360, 300), ImVec2(560, maximumHeight));
            if (ImGui::Begin("Dragon Tech Demo")) {
                const ImVec4 mandatoryColor(0.95f, 0.27f, 0.25f, 1.0f);
                const ImVec4 additionalColor(0.28f, 0.60f, 1.0f, 1.0f);
                const ImU32 mandatoryLine = ImGui::ColorConvertFloat4ToU32(mandatoryColor);
                const ImU32 additionalLine = ImGui::ColorConvertFloat4ToU32(additionalColor);
                auto featureHeading = [](const char* label, ImU32 color) {
                    const ImVec2 start = ImGui::GetCursorScreenPos();
                    ImGui::Indent(7.0f);
                    const bool open = ImGui::CollapsingHeader(label);
                    ImGui::Unindent(7.0f);
                    const ImVec2 lineStart(start.x + 1.0f, start.y + 2.0f);
                    const ImVec2 lineEnd(start.x + 1.0f, start.y + ImGui::GetFrameHeight() - 2.0f);
                    ImGui::GetWindowDrawList()->AddLine(lineStart, lineEnd, color, 2.0f);
                    return open;
                };
                ImGui::TextColored(mandatoryColor, "Red = Mandatory");
                ImGui::SameLine();
                ImGui::TextUnformatted("|");
                ImGui::SameLine();
                ImGui::TextColored(additionalColor, "Blue = Additional");
                ImGui::Separator();
                ImGui::TextUnformatted("Flight / Reset");
                const char* dragonModels[] = { "Original Dragon", "Segmented Dragon" };
                ImGui::SetNextItemWidth(170.0f);
                ImGui::Combo("Dragon model", &m_dragonModel, dragonModels, 2);
                resetDragon = ImGui::Button("Reset Dragon Position");
                ImGui::TextWrapped("W/S: Forward/Back | A/D: Turn | Q/E: Down/Up | 1: Fire");
                if (m_cameraMode == CameraMode::FreeCamera && m_flightMode == FlightMode::Manual)
                    ImGui::TextWrapped("Free Camera moves the camera; manual dragon flight is paused.");
                if (m_flightMode == FlightMode::Autopilot)
                    ImGui::TextWrapped("Autopilot active. Change Flight Mode in Additional 1.");
                ImGui::Separator();

                // Keep flight/reset controls visible while the feature list scrolls.
                ImGui::BeginChild("Presentation Features", ImVec2(0, 0), false);
                ImGui::PushItemWidth(145.0f);
                ImGui::TextColored(mandatoryColor, "Mandatory Requirements");
                if (featureHeading("1. Multiple Viewpoints", mandatoryLine)) {
                    const char* cameraModes[] = { "Follow Dragon", "Free Camera", "Top-Down" };
                    int selectedCamera = static_cast<int>(m_cameraMode);
                    if (ImGui::Combo("Camera mode", &selectedCamera, cameraModes, 3)) {
                        m_cameraMode = static_cast<CameraMode>(selectedCamera);
                        m_cameraDragging = false;
                    }
                    ImGui::TextWrapped("F1: Follow | F2: Free | F3: Top-Down");
                    if (m_cameraMode == CameraMode::FollowDragon)
                        ImGui::TextWrapped("Hold LMB to orbit; mouse wheel to zoom.");
                    else if (m_cameraMode == CameraMode::FreeCamera)
                        ImGui::TextWrapped("WASD: Move camera | Q/E: Down/Up | Hold LMB: Look");
                    else
                        ImGui::TextWrapped("WASD/QE control the dragon.");
                    if (ImGui::Button("Reset Camera View"))
                        resetCameraView();
                }
                if (featureHeading("2. Advanced Shading", mandatoryLine)) {
                    ImGui::Checkbox("Enable Lighting", &m_enableLighting);
                    const char* shadingModels[] = { "Blinn-Phong", "PBR" };
                    ImGui::Combo("Shading Model", &m_shadingModel, shadingModels, 2);
                    if (m_shadingModel == 0) {
                        ImGui::SliderFloat("Ambient Strength", &m_ambientStrength, 0.0f, 1.0f);
                        ImGui::SliderFloat("Diffuse Strength", &m_diffuseStrength, 0.0f, 1.0f);
                        ImGui::SliderFloat("Specular Strength", &m_specularStrength, 0.0f, 1.0f);
                        ImGui::SliderFloat("Shininess", &m_lightingShininess, 1.0f, 128.0f);
                    } else {
                        ImGui::TextWrapped("PBR: nonmetallic red body / metallic gold accents.");
                        ImGui::SliderFloat("Body Roughness", &m_jadeRoughness, 0.08f, 1.0f);
                        ImGui::SliderFloat("Gold Roughness", &m_goldRoughness, 0.08f, 1.0f);
                        if (m_usePbrPropertyMaps)
                            ImGui::TextWrapped("Body roughness is supplied by Material Property Maps while enabled.");
                    }
                }
                if (featureHeading("3. Material Textures", mandatoryLine)) {
                    ImGui::Checkbox("Dragon Scale Texture", &m_useDragonScaleTexture);
                    ImGui::Checkbox("Material Property Maps", &m_usePbrPropertyMaps);
                    ImGui::Checkbox("Use material if no texture", &m_useMaterial);
                }
                if (featureHeading("4. Normal Mapping", mandatoryLine)) {
                    ImGui::Checkbox("Dragon Normal Mapping", &m_useDragonNormalMapping);
                    ImGui::SliderFloat("Normal Strength", &m_dragonNormalStrength, 0.0f, 2.0f);
                }
                if (featureHeading("5. Environment Mapping", mandatoryLine)) {
                    ImGui::Checkbox("Environment Reflections", &m_environmentReflections);
                    ImGui::SliderFloat("Reflection Strength", &m_reflectionStrength, 0.0f, 1.0f);
                }
                if (featureHeading("6. B\xC3\xA9zier Curves", mandatoryLine)) {
                    ImGui::Checkbox("Show B\xC3\xA9zier Path", &m_showBezierPath);
                    ImGui::TextWrapped("Three connected cubic curves. Flight Mode is in Additional 1.");
                }
                if (featureHeading("7. Hierarchical Transformations", mandatoryLine)) {
                    if (ImGui::Checkbox("Animate Dragon Body", &m_animateDragonBody) && m_animateDragonBody)
                        m_bodyAnimationBlend = 0.0f;
                    ImGui::Checkbox("Responsive Flight Animation", &m_responsiveFlightAnimation);
                    ImGui::TextWrapped("Disable body animation to adjust joints manually.");
                    ImGui::BeginDisabled(m_animateDragonBody);
                    ImGui::SliderAngle("Body Joint 1 Angle", &m_manualBodyJointAngles[0], -30.0f, 30.0f);
                    ImGui::SliderAngle("Body Joint 4 Angle", &m_manualBodyJointAngles[3], -30.0f, 30.0f);
                    ImGui::EndDisabled();
                }

                ImGui::Spacing();
                ImGui::TextColored(additionalColor, "Additional Features");
                if (featureHeading("1. Constant-Speed B\xC3\xA9zier Autopilot", additionalLine)) {
                    const char* flightModes[] = { "Manual", "Autopilot" };
                    int selectedFlightMode = static_cast<int>(m_flightMode);
                    flightModeChanged = ImGui::Combo("Flight Mode", &selectedFlightMode, flightModes, 2);
                    if (flightModeChanged) {
                        m_flightMode = static_cast<FlightMode>(selectedFlightMode);
                        if (m_flightMode == FlightMode::Autopilot) {
                            m_autopilotElapsedSeconds = 0.0;
                            m_autopilotProgress = 0.0f;
                            m_dragonPosition = m_flightPath.front().p0;
                        }
                    }
                    ImGui::TextWrapped("Arc-length lookup; 15 seconds per loop. Entering Autopilot restarts the path.");
                    if (m_flightMode == FlightMode::Autopilot) {
                        const int segment = static_cast<int>(m_autopilotProgress);
                        ImGui::Text("Segment %d/3 | t: %.3f", segment + 1,
                            m_autopilotProgress - static_cast<float>(segment));
                        ImGui::TextWrapped("Autopilot ignores WASD/Q/E; bounds apply to Manual.");
                    }
                }
                if (featureHeading("2. Multiple Light Shadows", additionalLine)) {
                    ImGui::Checkbox("Shadow Mapping", &m_shadowMapping);
                    ImGui::Checkbox("PCF Filtering", &m_shadowPcf);
                    ImGui::Checkbox("Fire Light", &m_fireLight);
                    ImGui::Checkbox("Fire Light Shadows", &m_fireLightShadows);
                    ImGui::SliderFloat("Fire Beam Intensity", &m_fireLightIntensity, 0.0f, 30.0f);
                    ImGui::SliderFloat("Fire Beam Range", &m_fireLightRange, 1.0f, 8.0f);
                }
                if (featureHeading("3. GPU Particle Effects", additionalLine)) {
                    ImGui::Checkbox("Fire Breathing", &m_fireBreathing);
                    ImGui::Checkbox("Tail Ember Trail", &m_tailEmberTrail);
                    ImGui::TextWrapped("1: Toggle fire. GPU ping-pong particle updates and billboard rendering.");
                }
                if (featureHeading("4. Procedural Terrain", additionalLine)) {
                    ImGui::Checkbox("Show Ground", &m_showGround);
                    ImGui::TextWrapped("Procedural volcano heightfield. Show Ground also controls props, lava and water visibility.");
                }
                if (featureHeading("5. Procedural Environment Props", additionalLine)) {
                    ImGui::TextWrapped("Procedurally generated rocks and grass, batched by material. Visibility follows Show Ground in section 4.");
                }
                if (featureHeading("6. Bloom Post-Processing", additionalLine)) {
                    ImGui::Checkbox("Bloom", &m_bloomEnabled);
                    ImGui::BeginDisabled(!m_bloomEnabled);
                    ImGui::SliderFloat("Bloom Strength", &m_bloomStrength, 0.0f, 1.0f);
                    ImGui::EndDisabled();
                }
                if (featureHeading("7. Procedural Water", additionalLine)) {
                    ImGui::Checkbox("Show Water", &m_showWater);
                    ImGui::SliderFloat("Water Wave Strength", &m_waterWaveStrength, 0.0f, 0.04f, "%.3f");
                    ImGui::SliderFloat("Water Wave Speed", &m_waterWaveSpeed, 0.0f, 2.0f, "%.2f");
                }
                if (featureHeading("8. Animated Textures", additionalLine)) {
                    ImGui::Checkbox("Show Lava Lake", &m_showLavaLake);
                    ImGui::Checkbox("Mountain Lava Streams", &m_showLavaStreams);
                    ImGui::TextWrapped("Lava: 24 texture frames switched at 12 FPS.");
                }
                if (featureHeading("9. Day/Night Lighting", additionalLine)) {
                    ImGui::BeginDisabled(m_automaticDayNight);
                    ImGui::SliderFloat("Time of Day", &m_timeOfDay, 0.0f, 24.0f, "%.2f h");
                    ImGui::EndDisabled();
                    ImGui::Checkbox("Automatic Day/Night Cycle", &m_automaticDayNight);
                }
                if (featureHeading("10. Minimap", additionalLine)) {
                    ImGui::Checkbox("Show Minimap", &m_showMinimap);
                }
                if (featureHeading("11. Collision Detection", additionalLine)) {
                    constraintsChanged = ImGui::Checkbox("Constrain Flight to Terrain", &m_constrainFlightToTerrain);
                    ImGui::Checkbox("Mountain Collision Detection", &m_mountainCollision);
                    ImGui::TextWrapped("Manual flight: terrain collision and world-position limits.");
                }
                if (featureHeading("12. GPU Linear Blend Skinning", additionalLine)) {
                    ImGui::TextWrapped("Eight-bone body; two weighted bone transforms per vertex. Animation controls are in Mandatory 7.");
                }
                if (featureHeading("13. GPU Blend Shapes", additionalLine)) {
                    ImGui::TextWrapped("The jaw and lower teeth blend between closed/open vertex targets. Use 1 or Fire Breathing in section 3.");
                    ImGui::Text("Mouth Open: %.2f", m_mouthOpen);
                }
                ImGui::PopItemWidth();
                ImGui::EndChild();
            }
            ImGui::End();

            const bool manualMoved = updateDragonSimulation(!flightModeChanged && !resetDragon);

            if (m_flightMode == FlightMode::Manual && m_constrainFlightToTerrain && (manualMoved || constraintsChanged))
                m_dragonPosition = glm::clamp(m_dragonPosition, s_minFlightPosition, s_maxFlightPosition);
            if (resetDragon) {
                m_autopilotElapsedSeconds = 0.0;
                m_autopilotProgress = 0.0f;
                if (m_flightMode == FlightMode::Autopilot) {
                    m_dragonPosition = m_flightPath.front().p0;
                } else {
                    m_dragonPosition = glm::vec3(0.0f);
                }
                m_dragonYaw = 0.0f;
                m_bodyAnimationTime = 0.0;
                m_bodyAnimationBlend = 1.0f;
                m_steeringResponse.fill(glm::vec2(0));
                m_flightBend = {};
            }
            if (resetDragon) {
                const float frameTime = m_deltaTime;
                m_deltaTime = 0;
                updateDragonBodyAnimation();
                updateDragonSkeleton();
                m_deltaTime = frameTime;
            }

            m_modelMatrix = glm::translate(glm::mat4(1.0f), m_dragonPosition);
            m_modelMatrix = glm::rotate(m_modelMatrix, m_dragonYaw, glm::vec3(0, 1, 0));

            updateDragonMouth();
            if (std::isfinite(m_deltaTime) && m_deltaTime > 0.0f) {
                const double loopDuration = static_cast<double>(s_lavaFrameCount) / s_lavaFramesPerSecond;
                m_lavaAnimationSeconds = std::fmod(m_lavaAnimationSeconds + m_deltaTime, loopDuration);
                const double waterLoopDuration = 20.0 * glm::pi<double>();
                m_waterTime = std::fmod(m_waterTime + m_deltaTime * m_waterWaveSpeed, waterLoopDuration);
            }
            updateDayNightLighting();
            updateDragonParticles();
            updateFireLight();
            updateCamera();
            drawMinimap();
            if (m_shadowMapping && m_shadowMapReady && m_enableLighting && m_sunAboveHorizon)
                renderShadowMap();
            if (m_fireActive && m_fireLightShadows && m_fireShadowReady && m_enableLighting)
                renderShadowMap(true);

            const glm::vec3 cameraPosition = glm::vec3(glm::inverse(m_viewMatrix)[3]);
            m_defaultShader.bind();
            glUniform3fv(m_defaultShader.getUniformLocation("cameraPosition"), 1, glm::value_ptr(cameraPosition));

            glUniform3fv(m_defaultShader.getUniformLocation("lightDirection"), 1, glm::value_ptr(m_sunDirection));
            glUniform3fv(m_defaultShader.getUniformLocation("sunColor"), 1, glm::value_ptr(m_sunColor));
            glUniform1f(m_defaultShader.getUniformLocation("sunIntensity"), m_sunIntensity);
            glUniform1f(m_defaultShader.getUniformLocation("dayAmbient"), m_dayAmbient);
            setTimeOfDayEnvironment(m_defaultShader);
            glUniform1i(m_defaultShader.getUniformLocation("enableLighting"), m_enableLighting);
            glUniform1i(m_defaultShader.getUniformLocation("shadingModel"), m_shadingModel);
            glUniform1f(m_defaultShader.getUniformLocation("ambientStrength"), m_ambientStrength);
            glUniform1f(m_defaultShader.getUniformLocation("diffuseStrength"), m_diffuseStrength);
            glUniform1f(m_defaultShader.getUniformLocation("specularStrength"), m_specularStrength);
            glUniform1f(m_defaultShader.getUniformLocation("lightingShininess"), m_lightingShininess);

            glActiveTexture(GL_TEXTURE3);
            const bool shadows = m_shadowMapping && m_shadowMapReady && m_enableLighting && m_sunAboveHorizon;
            GLuint shadowDepthTextureBinding = 0;
            if (shadows) {
                shadowDepthTextureBinding = m_shadowDepthTexture;
            }
            glBindTexture(GL_TEXTURE_2D, shadowDepthTextureBinding);
            glUniform1i(m_defaultShader.getUniformLocation("shadowMap"), 3);
            glUniform1i(m_defaultShader.getUniformLocation("enableShadows"), shadows);
            glUniform1i(m_defaultShader.getUniformLocation("enableShadowPcf"), m_shadowPcf);
            glUniformMatrix4fv(m_defaultShader.getUniformLocation("lightViewProjection"), 1, GL_FALSE,
                glm::value_ptr(m_lightViewProjection));
            glActiveTexture(GL_TEXTURE0);

            bindFireLight();
            beginBloomScene();

            glClearColor(0.77f, 0.85f, 0.91f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderSky();

            glEnable(GL_DEPTH_TEST);

            const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * m_modelMatrix;

            const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));

            if (m_showGround) {
                const glm::mat4 groundModelMatrix(1.0f);
                const glm::mat4 groundMvpMatrix = m_projectionMatrix * m_viewMatrix * groundModelMatrix;
                const glm::mat3 groundNormalMatrix(1.0f);
                m_defaultShader.bind();
                setDragonSkinning(m_defaultShader, false);
                glUniformMatrix4fv(
                    m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(groundMvpMatrix));
                glUniformMatrix4fv(
                    m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(groundModelMatrix));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE,
                    glm::value_ptr(groundNormalMatrix));
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_groundSurfaceTexture);
                glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), m_groundMesh.hasTextureCoords());
                glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
                setPbrMaterial(0.0f, 0.8f);
                setPbrPropertyMaps(false);
                setEnvironmentReflections(false);
                setDragonNormalMapping(false);
                m_groundMesh.draw(m_defaultShader);
            }

            drawLandscape();
            drawLavaLake();

            if (m_dragonModel == 1) {
                drawSegmentedDragon();
            } else {
                for (GPUMesh& mesh : m_meshes) {
                    m_defaultShader.bind();
                    setDragonSkinning(m_defaultShader, false);
                    glUniformMatrix4fv(
                        m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                    glUniformMatrix4fv(
                        m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
                    glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE,
                        glm::value_ptr(normalModelMatrix));
                    if (mesh.hasTextureCoords()) {
                        m_texture.bind(GL_TEXTURE0);
                        glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_TRUE);
                        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
                    } else {
                        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
                    }
                    setPbrPropertyMaps(false);
                    setPbrMaterial(0.0f, 0.4f);
                    setEnvironmentReflections(false);
                    setDragonNormalMapping(false);
                    mesh.draw(m_defaultShader);
                }
            }

            if (m_showBezierPath) {
                const glm::mat4 pathMvpMatrix = m_projectionMatrix * m_viewMatrix;
                const std::array<glm::vec3, 3> colors { { { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f },
                    { 1.0f, 1.0f, 0.0f } } };
                m_bezierShader.bind();
                setDragonSkinning(m_bezierShader, false);
                glUniformMatrix4fv(
                    m_bezierShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(pathMvpMatrix));
                glBindVertexArray(m_bezierVao);
                for (GLint segment = 0; segment < 3; ++segment) {
                    glUniform3fv(m_bezierShader.getUniformLocation("pathColor"), 1, glm::value_ptr(colors[segment]));
                    glDrawArrays(GL_LINE_STRIP, segment * s_bezierSamplesPerSegment, s_bezierSamplesPerSegment);
                }
                glBindVertexArray(0);
            }

            drawWaterPond();
            renderDragonParticles();
            finishBloom();

            m_window.swapBuffers();
        }
    }

    void onKeyPressed(int key, int mods)
    {
        std::cout << "Key pressed: " << key << std::endl;
    }

    void onKeyReleased(int key, int mods)
    {
        std::cout << "Key released: " << key << std::endl;
    }

    void onMouseMove(const glm::dvec2& cursorPos)
    {
        std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }

    void onMouseClicked(int button, int mods)
    {
        std::cout << "Pressed mouse button: " << button << std::endl;
    }

    void onMouseReleased(int button, int mods)
    {
        std::cout << "Released mouse button: " << button << std::endl;
    }

private:
    struct FlightBend {
        std::array<float, 8> pitch {};
        float bank = 0, bob = 0, headFollow = 1;
        bool operator==(const FlightBend&) const = default;
    };

    glm::mat4 headAnimationSkin() const
    {
        if (m_flightBend.headFollow == 1.f)
            return m_skinMatrices[0];
        if (m_flightBend.headFollow == 0.f)
            return glm::mat4(1);
        return (1.f - m_flightBend.headFollow) * glm::mat4(1) + m_flightBend.headFollow * m_skinMatrices[0];
    }

    glm::mat4 dragonHeadWorld() const
    {
        return glm::translate(m_modelMatrix * headAnimationSkin(), glm::vec3(0, 0, -0.4f));
    }

    glm::mat4 dragonSnoutWorld() const
    {
        return glm::translate(dragonHeadWorld(), glm::vec3(0, -0.006f, -0.215f));
    }

    glm::vec3 dragonNostrilLocal(float side) const
    {
        return glm::vec3(side * 0.042f, 0.072f, -0.302f);
    }

    void drawMinimap()
    {
        if (!m_showMinimap)
            return;

        const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        const float mapSize = std::min(210.0f, std::min(screenSize.x * 0.30f, screenSize.y * 0.30f));
        const float windowHeight = mapSize + 64.0f;
        ImGui::SetNextWindowPos(ImVec2(14.0f, screenSize.y - windowHeight - 14.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(mapSize + 16.0f, windowHeight), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.94f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::Begin("Minimap", nullptr, flags);
        ImGui::TextUnformatted("MINIMAP  |  N: -Z");
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 mapEnd(origin.x + mapSize, origin.y + mapSize);
        auto mapPoint = [origin, mapSize](float x, float z) {
            return ImVec2(origin.x + (x + 10.0f) * mapSize / 20.0f,
                origin.y + (z + 10.0f) * mapSize / 20.0f);
        };
        draw->AddRectFilled(origin, mapEnd, IM_COL32(125, 110, 80, 255));
        draw->PushClipRect(origin, mapEnd, true);
        for (int coordinate = -8; coordinate <= 8; coordinate += 4) {
            const float gridCoordinate = static_cast<float>(coordinate);
            draw->AddLine(mapPoint(gridCoordinate, -10), mapPoint(gridCoordinate, 10), IM_COL32(160, 144, 109, 120));
            draw->AddLine(mapPoint(-10, gridCoordinate), mapPoint(10, gridCoordinate), IM_COL32(160, 144, 109, 120));
        }

        if (m_showGround && m_showWater) {
            for (int point = 0; point < 64; ++point) {
                const float angle = glm::two_pi<float>() * static_cast<float>(point) / 64.0f;
                const float shoreline = 1.0f + 0.05f * std::sin(3 * angle) + 0.03f * std::cos(5 * angle);
                const glm::vec2 direction(std::cos(angle), std::sin(angle));
                const glm::vec2 world = waterPondCenter + direction * waterPondRadii * shoreline;
                draw->PathLineTo(mapPoint(world.x, world.y));
            }
            draw->PathFillConvex(IM_COL32(49, 154, 172, 255));
        }

        // Sample static terrain once; the minimap adds no scene render or heightfield search per frame.
        constexpr int terrainCells = 32;
        static const auto terrainColors = [] {
            std::array<ImU32, terrainCells * terrainCells> colors {};
            for (int z = 0; z < terrainCells; ++z) {
                for (int x = 0; x < terrainCells; ++x) {
                    const glm::vec2 cellCenter((x + 0.5f) / terrainCells, (z + 0.5f) / terrainCells);
                    const glm::vec2 world = mountainCenter + (cellCenter * 2.0f - 1.0f) * mountainHalfExtent;
                    const float height = terrainHeightAt(world.x, world.y);
                    if (height <= -0.98f)
                        continue;
                    const float elevation = glm::clamp((height + 1.0f) / 2.6f, 0.0f, 1.0f);
                    const int shade = static_cast<int>(80.0f + elevation * 130.0f);
                    colors[z * terrainCells + x] = IM_COL32(shade, shade, shade - 12, 255);
                }
            }
            return colors;
        }();
        const float cellSize = 2.0f * mountainHalfExtent / terrainCells;
        const glm::vec2 mountainMinimum = mountainCenter - glm::vec2(mountainHalfExtent);
        for (int z = 0; z < terrainCells; ++z) {
            for (int x = 0; x < terrainCells; ++x) {
                const ImU32 color = terrainColors[z * terrainCells + x];
                if (color == 0)
                    continue;
                const float worldX = mountainMinimum.x + x * cellSize;
                const float worldZ = mountainMinimum.y + z * cellSize;
                draw->AddRectFilled(mapPoint(worldX, worldZ), mapPoint(worldX + cellSize, worldZ + cellSize), color);
            }
        }
        if (m_showBezierPath) {
            for (const auto& segment : m_flightPath) {
                glm::vec3 previous = segment.p0;
                for (int sample = 1; sample <= 32; ++sample) {
                    const glm::vec3 current = evaluateCubicBezier(segment, sample / 32.0f);
                    draw->AddLine(mapPoint(previous.x, previous.z), mapPoint(current.x, current.z),
                        IM_COL32(102, 211, 241, 230), 1.5f);
                    previous = current;
                }
            }
        }
        const float markerLimit = 10.0f - 10.0f * 20.0f / mapSize;
        const glm::vec2 markerPosition = glm::clamp(glm::vec2(m_dragonPosition.x, m_dragonPosition.z),
            glm::vec2(-markerLimit), glm::vec2(markerLimit));
        const ImVec2 center = mapPoint(markerPosition.x, markerPosition.y);
        const glm::vec2 forward(-std::sin(m_dragonYaw), -std::cos(m_dragonYaw));
        const glm::vec2 right(-forward.y, forward.x);
        const ImVec2 tip(center.x + forward.x * 9, center.y + forward.y * 9);
        const glm::vec2 rear = -forward * 5.0f;
        const ImVec2 leftCorner(center.x + rear.x - right.x * 5, center.y + rear.y - right.y * 5);
        const ImVec2 rightCorner(center.x + rear.x + right.x * 5, center.y + rear.y + right.y * 5);
        draw->AddCircleFilled(center, 10.0f, IM_COL32(20, 24, 29, 210));
        draw->AddTriangleFilled(tip, leftCorner, rightCorner, IM_COL32(255, 202, 70, 255));
        draw->PopClipRect();
        draw->AddRect(origin, mapEnd, IM_COL32(205, 187, 145, 255));
        ImGui::Dummy(ImVec2(mapSize, mapSize));
        if (std::abs(m_dragonPosition.x) > 10 || std::abs(m_dragonPosition.z) > 10) {
            ImGui::TextUnformatted("Dragon outside map");
        } else {
            ImGui::Text("Altitude: %.1f", m_dragonPosition.y);
        }
        ImGui::End();
    }

    struct BreathEmitter {
        std::array<glm::vec3, 3> positions;
        glm::vec3 forward;
    };

    BreathEmitter dragonBreathEmitter() const
    {
        const glm::mat4 snout = dragonSnoutWorld();
        const glm::vec3 closedMouthPoint(0, -0.025f, -0.310f);
        const glm::vec3 lowerLipOffset = dragonJawMorphOffset(closedMouthPoint);
        const glm::vec3 mouthCenter = closedMouthPoint + 0.5f * m_mouthOpen * lowerLipOffset;
        const glm::vec3 mouthPosition(snout * glm::vec4(mouthCenter, 1));
        const glm::vec3 leftNostril = dragonNostrilLocal(-1) + glm::vec3(0, 0, -0.015f);
        const glm::vec3 leftPosition(snout * glm::vec4(leftNostril, 1));
        const glm::vec3 rightNostril = dragonNostrilLocal(1) + glm::vec3(0, 0, -0.015f);
        const glm::vec3 rightPosition(snout * glm::vec4(rightNostril, 1));
        const float downwardAim = -0.12f - 0.10f * m_mouthOpen;
        const glm::vec3 forwardVector(snout * glm::vec4(0, downwardAim, -1, 0));
        const glm::vec3 forwardDirection = glm::normalize(forwardVector);
        return { { mouthPosition, leftPosition, rightPosition }, forwardDirection };
    }

    float jawTerrainClearance(float mouthOpen) const
    {
        const glm::mat4 snoutWorld = dragonSnoutWorld();
        float clearance = std::numeric_limits<float>::infinity();
        for (const glm::vec3& point : m_collisionGeometry[3]) {
            const glm::vec3 jawPoint = point + mouthOpen * dragonJawMorphOffset(point);
            const glm::vec3 worldPoint(snoutWorld * glm::vec4(jawPoint, 1));
            const float terrainHeight = terrainHeightAt(worldPoint.x, worldPoint.z);
            clearance = glm::min(clearance, worldPoint.y - terrainHeight);
        }
        return clearance;
    }

    void updateDragonMouth()
    {
        float target = 0.0f;
        if (m_fireBreathing && m_dragonModel == 1) {
            target = 1.0f;
        }
        const float response = 1.0f - std::exp(-8.0f * m_deltaTime);
        float proposedOpen = glm::mix(m_mouthOpen, target, response);
        if (std::abs(proposedOpen - target) < 0.001f) {
            proposedOpen = target;
        }
        if (proposedOpen == m_mouthOpen) {
            return;
        }
        // Opening must not push the animated jaw through the existing heightfield.
        if (proposedOpen > m_mouthOpen && m_mountainCollision && m_flightMode == FlightMode::Manual) {
            const float currentClearance = jawTerrainClearance(m_mouthOpen);
            const float minimumClearance = glm::min(currentClearance, s_collisionMargin);
            if (jawTerrainClearance(proposedOpen) < minimumClearance) {
                float safeOpen = m_mouthOpen;
                float blockedOpen = proposedOpen;
                for (int iteration = 0; iteration < 6; ++iteration) {
                    const float candidateOpen = (safeOpen + blockedOpen) * 0.5f;
                    if (jawTerrainClearance(candidateOpen) >= minimumClearance) {
                        safeOpen = candidateOpen;
                    } else {
                        blockedOpen = candidateOpen;
                    }
                }
                proposedOpen = safeOpen;
            }
        }
        m_mouthOpen = proposedOpen;
        // Exact inverse-transpose of the blended target's linear part, not a lerped normal.
        const glm::mat3 closedLinear(1);
        const glm::mat3 openLinear(dragonJawOpenTransform());
        const glm::mat3 blendedLinear = closedLinear + m_mouthOpen * (openLinear - closedLinear);
        m_mouthNormalMatrix = glm::inverseTranspose(blendedLinear);
        m_collisionPoseDirty = true;
        m_clearanceCached = false;
    }

    void initializeDragonParticles()
    {
        ParticlePassState previous;
        glGenVertexArrays(1, &m_particleVao);
        glGenFramebuffers(2, m_particleFramebuffers.data());
        glGenTextures(2, m_particlePositions.data());
        glGenTextures(2, m_particleVelocities.data());
        const std::vector<glm::vec4> positions(s_particleGridSize * s_particleGridSize, glm::vec4(0, 0, 0, -1));
        const std::vector<glm::vec4> velocities(s_particleGridSize * s_particleGridSize, glm::vec4(0));
        const GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        m_particlesReady = true;
        glActiveTexture(GL_TEXTURE6);
        for (std::size_t state = 0; state < 2; ++state) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_particleFramebuffers[state]);
            for (int field = 0; field < 2; ++field) {
                GLuint texture;
                const glm::vec4* initialState;
                if (field == 0) {
                    texture = m_particlePositions[state];
                    initialState = positions.data();
                } else {
                    texture = m_particleVelocities[state];
                    initialState = velocities.data();
                }
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, s_particleGridSize, s_particleGridSize, 0, GL_RGBA, GL_FLOAT,
                    initialState);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[field], GL_TEXTURE_2D, texture, 0);
            }
            glDrawBuffers(2, attachments);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                m_particlesReady = false;
                std::cerr << "Dragon particle framebuffer is incomplete\n";
            }
        }
    }

    void bindParticleState(const Shader& shader)
    {
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, m_particlePositions[m_particleReadState]);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, m_particleVelocities[m_particleReadState]);
        glUniform1i(shader.getUniformLocation("positionAge"), 6);
        glUniform1i(shader.getUniformLocation("velocityLifetime"), 7);
    }

    bool dragonBreathActive() const
    {
        return m_particlesReady && m_fireBreathing && m_mouthOpen >= 0.65f && m_dragonModel == 1;
    }

    void updateDragonParticles()
    {
        if (!m_particlesReady || m_dragonModel != 1 || (!dragonBreathActive() && !m_tailEmberTrail)) {
            m_particlesNeedReset = true;
            return;
        }
        ParticlePassState previous;
        const BreathEmitter emitter = dragonBreathEmitter();
        const bool reset = m_particlesNeedReset || !std::isfinite(m_deltaTime) || m_deltaTime > 0.25f;
        float timestep = 0.0f;
        if (std::isfinite(m_deltaTime)) {
            timestep = glm::clamp(m_deltaTime, 0.0f, 0.05f);
        }
        const int writeState = 1 - m_particleReadState;
        glBindFramebuffer(GL_FRAMEBUFFER, m_particleFramebuffers[writeState]);
        glViewport(0, 0, s_particleGridSize, s_particleGridSize);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        m_particleUpdateShader.bind();
        bindParticleState(m_particleUpdateShader);
        glUniform1f(m_particleUpdateShader.getUniformLocation("timestep"), timestep);
        glUniform1ui(m_particleUpdateShader.getUniformLocation("simulationStep"), m_particleStep++);
        glUniform1i(m_particleUpdateShader.getUniformLocation("resetParticles"), reset);
        glUniform1i(m_particleUpdateShader.getUniformLocation("breathActive"), dragonBreathActive());
        glUniform1i(m_particleUpdateShader.getUniformLocation("tailActive"), m_tailEmberTrail);
        // Scatter embers along the animated back from behind the neck to the tail tip.
        std::array<glm::vec3, s_tailEmberCount> tailPositions;
        std::array<glm::vec3, s_tailEmberCount> tailDirections;
        for (std::size_t ember = 0; ember < tailPositions.size(); ++ember) {
            const float tailFraction = static_cast<float>(ember) / (tailPositions.size() - 1);
            const float tailZ = glm::mix(dragonNeckZ + 0.10f, dragonTailTipZ - 0.025f, tailFraction);
            const glm::mat4 tailWorld = m_modelMatrix * dragonSkinAt(tailZ);
            const float tailTop = dragonBodyRadius(tailZ) + 0.035f;
            const float lateralOffset = 0.025f * std::sin(static_cast<float>(ember) * 2.4f);
            tailPositions[ember] = glm::vec3(tailWorld * glm::vec4(lateralOffset, tailTop, tailZ, 1));
            const glm::vec3 backwardVector(tailWorld * glm::vec4(0, 0, 1, 0));
            tailDirections[ember] = glm::normalize(backwardVector);
        }
        glUniform3fv(m_particleUpdateShader.getUniformLocation("tailPositions[0]"),
            static_cast<GLsizei>(tailPositions.size()), glm::value_ptr(tailPositions[0]));
        glUniform3fv(m_particleUpdateShader.getUniformLocation("tailDirections[0]"),
            static_cast<GLsizei>(tailDirections.size()), glm::value_ptr(tailDirections[0]));
        glUniform3fv(
            m_particleUpdateShader.getUniformLocation("emitterPositions[0]"), 3, glm::value_ptr(emitter.positions[0]));
        glUniform3fv(m_particleUpdateShader.getUniformLocation("emitterForward"), 1, glm::value_ptr(emitter.forward));
        glBindVertexArray(m_particleVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        m_particleReadState = writeState;
        m_particlesNeedReset = false;
    }

    void renderDragonParticles()
    {
        if (!m_particlesReady || m_dragonModel != 1 || (!dragonBreathActive() && !m_tailEmberTrail))
            return;
        ParticlePassState previous;
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
        glDisable(GL_CULL_FACE);
        m_particleRenderShader.bind();
        bindParticleState(m_particleRenderShader);
        glUniform1i(m_particleRenderShader.getUniformLocation("breathActive"), dragonBreathActive());
        glUniform1i(m_particleRenderShader.getUniformLocation("tailActive"), m_tailEmberTrail);
        const glm::mat4 viewProjection = m_projectionMatrix * m_viewMatrix;
        const glm::mat4 cameraWorld = glm::inverse(m_viewMatrix);
        glUniformMatrix4fv(
            m_particleRenderShader.getUniformLocation("viewProjection"), 1, GL_FALSE, glm::value_ptr(viewProjection));
        glUniform3fv(m_particleRenderShader.getUniformLocation("cameraRight"), 1, glm::value_ptr(cameraWorld[0]));
        glUniform3fv(m_particleRenderShader.getUniformLocation("cameraUp"), 1, glm::value_ptr(cameraWorld[1]));
        glBindVertexArray(m_particleVao);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, s_particleCount);
    }

    void cacheCollisionGeometry(const Mesh& mesh, int index, unsigned int columns)
    {
        auto& points = m_collisionGeometry[index];

        unsigned int rowStep;
        unsigned int sideStep;
        if (index == 0 || index == 7) {
            rowStep = 1;
            sideStep = 1;
        } else {
            if (index >= 5 && index <= 10) {
                rowStep = 4;
            } else {
                rowStep = 2;
            }
            if (columns == 33 || columns == 17) {
                sideStep = 4;
            } else {
                sideStep = 3;
            }
        }
        for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
            if (index == 0 || index == 7) {
                if (!(mesh.vertices[i].position.y <= .000001f)) {
                    continue;
                }
            }
            if (columns != 0) {
                const std::size_t row = i / columns;
                const std::size_t side = i % columns;
                if (row % rowStep != 0 || side % sideStep != 0) {
                    continue;
                }
            }
            points.push_back(mesh.vertices[i].position);
        }

        for (int axis = 0; axis < 3; ++axis)
            for (int sign : { -1, 1 }) {
                const auto found = std::max_element(mesh.vertices.begin(), mesh.vertices.end(),
                    [=](const auto& a, const auto& b) { return sign * a.position[axis] < sign * b.position[axis]; });
                if (found != mesh.vertices.end())
                    points.push_back(found->position);
            }
    }

    GPUMesh makeCollisionMesh(const Mesh& mesh, int index, unsigned int columns)
    {
        cacheCollisionGeometry(mesh, index, columns);
        return GPUMesh(mesh);
    }

    GPUMesh makeCollisionMouthMesh(const Mesh& mesh, int collisionIndex, unsigned int columns)
    {
        cacheCollisionGeometry(mesh, collisionIndex, columns);
        std::vector<glm::vec3> mouthOffsets;
        mouthOffsets.reserve(mesh.vertices.size());
        for (const Vertex& vertex : mesh.vertices) {
            mouthOffsets.push_back(dragonJawMorphOffset(vertex.position));
        }
        return GPUMesh(mesh, {}, mouthOffsets);
    }

    GPUMesh makeCollisionBody()
    {
        const Mesh mesh = createDragonBodyMesh();
        cacheCollisionGeometry(mesh, 0, 33);
        std::vector<SkinInfluence> skin;
        skin.reserve(mesh.vertices.size());
        for (const auto& vertex : mesh.vertices)
            skin.push_back(dragonSkinInfluence(vertex.position.z));
        return GPUMesh(mesh, skin);
    }

    void initializeCollisionSamples()
    {

        m_collectCollisionSamples = true;
        drawSegmentedDragon();
        m_collectCollisionSamples = false;
        refreshCollisionPose();

        std::unordered_set<std::uint64_t> occupied;
        for (const auto& mesh : loadMesh(RESOURCE_ROOT "resources/dragon.obj"))
            for (const auto& vertex : mesh.vertices) {
                const glm::ivec3 cell(glm::floor(vertex.position / .025f));
                const auto key = (static_cast<std::uint64_t>(cell.x & 0x1fffff) << 42)
                    | (static_cast<std::uint64_t>(cell.y & 0x1fffff) << 21)
                    | static_cast<std::uint64_t>(cell.z & 0x1fffff);
                if (occupied.insert(key).second)
                    m_originalCollisionSamples.push_back(vertex.position);
            }
    }

    void refreshCollisionPose()
    {
        m_collisionPoseDirty = false;
        m_clearanceCached = false;
        m_collisionPose.clear();
        m_collisionPose.reserve(m_collisionSamples.size());
        float lastAttachmentZ = std::numeric_limits<float>::infinity();
        glm::mat4 skinMatrix(1);
        const float* skinMatrixElements = glm::value_ptr(skinMatrix);
        for (const auto& sample : m_collisionSamples) {
            glm::vec3 localPosition = sample.point;
            if (sample.mouthMorph) {
                localPosition += m_mouthOpen * sample.mouthOffset;
            }
            if (sample.skinZ != std::numeric_limits<float>::infinity()) {

                if (sample.skinZ != lastAttachmentZ) {
                    skinMatrix = collisionSkinAt(sample.skinZ);
                    lastAttachmentZ = sample.skinZ;
                }

                localPosition = { skinMatrixElements[0] * localPosition.x + skinMatrixElements[4] * localPosition.y
                        + skinMatrixElements[8] * localPosition.z + skinMatrixElements[12],
                    skinMatrixElements[1] * localPosition.x + skinMatrixElements[5] * localPosition.y
                        + skinMatrixElements[9] * localPosition.z + skinMatrixElements[13],
                    skinMatrixElements[2] * localPosition.x + skinMatrixElements[6] * localPosition.y
                        + skinMatrixElements[10] * localPosition.z + skinMatrixElements[14] };
            }
            m_collisionPose.push_back(localPosition);
        }
    }

    float dragonTerrainClearance(
        glm::vec3 position, float yaw, float stopBelow = -std::numeric_limits<float>::infinity())
    {
        if (m_clearanceCached && m_clearanceModel == m_dragonModel) {
            const bool sameTransform = m_clearancePosition == position && m_clearanceYaw == yaw;
            if (sameTransform && m_clearanceAngles == m_bodyJointAngles && m_clearanceBend == m_flightBend
                && m_clearanceMouthOpen == m_mouthOpen) {
                return m_cachedClearance;
            }
        }
        const std::vector<glm::vec3>* selectedPoints;
        if (m_dragonModel == 1) {
            selectedPoints = &m_collisionPose;
        } else {
            selectedPoints = &m_originalCollisionSamples;
        }
        const auto& collisionPoints = *selectedPoints;
        const bool rebuildCollisionPose = m_dragonModel == 1 && m_collisionPoseDirty;
        const float yawCosine = std::cos(yaw), yawSine = std::sin(yaw);
        float clearance = std::numeric_limits<float>::infinity();
        float lastAttachmentZ = std::numeric_limits<float>::infinity();
        glm::mat4 skinMatrix(1);
        const float* skinMatrixElements = glm::value_ptr(skinMatrix);
        auto testPoint = [&](std::size_t sampleIndex) {
            glm::vec3 localPosition = collisionPoints[sampleIndex];
            if (rebuildCollisionPose) {
                const auto& sample = m_collisionSamples[sampleIndex];
                localPosition = sample.point;
                if (sample.mouthMorph) {
                    localPosition += m_mouthOpen * sample.mouthOffset;
                }
                if (sample.skinZ != std::numeric_limits<float>::infinity()) {
                    if (sample.skinZ != lastAttachmentZ) {
                        skinMatrix = collisionSkinAt(sample.skinZ);
                        lastAttachmentZ = sample.skinZ;
                    }
                    localPosition = { skinMatrixElements[0] * localPosition.x + skinMatrixElements[4] * localPosition.y
                            + skinMatrixElements[8] * localPosition.z + skinMatrixElements[12],
                        skinMatrixElements[1] * localPosition.x + skinMatrixElements[5] * localPosition.y
                            + skinMatrixElements[9] * localPosition.z + skinMatrixElements[13],
                        skinMatrixElements[2] * localPosition.x + skinMatrixElements[6] * localPosition.y
                            + skinMatrixElements[10] * localPosition.z + skinMatrixElements[14] };
                }
                m_collisionPose[sampleIndex] = localPosition;
            }
            const float worldX = position.x + yawCosine * localPosition.x + yawSine * localPosition.z,
                        worldZ = position.z - yawSine * localPosition.x + yawCosine * localPosition.z;
            const float gap = position.y + localPosition.y - terrainHeightAt(worldX, worldZ);
            if (gap < clearance) {
                clearance = gap;
                m_collisionWitness[m_dragonModel] = sampleIndex;
            }
            return clearance < stopBelow;
        };

        const std::size_t firstSample = m_collisionWitness[m_dragonModel];
        if (firstSample < collisionPoints.size() && testPoint(firstSample))
            return clearance;
        for (std::size_t sampleIndex = 0; sampleIndex < collisionPoints.size(); ++sampleIndex)
            if (sampleIndex != firstSample && testPoint(sampleIndex))
                return clearance;

        if (m_dragonModel == 1)
            m_collisionPoseDirty = false;
        m_clearanceCached = true;
        m_clearanceModel = m_dragonModel;
        m_clearanceAngles = m_bodyJointAngles;
        m_clearanceBend = m_flightBend;
        m_clearanceMouthOpen = m_mouthOpen;
        m_clearancePosition = position;
        m_clearanceYaw = yaw;
        m_cachedClearance = clearance;
        return clearance;
    }

    bool sweepDragonPose(glm::vec3 targetPosition, float targetYaw, const std::array<float, 8>& targetAngles,
        const FlightBend* targetBend = nullptr)
    {
        const glm::vec3 startPosition = m_dragonPosition;
        const float startYaw = m_dragonYaw, yawDelta = std::remainder(targetYaw - startYaw, glm::two_pi<float>());
        const auto startAngles = m_bodyJointAngles;
        const FlightBend startBend = m_flightBend;
        FlightBend endBend = startBend;
        if (targetBend) {
            endBend = *targetBend;
        }
        if (targetPosition == startPosition && targetYaw == startYaw && targetAngles == startAngles
            && endBend == startBend)
            return true;
        float angleDistance = glm::max(std::abs(yawDelta), std::abs(endBend.bank - startBend.bank));
        angleDistance = glm::max(angleDistance, std::abs(endBend.bob - startBend.bob) * glm::radians(1.f) / .015f);
        angleDistance =
            glm::max(angleDistance, std::abs(endBend.headFollow - startBend.headFollow) * glm::radians(40.f));
        for (std::size_t jointIndex = 0; jointIndex < 8; ++jointIndex) {
            angleDistance = glm::max(angleDistance, std::abs(targetAngles[jointIndex] - startAngles[jointIndex]));
            angleDistance = glm::max(angleDistance, std::abs(endBend.pitch[jointIndex] - startBend.pitch[jointIndex]));
        }
        const float translationSteps = glm::length(targetPosition - startPosition) / .015f;
        const float rotationSteps = angleDistance / glm::radians(1.f);
        const float requiredSteps = glm::max(translationSteps, rotationSteps);
        const int sweepSteps = std::max(1, static_cast<int>(std::ceil(requiredSteps)));
        const float initial = dragonTerrainClearance(startPosition, startYaw);

        float requiredClearance;
        if (initial < s_collisionMargin - .00001f) {
            requiredClearance = initial + .000001f;
        } else {
            requiredClearance = s_collisionMargin;
        }
        auto applyPose = [&](float fraction) {
            if (targetPosition == startPosition) {
                m_dragonPosition = startPosition;
            } else {
                m_dragonPosition = glm::mix(startPosition, targetPosition, fraction);
            }
            m_dragonYaw = std::remainder(startYaw + yawDelta * fraction, glm::two_pi<float>());
            std::array<float, 8> interpolatedAngles;
            for (std::size_t jointIndex = 0; jointIndex < 8; ++jointIndex)
                interpolatedAngles[jointIndex] = glm::mix(startAngles[jointIndex], targetAngles[jointIndex], fraction);
            FlightBend interpolatedBend;
            interpolatedBend.bank = glm::mix(startBend.bank, endBend.bank, fraction);
            interpolatedBend.bob = glm::mix(startBend.bob, endBend.bob, fraction);
            interpolatedBend.headFollow = glm::mix(startBend.headFollow, endBend.headFollow, fraction);
            for (std::size_t jointIndex = 0; jointIndex < 8; ++jointIndex)
                interpolatedBend.pitch[jointIndex] =
                    glm::mix(startBend.pitch[jointIndex], endBend.pitch[jointIndex], fraction);
            if (interpolatedAngles != m_bodyJointAngles || interpolatedBend != m_flightBend) {
                m_bodyJointAngles = interpolatedAngles;
                m_flightBend = interpolatedBend;
                updateDragonSkeleton();
            }
        };
        float safeFraction = 0;
        for (int stepIndex = 1; stepIndex <= sweepSteps; ++stepIndex) {
            const float fraction = stepIndex / static_cast<float>(sweepSteps);
            applyPose(fraction);
            if (dragonTerrainClearance(m_dragonPosition, m_dragonYaw, requiredClearance) < requiredClearance) {
                float blockedFraction = fraction;
                for (int iteration = 0; iteration < 8; ++iteration) {
                    const float midpointFraction = (safeFraction + blockedFraction) * .5f;
                    applyPose(midpointFraction);
                    if (dragonTerrainClearance(m_dragonPosition, m_dragonYaw, requiredClearance) >= requiredClearance)
                        safeFraction = midpointFraction;
                    else
                        blockedFraction = midpointFraction;
                }
                applyPose(safeFraction);
                return false;
            }
            safeFraction = fraction;
        }
        return true;
    }

    bool updateDragonSimulation(bool advanceFlight)
    {
        if (m_flightMode != FlightMode::Manual || !m_mountainCollision) {
            const bool moved = advanceFlight && updateDragonFlight();
            updateDragonBodyAnimation();
            updateDragonSkeleton();
            return moved;
        }
        const float frameTime = m_deltaTime;

        const float requestedSteps = std::ceil(frameTime * 120.f);
        const float boundedSteps = std::clamp(requestedSteps, 1.0f, 8.0f);
        const int steps = static_cast<int>(boundedSteps);
        m_deltaTime = frameTime / steps;
        bool moved = false;
        for (int step = 0; step < steps; ++step) {
            const auto oldAngles = m_bodyJointAngles;
            const FlightBend oldBend = m_flightBend;
            updateDragonBodyAnimation();
            auto proposedAngles = m_bodyJointAngles;
            const FlightBend proposedBend = m_flightBend;
            if (m_animateDragonBody && m_dragonModel == 1) {
                for (std::size_t joint = 0; joint < 8; ++joint) {
                    const float angleChange = proposedAngles[joint] - oldAngles[joint];
                    const float minimumChange = -glm::radians(120.f) * m_deltaTime;
                    const float maximumChange = glm::radians(120.f) * m_deltaTime;
                    const float boundedChange = glm::clamp(angleChange, minimumChange, maximumChange);
                    proposedAngles[joint] = oldAngles[joint] + boundedChange;
                }
            }
            m_bodyJointAngles = oldAngles;
            m_flightBend = oldBend;
            if (proposedAngles != oldAngles || proposedBend != oldBend)
                sweepDragonPose(m_dragonPosition, m_dragonYaw, proposedAngles, &proposedBend);
            const glm::vec3 oldPosition = m_dragonPosition;
            const float oldYaw = m_dragonYaw;
            const bool requested = advanceFlight && updateDragonFlight();
            moved = moved || requested;
            const glm::vec3 delta = m_dragonPosition - oldPosition;
            const float proposedYaw = m_dragonYaw;
            m_dragonPosition = oldPosition;
            m_dragonYaw = oldYaw;
            if (proposedYaw != oldYaw)
                sweepDragonPose(oldPosition, proposedYaw, m_bodyJointAngles);
            if (requested) {
                const float acceptedYawDifference = m_dragonYaw - proposedYaw;
                const glm::mat4 movementRotation = glm::rotate(glm::mat4(1), acceptedYawDifference, glm::vec3(0, 1, 0));
                const glm::vec3 movement(movementRotation * glm::vec4(delta, 0));
                glm::vec3 target = m_dragonPosition + movement;
                if (m_constrainFlightToTerrain)
                    target = glm::clamp(target, s_minFlightPosition, s_maxFlightPosition);
                if (!sweepDragonPose(target, m_dragonYaw, m_bodyJointAngles)) {

                    for (int axis : { 1, 0, 2 }) {
                        glm::vec3 slide = m_dragonPosition;
                        slide[axis] = target[axis];
                        if (slide != m_dragonPosition)
                            sweepDragonPose(slide, m_dragonYaw, m_bodyJointAngles);
                    }
                }
            }
        }
        m_deltaTime = frameTime;
        return moved;
    }

    void initializeLandscape()
    {
        for (const auto& mesh : createLandscapeDetails())
            m_landscapeDetails.emplace_back(mesh);
        constexpr int size = 256;
        std::vector<unsigned char> pixels(size * size * 4);
        for (int z = 0; z < size; ++z)
            for (int x = 0; x < size; ++x) {
                const glm::vec2 uv((x + 0.5f) / size, (z + 0.5f) / size),
                    world = mountainCenter + (uv * 2.0f - 1.0f) * mountainHalfExtent;
                const float height = mountainHeight(world), slope = 1.0f - mountainNormal(world).y;
                const float variation = landscapeNoise(world * 5.0f), grain = landscapeNoise(world * 42.0f);
                glm::vec3 color = glm::mix(
                    glm::vec3(.43f, .36f, .27f), glm::vec3(.55f, .53f, .49f), glm::smoothstep(.05f, .7f, slope));
                color = glm::mix(color, glm::vec3(.36f, .40f, .29f),
                    (1.0f - glm::smoothstep(-.7f, .5f, height)) * (1.0f - slope) * variation * .65f);
                color = glm::mix(color, glm::vec3(.62f, .59f, .54f), glm::smoothstep(.9f, 2.1f, height) * .45f);
                color *= .90f + .14f * variation + .06f * grain;
                const int offset = (z * size + x) * 4;
                for (int c = 0; c < 3; ++c)
                    pixels[offset + c] = static_cast<unsigned char>(glm::clamp(color[c], 0.0f, 1.0f) * 255.0f + .5f);
                pixels[offset + 3] = 255;
            }
        GLint active = 0, binding = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        glGenTextures(1, &m_mountainTexture);
        glBindTexture(GL_TEXTURE_2D, m_mountainTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, binding);
        glActiveTexture(active);
    }

    void drawLandscape(bool shadowPass = false)
    {
        if (!m_showGround)
            return;
        const glm::mat4 identity(1);
        if (shadowPass) {
            drawShadowCaster(m_mountainMesh, identity);
            for (auto& mesh : m_landscapeDetails)
                drawShadowCaster(mesh, identity);
            return;
        }
        m_defaultShader.bind();
        setDragonSkinning(m_defaultShader, false);
        const glm::mat4 mvp = m_projectionMatrix * m_viewMatrix;
        const glm::mat3 normal(1);
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix3fv(
            m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normal));
        glUniform1i(m_defaultShader.getUniformLocation("useNormalMap"), false);
        glUniform1i(m_defaultShader.getUniformLocation("usePbrPropertyMaps"), false);
        glUniform1i(m_defaultShader.getUniformLocation("enableEnvironmentReflections"), false);
        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), true);
        setPbrMaterial(0.0f, .88f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_mountainTexture);
        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), true);
        glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
        m_mountainMesh.draw(m_defaultShader);
        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), false);
        for (std::size_t i = 0; i < 3; ++i)
            m_landscapeDetails[i].draw(m_defaultShader);

        const bool cull = glIsEnabled(GL_CULL_FACE);
        GLint cullMode = 0;
        glGetIntegerv(GL_CULL_FACE_MODE, &cullMode);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        for (std::size_t i = 3; i < m_landscapeDetails.size(); ++i)
            m_landscapeDetails[i].draw(m_defaultShader);
        glCullFace(cullMode);
        if (!cull)
            glDisable(GL_CULL_FACE);
    }

    void freeBloomTargets()
    {
        glDeleteFramebuffers(1, &m_bloomSceneFramebuffer);
        glDeleteTextures(1, &m_bloomSceneTexture);
        glDeleteRenderbuffers(1, &m_bloomDepth);
        glDeleteFramebuffers(2, m_bloomFramebuffers.data());
        glDeleteTextures(2, m_bloomTextures.data());
        m_bloomSceneFramebuffer = m_bloomSceneTexture = m_bloomDepth = 0;
        m_bloomFramebuffers.fill(0);
        m_bloomTextures.fill(0);
        m_bloomTargetsReady = false;
    }

    bool ensureBloomTargets(glm::ivec2 size)
    {
        if (size.x <= 0 || size.y <= 0)
            return false;
        if (size == m_bloomSize)
            return m_bloomTargetsReady;
        BloomPassState previous;
        freeBloomTargets();
        m_bloomSize = size;
        m_bloomHalfSize = glm::max((size + glm::ivec2(1)) / 2, glm::ivec2(1));
        glActiveTexture(GL_TEXTURE8);
        auto makeTarget = [](GLuint& framebuffer, GLuint& texture, glm::ivec2 dimensions) {
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, dimensions.x, dimensions.y, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
        };
        makeTarget(m_bloomSceneFramebuffer, m_bloomSceneTexture, size);
        glGenRenderbuffers(1, &m_bloomDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_bloomDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_bloomDepth);
        bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        for (int i = 0; i < 2; ++i) {
            makeTarget(m_bloomFramebuffers[i], m_bloomTextures[i], m_bloomHalfSize);
            complete = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) && complete;
        }
        if (!complete) {
            std::cerr << "Bloom framebuffer incomplete; using the direct scene render." << std::endl;
            freeBloomTargets();
            return false;
        }
        m_bloomTargetsReady = true;
        return true;
    }

    void beginBloomScene()
    {
        m_bloomSceneActive = false;
        if (!m_bloomEnabled || !m_bloomShadersReady)
            return;
        glGetIntegerv(GL_VIEWPORT, m_bloomDestinationViewport);
        if (!ensureBloomTargets({ m_bloomDestinationViewport[2], m_bloomDestinationViewport[3] }))
            return;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_bloomDestinationDraw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_bloomDestinationRead);
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomSceneFramebuffer);
        glViewport(0, 0, m_bloomSize.x, m_bloomSize.y);
        m_bloomSceneActive = true;
    }

    void finishBloom()
    {
        if (!m_bloomSceneActive)
            return;
        {
            BloomPassState previous;
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);
            glDisable(GL_CULL_FACE);
            glDisable(GL_SCISSOR_TEST);
            glBindVertexArray(m_bloomVao);
            glViewport(0, 0, m_bloomHalfSize.x, m_bloomHalfSize.y);
            glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[0]);
            glActiveTexture(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_2D, m_bloomSceneTexture);
            m_bloomExtractShader.bind();
            glUniform1i(m_bloomExtractShader.getUniformLocation("scene"), 8);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            int read = 0;
            m_bloomBlurShader.bind();
            glUniform1i(m_bloomBlurShader.getUniformLocation("image"), 8);

            for (int pass = 0; pass < 6; ++pass) {
                const int write = 1 - read;
                glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[write]);
                glBindTexture(GL_TEXTURE_2D, m_bloomTextures[read]);
                glUniform1i(m_bloomBlurShader.getUniformLocation("horizontal"), pass % 2 == 0);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                read = write;
            }
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_bloomDestinationDraw);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_bloomDestinationRead);
            glViewport(m_bloomDestinationViewport[0], m_bloomDestinationViewport[1], m_bloomDestinationViewport[2],
                m_bloomDestinationViewport[3]);
            glActiveTexture(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_2D, m_bloomSceneTexture);
            glActiveTexture(GL_TEXTURE9);
            glBindTexture(GL_TEXTURE_2D, m_bloomTextures[read]);
            m_bloomCombineShader.bind();
            glUniform1i(m_bloomCombineShader.getUniformLocation("scene"), 8);
            glUniform1i(m_bloomCombineShader.getUniformLocation("bloom"), 9);
            glUniform1f(m_bloomCombineShader.getUniformLocation("strength"), m_bloomStrength);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_bloomDestinationDraw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_bloomDestinationRead);
        glViewport(m_bloomDestinationViewport[0], m_bloomDestinationViewport[1], m_bloomDestinationViewport[2],
            m_bloomDestinationViewport[3]);
        m_bloomSceneActive = false;
    }

    void initializeLavaFrames()
    {
        // Precompute distinct looping texture frames; playback binds one frame at exactly 12 fps.
        constexpr int size = 128;
        constexpr int cells = 6;
        std::vector<unsigned char> pixels(size * size * 4);
        GLint active = 0, previous = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        glGenTextures(s_lavaFrameCount, m_lavaFrames.data());
        for (int frame = 0; frame < s_lavaFrameCount; ++frame) {
            const float phase = glm::two_pi<float>() * static_cast<float>(frame) / s_lavaFrameCount;
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const glm::vec2 point((x + 0.5f) * cells / size, (y + 0.5f) * cells / size);
                    const glm::ivec2 cell(glm::floor(point));
                    float nearest = 100.0f, secondNearest = 100.0f;
                    for (int dz = -1; dz <= 1; ++dz) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const glm::ivec2 neighbor = cell + glm::ivec2(dx, dz);
                            const int wrappedX = (neighbor.x + cells) % cells;
                            const int wrappedZ = (neighbor.y + cells) % cells;
                            const std::uint32_t hash = landscapeHash(static_cast<std::uint32_t>(wrappedX + wrappedZ * cells + 4515));
                            const float randomPhase = static_cast<float>(hash & 65535u) / 65535.0f * glm::two_pi<float>();
                            const glm::vec2 motion(std::sin(phase + randomPhase), std::cos(phase + randomPhase * 2));
                            const glm::vec2 center = glm::vec2(neighbor) + glm::vec2(0.5f) + motion * 0.23f;
                            const float distance = glm::length(point - center);
                            if (distance < nearest) {
                                secondNearest = nearest;
                                nearest = distance;
                            } else if (distance < secondNearest) {
                                secondNearest = distance;
                            }
                        }
                    }
                    const float crack = 1.0f - glm::smoothstep(0.015f, 0.13f, secondNearest - nearest);
                    const float pulse = 0.5f + 0.5f * std::sin(phase + point.x * glm::two_pi<float>());
                    const glm::vec3 color(1.0f, 0.32f + pulse * 0.25f, 0.025f);
                    const int offset = (y * size + x) * 4;
                    for (int channel = 0; channel < 3; ++channel)
                        pixels[offset + channel] = static_cast<unsigned char>(color[channel] * 255.0f + 0.5f);
                    pixels[offset + 3] = static_cast<unsigned char>(crack * 255.0f + 0.5f);
                }
            }
            glBindTexture(GL_TEXTURE_2D, m_lavaFrames[frame]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        glBindTexture(GL_TEXTURE_2D, previous);
        glActiveTexture(active);
    }

    void drawWaterPond()
    {
        if (!m_showGround || !m_showWater || !m_waterReady)
            return;
        m_waterShader.bind();
        const glm::mat4 viewProjection = m_projectionMatrix * m_viewMatrix;
        const glm::mat4 cameraWorld = glm::inverse(m_viewMatrix);
        const glm::vec3 eye(cameraWorld[3]);
        glUniformMatrix4fv(m_waterShader.getUniformLocation("viewProjection"), 1, GL_FALSE, glm::value_ptr(viewProjection));
        glUniform2fv(m_waterShader.getUniformLocation("pondCenter"), 1, glm::value_ptr(waterPondCenter));
        glUniform2fv(m_waterShader.getUniformLocation("pondRadii"), 1, glm::value_ptr(waterPondRadii));
        glUniform1f(m_waterShader.getUniformLocation("waveTime"), static_cast<float>(m_waterTime));
        glUniform1f(m_waterShader.getUniformLocation("waveStrength"), m_waterWaveStrength);
        glUniform3fv(m_waterShader.getUniformLocation("cameraPosition"), 1, glm::value_ptr(eye));
        glUniform3fv(m_waterShader.getUniformLocation("lightDirection"), 1, glm::value_ptr(m_sunDirection));
        glUniform3fv(m_waterShader.getUniformLocation("sunColor"), 1, glm::value_ptr(m_sunColor));
        glUniform1f(m_waterShader.getUniformLocation("sunIntensity"), m_sunIntensity);
        glUniform1f(m_waterShader.getUniformLocation("dayAmbient"), m_dayAmbient);
        glUniform1i(m_waterShader.getUniformLocation("enableLighting"), m_enableLighting);
        glUniform1i(m_waterShader.getUniformLocation("enableReflections"), m_environmentReflections);
        glUniform1i(m_waterShader.getUniformLocation("environmentMap"), 2);
        setTimeOfDayEnvironment(m_waterShader);
        const bool sunShadows = m_shadowMapping && m_shadowMapReady && m_enableLighting && m_sunAboveHorizon;
        glUniform1i(m_waterShader.getUniformLocation("enableShadows"), sunShadows);
        glUniform1i(m_waterShader.getUniformLocation("enableShadowPcf"), m_shadowPcf);
        glUniform1i(m_waterShader.getUniformLocation("shadowMap"), 3);
        glUniformMatrix4fv(m_waterShader.getUniformLocation("lightViewProjection"), 1, GL_FALSE,
            glm::value_ptr(m_lightViewProjection));
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);

        glUniform1i(m_waterShader.getUniformLocation("enableFireLight"), m_fireActive);
        glUniform3fv(m_waterShader.getUniformLocation("fireLightPosition"), 1, glm::value_ptr(m_fireLightPosition));
        glUniform3fv(m_waterShader.getUniformLocation("fireLightDirection"), 1, glm::value_ptr(m_fireLightDirection));
        glUniform3fv(m_waterShader.getUniformLocation("mouthGlowPosition"), 1, glm::value_ptr(m_mouthGlowPosition));
        const float primaryFlicker = 0.04f * static_cast<float>(std::sin(17 * m_fireLightTime));
        const float secondaryFlicker = 0.02f * static_cast<float>(std::sin(29 * m_fireLightTime + 1));
        const float flicker = 0.94f + primaryFlicker + secondaryFlicker;
        glUniform1f(m_waterShader.getUniformLocation("fireLightIntensity"), m_fireLightIntensity * flicker);
        glUniform1f(m_waterShader.getUniformLocation("fireLightRange"), m_fireLightRange);
        const bool fireShadows = m_fireActive && m_fireLightShadows && m_fireShadowReady && m_enableLighting;
        glUniform1i(m_waterShader.getUniformLocation("enableFireShadows"), fireShadows);
        glUniform1i(m_waterShader.getUniformLocation("fireShadowMap"), 10);
        glUniformMatrix4fv(m_waterShader.getUniformLocation("fireViewProjection"), 1, GL_FALSE,
            glm::value_ptr(m_fireViewProjection));
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, m_fireShadowDepthTexture);
        glActiveTexture(GL_TEXTURE2);
        GLint previousCube = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previousCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_environmentMap);
        m_waterMesh.draw(m_waterShader);
        glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(previousCube));
        glActiveTexture(GL_TEXTURE0);
    }

    void drawLavaLake()
    {
        if (!m_showGround || (!m_showLavaLake && !m_showLavaStreams))
            return;
        const int frame = static_cast<int>(m_lavaAnimationSeconds * s_lavaFramesPerSecond) % s_lavaFrameCount;
        m_defaultShader.bind();
        setDragonSkinning(m_defaultShader, false);
        const glm::mat4 identity(1);
        const glm::mat3 normal(1);
        const glm::mat4 mvp = m_projectionMatrix * m_viewMatrix;
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normal));
        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), true);
        glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
        glUniform1i(m_defaultShader.getUniformLocation("isLavaSurface"), true);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_lavaFrames[frame]);
        glUniform1i(m_defaultShader.getUniformLocation("isLavaStream"), false);
        glUniform1f(m_defaultShader.getUniformLocation("lavaFlowOffset"), 0.0f);
        if (m_showLavaLake)
            m_lavaMesh.draw(m_defaultShader);
        if (m_showLavaStreams) {
            glUniform1i(m_defaultShader.getUniformLocation("isLavaStream"), true);
            // Advance flow in discrete steps alongside the fixed-rate texture frames.
            const float flowOffset = static_cast<float>(frame) / s_lavaFrameCount;
            glUniform1f(m_defaultShader.getUniformLocation("lavaFlowOffset"), flowOffset);
            const bool cull = glIsEnabled(GL_CULL_FACE);
            glDisable(GL_CULL_FACE);
            m_lavaStreamsMesh.draw(m_defaultShader);
            if (cull)
                glEnable(GL_CULL_FACE);
        }
        glUniform1i(m_defaultShader.getUniformLocation("isLavaStream"), false);
        glUniform1i(m_defaultShader.getUniformLocation("isLavaSurface"), false);
    }

    void initializeGroundSurface()
    {

        constexpr int size = 512;
        constexpr float tau = 6.28318530718f;
        std::vector<unsigned char> pixels(size * size * 4);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const float u = (x + 0.5f) / size, v = (y + 0.5f) / size;
                const float tileU = u * 8.0f;
                const float tileV = v * 8.0f;
                const float tx = tileU - std::floor(tileU);
                const float ty = tileV - std::floor(tileV);
                const float horizontalEdge = glm::min(tx, 1.0f - tx);
                const float verticalEdge = glm::min(ty, 1.0f - ty);
                const float edge = glm::min(horizontalEdge, verticalEdge);
                const float grout = 1.0f - glm::smoothstep(0.01f, 0.035f, edge);
                const int checker = (static_cast<int>(u * 8.0f) + static_cast<int>(v * 8.0f)) % 2;
                const float grain = 0.004f * std::sin(tau * (5 * u - 7 * v));
                const glm::vec3 tileColor = glm::mix(
                    glm::vec3(0.63f, 0.56f, 0.43f), glm::vec3(0.78f, 0.72f, 0.60f), static_cast<float>(checker));
                const float tileShading = 1.0f + grain - 0.05f * grout;
                const glm::vec3 color = tileColor * tileShading;
                const int offset = (y * size + x) * 4;
                for (int channel = 0; channel < 3; ++channel)
                    pixels[offset + channel] =
                        static_cast<unsigned char>(glm::clamp(color[channel], 0.0f, 1.0f) * 255.0f + 0.5f);
                pixels[offset + 3] = 255;
            }
        }
        GLint active = 0, previous = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        glGenTextures(1, &m_groundSurfaceTexture);
        glBindTexture(GL_TEXTURE_2D, m_groundSurfaceTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (glfwExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
            constexpr GLenum textureAnisotropy = 0x84FE, maxAnisotropy = 0x84FF;
            GLfloat maximum = 1.0f;
            glGetFloatv(maxAnisotropy, &maximum);
            glTexParameterf(GL_TEXTURE_2D, textureAnisotropy, glm::min(8.0f, maximum));
        }
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, previous);
        glActiveTexture(active);
    }

    void renderSky()
    {
        if (!m_skyReady)
            return;
        ParticlePassState previous;
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glActiveTexture(GL_TEXTURE2);
        GLint previousCube = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previousCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_environmentMap);
        m_skyShader.bind();
        const glm::mat4 inverseProjection = glm::inverse(m_projectionMatrix);
        const glm::mat3 cameraRotation = glm::mat3(glm::inverse(m_viewMatrix));
        glUniformMatrix4fv(
            m_skyShader.getUniformLocation("inverseProjection"), 1, GL_FALSE, glm::value_ptr(inverseProjection));
        glUniformMatrix3fv(
            m_skyShader.getUniformLocation("cameraRotation"), 1, GL_FALSE, glm::value_ptr(cameraRotation));
        glUniform1i(m_skyShader.getUniformLocation("environmentMap"), 2);
        setTimeOfDayEnvironment(m_skyShader);
        glBindVertexArray(m_skyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindTexture(GL_TEXTURE_CUBE_MAP, previousCube);
    }

    void initializeDragonScaleTexture()
    {
        constexpr int size = 256;
        constexpr float tau = 6.28318530718f;
        std::vector<unsigned char> pixels(size * size * 4);
        std::vector<float> heights(size * size);
        std::vector<unsigned char> roughnessPixels(size * size), metallicPixels(size * size);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const float u = (x + 0.5f) / size;
                const float v = (y + 0.5f) / size;
                const float rowPosition = 4.0f * v;
                const int row = static_cast<int>(std::floor(rowPosition));
                float nearest = 2.0f;

                for (int candidate = row - 1; candidate <= row + 1; ++candidate) {
                    const float across = 2.0f * glm::fract(4.0f * u + 0.5f * (candidate & 1)) - 1.0f;
                    const float squaredArcHeight = glm::max(0.0f, 1.0f - across * across);
                    const float arcHeight = std::sqrt(squaredArcHeight);
                    const float edge = candidate + 0.5f + 0.5f * arcHeight;
                    const float distance = rowPosition - edge;
                    if (std::abs(distance) < std::abs(nearest))
                        nearest = distance;
                }

                const float edgeDistance = nearest / 0.13f;
                heights[y * size + x] = 1.0f - std::exp(-edgeDistance * edgeDistance);

                const float roughness = 0.20f + 0.50f * (1.0f - heights[y * size + x])
                    + 0.025f * std::sin(tau * 4.0f * u) * std::sin(tau * 4.0f * v);
                const float rim = 1.0f - glm::smoothstep(0.02f, 0.07f, std::abs(nearest - 0.12f));
                const float accent = std::pow(glm::max(std::sin(tau * 4.0f * u), 0.0f), 6.0f);
                roughnessPixels[y * size + x] =
                    static_cast<unsigned char>(glm::clamp(roughness, 0.08f, 1.0f) * 255.0f + 0.5f);
                metallicPixels[y * size + x] = static_cast<unsigned char>(0.9f * rim * accent * 255.0f + 0.5f);
                const float boundary = 1.0f - glm::smoothstep(0.015f, 0.065f, std::abs(nearest));
                const float shade = 0.98f + 0.06f * std::sin(tau * 4.0f * u) * std::sin(tau * 4.0f * v)
                    + 0.025f * std::cos(tau * 4.0f * v);
                glm::vec3 color = glm::mix(
                    glm::vec3(0.68f, 0.075f, 0.055f) * shade, glm::vec3(0.32f, 0.025f, 0.02f), boundary * 0.75f);
                const float highlight = (nearest - 0.07f) / 0.045f;
                color = glm::mix(color, glm::vec3(0.88f, 0.18f, 0.09f), 0.18f * std::exp(-highlight * highlight));
                const int offset = (y * size + x) * 4;
                for (int channel = 0; channel < 3; ++channel)
                    pixels[offset + channel] =
                        static_cast<unsigned char>(glm::clamp(color[channel], 0.0f, 1.0f) * 255.0f + 0.5f);
                pixels[offset + 3] = 255;
            }
        }
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glGenTextures(1, &m_dragonScaleTexture);
        glBindTexture(GL_TEXTURE_2D, m_dragonScaleTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);

        std::vector<unsigned char> normalPixels(size * size * 4);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const int rightIndex = y * size + (x + 1) % size;
                const int leftIndex = y * size + (x + size - 1) % size;
                const int upperIndex = ((y + 1) % size) * size + x;
                const int lowerIndex = ((y + size - 1) % size) * size + x;
                const float du = (heights[rightIndex] - heights[leftIndex]) * (size * 0.5f);
                const float dv = (heights[upperIndex] - heights[lowerIndex]) * (size * 0.5f);
                const glm::vec3 normal = glm::normalize(glm::vec3(-0.008f * du, -0.008f * dv, 1.0f));
                const int offset = (y * size + x) * 4;
                for (int channel = 0; channel < 3; ++channel)
                    normalPixels[offset + channel] =
                        static_cast<unsigned char>((normal[channel] * 0.5f + 0.5f) * 255.0f + 0.5f);
                normalPixels[offset + 3] = 255;
            }
        }
        glGenTextures(1, &m_dragonNormalTexture);
        glBindTexture(GL_TEXTURE_2D, m_dragonNormalTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, normalPixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);

        auto uploadPropertyMap = [size](GLuint& texture, const std::vector<unsigned char>& data) {
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());
            glGenerateMipmap(GL_TEXTURE_2D);
        };
        uploadPropertyMap(m_dragonRoughnessTexture, roughnessPixels);
        uploadPropertyMap(m_dragonMetallicTexture, metallicPixels);
        glBindTexture(GL_TEXTURE_2D, previousTexture);
    }

    void updateFireLight()
    {
        m_fireActive = m_fireLight && dragonBreathActive() && m_fireLightIntensity > 0;
        if (!m_fireActive)
            return;
        float elapsedFireTime = 0.f;
        if (std::isfinite(m_deltaTime)) {
            elapsedFireTime = glm::max(m_deltaTime, 0.f);
        }
        m_fireLightTime += elapsedFireTime;
        const auto emitter = dragonBreathEmitter();

        m_fireLightPosition = emitter.positions[0] + emitter.forward * .08f;
        m_mouthGlowPosition = emitter.positions[0] + emitter.forward * .12f;
        m_fireLightDirection = emitter.forward;
        glm::vec3 up;
        if (std::abs(m_fireLightDirection.y) > .98f) {
            up = glm::vec3(0, 0, 1);
        } else {
            up = glm::vec3(0, 1, 0);
        }
        const glm::mat4 fireProjection = glm::perspective(glm::radians(44.f), 1.f, .05f, m_fireLightRange);
        const glm::vec3 fireTarget = m_fireLightPosition + m_fireLightDirection;
        const glm::mat4 fireView = glm::lookAt(m_fireLightPosition, fireTarget, up);
        m_fireViewProjection = fireProjection * fireView;
    }

    void bindFireLight()
    {
        m_defaultShader.bind();
        glUniform1i(m_defaultShader.getUniformLocation("enableFireLight"), m_fireActive);
        glUniform3fv(m_defaultShader.getUniformLocation("fireLightPosition"), 1, glm::value_ptr(m_fireLightPosition));
        glUniform3fv(m_defaultShader.getUniformLocation("fireLightDirection"), 1, glm::value_ptr(m_fireLightDirection));
        glUniform3fv(m_defaultShader.getUniformLocation("mouthGlowPosition"), 1, glm::value_ptr(m_mouthGlowPosition));
        const float primaryFlicker = .04f * static_cast<float>(std::sin(17 * m_fireLightTime));
        const float secondaryFlicker = .02f * static_cast<float>(std::sin(29 * m_fireLightTime + 1));
        const float flicker = .94f + primaryFlicker + secondaryFlicker;
        glUniform1f(m_defaultShader.getUniformLocation("fireLightIntensity"), m_fireLightIntensity * flicker);
        glUniform1f(m_defaultShader.getUniformLocation("fireLightRange"), m_fireLightRange);
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("fireViewProjection"), 1, GL_FALSE,
            glm::value_ptr(m_fireViewProjection));
        glUniform1i(m_defaultShader.getUniformLocation("enableFireShadows"),
            m_fireActive && m_fireLightShadows && m_fireShadowReady);
        glUniform1i(m_defaultShader.getUniformLocation("fireShadowMap"), 10);
        glActiveTexture(GL_TEXTURE10);
        GLuint fireShadowDepthTextureBinding = 0;
        if (m_fireActive && m_fireLightShadows && m_fireShadowReady) {
            fireShadowDepthTextureBinding = m_fireShadowDepthTexture;
        }
        glBindTexture(GL_TEXTURE_2D, fireShadowDepthTextureBinding);
        glActiveTexture(GL_TEXTURE0);
    }

    void updateDayNightLighting()
    {
        if (m_automaticDayNight)
            m_timeOfDay = static_cast<float>(std::fmod(m_timeOfDay + static_cast<double>(m_deltaTime) * 0.1, 24.0));
        if (m_timeOfDay == m_lastLightingTime)
            return;
        m_lastLightingTime = m_timeOfDay;
        const float phase = (m_timeOfDay - 6.0f) * glm::pi<float>() / 12.0f;
        const float elevation = std::sin(phase);
        const glm::vec3 east = glm::normalize(glm::vec3(.6f, 0, -.5f));
        const glm::vec3 horizontalSun = east * std::cos(phase);
        const glm::vec3 elevatedSun = s_directionalLight * elevation;
        const glm::vec3 sunVector = horizontalSun + elevatedSun + glm::vec3(0, .04f, 0);
        m_sunDirection = glm::normalize(sunVector);
        const float daylight = glm::smoothstep(-.15f, .35f, elevation);
        glm::vec3 warm;
        if (m_timeOfDay < 12.0f) {
            warm = glm::vec3(1, .43f, .24f);
        } else {
            warm = glm::vec3(1, .25f, .08f);
        }
        m_sunColor = glm::mix(warm, glm::vec3(1), glm::smoothstep(.05f, .65f, elevation));
        m_sunIntensity = (.2f + .8f * daylight) * glm::smoothstep(0.f, .05f, m_sunDirection.y);
        m_dayAmbient = glm::mix(.035f, 1.f, daylight);
        m_environmentTint = glm::mix(glm::vec3(.004f, .009f, .035f), glm::vec3(1), daylight);
        m_twilightAmount = 1.f - glm::smoothstep(.03f, .4f, std::abs(elevation));
        if (m_timeOfDay < 12.0f) {
            m_twilightColor = glm::vec3(.68f, .18f, .12f);
        } else {
            m_twilightColor = glm::vec3(.75f, .12f, .045f);
        }
        m_sunAboveHorizon = m_sunDirection.y > .015f && m_sunIntensity > .001f;
        if (m_sunAboveHorizon)
            updateShadowMatrix();
    }

    void setTimeOfDayEnvironment(const Shader& shader)
    {
        glUniform3fv(shader.getUniformLocation("environmentTint"), 1, glm::value_ptr(m_environmentTint));
        glUniform3fv(shader.getUniformLocation("twilightColor"), 1, glm::value_ptr(m_twilightColor));
        glUniform1f(shader.getUniformLocation("twilightAmount"), m_twilightAmount);
    }

    void updateShadowMatrix()
    {

        const glm::vec3 margin(1.25f);
        glm::vec3 sceneMin = glm::min(glm::vec3(-10.0f, -1.0f, -10.0f), s_minFlightPosition - margin);
        glm::vec3 sceneMax = glm::max(glm::vec3(10.0f, -1.0f, 10.0f), s_maxFlightPosition + margin);
        for (const auto& curve : m_flightPath) {
            for (const auto& point : { curve.p0, curve.p1, curve.p2, curve.p3 }) {
                sceneMin = glm::min(sceneMin, point - margin);
                sceneMax = glm::max(sceneMax, point + margin);
            }
        }
        const glm::vec3 center = (sceneMin + sceneMax) * 0.5f;
        glm::vec3 lightUp;
        if (std::abs(m_sunDirection.y) > 0.98f) {
            lightUp = glm::vec3(0, 0, 1);
        } else {
            lightUp = glm::vec3(0, 1, 0);
        }
        const float lightDistance = glm::length(sceneMax - sceneMin) + 1.0f;
        const glm::vec3 lightPosition = center + m_sunDirection * lightDistance;
        const glm::mat4 lightView = glm::lookAt(lightPosition, center, lightUp);
        glm::vec3 lightMin(1e10f), lightMax(-1e10f);
        for (int corner = 0; corner < 8; ++corner) {
            glm::vec3 point = sceneMin;
            if (corner & 1) {
                point.x = sceneMax.x;
            }
            if (corner & 2) {
                point.y = sceneMax.y;
            }
            if (corner & 4) {
                point.z = sceneMax.z;
            }
            const glm::vec3 lightPoint(lightView * glm::vec4(point, 1.0f));
            lightMin = glm::min(lightMin, lightPoint);
            lightMax = glm::max(lightMax, lightPoint);
        }

        const glm::mat4 lightProjection = glm::ortho(lightMin.x - 0.5f, lightMax.x + 0.5f, lightMin.y - 0.5f,
            lightMax.y + 0.5f, -lightMax.z - 0.5f, -lightMin.z + 0.5f);
        m_lightViewProjection = lightProjection * lightView;
    }

    void initializeShadowMap(bool fire = false)
    {
        if (!fire)
            updateShadowMatrix();
        GLuint* selectedFramebuffer;
        GLuint* selectedDepthTexture;
        bool* selectedReady;
        GLsizei size;
        if (fire) {
            selectedFramebuffer = &m_fireShadowFramebuffer;
            selectedDepthTexture = &m_fireShadowDepthTexture;
            selectedReady = &m_fireShadowReady;
            size = s_fireShadowMapSize;
        } else {
            selectedFramebuffer = &m_shadowFramebuffer;
            selectedDepthTexture = &m_shadowDepthTexture;
            selectedReady = &m_shadowMapReady;
            size = s_shadowMapSize;
        }
        GLuint& framebuffer = *selectedFramebuffer;
        GLuint& depthTexture = *selectedDepthTexture;
        bool& ready = *selectedReady;
        GLint previousDraw = 0, previousRead = 0, previousTexture = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        const float border[] = { 1, 1, 1, 1 };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        ready = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (!ready)
            std::cerr << "Shadow framebuffer incomplete; affected light shadows disabled." << std::endl;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDraw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, previousRead);
        glBindTexture(GL_TEXTURE_2D, previousTexture);
    }

    void initializeDragonSkeleton()
    {
        for (std::size_t bone = 0; bone < dragonBoneCount; ++bone) {
            const glm::mat4 rest =
                glm::translate(glm::mat4(1), glm::vec3(0, 0, -0.32f + static_cast<float>(bone) * dragonJointSpacing));
            m_inverseBindMatrices[bone] = glm::inverse(rest);
        }
        updateDragonSkeleton();
    }

    void updateDragonSkeleton()
    {
        glm::mat4 current = glm::translate(glm::mat4(1), glm::vec3(0, m_flightBend.bob, -0.32f));
        if (m_flightBend.bank != 0)
            current = glm::rotate(current, m_flightBend.bank, glm::vec3(0, 0, 1));
        for (std::size_t bone = 0; bone < dragonBoneCount; ++bone) {
            if (bone != 0)
                current = glm::translate(current, glm::vec3(0, 0, dragonJointSpacing));
            if (m_flightBend.pitch[bone] != 0)
                current = glm::rotate(current, m_flightBend.pitch[bone], glm::vec3(1, 0, 0));
            current = glm::rotate(current, m_bodyJointAngles[bone], glm::vec3(0, 1, 0));
            m_currentBoneMatrices[bone] = current;
            m_skinMatrices[bone] = current * m_inverseBindMatrices[bone];
        }
        m_collisionPoseDirty = true;
    }

    glm::mat4 collisionSkinAt(float z) const
    {
        if (z == -std::numeric_limits<float>::infinity()) {
            return headAnimationSkin();
        } else {
            return dragonSkinAt(z);
        }
    }

    glm::mat4 dragonSkinAt(float z) const
    {
        const SkinInfluence skin = dragonSkinInfluence(z);
        return skin.weights.x * m_skinMatrices[skin.bones.x] + skin.weights.y * m_skinMatrices[skin.bones.y];
    }

    void setDragonSkinning(const Shader& shader, bool enabled)
    {
        glUniform1i(shader.getUniformLocation("useSkinning"), enabled);
        glUniform1i(shader.getUniformLocation("useMouthMorph"), false);
        if (enabled)
            glUniformMatrix4fv(shader.getUniformLocation("boneMatrices[0]"), static_cast<GLsizei>(dragonBoneCount),
                GL_FALSE, glm::value_ptr(m_skinMatrices[0]));
    }

    void drawShadowCaster(GPUMesh& mesh, const glm::mat4& modelMatrix)
    {
        m_shadowShader.bind();
        setDragonSkinning(m_shadowShader, &mesh == &m_bodyMesh);
        if ((&mesh == &m_jawMesh || &mesh == &m_lowerTeethMesh)) {
            glUniform1i(m_shadowShader.getUniformLocation("useMouthMorph"), true);
            glUniform1f(m_shadowShader.getUniformLocation("mouthOpen"), m_mouthOpen);
        }
        glUniformMatrix4fv(m_shadowShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(modelMatrix));

        mesh.draw(m_defaultShader);
    }

    void renderShadowMap(bool fire = false)
    {
        GLuint framebuffer;
        GLsizei size;
        const glm::mat4* selectedLightMatrix;
        GLenum shadowTextureUnit;
        if (fire) {
            framebuffer = m_fireShadowFramebuffer;
            size = s_fireShadowMapSize;
            selectedLightMatrix = &m_fireViewProjection;
            shadowTextureUnit = GL_TEXTURE10;
        } else {
            framebuffer = m_shadowFramebuffer;
            size = s_shadowMapSize;
            selectedLightMatrix = &m_lightViewProjection;
            shadowTextureUnit = GL_TEXTURE3;
        }
        const glm::mat4& lightMatrix = *selectedLightMatrix;
        GLint previousDraw = 0, previousRead = 0, viewport[4], depthFunction = 0;
        GLboolean depthWrite = GL_TRUE;
        GLdouble clearDepth = 1.0;
        GLfloat offsetFactor = 0.0f, offsetUnits = 0.0f;
        const bool polygonOffset = glIsEnabled(GL_POLYGON_OFFSET_FILL);
        const bool depthTest = glIsEnabled(GL_DEPTH_TEST);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &offsetFactor);
        glGetFloatv(GL_POLYGON_OFFSET_UNITS, &offsetUnits);
        glActiveTexture(shadowTextureUnit);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, size, size);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        m_shadowShader.bind();
        glUniformMatrix4fv(
            m_shadowShader.getUniformLocation("lightViewProjection"), 1, GL_FALSE, glm::value_ptr(lightMatrix));
        drawLandscape(true);
        if (m_dragonModel == 1)
            drawSegmentedDragon(true);
        else
            for (GPUMesh& mesh : m_meshes)
                drawShadowCaster(mesh, m_modelMatrix);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDraw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, previousRead);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glPolygonOffset(offsetFactor, offsetUnits);
        if (!polygonOffset)
            glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthFunc(depthFunction);
        glDepthMask(depthWrite);
        glClearDepth(clearDepth);
        if (!depthTest)
            glDisable(GL_DEPTH_TEST);
    }

    void initializeEnvironmentMap()
    {
        constexpr int size = 128;
        m_environmentMaxLod = std::log2(static_cast<float>(size));

        const std::array<glm::vec3, 6> centers { { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 },
            { 0, 0, -1 } } };
        const std::array<glm::vec3, 6> right { { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 },
            { -1, 0, 0 } } };
        const std::array<glm::vec3, 6> down { { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 },
            { 0, -1, 0 } } };
        GLint previousCube = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previousCube);
        glGenTextures(1, &m_environmentMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_environmentMap);
        std::vector<float> pixels(size * size * 3);
        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const float s = 2.0f * (x + 0.5f) / size - 1.0f;
                    const float t = 2.0f * (y + 0.5f) / size - 1.0f;
                    const glm::vec3 direction = glm::normalize(centers[face] + s * right[face] + t * down[face]);

                    const float skyHeight = glm::max(direction.y, 0.0f);
                    const float skyBlend = glm::smoothstep(0.0f, 1.0f, skyHeight);
                    const glm::vec3 sky =
                        glm::mix(glm::vec3(0.62f, 0.72f, 0.80f), glm::vec3(0.055f, 0.18f, 0.40f), skyBlend);
                    const glm::vec3 ground = glm::mix(
                        glm::vec3(0.38f, 0.31f, 0.20f), glm::vec3(0.20f, 0.16f, 0.10f), glm::max(-direction.y, 0.0f));
                    const glm::vec3 radiance = glm::mix(ground, sky, glm::smoothstep(-0.15f, 0.05f, direction.y));
                    const int offset = (y * size + x) * 3;
                    for (int channel = 0; channel < 3; ++channel)
                        pixels[offset + channel] = radiance[channel];
                }
            }

            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, pixels.data());
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        glBindTexture(GL_TEXTURE_CUBE_MAP, previousCube);
    }

    void setEnvironmentReflections(bool eligible)
    {
        const bool enabled = eligible && m_environmentReflections && m_shadingModel == 1;
        glActiveTexture(GL_TEXTURE2);
        GLuint environmentTexture = 0;
        if (enabled) {
            environmentTexture = m_environmentMap;
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, environmentTexture);
        glUniform1i(m_defaultShader.getUniformLocation("environmentMap"), 2);
        glUniform1i(m_defaultShader.getUniformLocation("enableEnvironmentReflections"), enabled);
        glUniform1f(m_defaultShader.getUniformLocation("reflectionStrength"), m_reflectionStrength);
        glUniform1f(m_defaultShader.getUniformLocation("environmentMaxLod"), m_environmentMaxLod);
        glActiveTexture(GL_TEXTURE0);
    }

    void resetCameraView()
    {
        m_cameraDragging = false;
        m_pendingCameraScroll = 0.0f;
        if (m_cameraMode == CameraMode::FollowDragon) {
            m_followOrbitYaw = 0.0f;
            m_followPitch = glm::radians(25.0f);
            m_followDistance = 2.5f;
        } else if (m_cameraMode == CameraMode::FreeCamera) {
            m_freeCameraPosition = glm::vec3(-1, 1, -1);
            m_freeLookYaw = glm::radians(45.0f);
            m_freeLookPitch = -std::atan(1.0f / std::sqrt(2.0f));
        }
    }

    void updateCamera()
    {
        const ImGuiIO& io = ImGui::GetIO();
        const bool leftDown = m_window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
        const glm::vec2 cursorPos = m_window.getCursorPos();
        if (!leftDown || m_cameraMode == CameraMode::TopDown)
            m_cameraDragging = false;

        const bool startDrag = leftDown && !m_cameraLeftWasDown && !io.WantCaptureMouse
            && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && m_cameraMode != CameraMode::TopDown;
        if (startDrag)
            m_cameraDragging = true;
        if (m_cameraDragging && !startDrag) {
            constexpr float sensitivity = 0.0035f;

            const glm::vec2 delta(cursorPos.x - m_previousCameraCursor.x, m_previousCameraCursor.y - cursorPos.y);
            const float yawDelta = -delta.x * sensitivity;
            const float pitchDelta = -delta.y * sensitivity;
            if (m_cameraMode == CameraMode::FollowDragon) {
                m_followOrbitYaw = std::remainder(m_followOrbitYaw + yawDelta, glm::radians(360.0f));
                m_followPitch = glm::clamp(m_followPitch + pitchDelta, glm::radians(5.0f), glm::radians(80.0f));
            } else {
                m_freeLookYaw = std::remainder(m_freeLookYaw + yawDelta, glm::radians(360.0f));
                m_freeLookPitch = glm::clamp(m_freeLookPitch + pitchDelta, glm::radians(-85.0f), glm::radians(85.0f));
            }
        }
        m_cameraLeftWasDown = leftDown;
        m_previousCameraCursor = cursorPos;
        if (!io.WantCaptureMouse && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)
            && m_cameraMode == CameraMode::FollowDragon && m_pendingCameraScroll != 0.0f)
            m_followDistance = glm::clamp(m_followDistance * std::exp(-0.12f * m_pendingCameraScroll), 1.2f, 8.0f);
        m_pendingCameraScroll = 0.0f;

        switch (m_cameraMode) {
        case CameraMode::FreeCamera: {
            const glm::vec3 forward(std::cos(m_freeLookPitch) * std::sin(m_freeLookYaw), std::sin(m_freeLookPitch),
                std::cos(m_freeLookPitch) * std::cos(m_freeLookYaw));

            if (!io.WantCaptureKeyboard) {
                const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
                glm::vec3 movement(0.0f);
                if (m_window.isKeyPressed(GLFW_KEY_W))
                    movement += forward;
                if (m_window.isKeyPressed(GLFW_KEY_S))
                    movement -= forward;
                if (m_window.isKeyPressed(GLFW_KEY_A))
                    movement -= right;
                if (m_window.isKeyPressed(GLFW_KEY_D))
                    movement += right;
                if (m_window.isKeyPressed(GLFW_KEY_Q))
                    movement.y -= 1.0f;
                if (m_window.isKeyPressed(GLFW_KEY_E))
                    movement.y += 1.0f;
                if (glm::dot(movement, movement) > 0.0f) {
                    constexpr float cameraSpeed = 2.5f;
                    m_freeCameraPosition += glm::normalize(movement) * cameraSpeed * m_deltaTime;
                }
            }
            m_viewMatrix = glm::lookAt(m_freeCameraPosition, m_freeCameraPosition + forward, glm::vec3(0, 1, 0));
            break;
        }
        case CameraMode::FollowDragon: {
            const float yaw = m_dragonYaw + m_followOrbitYaw;
            const float offsetX = std::cos(m_followPitch) * std::sin(yaw);
            const float offsetY = std::sin(m_followPitch);
            const float offsetZ = std::cos(m_followPitch) * std::cos(yaw);
            const glm::vec3 offset = m_followDistance * glm::vec3(offsetX, offsetY, offsetZ);
            m_viewMatrix = glm::lookAt(m_dragonPosition + offset, m_dragonPosition, glm::vec3(0, 1, 0));
            break;
        }
        case CameraMode::TopDown:
            m_viewMatrix = glm::lookAt(m_dragonPosition + glm::vec3(0, 3, 0), m_dragonPosition, glm::vec3(0, 0, -1));
            break;
        }
    }

    void updateDragonBodyAnimation()
    {
        m_bodyJointAngles = m_manualBodyJointAngles;
        m_flightBend = {};
        if (m_responsiveFlightAnimation) {
            m_flightBend.headFollow = 1.f;
        } else {
            m_flightBend.headFollow = 0.f;
        }
        if (!m_animateDragonBody || m_dragonModel != 1) {
            m_steeringResponse.fill(glm::vec2(0));
            return;
        }

        m_bodyAnimationTime += static_cast<double>(m_deltaTime);

        const float frontAmplitude = glm::radians(4.0f);
        const float rearAmplitude = glm::radians(16.0f);
        constexpr double frequency = 0.8;
        constexpr double phaseOffset = 0.5;
        const double phase = glm::radians(360.0) * frequency * m_bodyAnimationTime;
        m_bodyAnimationBlend = glm::min(1.0f, m_bodyAnimationBlend + m_deltaTime / 0.25f);
        const float blend = glm::smoothstep(0.0f, 1.0f, m_bodyAnimationBlend);
        for (std::size_t joint = 0; joint < m_bodyJointAngles.size(); ++joint) {
            const float alongBody = static_cast<float>(joint) / static_cast<float>(m_bodyJointAngles.size() - 1);
            const float amplitude = glm::mix(frontAmplitude, rearAmplitude, alongBody);
            const float waveAngle =
                amplitude * static_cast<float>(std::sin(phase - static_cast<double>(joint) * phaseOffset));
            m_bodyJointAngles[joint] = glm::mix(m_manualBodyJointAngles[joint], waveAngle, blend);
        }
        if (!m_responsiveFlightAnimation) {
            m_steeringResponse.fill(glm::vec2(0));
            return;
        }

        glm::vec2 steering(0);
        if (m_flightMode == FlightMode::Autopilot) {
            const auto segmentIndex = static_cast<std::size_t>(m_autopilotProgress);
            const glm::vec3 tangent =
                evaluateCubicBezierTangent(m_flightPath[segmentIndex], m_autopilotProgress - segmentIndex);
            const float horizontalSpeed = glm::length(glm::vec2(tangent.x, tangent.z));
            if (horizontalSpeed > 1e-5f) {
                const float targetYaw = std::atan2(-tangent.x, -tangent.z);
                const float yawDifference = std::remainder(targetYaw - m_dragonYaw, glm::two_pi<float>());
                const float steeringAmount = yawDifference / glm::radians(8.f);
                steering.x = glm::clamp(steeringAmount, -1.f, 1.f);
                steering.y = glm::clamp(tangent.y / horizontalSpeed, -1.f, 1.f);
            }
        } else if (m_cameraMode != CameraMode::FreeCamera && !ImGui::GetIO().WantCaptureKeyboard) {
            steering.x = float(m_window.isKeyPressed(GLFW_KEY_A)) - float(m_window.isKeyPressed(GLFW_KEY_D));
            steering.y = float(m_window.isKeyPressed(GLFW_KEY_E)) - float(m_window.isKeyPressed(GLFW_KEY_Q));
        }
        const float requestedResponseSteps = std::ceil(m_deltaTime * 120.f);
        const float boundedResponseSteps = std::clamp(requestedResponseSteps, 1.f, 8.f);
        const int responseSteps = static_cast<int>(boundedResponseSteps);
        const float headDamping = 1.f - std::exp(-10.f * m_deltaTime / responseSteps);
        const float tailDamping = 1.f - std::exp(-18.f * m_deltaTime / responseSteps);
        for (int step = 0; step < responseSteps; ++step) {
            for (std::size_t joint = 0; joint < 8; ++joint) {
                glm::vec2 targetResponse;
                float damping;
                if (joint != 0) {
                    targetResponse = m_steeringResponse[joint - 1];
                    damping = tailDamping;
                } else {
                    targetResponse = steering;
                    damping = headDamping;
                }
                m_steeringResponse[joint] = glm::mix(m_steeringResponse[joint], targetResponse, damping);
            }
        }
        for (std::size_t joint = 0; joint < 8; ++joint) {
            const float alongBody = joint / 7.f;
            float yawDegrees;
            float pitchDegrees;
            if (joint != 0) {
                yawDegrees = -1.5f;
                pitchDegrees = -2.f;
            } else {
                yawDegrees = 6.f;
                pitchDegrees = 12.f;
            }
            const float steeringYaw = glm::radians(yawDegrees) * m_steeringResponse[joint].x * blend;
            m_bodyJointAngles[joint] += steeringYaw;
            const float idleAmplitude = glm::radians(glm::mix(1.f, 2.5f, alongBody));
            const float idleWave = static_cast<float>(std::sin(phase - .5 * joint));
            const float idlePitch = idleAmplitude * idleWave;
            float previousIdlePitch = 0.f;
            if (joint != 0) {
                const float previousAmplitude = glm::radians(glm::mix(1.f, 2.5f, (joint - 1) / 7.f));
                const float previousWave = static_cast<float>(std::sin(phase - .5 * (joint - 1)));
                previousIdlePitch = previousAmplitude * previousWave;
            }
            const float steeringPitch = glm::radians(pitchDegrees) * m_steeringResponse[joint].y;
            m_flightBend.pitch[joint] = (steeringPitch + idlePitch - previousIdlePitch) * blend;
        }
        m_flightBend.bank = glm::radians(10.f) * m_steeringResponse[0].x * blend;
        m_flightBend.bob = .012f * static_cast<float>(std::sin(phase * .6)) * blend;
    }

    enum class DragonMaterial { Jade, Gold, Pale, Eye };

    void setPbrMaterial(float metallic, float roughness)
    {

        glUniform1f(m_defaultShader.getUniformLocation("pbrMetallic"), metallic);
        glUniform1f(m_defaultShader.getUniformLocation("pbrRoughness"), roughness);
    }

    bool setPbrPropertyMaps(bool eligible)
    {
        const bool enabled = eligible && m_usePbrPropertyMaps && m_shadingModel == 1;
        glActiveTexture(GL_TEXTURE4);
        GLuint dragonRoughnessTextureBinding = 0;
        if (enabled) {
            dragonRoughnessTextureBinding = m_dragonRoughnessTexture;
        }
        glBindTexture(GL_TEXTURE_2D, dragonRoughnessTextureBinding);
        glActiveTexture(GL_TEXTURE5);
        GLuint dragonMetallicTextureBinding = 0;
        if (enabled) {
            dragonMetallicTextureBinding = m_dragonMetallicTexture;
        }
        glBindTexture(GL_TEXTURE_2D, dragonMetallicTextureBinding);
        glUniform1i(m_defaultShader.getUniformLocation("roughnessMap"), 4);
        glUniform1i(m_defaultShader.getUniformLocation("metallicMap"), 5);
        glUniform1i(m_defaultShader.getUniformLocation("usePbrPropertyMaps"), enabled);
        glActiveTexture(GL_TEXTURE0);
        return enabled;
    }

    void setDragonNormalMapping(bool eligible)
    {
        const bool enabled = eligible && m_useDragonNormalMapping;
        glActiveTexture(GL_TEXTURE1);
        GLuint dragonNormalTextureBinding = 0;
        if (enabled) {
            dragonNormalTextureBinding = m_dragonNormalTexture;
        }
        glBindTexture(GL_TEXTURE_2D, dragonNormalTextureBinding);
        glUniform1i(m_defaultShader.getUniformLocation("normalMap"), 1);
        glUniform1i(m_defaultShader.getUniformLocation("useNormalMap"), enabled);
        glUniform1f(m_defaultShader.getUniformLocation("normalStrength"), m_dragonNormalStrength);
        glActiveTexture(GL_TEXTURE0);
    }

    void drawDragonPart(GPUMesh& mesh, const glm::mat4& modelMatrix, DragonMaterial material, bool shadowPass = false)
    {
        if (m_collectCollisionSamples) {
            const std::array<const GPUMesh*, 17> meshes { { &m_bodyMesh, &m_headMesh, &m_snoutMesh, &m_jawMesh,
                &m_throatMesh, &m_faceDetailMesh, &m_goldDetailMesh, &m_lightDetailMesh, &m_eyeMesh, &m_hornMesh,
                &m_whiskerMesh, &m_browMesh, &m_eyelidMesh, &m_foreheadMesh, &m_teethMesh, &m_lowerTeethMesh, &m_muzzleDetailsMesh } };
            const auto found = std::find(meshes.begin(), meshes.end(), &mesh);
            if (found != meshes.end()) {
                for (auto point : m_collisionGeometry[found - meshes.begin()]) {
                    float attachmentZ;
                    if (&mesh == &m_bodyMesh) {
                        attachmentZ = point.z;
                    } else {
                        attachmentZ = m_collisionSkinZ;
                    }
                    const glm::vec3 transformedPoint(modelMatrix * glm::vec4(point, 1));
                    CollisionSample sample { transformedPoint, attachmentZ };
                    if ((&mesh == &m_jawMesh || &mesh == &m_lowerTeethMesh)) {
                        const glm::vec3 localOffset = dragonJawMorphOffset(point);
                        sample.mouthOffset = glm::vec3(modelMatrix * glm::vec4(localOffset, 0));
                        sample.mouthMorph = true;
                    }
                    m_collisionSamples.push_back(sample);
                }
            }
            return;
        }
        if (shadowPass) {
            drawShadowCaster(mesh, modelMatrix);
            return;
        }
        const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * modelMatrix;
        const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(modelMatrix));
        m_defaultShader.bind();
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));

        glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        glUniformMatrix3fv(
            m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));

        const bool scaleSurface = &mesh == &m_bodyMesh;
        setDragonSkinning(m_defaultShader, scaleSurface);
        if ((&mesh == &m_jawMesh || &mesh == &m_lowerTeethMesh)) {
            glUniform1i(m_defaultShader.getUniformLocation("useMouthMorph"), true);
            glUniform1f(m_defaultShader.getUniformLocation("mouthOpen"), m_mouthOpen);
            glUniformMatrix3fv(m_defaultShader.getUniformLocation("mouthNormalMatrix"), 1, GL_FALSE,
                glm::value_ptr(m_mouthNormalMatrix));
        }
        const bool textured = m_useDragonScaleTexture && scaleSurface;
        glActiveTexture(GL_TEXTURE0);
        GLuint dragonScaleTextureBinding = 0;
        if (textured) {
            dragonScaleTextureBinding = m_dragonScaleTexture;
        }
        glBindTexture(GL_TEXTURE_2D, dragonScaleTextureBinding);
        glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), textured);
        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
        switch (material) {
        case DragonMaterial::Jade:
            setPbrMaterial(0.0f, m_jadeRoughness);
            break;
        case DragonMaterial::Gold:
            setPbrMaterial(1.0f, m_goldRoughness);
            break;
        case DragonMaterial::Pale:
            setPbrMaterial(0.0f, 0.45f);
            break;
        case DragonMaterial::Eye:
            setPbrMaterial(0.0f, 0.12f);
            break;
        }
        const bool propertyMapped = setPbrPropertyMaps(scaleSurface);
        setEnvironmentReflections(material == DragonMaterial::Gold || propertyMapped);
        setDragonNormalMapping(scaleSurface);
        mesh.draw(m_defaultShader);
    }

    void drawDragonHeadDetails(const glm::mat4& headWorld, const glm::mat4& snoutWorld, bool shadowPass)
    {
        drawDragonPart(m_jawMesh, snoutWorld, DragonMaterial::Pale, shadowPass);
        drawDragonPart(m_teethMesh, snoutWorld, DragonMaterial::Pale, shadowPass);
        drawDragonPart(m_lowerTeethMesh, snoutWorld, DragonMaterial::Pale, shadowPass);
        drawDragonPart(m_muzzleDetailsMesh, snoutWorld, DragonMaterial::Gold, shadowPass);
        // Visual polish: removed tongue geometry; no interior insert replaces it.
        const glm::mat4 bridge = glm::translate(snoutWorld, glm::vec3(0, 0.063f, -0.020f));
        drawDragonPart(
            m_faceDetailMesh, glm::scale(bridge, glm::vec3(0.029f, 0.016f, 0.068f)), DragonMaterial::Jade, shadowPass);
        for (float side : { -1.0f, 1.0f }) {

            glm::mat4 socket = glm::translate(headWorld, glm::vec3(side * 0.105f, 0.055f, -0.145f));
            socket = glm::rotate(socket, glm::radians(side * -12.0f), glm::vec3(0, 1, 0));
            drawDragonPart(m_faceDetailMesh, glm::scale(socket, glm::vec3(0.027f, 0.043f, 0.078f)),
                DragonMaterial::Jade, shadowPass);
            glm::mat4 eye = glm::translate(headWorld, glm::vec3(side * 0.125f, 0.062f, -0.16f));
            eye = glm::rotate(eye, glm::radians(side * -12.0f), glm::vec3(0, 1, 0));
            drawDragonPart(
                m_eyeMesh, glm::scale(eye, glm::vec3(0.012f, 0.019f, 0.038f)), DragonMaterial::Eye, shadowPass);
            const glm::mat4 iris = glm::translate(eye, glm::vec3(side * 0.011f, 0, -0.004f));
            drawDragonPart(m_lightDetailMesh, glm::scale(iris, glm::vec3(0.004f, 0.012f, 0.020f)),
                DragonMaterial::Pale, shadowPass);
            const glm::mat4 pupil = glm::translate(iris, glm::vec3(side * 0.003f, 0, 0));
            drawDragonPart(m_eyeMesh, glm::scale(pupil, glm::vec3(0.002f, 0.010f, 0.006f)),
                DragonMaterial::Eye, shadowPass);
            glm::mat4 cheek = glm::translate(headWorld, glm::vec3(side * 0.105f, -0.003f, -0.005f));
            cheek = glm::rotate(cheek, glm::radians(side * -12.0f), glm::vec3(0, 1, 0));
            drawDragonPart(m_faceDetailMesh, glm::scale(cheek, glm::vec3(0.036f, 0.054f, 0.090f)), DragonMaterial::Jade,
                shadowPass);

            glm::mat4 horn = glm::translate(headWorld, glm::vec3(side * 0.064f, 0.11f, 0.02f));
            horn = glm::rotate(horn, glm::radians(-67.0f), glm::vec3(1, 0, 0));
            horn = glm::rotate(horn, glm::radians(side * 22.0f), glm::vec3(0, 1, 0));
            const glm::mat4 antler = glm::scale(horn, glm::vec3(0.30f));
            drawDragonPart(m_hornMesh, antler, DragonMaterial::Gold, shadowPass);
            glm::mat4 branch = glm::translate(antler, dragonTendrilPoint(0.48f, -0.45f));
            branch = glm::rotate(branch, glm::radians(side * 50.0f), glm::vec3(0, 1, 0));
            branch = glm::rotate(branch, glm::radians(30.0f), glm::vec3(1, 0, 0));
            drawDragonPart(m_hornMesh, glm::scale(branch, glm::vec3(0.40f)), DragonMaterial::Gold, shadowPass);

            for (int layer = 0; layer < 3; ++layer) {
                glm::mat4 mane = glm::translate(
                    headWorld, glm::vec3(side * 0.108f, -0.005f + 0.035f * layer, 0.015f + 0.025f * layer));
                mane = glm::rotate(mane, glm::radians(side * 55.0f), glm::vec3(0, 1, 0));
                mane = glm::rotate(mane, glm::radians(-60.0f + 12.0f * layer), glm::vec3(1, 0, 0));
                drawDragonPart(
                    m_hornMesh, glm::scale(mane, glm::vec3(0.16f - 0.02f * layer)), DragonMaterial::Gold, shadowPass);
            }

            const glm::mat4 nostril = glm::translate(snoutWorld, dragonNostrilLocal(side));
            const glm::mat4 noseRim = glm::translate(nostril, glm::vec3(0, -0.005f, 0));
            drawDragonPart(m_faceDetailMesh, glm::scale(noseRim, glm::vec3(0.020f, 0.012f, 0.020f)),
                DragonMaterial::Jade, shadowPass);
            drawDragonPart(m_eyeMesh, glm::scale(nostril, glm::vec3(0.012f, 0.008f, 0.012f)),
                DragonMaterial::Pale, shadowPass);
            const glm::mat4 hingeCover = glm::translate(snoutWorld, glm::vec3(side * 0.059f, -0.018f, 0.065f));
            drawDragonPart(m_faceDetailMesh, glm::scale(hingeCover, glm::vec3(0.030f, 0.044f, 0.060f)),
                DragonMaterial::Jade, shadowPass);
            const std::array<glm::vec3, 3> lipCenters {
                glm::vec3(side * 0.054f, 0.005f, -0.241f),
                glm::vec3(side * 0.043f, 0.010f, -0.156f),
                glm::vec3(side * 0.057f, -0.005f, -0.040f) };
            for (const glm::vec3& center : lipCenters) {
                const glm::mat4 lip = glm::translate(snoutWorld, center);
                drawDragonPart(m_lightDetailMesh, glm::scale(lip, glm::vec3(0.006f, 0.006f, 0.047f)),
                    DragonMaterial::Pale, shadowPass);
            }
            glm::mat4 whisker = glm::translate(snoutWorld, glm::vec3(side * 0.051f, -0.015f, -0.195f));
            whisker = glm::rotate(whisker, glm::radians(side * 130.0f), glm::vec3(0, 1, 0));
            drawDragonPart(m_whiskerMesh, glm::scale(whisker, glm::vec3(0.42f)), DragonMaterial::Pale, shadowPass);
            glm::mat4 beard = glm::translate(snoutWorld, glm::vec3(side * 0.025f, -0.09f, -0.175f));
            beard = glm::rotate(beard, glm::radians(side * 155.0f), glm::vec3(0, 1, 0));
            beard = glm::rotate(beard, glm::radians(12.0f), glm::vec3(1, 0, 0));
            drawDragonPart(m_whiskerMesh, glm::scale(beard, glm::vec3(0.20f)), DragonMaterial::Pale, shadowPass);
        }

        drawDragonPart(m_browMesh, headWorld, DragonMaterial::Gold, shadowPass);
        drawDragonPart(m_eyelidMesh, headWorld, DragonMaterial::Jade, shadowPass);
        drawDragonPart(m_foreheadMesh, headWorld, DragonMaterial::Jade, shadowPass);
        for (float z : { 0.04f, 0.115f }) {
            glm::mat4 crest = glm::translate(headWorld, glm::vec3(0, 0.12f - z * 0.2f, z));
            crest = glm::rotate(crest, glm::radians(-35.0f), glm::vec3(1, 0, 0));
            drawDragonPart(
                m_hornMesh, glm::scale(crest, glm::vec3(0.13f, 0.13f, 0.15f)), DragonMaterial::Gold, shadowPass);
        }
    }

    void drawSegmentedDragon(bool shadowPass = false)
    {

        m_collisionSkinZ = -std::numeric_limits<float>::infinity();
        const glm::mat4 headWorld = dragonHeadWorld();
        drawDragonPart(m_headMesh, headWorld, DragonMaterial::Jade, shadowPass);
        const glm::mat4 snoutWorld = dragonSnoutWorld();
        drawDragonPart(m_snoutMesh, snoutWorld, DragonMaterial::Jade, shadowPass);
        drawDragonHeadDetails(headWorld, snoutWorld, shadowPass);

        drawDragonPart(m_throatMesh, m_modelMatrix * headAnimationSkin(), DragonMaterial::Pale, shadowPass);
        m_collisionSkinZ = std::numeric_limits<float>::infinity();

        drawDragonPart(m_bodyMesh, m_modelMatrix, DragonMaterial::Jade, shadowPass);

        for (std::size_t joint = 0; joint < dragonBoneCount; ++joint) {
            const float z = -0.32f + (static_cast<float>(joint) + 0.5f) * dragonJointSpacing;
            const float radius = dragonBodyRadius(z);
            const glm::mat4 belly = glm::translate(m_modelMatrix * dragonSkinAt(z), glm::vec3(0, -radius * 0.96f, z));
            m_collisionSkinZ = z;
            drawDragonPart(m_lightDetailMesh,
                glm::scale(belly, glm::vec3(radius * 0.72f, radius * 0.16f, dragonJointSpacing * 0.43f)),
                DragonMaterial::Pale, shadowPass);
        }
        for (std::size_t joint = 0; joint < dragonBoneCount; joint += 2) {
            const float z = -0.32f + (static_cast<float>(joint) + 0.5f) * dragonJointSpacing;
            const float radius = dragonBodyRadius(z);

            const glm::mat4 surface = m_modelMatrix * dragonSkinAt(z);
            const glm::mat4 spikeWorld = glm::rotate(
                glm::translate(surface, glm::vec3(0, radius * 0.8f, z)), glm::radians(-65.0f), glm::vec3(1, 0, 0));
            m_collisionSkinZ = z;
            drawDragonPart(m_goldDetailMesh,
                glm::scale(spikeWorld, glm::vec3(radius * 0.25f, radius * 0.25f, radius * 0.9f)), DragonMaterial::Gold,
                shadowPass);
        }
        m_collisionSkinZ = std::numeric_limits<float>::infinity();
    }

    bool updateDragonFlight()
    {
        glm::vec3 movementDirection { 0.0f };
        if (m_flightMode == FlightMode::Autopilot) {

            m_autopilotElapsedSeconds =
                std::fmod(m_autopilotElapsedSeconds + static_cast<double>(m_deltaTime), s_autopilotLoopSeconds);
            const double targetDistance = m_totalFlightLength * (m_autopilotElapsedSeconds / s_autopilotLoopSeconds);
            m_autopilotProgress = autopilotParameterAtDistance(targetDistance);
            const auto segmentIndex = static_cast<std::size_t>(m_autopilotProgress);
            const float t = m_autopilotProgress - static_cast<float>(segmentIndex);
            const CubicBezierSegment& segment = m_flightPath[segmentIndex];
            m_dragonPosition = evaluateCubicBezier(segment, t);
            movementDirection = evaluateCubicBezierTangent(segment, t);
        } else if (m_cameraMode != CameraMode::FreeCamera && !ImGui::GetIO().WantCaptureKeyboard) {

            const float steering = static_cast<float>(m_window.isKeyPressed(GLFW_KEY_A))
                - static_cast<float>(m_window.isKeyPressed(GLFW_KEY_D));
            const float turnSpeed = glm::radians(100.0f);
            m_dragonYaw = std::remainder(m_dragonYaw + steering * turnSpeed * m_deltaTime, glm::radians(360.0f));
            const glm::vec3 forward(-std::sin(m_dragonYaw), 0.0f, -std::cos(m_dragonYaw));
            const float forwardInput = static_cast<float>(m_window.isKeyPressed(GLFW_KEY_W))
                - static_cast<float>(m_window.isKeyPressed(GLFW_KEY_S));
            movementDirection = forward * forwardInput;
            if (m_window.isKeyPressed(GLFW_KEY_Q))
                movementDirection.y -= 1.0f;
            if (m_window.isKeyPressed(GLFW_KEY_E))
                movementDirection.y += 1.0f;

            if (glm::dot(movementDirection, movementDirection) > 0.0f) {
                constexpr float movementSpeed = 1.0f;
                m_dragonPosition += glm::normalize(movementDirection) * movementSpeed * m_deltaTime;
            }
        }

        if (m_flightMode == FlightMode::Autopilot
            && movementDirection.x * movementDirection.x + movementDirection.z * movementDirection.z > 1e-8f) {
            const float targetYaw = std::atan2(-movementDirection.x, -movementDirection.z);
            const float fullTurn = glm::radians(360.0f);
            const float yawDifference = std::remainder(targetYaw - m_dragonYaw, fullTurn);
            constexpr float turnResponse = 8.0f;
            const float blend = 1.0f - std::exp(-turnResponse * m_deltaTime);
            m_dragonYaw = std::remainder(m_dragonYaw + yawDifference * blend, fullTurn);
        }
        return m_flightMode == FlightMode::Manual && glm::dot(movementDirection, movementDirection) > 0.0f;
    }

    enum class FlightMode { Manual, Autopilot };
    FlightMode m_flightMode { FlightMode::Manual };
    float m_autopilotProgress { 0.0f };
    double m_autopilotElapsedSeconds { 0.0 };
    static constexpr double s_autopilotLoopSeconds = 15.0;

    struct FlightArcLengthSample {
        std::size_t segment;
        float t;
        double distance;
    };

    std::vector<FlightArcLengthSample> m_flightArcLengths;
    double m_totalFlightLength { 0.0 };

    void initializeFlightArcLengths()
    {
        constexpr int intervalsPerSegment = 256;
        m_flightArcLengths.clear();
        m_flightArcLengths.reserve(1 + m_flightPath.size() * intervalsPerSegment);
        m_totalFlightLength = 0.0;
        m_flightArcLengths.push_back({ 0, 0.0f, 0.0 });
        glm::vec3 previousPosition = m_flightPath.front().p0;
        for (std::size_t segment = 0; segment < m_flightPath.size(); ++segment) {
            for (int sample = 1; sample <= intervalsPerSegment; ++sample) {
                const float t = static_cast<float>(sample) / intervalsPerSegment;
                const glm::vec3 position = evaluateCubicBezier(m_flightPath[segment], t);
                m_totalFlightLength += static_cast<double>(glm::length(position - previousPosition));
                m_flightArcLengths.push_back({ segment, t, m_totalFlightLength });
                previousPosition = position;
            }
        }
    }

    float autopilotParameterAtDistance(double distance) const
    {
        if (m_flightArcLengths.empty() || m_totalFlightLength <= 0.0)
            return 0.0f;
        distance = std::clamp(distance, 0.0, m_totalFlightLength);
        const auto upper = std::lower_bound(m_flightArcLengths.begin(), m_flightArcLengths.end(), distance,
            [](const FlightArcLengthSample& sample, double target) { return sample.distance < target; });
        if (upper == m_flightArcLengths.begin())
            return 0.0f;
        const FlightArcLengthSample* nextSample;
        if (upper == m_flightArcLengths.end()) {
            nextSample = &m_flightArcLengths.back();
        } else {
            nextSample = &*upper;
        }
        const auto& next = *nextSample;
        const auto& previous = *(upper - 1);
        const double intervalLength = next.distance - previous.distance;
        double blend = 0.0;
        if (intervalLength > 0.0) {
            blend = (distance - previous.distance) / intervalLength;
        }

        const double from = static_cast<double>(previous.segment) + previous.t;
        const double to = static_cast<double>(next.segment) + next.t;
        const float parameter = static_cast<float>(from + blend * (to - from));

        return std::clamp(parameter, 0.0f, std::nextafter(static_cast<float>(m_flightPath.size()), 0.0f));
    }

    void initializeBezierLines()
    {

        std::vector<glm::vec3> positions;
        positions.reserve(m_flightPath.size() * s_bezierSamplesPerSegment);
        for (const CubicBezierSegment& segment : m_flightPath) {
            for (int sample = 0; sample < s_bezierSamplesPerSegment; ++sample) {
                const float t = static_cast<float>(sample) / static_cast<float>(s_bezierSamplesPerSegment - 1);
                positions.push_back(evaluateCubicBezier(segment, t));
            }
        }

        glGenVertexArrays(1, &m_bezierVao);
        glGenBuffers(1, &m_bezierVbo);
        glBindVertexArray(m_bezierVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_bezierVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(positions.size() * sizeof(glm::vec3)), positions.data(),
            GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(glm::vec3)), nullptr);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    static constexpr GLsizei s_bezierSamplesPerSegment = 65;
    GLuint m_bezierVao { 0 };
    GLuint m_bezierVbo { 0 };
    bool m_showBezierPath { true };
    bool m_showMinimap { true };
    bool m_showLavaLake { true };
    bool m_showLavaStreams { true };
    static constexpr int s_lavaFrameCount = 24;
    static constexpr double s_lavaFramesPerSecond = 12.0;
    std::array<GLuint, s_lavaFrameCount> m_lavaFrames {};
    double m_lavaAnimationSeconds { 0.0 };

    static constexpr glm::vec3 s_minFlightPosition { -8.0f, 0.0f, -8.0f };
    static constexpr glm::vec3 s_maxFlightPosition { 8.0f, 6.0f, 8.0f };
    bool m_constrainFlightToTerrain { true };

    enum class CameraMode { FollowDragon, FreeCamera, TopDown };
    CameraMode m_cameraMode { CameraMode::FollowDragon };
    float m_followOrbitYaw { 0.0f };
    float m_followPitch { glm::radians(25.0f) };
    float m_followDistance { 2.5f };
    glm::vec3 m_freeCameraPosition { -1.0f, 1.0f, -1.0f };
    float m_freeLookYaw { glm::radians(45.0f) };
    float m_freeLookPitch { -std::atan(1.0f / std::sqrt(2.0f)) };
    float m_pendingCameraScroll { 0.0f };
    bool m_cameraDragging { false };
    bool m_cameraLeftWasDown { false };
    std::array<bool, 3> m_cameraShortcutWasDown {};
    glm::vec2 m_previousCameraCursor { 0.0f };

    Window m_window;

    Shader m_defaultShader;
    Shader m_shadowShader;
    static constexpr GLsizei s_shadowMapSize = 2048;
    static constexpr glm::vec3 s_directionalLight { 0.5f, 1.0f, 0.6f };
    float m_timeOfDay { 12.0f }, m_lastLightingTime { -1.0f };
    bool m_automaticDayNight { false }, m_sunAboveHorizon { true };
    glm::vec3 m_sunDirection { glm::normalize(s_directionalLight) }, m_sunColor { 1.0f };
    float m_sunIntensity { 1.0f }, m_dayAmbient { 1.0f }, m_twilightAmount { 0.0f };
    glm::vec3 m_environmentTint { 1.0f }, m_twilightColor { 0.0f };
    GLuint m_shadowFramebuffer { 0 };
    GLuint m_shadowDepthTexture { 0 };
    glm::mat4 m_lightViewProjection { 1.0f };
    bool m_shadowMapReady { false };
    bool m_shadowMapping { true };
    bool m_shadowPcf { true };
    static constexpr GLsizei s_fireShadowMapSize = 1024;
    GLuint m_fireShadowFramebuffer { 0 }, m_fireShadowDepthTexture { 0 };
    bool m_fireShadowReady { false }, m_fireLight { true }, m_fireLightShadows { true }, m_fireActive { false };
    float m_fireLightIntensity { 7.f }, m_fireLightRange { 3.5f };
    double m_fireLightTime { 0 };
    glm::vec3 m_fireLightPosition { 0 }, m_fireLightDirection { 0, 0, -1 }, m_mouthGlowPosition { 0 };
    glm::mat4 m_fireViewProjection { 1 };
    Shader m_bezierShader;
    Shader m_bloomExtractShader, m_bloomBlurShader, m_bloomCombineShader;
    GLuint m_bloomVao { 0 }, m_bloomSceneFramebuffer { 0 }, m_bloomSceneTexture { 0 }, m_bloomDepth { 0 };
    std::array<GLuint, 2> m_bloomFramebuffers {}, m_bloomTextures {};
    glm::ivec2 m_bloomSize { 0 }, m_bloomHalfSize { 0 };
    GLint m_bloomDestinationDraw { 0 }, m_bloomDestinationRead { 0 }, m_bloomDestinationViewport[4] {};
    bool m_bloomShadersReady { false }, m_bloomTargetsReady { false }, m_bloomSceneActive { false };
    bool m_bloomEnabled { true };
    float m_bloomStrength { 0.20f };
    Shader m_waterShader;
    bool m_waterReady { false }, m_showWater { true };
    float m_waterWaveStrength { 0.018f }, m_waterWaveSpeed { 1.0f };
    double m_waterTime { 0.0 };
    Shader m_skyShader;
    GLuint m_skyVao { 0 };
    GLuint m_groundSurfaceTexture { 0 };
    bool m_skyReady { false };
    Shader m_particleUpdateShader, m_particleRenderShader;
    static constexpr int s_particleGridSize = 33;
    static constexpr int s_tailEmberCount = 64;
    static constexpr int s_particleCount = 1024 + s_tailEmberCount;
    std::array<GLuint, 2> m_particleFramebuffers {}, m_particlePositions {}, m_particleVelocities {};
    GLuint m_particleVao { 0 };
    GLuint m_particleStep { 0 };
    int m_particleReadState { 0 };
    bool m_particlesReady { false };
    bool m_particlesNeedReset { true };
    bool m_tailEmberTrail { true };
    bool m_fireBreathing { false };
    bool m_fireKeyWasDown { false };
    float m_mouthOpen { 0.0f };
    glm::mat3 m_mouthNormalMatrix { 1.0f };

    std::vector<GPUMesh> m_meshes;

    struct CollisionSample {
        glm::vec3 point;
        float skinZ;
        glm::vec3 mouthOffset { 0 };
        bool mouthMorph { false };
    };

    std::array<std::vector<glm::vec3>, 17> m_collisionGeometry;
    std::vector<CollisionSample> m_collisionSamples;
    std::vector<glm::vec3> m_collisionPose, m_originalCollisionSamples;
    bool m_collectCollisionSamples { false }, m_mountainCollision { true };

    bool m_collisionPoseDirty { true }, m_clearanceCached { false };
    int m_clearanceModel { -1 };
    std::array<std::size_t, 2> m_collisionWitness {};
    std::array<float, 8> m_clearanceAngles {};
    FlightBend m_clearanceBend {};
    float m_clearanceMouthOpen { 0.0f };
    glm::vec3 m_clearancePosition { 0 };
    float m_clearanceYaw { 0 }, m_cachedClearance { 0 };
    float m_collisionSkinZ { std::numeric_limits<float>::infinity() };
    static constexpr float s_collisionMargin = .04f;
    GPUMesh m_groundMesh;
    GPUMesh m_waterMesh;
    GPUMesh m_lavaMesh;
    GPUMesh m_lavaStreamsMesh;
    GPUMesh m_mountainMesh;
    std::vector<GPUMesh> m_landscapeDetails;
    GLuint m_mountainTexture { 0 };
    GPUMesh m_bodyMesh;
    GPUMesh m_headMesh;
    GPUMesh m_snoutMesh;
    GPUMesh m_jawMesh;
    GPUMesh m_throatMesh;
    GPUMesh m_faceDetailMesh;
    GPUMesh m_goldDetailMesh;
    GPUMesh m_lightDetailMesh;
    GPUMesh m_eyeMesh;
    GPUMesh m_hornMesh;
    GPUMesh m_whiskerMesh;
    GPUMesh m_browMesh, m_eyelidMesh, m_foreheadMesh;
    GPUMesh m_teethMesh;
    GPUMesh m_lowerTeethMesh;
    GPUMesh m_muzzleDetailsMesh;
    int m_dragonModel { 1 };
    std::array<glm::mat4, dragonBoneCount> m_inverseBindMatrices {};
    std::array<glm::mat4, dragonBoneCount> m_currentBoneMatrices {};
    std::array<glm::mat4, dragonBoneCount> m_skinMatrices {};
    std::array<float, 8> m_bodyJointAngles {};
    std::array<float, 8> m_manualBodyJointAngles {};
    bool m_animateDragonBody { true }, m_responsiveFlightAnimation { true };
    std::array<glm::vec2, 8> m_steeringResponse {};
    FlightBend m_flightBend {};
    double m_bodyAnimationTime { 0.0 };
    float m_bodyAnimationBlend { 1.0f };
    Texture m_texture;
    GLuint m_dragonScaleTexture { 0 };
    GLuint m_dragonNormalTexture { 0 };
    GLuint m_dragonRoughnessTexture { 0 };
    GLuint m_dragonMetallicTexture { 0 };
    bool m_usePbrPropertyMaps { true };
    GLuint m_environmentMap { 0 };
    float m_environmentMaxLod { 0.0f };
    bool m_environmentReflections { true };
    float m_reflectionStrength { 0.5f };
    bool m_useDragonNormalMapping { true };
    float m_dragonNormalStrength { 1.0f };
    bool m_useDragonScaleTexture { true };
    bool m_showGround { true };
    bool m_useMaterial { true };
    bool m_enableLighting { true };
    int m_shadingModel { 1 };
    float m_jadeRoughness { 0.22f };
    float m_goldRoughness { 0.28f };
    float m_ambientStrength { 0.25f };
    float m_diffuseStrength { 0.75f };
    float m_specularStrength { 0.35f };
    float m_lightingShininess { 32.0f };
    float m_deltaTime { 0.0f };
    glm::vec3 m_dragonPosition { 0.0f };
    float m_dragonYaw { 0.0f };

    std::array<CubicBezierSegment, 3> m_flightPath { { { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.2f, 0.0f),
                                                           glm::vec3(2.5f, 1.0f, 2.25f), glm::vec3(2.0f, 1.0f, 3.0f) },
        { glm::vec3(2.0f, 1.0f, 3.0f), glm::vec3(1.5f, 1.0f, 3.75f), glm::vec3(-1.5f, 0.7f, 3.75f),
            glm::vec3(-2.0f, 0.5f, 3.0f) },
        { glm::vec3(-2.0f, 0.5f, 3.0f), glm::vec3(-2.5f, 0.3f, 2.25f), glm::vec3(-1.0f, -0.2f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f) } } };

    glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 m_modelMatrix { 1.0f };
};

int main()
{
    Application app;
    app.update();

    return 0;
}
