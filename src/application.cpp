//#include "Image.h"
#include "mesh.h"
#include "texture.h"
// Always include window first (because it includes glfw, which includes GL which needs to be included AFTER glew).
// Can't wait for modules to fix this stuff...
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
// Include glad before glfw3
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
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

// Implementation source (B5.1): AI-generated with OpenAI Codex.
// Reference: Lecture 3 — Graphics Pipeline.
Mesh createDragonRevolvedMesh(const std::vector<glm::vec2>& profile, const glm::vec3& color,
    const glm::vec2& textureRepeat = glm::vec2(1.0f), float vOffset = 0.0f)
{
    // Profile entries are (radius, Z); eight radial sides surround the local Z axis.
    Mesh mesh {};
    mesh.material.kd = color;
    constexpr unsigned int sides = 8;
    auto addTriangle = [&mesh](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                           const glm::vec2& uvA, const glm::vec2& uvB, const glm::vec2& uvC) {
        const glm::vec3 cross = glm::cross(b - a, c - a);
        if (glm::dot(cross, cross) < 1e-12f)
            return; // At a pole, one half of a ring quad collapses.
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
            const glm::vec3 b(profile[ring].x * std::cos(nextAngle), profile[ring].x * std::sin(nextAngle), profile[ring].y);
            const glm::vec3 c(profile[ring + 1].x * std::cos(nextAngle), profile[ring + 1].x * std::sin(nextAngle), profile[ring + 1].y);
            const glm::vec3 d(profile[ring + 1].x * std::cos(angle), profile[ring + 1].x * std::sin(angle), profile[ring + 1].y);
            // C3 implementation source: AI-generated with OpenAI Codex.
            // Reference: Lecture 6 — Texture Mapping.
            // Independent triangle vertices retain flat normals and duplicate the -Y seam.
            const float u0 = static_cast<float>((side + 2) % sides) / sides * textureRepeat.x;
            const float u1 = u0 + textureRepeat.x / sides;
            const float midU = (u0 + u1) * 0.5f;
            const float v0 = profile[ring].y * textureRepeat.y + vOffset;
            const float v1 = profile[ring + 1].y * textureRepeat.y + vOffset;
            const glm::vec2 uvA(profile[ring].x == 0 ? midU : u0, v0);
            const glm::vec2 uvB(profile[ring].x == 0 ? midU : u1, v0);
            const glm::vec2 uvC(profile[ring + 1].x == 0 ? midU : u1, v1);
            const glm::vec2 uvD(profile[ring + 1].x == 0 ? midU : u0, v1);
            addTriangle(a, b, c, uvA, uvB, uvC);
            addTriangle(a, c, d, uvA, uvC, uvD);
        }
    }
    return mesh;
}

Mesh createGroundMesh()
{
    // Reference: Practical 1 — Intro to OpenGL, drawUnitFace().
    // Reference: Lecture 6 — Texture and Transparency, texture coordinates.
    Mesh ground {};
    ground.vertices = {
        { { -10.0f, -1.0f, -10.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
        { { -10.0f, -1.0f, 10.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 10.0f } },
        { { 10.0f, -1.0f, 10.0f }, { 0.0f, 1.0f, 0.0f }, { 10.0f, 10.0f } },
        { { 10.0f, -1.0f, -10.0f }, { 0.0f, 1.0f, 0.0f }, { 10.0f, 0.0f } }
    };
    ground.triangles = { { 0, 1, 2 }, { 0, 2, 3 } };
    ground.material.kd = glm::vec3(1.0f);
    // GPUMesh detects texture coordinates through kdTexture; drawing reuses m_texture.
    ground.material.kdTexture = std::make_shared<Image>(RESOURCE_ROOT "resources/checkerboard.png");
    return ground;
}

// Reference: Lecture 5 — Computer Animation, cubic and composite Bézier curves.
struct CubicBezierSegment {
    glm::vec3 p0;
    glm::vec3 p1;
    glm::vec3 p2;
    glm::vec3 p3;
};

glm::vec3 evaluateCubicBezier(const CubicBezierSegment& segment, float t)
{
    const float u = 1.0f - t;
    return u * u * u * segment.p0
        + 3.0f * u * u * t * segment.p1
        + 3.0f * u * t * t * segment.p2
        + t * t * t * segment.p3;
}

// Reference: Lecture 5 — Computer Animation, composite Bézier curves.
glm::vec3 evaluateCubicBezierTangent(const CubicBezierSegment& segment, float t)
{
    const float u = 1.0f - t;
    return 3.0f * u * u * (segment.p1 - segment.p0)
        + 6.0f * u * t * (segment.p2 - segment.p1)
        + 3.0f * t * t * (segment.p3 - segment.p2);
}

class Application {
public:
    Application()
        : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41)
        , m_groundMesh(createGroundMesh())
        // C3: four circumference wraps; one lengthwise tile per 0.11 world units.
        , m_bodySegmentMesh(createDragonRevolvedMesh({ { 0, -0.5f }, { 1, -0.35f }, { 1, 0.35f }, { 0, 0.5f } }, glm::vec3(0.12f, 0.55f, 0.3f), glm::vec2(4.0f, 0.15f / 0.11f)))
        , m_headMesh(createDragonRevolvedMesh({ { 0, -1 }, { 0.7071f, -0.7071f }, { 1, 0 }, { 0.7071f, 0.7071f }, { 0, 1 } }, glm::vec3(0.18f, 0.65f, 0.36f)))
        // Match the body texture density and phase at the tail attachment.
        , m_tailMesh(createDragonRevolvedMesh({ { 0, 0 }, { 1, 0 }, { 0, 1 } }, glm::vec3(0.12f, 0.55f, 0.3f), glm::vec2(4.0f, 0.18f / 0.11f), 0.04f / 0.11f))
        // B5.2: reuse the existing revolved primitive generator with detail materials.
        , m_goldDetailMesh(createDragonRevolvedMesh({ { 0, 0 }, { 1, 0 }, { 0, 1 } }, glm::vec3(0.95f, 0.74f, 0.3f)))
        , m_lightDetailMesh(createDragonRevolvedMesh({ { 0, -1 }, { 0.7071f, -0.7071f }, { 1, 0 }, { 0.7071f, 0.7071f }, { 0, 1 } }, glm::vec3(0.65f, 0.85f, 0.55f)))
        , m_eyeMesh(createDragonRevolvedMesh({ { 0, -1 }, { 0.7071f, -0.7071f }, { 1, 0 }, { 0.7071f, 0.7071f }, { 0, 1 } }, glm::vec3(0.025f, 0.04f, 0.03f)))
        , m_texture(RESOURCE_ROOT "resources/checkerboard.png")
    {
        m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                onKeyPressed(key, mods);
            else if (action == GLFW_RELEASE)
                onKeyReleased(key, mods);
        });
        m_window.registerScrollCallback([this](const glm::vec2& offset) {
            m_pendingCameraScroll += offset.y;
        });
        m_window.registerMouseMoveCallback(std::bind(&Application::onMouseMove, this, std::placeholders::_1));
        m_window.registerMouseButtonCallback([this](int button, int action, int mods) {
            if (action == GLFW_PRESS)
                onMouseClicked(button, mods);
            else if (action == GLFW_RELEASE)
                onMouseReleased(button, mods);
        });

        initializeFlightArcLengths();
        initializeDragonScaleTexture();
        initializeEnvironmentMap();
        initializeShadowMap();

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

            // Any new shaders can be added below in similar fashion.
            // ==> Don't forget to reconfigure CMake when you do!
            //     Visual Studio: PROJECT => Generate Cache for ComputerGraphics
            //     VS Code: ctrl + shift + p => CMake: Configure => enter
            // ....
        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }
    }

    ~Application()
    {
        // m_window is still alive, so the OpenGL context is valid here.
        glDeleteFramebuffers(1, &m_shadowFramebuffer);
        glDeleteTextures(1, &m_shadowDepthTexture);
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
        int dummyInteger = 0; // Initialized to 0
        // Reference: Lecture 5 — Computer Animation, time-dependent animation.
        auto previousFrameTime = std::chrono::steady_clock::now();
        while (!m_window.shouldClose()) {
            const auto currentFrameTime = std::chrono::steady_clock::now();
            m_deltaTime = std::chrono::duration<float>(currentFrameTime - previousFrameTime).count();
            previousFrameTime = currentFrameTime;

            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();
            // B10 implementation source: AI-generated with OpenAI Codex.
            // Use the same mode state as the dropdown; consume edges even while typing.
            for (int shortcut = 0; shortcut < 3; ++shortcut) {
                const bool down = m_window.isKeyPressed(GLFW_KEY_1 + shortcut);
                if (down && !m_cameraShortcutWasDown[shortcut] && !ImGui::GetIO().WantCaptureKeyboard) {
                    const auto mode = static_cast<CameraMode>(shortcut);
                    if (mode != m_cameraMode) {
                        m_cameraMode = mode;
                        m_cameraDragging = false;
                    }
                }
                m_cameraShortcutWasDown[shortcut] = down;
            }

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            // Reference: Practical 1 — Intro to OpenGL, "Background: ImGui".
            ImGui::Begin("Dragon Tech Demo");
            ImGui::Text("Flight Controls");
            if (ImGui::Checkbox("Animate Dragon Body", &m_animateDragonBody) && m_animateDragonBody)
                m_bodyAnimationBlend = 0.0f;
            const char* flightModes[] = { "Manual", "Autopilot" };
            int selectedFlightMode = static_cast<int>(m_flightMode);
            const bool flightModeChanged = ImGui::Combo("Flight Mode", &selectedFlightMode, flightModes, 2);
            if (flightModeChanged) {
                m_flightMode = static_cast<FlightMode>(selectedFlightMode);
                if (m_flightMode == FlightMode::Autopilot) {
                    m_autopilotElapsedSeconds = 0.0;
                    m_autopilotProgress = 0.0f;
                    m_dragonPosition = m_flightPath.front().p0;
                }
            }
            ImGui::Text("Entering Autopilot restarts at the first path point.");
            if (m_flightMode == FlightMode::Manual) {
                ImGui::Text("W/S: Forward/Backward | A/D: Turn Left/Right | Q/E: Down/Up");
                if (m_cameraMode == CameraMode::FreeCamera)
                    ImGui::Text("Free Camera controls the camera; manual dragon flight is paused.");
            } else {
                ImGui::Text("Autopilot ignores WASD/Q/E; bounds apply to Manual.");
            }
            const bool constraintsChanged = ImGui::Checkbox("Constrain Flight to Terrain", &m_constrainFlightToTerrain);
            const bool resetDragon = ImGui::Button("Reset Dragon Position");
            if (m_flightMode == FlightMode::Autopilot) {
                const int segment = static_cast<int>(m_autopilotProgress);
                ImGui::Text("Segment %d/3 | t: %.3f | 15 s/loop", segment + 1, m_autopilotProgress - static_cast<float>(segment));
            }
            ImGui::Separator();

            ImGui::Text("Camera Controls");
            const char* cameraModes[] = { "Follow Dragon", "Free Camera", "Top-Down" };
            int selectedCamera = static_cast<int>(m_cameraMode);
            if (ImGui::Combo("Camera mode", &selectedCamera, cameraModes, 3)) {
                m_cameraMode = static_cast<CameraMode>(selectedCamera);
                m_cameraDragging = false;
            }
            if (m_cameraMode == CameraMode::FollowDragon)
                ImGui::Text("Hold LMB: Orbit around dragon | Wheel: Zoom");
            else if (m_cameraMode == CameraMode::FreeCamera)
                ImGui::Text("WASD: Move camera | Q/E: Down/Up | Hold LMB: Look around");
            else
                ImGui::Text("WASD/QE: Control dragon");
            ImGui::Text("1: Follow | 2: Free | 3: Top-Down");
            if (ImGui::Button("Reset Camera View"))
                resetCameraView();
            ImGui::Separator();

            ImGui::Text("Graphics Effects");
            ImGui::Checkbox("Show Ground", &m_showGround);
            ImGui::Checkbox("Show B\xC3\xA9zier Path", &m_showBezierPath);
            ImGui::Checkbox("Use material if no texture", &m_useMaterial);
            ImGui::Checkbox("Dragon Scale Texture", &m_useDragonScaleTexture);
            ImGui::Checkbox("Material Property Maps", &m_usePbrPropertyMaps);
            ImGui::Checkbox("Dragon Normal Mapping", &m_useDragonNormalMapping);
            ImGui::SliderFloat("Normal Strength", &m_dragonNormalStrength, 0.0f, 2.0f);
            ImGui::Checkbox("Environment Reflections", &m_environmentReflections);
            ImGui::SliderFloat("Reflection Strength", &m_reflectionStrength, 0.0f, 1.0f);
            ImGui::Checkbox("Shadow Mapping", &m_shadowMapping);
            ImGui::Checkbox("PCF Filtering", &m_shadowPcf);
            // Implementation source (C1): AI-generated with OpenAI Codex.
            // Reference: Lecture 4 — Materials and Shading.
            ImGui::Checkbox("Enable Lighting", &m_enableLighting);
            // Implementation source (C2): AI-generated with OpenAI Codex.
            // Reference: Lecture 9 — Advanced Materials, microfacet BRDF.
            const char* shadingModels[] = { "Blinn-Phong", "PBR" };
            ImGui::Combo("Shading Model", &m_shadingModel, shadingModels, 2);
            if (m_shadingModel == 0) {
                ImGui::Text("Blinn-Phong controls");
                ImGui::SliderFloat("Ambient Strength", &m_ambientStrength, 0.0f, 1.0f);
                ImGui::SliderFloat("Diffuse Strength", &m_diffuseStrength, 0.0f, 1.0f);
                ImGui::SliderFloat("Specular Strength", &m_specularStrength, 0.0f, 1.0f);
                ImGui::SliderFloat("Shininess", &m_lightingShininess, 1.0f, 128.0f);
            } else {
                ImGui::Text("PBR materials: nonmetallic jade / metallic gold");
                ImGui::SliderFloat("Jade Roughness", &m_jadeRoughness, 0.08f, 1.0f);
                ImGui::SliderFloat("Gold Roughness", &m_goldRoughness, 0.08f, 1.0f);
            }
            ImGui::Separator();

            if (ImGui::CollapsingHeader("Debug")) {
                const char* dragonModels[] = { "Original Dragon", "Segmented Dragon" };
                ImGui::Combo("Dragon model", &m_dragonModel, dragonModels, 2);
                ImGui::Text("Manual joint sliders apply only when body animation is off.");
                ImGui::BeginDisabled(m_animateDragonBody);
                ImGui::SliderAngle("Body Joint 1 Angle", &m_manualBodyJointAngles[0], -30.0f, 30.0f);
                ImGui::SliderAngle("Body Joint 4 Angle", &m_manualBodyJointAngles[3], -30.0f, 30.0f);
                ImGui::EndDisabled();
                ImGui::Text("Dragon X position: %.3f", m_dragonPosition.x);
                ImGui::Text("Dragon Y position: %.3f", m_dragonPosition.y);
                ImGui::Text("Dragon Z position: %.3f", m_dragonPosition.z);
                ImGui::Text("Delta time: %.3f ms", m_deltaTime * 1000.0f);
                ImGui::Text("FPS: %.1f", m_deltaTime > 0.0f ? 1.0f / m_deltaTime : 0.0f);
                ImGui::InputInt("This is an integer input", &dummyInteger); // Use ImGui::DragInt or ImGui::DragFloat for larger range of numbers.
                ImGui::Text("Value is: %i", dummyInteger); // Use C printf formatting rules (%i is a signed integer)
            }
            ImGui::End();
            // Apply camera selection before flight input, including Free Camera + Manual.
            const bool manualMoved = !flightModeChanged && updateDragonFlight();
            // Preserve the handoff position until Manual movement or a boundary change.
            if (m_flightMode == FlightMode::Manual && m_constrainFlightToTerrain && (manualMoved || constraintsChanged))
                m_dragonPosition = glm::clamp(m_dragonPosition, s_minFlightPosition, s_maxFlightPosition);
            if (resetDragon) {
                m_autopilotElapsedSeconds = 0.0;
                m_autopilotProgress = 0.0f;
                m_dragonPosition = m_flightMode == FlightMode::Autopilot ? m_flightPath.front().p0 : glm::vec3(0.0f);
                m_dragonYaw = 0.0f;
                m_bodyAnimationTime = 0.0;
                m_bodyAnimationBlend = 1.0f;
            }
            updateDragonBodyAnimation();

            // Reference: Lecture 2 — Transformations and Projections.
            m_modelMatrix = glm::translate(glm::mat4(1.0f), m_dragonPosition);
            m_modelMatrix = glm::rotate(m_modelMatrix, m_dragonYaw, glm::vec3(0, 1, 0)); // T * R: rotate locally, then translate.

            updateCamera();
            if (m_shadowMapping && m_shadowMapReady && m_enableLighting)
                renderShadowMap();
            // Reference: Lecture 3 — OpenGL Graphics Pipeline.
            // inverse(view) places the active eye in world space in every camera mode.
            const glm::vec3 cameraPosition = glm::vec3(glm::inverse(m_viewMatrix)[3]);
            m_defaultShader.bind();
            glUniform3fv(m_defaultShader.getUniformLocation("cameraPosition"), 1, glm::value_ptr(cameraPosition));
            // Direction from a surface toward the single white world-space light.
            glUniform3fv(m_defaultShader.getUniformLocation("lightDirection"), 1, glm::value_ptr(s_directionalLight));
            glUniform1i(m_defaultShader.getUniformLocation("enableLighting"), m_enableLighting);
            glUniform1i(m_defaultShader.getUniformLocation("shadingModel"), m_shadingModel);
            glUniform1f(m_defaultShader.getUniformLocation("ambientStrength"), m_ambientStrength);
            glUniform1f(m_defaultShader.getUniformLocation("diffuseStrength"), m_diffuseStrength);
            glUniform1f(m_defaultShader.getUniformLocation("specularStrength"), m_specularStrength);
            glUniform1f(m_defaultShader.getUniformLocation("lightingShininess"), m_lightingShininess);

            glActiveTexture(GL_TEXTURE3);
            const bool shadows = m_shadowMapping && m_shadowMapReady && m_enableLighting;
            glBindTexture(GL_TEXTURE_2D, shadows ? m_shadowDepthTexture : 0);
            glUniform1i(m_defaultShader.getUniformLocation("shadowMap"), 3);
            glUniform1i(m_defaultShader.getUniformLocation("enableShadows"), shadows);
            glUniform1i(m_defaultShader.getUniformLocation("enableShadowPcf"), m_shadowPcf);
            glUniformMatrix4fv(m_defaultShader.getUniformLocation("lightViewProjection"), 1, GL_FALSE, glm::value_ptr(m_lightViewProjection));
            glActiveTexture(GL_TEXTURE0);

            // Clear the screen
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // ...
            glEnable(GL_DEPTH_TEST);

            const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * m_modelMatrix;
            // Normals should be transformed differently than positions (ignoring translations + dealing with scaling):
            // https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
            const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));

            // Reference: Lecture 3 — Graphics Pipeline with Modern OpenGL.
            if (m_showGround) {
                const glm::mat4 groundModelMatrix(1.0f);
                const glm::mat4 groundMvpMatrix = m_projectionMatrix * m_viewMatrix * groundModelMatrix;
                const glm::mat3 groundNormalMatrix(1.0f);
                m_defaultShader.bind();
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(groundMvpMatrix));
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(groundModelMatrix));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(groundNormalMatrix));
                m_texture.bind(GL_TEXTURE0);
                glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), m_groundMesh.hasTextureCoords());
                glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
                setPbrMaterial(0.0f, 0.8f);
                setPbrPropertyMaps(false);
                setEnvironmentReflections(false);
                setDragonNormalMapping(false);
                m_groundMesh.draw(m_defaultShader);
            }

            if (m_dragonModel == 1) {
                drawSegmentedDragon();
            } else {
                for (GPUMesh& mesh : m_meshes) {
                    m_defaultShader.bind();
                    glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                    glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
                    glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModelMatrix));
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
                    setPbrMaterial(0.0f, 0.4f); // Original Dragon fallback, retaining its existing base color.
                    setEnvironmentReflections(false);
                    setDragonNormalMapping(false);
                    mesh.draw(m_defaultShader);
                }
            }

            if (m_showBezierPath) {
                const glm::mat4 pathMvpMatrix = m_projectionMatrix * m_viewMatrix;
                const std::array<glm::vec3, 3> colors {{
                    { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }
                }};
                m_bezierShader.bind();
                glUniformMatrix4fv(m_bezierShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(pathMvpMatrix));
                glBindVertexArray(m_bezierVao);
                for (GLint segment = 0; segment < 3; ++segment) {
                    glUniform3fv(m_bezierShader.getUniformLocation("pathColor"), 1, glm::value_ptr(colors[segment]));
                    glDrawArrays(GL_LINE_STRIP, segment * s_bezierSamplesPerSegment, s_bezierSamplesPerSegment);
                }
                glBindVertexArray(0);
            }

            // Processes input and swaps the window buffer
            m_window.swapBuffers();
        }
    }

    // In here you can handle key presses
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyPressed(int key, int mods)
    {
        std::cout << "Key pressed: " << key << std::endl;
    }

    // In here you can handle key releases
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyReleased(int key, int mods)
    {
        std::cout << "Key released: " << key << std::endl;
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }

    // If one of the mouse buttons is pressed this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseClicked(int button, int mods)
    {
        std::cout << "Pressed mouse button: " << button << std::endl;
    }

    // If one of the mouse buttons is released this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseReleased(int button, int mods)
    {
        std::cout << "Released mouse button: " << button << std::endl;
    }

private:
    // C3 implementation source: AI-generated with OpenAI Codex.
    // Reference: Lecture 6 — Texture Mapping.
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
                // Four staggered scallop rows tile in both axes, including the borders.
                for (int candidate = row - 1; candidate <= row + 1; ++candidate) {
                    const float across = 2.0f * glm::fract(4.0f * u + 0.5f * (candidate & 1)) - 1.0f;
                    const float edge = candidate + 0.5f + 0.5f * std::sqrt(glm::max(0.0f, 1.0f - across * across));
                    const float distance = rowPosition - edge;
                    if (std::abs(distance) < std::abs(nearest))
                        nearest = distance;
                }
                // C4 implementation source: AI-generated with OpenAI Codex.
                // Reference: Lecture 6 — Texture Mapping, normal and bump maps.
                // Raised interiors with a smooth recess at the same C3 scallop boundaries.
                const float edgeDistance = nearest / 0.13f;
                heights[y * size + x] = 1.0f - std::exp(-edgeDistance * edgeDistance);
                // C6 implementation source: AI-generated with OpenAI Codex.
                // Reference: Lecture 6 — Texture Mapping; Lecture 9 — Advanced Materials.
                // Rough recessed gaps, smoother interiors; sparse metallic arcs on scale rims.
                const float roughness = 0.20f + 0.50f * (1.0f - heights[y * size + x])
                    + 0.025f * std::sin(tau * 4.0f * u) * std::sin(tau * 4.0f * v);
                const float rim = 1.0f - glm::smoothstep(0.02f, 0.07f, std::abs(nearest - 0.12f));
                const float accent = std::pow(glm::max(std::sin(tau * 4.0f * u), 0.0f), 6.0f);
                roughnessPixels[y * size + x] = static_cast<unsigned char>(glm::clamp(roughness, 0.08f, 1.0f) * 255.0f + 0.5f);
                metallicPixels[y * size + x] = static_cast<unsigned char>(0.9f * rim * accent * 255.0f + 0.5f);
                const float boundary = 1.0f - glm::smoothstep(0.015f, 0.065f, std::abs(nearest));
                const float shade = 0.98f + 0.06f * std::sin(tau * 4.0f * u) * std::sin(tau * 4.0f * v)
                    + 0.025f * std::cos(tau * 4.0f * v);
                glm::vec3 color = glm::mix(glm::vec3(0.12f, 0.55f, 0.30f) * shade,
                    glm::vec3(0.08f, 0.37f, 0.20f), boundary * 0.75f);
                const float highlight = (nearest - 0.07f) / 0.045f;
                color = glm::mix(color, glm::vec3(0.20f, 0.64f, 0.37f), 0.18f * std::exp(-highlight * highlight));
                const int offset = (y * size + x) * 4;
                for (int channel = 0; channel < 3; ++channel)
                    pixels[offset + channel] = static_cast<unsigned char>(glm::clamp(color[channel], 0.0f, 1.0f) * 255.0f + 0.5f);
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
        // Encoded color bytes match the checkerboard; C2 decodes sRGB only in PBR.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        // C4: central differences wrap around the tile; +U/+V are tangent X/Y.
        std::vector<unsigned char> normalPixels(size * size * 4);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const float du = (heights[y * size + (x + 1) % size] - heights[y * size + (x + size - 1) % size]) * (size * 0.5f);
                const float dv = (heights[((y + 1) % size) * size + x] - heights[((y + size - 1) % size) * size + x]) * (size * 0.5f);
                const glm::vec3 normal = glm::normalize(glm::vec3(-0.008f * du, -0.008f * dv, 1.0f));
                const int offset = (y * size + x) * 4;
                for (int channel = 0; channel < 3; ++channel)
                    normalPixels[offset + channel] = static_cast<unsigned char>((normal[channel] * 0.5f + 0.5f) * 255.0f + 0.5f);
                normalPixels[offset + 3] = 255;
            }
        }
        glGenTextures(1, &m_dragonNormalTexture);
        glBindTexture(GL_TEXTURE_2D, m_dragonNormalTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        // Vector components are linear data, never an sRGB texture format.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, normalPixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        // Scalar maps use linear R8 data, with the same tiling/filtering as C3/C4.
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

    // D2 implementation source: AI-generated with OpenAI Codex.
    // Reference: Lecture 7 — Real-Time Shadows; Exercise 4 — Shadow Mapping.
    void initializeShadowMap()
    {
        // Ground extent +/-10, manual flight bounds, curve hull, and a 1.25-unit dragon envelope.
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
        const glm::mat4 lightView = glm::lookAt(center + glm::normalize(s_directionalLight) * (glm::length(sceneMax - sceneMin) + 1.0f), center, glm::vec3(0, 1, 0));
        glm::vec3 lightMin(1e10f), lightMax(-1e10f);
        for (int corner = 0; corner < 8; ++corner) {
            const glm::vec3 point((corner & 1) ? sceneMax.x : sceneMin.x,
                (corner & 2) ? sceneMax.y : sceneMin.y, (corner & 4) ? sceneMax.z : sceneMin.z);
            const glm::vec3 lightPoint(lightView * glm::vec4(point, 1.0f));
            lightMin = glm::min(lightMin, lightPoint);
            lightMax = glm::max(lightMax, lightPoint);
        }
        // Fit all eight corners once; independent of the viewing camera.
        m_lightViewProjection = glm::ortho(lightMin.x - 0.5f, lightMax.x + 0.5f,
            lightMin.y - 0.5f, lightMax.y + 0.5f, -lightMax.z - 0.5f, -lightMin.z + 0.5f) * lightView;

        GLint previousDraw = 0, previousRead = 0, previousTexture = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glGenTextures(1, &m_shadowDepthTexture);
        glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, s_shadowMapSize, s_shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE); // Compare each PCF tap in GLSL.
        const float border[] = { 1, 1, 1, 1 };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glGenFramebuffers(1, &m_shadowFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowDepthTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        m_shadowMapReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (!m_shadowMapReady)
            std::cerr << "Shadow framebuffer incomplete; shadows disabled." << std::endl;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDraw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, previousRead);
        glBindTexture(GL_TEXTURE_2D, previousTexture);
    }

    void drawShadowCaster(GPUMesh& mesh, const glm::mat4& modelMatrix)
    {
        m_shadowShader.bind();
        glUniformMatrix4fv(m_shadowShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        // GPUMesh::draw uses its Shader argument only for Material UBO binding, without
        // changing the active program. Reuse that valid block; the depth shader stays bound.
        mesh.draw(m_defaultShader);
    }

    void renderShadowMap()
    {
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
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, 0); // Do not leave the depth attachment bound for sampling.
        glActiveTexture(GL_TEXTURE0);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);
        glViewport(0, 0, s_shadowMapSize, s_shadowMapSize);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
        // Reference: Lecture 7 — Real-Time Shadows, rasterizer slope-based depth bias.
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        m_shadowShader.bind();
        glUniformMatrix4fv(m_shadowShader.getUniformLocation("lightViewProjection"), 1, GL_FALSE, glm::value_ptr(m_lightViewProjection));
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

    // C5 implementation source: AI-generated with OpenAI Codex.
    // Reference: Lecture 6 — Texture Mapping, environment maps and mipmaps.
    void initializeEnvironmentMap()
    {
        constexpr int size = 128;
        m_environmentMaxLod = std::log2(static_cast<float>(size));
        // OpenGL +X, -X, +Y, -Y, +Z, -Z faces: direction = center + s*right + t*down.
        // s/t increase with the uploaded texel column/row; rows are not image-flipped.
        const std::array<glm::vec3, 6> centers {{ { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }};
        const std::array<glm::vec3, 6> right {{ { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } }};
        const std::array<glm::vec3, 6> down {{ { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } }};
        const std::array<glm::vec3, 3> brightDirections {{ glm::normalize(glm::vec3(0.7f, 0.5f, -0.4f)),
            glm::normalize(glm::vec3(-0.8f, 0.25f, 0.5f)), glm::normalize(glm::vec3(0.0f, 0.1f, -1.0f)) }};
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
                    const glm::vec3 sky = glm::mix(glm::vec3(0.42f, 0.55f, 0.68f), glm::vec3(0.08f, 0.22f, 0.50f), glm::max(direction.y, 0.0f));
                    const glm::vec3 ground = glm::mix(glm::vec3(0.12f, 0.10f, 0.075f), glm::vec3(0.035f, 0.045f, 0.06f), glm::max(-direction.y, 0.0f));
                    glm::vec3 radiance = glm::mix(ground, sky, glm::smoothstep(-0.08f, 0.12f, direction.y));
                    // Broad distant bright regions, evaluated from the same world direction on every face.
                    radiance += glm::vec3(1.5f, 1.35f, 1.1f) * std::pow(glm::max(glm::dot(direction, brightDirections[0]), 0.0f), 32.0f);
                    radiance += glm::vec3(0.65f, 0.80f, 1.0f) * std::pow(glm::max(glm::dot(direction, brightDirections[1]), 0.0f), 48.0f);
                    radiance += glm::vec3(0.80f, 0.70f, 0.50f) * std::pow(glm::max(glm::dot(direction, brightDirections[2]), 0.0f), 64.0f);
                    const int offset = (y * size + x) * 3;
                    for (int channel = 0; channel < 3; ++channel)
                        pixels[offset + channel] = radiance[channel];
                }
            }
            // Authored linear radiance, stored as floating-point RGB; no sRGB conversion.
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, pixels.data());
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
        glBindTexture(GL_TEXTURE_CUBE_MAP, enabled ? m_environmentMap : 0);
        glUniform1i(m_defaultShader.getUniformLocation("environmentMap"), 2);
        glUniform1i(m_defaultShader.getUniformLocation("enableEnvironmentReflections"), enabled);
        glUniform1f(m_defaultShader.getUniformLocation("reflectionStrength"), m_reflectionStrength);
        glUniform1f(m_defaultShader.getUniformLocation("environmentMaxLod"), m_environmentMaxLod);
        glActiveTexture(GL_TEXTURE0);
    }

    // Implementation source (B8): AI-generated with OpenAI Codex.
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
        // B10: decide ownership only on the initial press, including blank UI areas.
        // An outside drag stays owned by the camera until release, even across the UI.
        const bool startDrag = leftDown && !m_cameraLeftWasDown
            && !io.WantCaptureMouse && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)
            && m_cameraMode != CameraMode::TopDown;
        if (startDrag)
            m_cameraDragging = true;
        if (m_cameraDragging && !startDrag) {
            constexpr float sensitivity = 0.0035f; // Radians per pixel; independent of frame time.
            // B10 implementation source: AI-generated with OpenAI Codex.
            // Reference: Exercise 4 — Shadow Mapping, camera implementation.
            // Window cursor coordinates have their origin at the bottom left.
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
        m_pendingCameraScroll = 0.0f; // Discard scroll in other modes or over ImGui.

        // Reference: Lecture 2 — Transformations and Projections.
        // Reference: Lecture 3 — Graphics Pipeline, model-view transformations.
        // Reference: Exercise 4 — Shadow Mapping, camera implementation.
        // Adapt its lookAt construction; orbit coordinates are this project's design.
        switch (m_cameraMode) {
        case CameraMode::FreeCamera: {
            const glm::vec3 forward(std::cos(m_freeLookPitch) * std::sin(m_freeLookYaw),
                std::sin(m_freeLookPitch), std::cos(m_freeLookPitch) * std::cos(m_freeLookYaw));
            // Reference: Exercise 4 — Shadow Mapping, camera implementation.
            // Adapt its forward/right movement with elapsed seconds and world-Y ascent.
            if (!io.WantCaptureKeyboard) {
                const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
                glm::vec3 movement(0.0f);
                if (m_window.isKeyPressed(GLFW_KEY_W)) movement += forward;
                if (m_window.isKeyPressed(GLFW_KEY_S)) movement -= forward;
                if (m_window.isKeyPressed(GLFW_KEY_A)) movement -= right;
                if (m_window.isKeyPressed(GLFW_KEY_D)) movement += right;
                if (m_window.isKeyPressed(GLFW_KEY_Q)) movement.y -= 1.0f;
                if (m_window.isKeyPressed(GLFW_KEY_E)) movement.y += 1.0f;
                if (glm::dot(movement, movement) > 0.0f) {
                    constexpr float cameraSpeed = 2.5f; // World units per second.
                    m_freeCameraPosition += glm::normalize(movement) * cameraSpeed * m_deltaTime;
                }
            }
            m_viewMatrix = glm::lookAt(m_freeCameraPosition, m_freeCameraPosition + forward, glm::vec3(0, 1, 0));
            break;
        }
        case CameraMode::FollowDragon: {
            const float yaw = m_dragonYaw + m_followOrbitYaw;
            const glm::vec3 offset = m_followDistance * glm::vec3(std::cos(m_followPitch) * std::sin(yaw),
                std::sin(m_followPitch), std::cos(m_followPitch) * std::cos(yaw));
            m_viewMatrix = glm::lookAt(m_dragonPosition + offset, m_dragonPosition, glm::vec3(0, 1, 0));
            break;
        }
        case CameraMode::TopDown:
            m_viewMatrix = glm::lookAt(m_dragonPosition + glm::vec3(0, 3, 0), m_dragonPosition, glm::vec3(0, 0, -1));
            break;
        }
    }

    // Implementation source (B6): AI-generated with OpenAI Codex.
    // Reference: Lecture 5 — Computer Animation, time-dependent animation.
    void updateDragonBodyAnimation()
    {
        m_bodyJointAngles = m_manualBodyJointAngles;
        if (!m_animateDragonBody || m_dragonModel != 1)
            return;

        m_bodyAnimationTime += static_cast<double>(m_deltaTime);
        // Traveling-wave parameters are the project's design, not lecture-provided values.
        // B8.1 (OpenAI Codex): grow the local wave toward the tail for rear-view visibility.
        const float frontAmplitude = glm::radians(4.0f);
        const float rearAmplitude = glm::radians(16.0f);
        constexpr double frequency = 0.8; // Cycles per second.
        constexpr double phaseOffset = 0.5; // Radians between adjacent joints.
        const double phase = glm::radians(360.0) * frequency * m_bodyAnimationTime;
        m_bodyAnimationBlend = glm::min(1.0f, m_bodyAnimationBlend + m_deltaTime / 0.25f);
        const float blend = glm::smoothstep(0.0f, 1.0f, m_bodyAnimationBlend);
        for (std::size_t joint = 0; joint < m_bodyJointAngles.size(); ++joint) {
            const float alongBody = static_cast<float>(joint) / static_cast<float>(m_bodyJointAngles.size() - 1);
            const float amplitude = glm::mix(frontAmplitude, rearAmplitude, alongBody);
            const float waveAngle = amplitude * static_cast<float>(std::sin(phase - static_cast<double>(joint) * phaseOffset));
            m_bodyJointAngles[joint] = glm::mix(m_manualBodyJointAngles[joint], waveAngle, blend);
        }
    }

    // Implementation source (C2): AI-generated with OpenAI Codex.
    enum class DragonMaterial { Jade, Gold, Pale, Eye };

    void setPbrMaterial(float metallic, float roughness)
    {
        // The mesh Material.kd (or existing texture) remains the base color.
        glUniform1f(m_defaultShader.getUniformLocation("pbrMetallic"), metallic);
        glUniform1f(m_defaultShader.getUniformLocation("pbrRoughness"), roughness);
    }

    // C6: compatible scale UVs only; material uniforms remain the fallback.
    bool setPbrPropertyMaps(bool eligible)
    {
        const bool enabled = eligible && m_usePbrPropertyMaps && m_shadingModel == 1;
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, enabled ? m_dragonRoughnessTexture : 0);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, enabled ? m_dragonMetallicTexture : 0);
        glUniform1i(m_defaultShader.getUniformLocation("roughnessMap"), 4);
        glUniform1i(m_defaultShader.getUniformLocation("metallicMap"), 5);
        glUniform1i(m_defaultShader.getUniformLocation("usePbrPropertyMaps"), enabled);
        glActiveTexture(GL_TEXTURE0);
        return enabled;
    }

    // C4 implementation source: AI-generated with OpenAI Codex.
    // Reference: Lecture 3 — OpenGL Graphics Pipeline.
    void setDragonNormalMapping(bool eligible)
    {
        const bool enabled = eligible && m_useDragonNormalMapping;
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, enabled ? m_dragonNormalTexture : 0);
        glUniform1i(m_defaultShader.getUniformLocation("normalMap"), 1);
        glUniform1i(m_defaultShader.getUniformLocation("useNormalMap"), enabled);
        glUniform1f(m_defaultShader.getUniformLocation("normalStrength"), m_dragonNormalStrength);
        glActiveTexture(GL_TEXTURE0); // Preserve color-map binding conventions.
    }

    void drawDragonPart(GPUMesh& mesh, const glm::mat4& modelMatrix, DragonMaterial material, bool shadowPass = false)
    {
        if (shadowPass) {
            drawShadowCaster(mesh, modelMatrix);
            return;
        }
        const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * modelMatrix;
        const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(modelMatrix));
        m_defaultShader.bind();
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
        // Reference: Lecture 2 — Transformations and normal matrices.
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
        glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
        // Reference: Lecture 3 — OpenGL Graphics Pipeline.
        // Reference: Lecture 9 — Advanced Materials, PBR.
        // Explicit per-draw selection keeps decorations and their material presets intact.
        const bool scaleSurface = &mesh == &m_bodySegmentMesh || &mesh == &m_tailMesh;
        const bool textured = m_useDragonScaleTexture && scaleSurface;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textured ? m_dragonScaleTexture : 0);
        glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), textured);
        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
        switch (material) {
        case DragonMaterial::Jade: setPbrMaterial(0.0f, m_jadeRoughness); break;
        case DragonMaterial::Gold: setPbrMaterial(1.0f, m_goldRoughness); break;
        case DragonMaterial::Pale: setPbrMaterial(0.0f, 0.45f); break;
        case DragonMaterial::Eye: setPbrMaterial(0.0f, 0.12f); break;
        }
        const bool propertyMapped = setPbrPropertyMaps(scaleSurface);
        setEnvironmentReflections(material == DragonMaterial::Gold || propertyMapped);
        setDragonNormalMapping(scaleSurface);
        mesh.draw(m_defaultShader);
    }

    // Implementation source (B5.2): AI-generated with OpenAI Codex.
    // Reference: Practical 1 — Intro to OpenGL, primitive geometry.
    // Reference: Lecture 2 — Transformations and Projections.
    void drawDragonHeadDetails(const glm::mat4& headWorld, const glm::mat4& snoutWorld, bool shadowPass)
    {
        const glm::mat4 jawWorld = glm::translate(snoutWorld, glm::vec3(0, -0.045f, -0.015f));
        drawDragonPart(m_lightDetailMesh, glm::scale(jawWorld, glm::vec3(0.085f, 0.028f, 0.125f)), DragonMaterial::Pale, shadowPass);
        for (float side : { -1.0f, 1.0f }) {
            glm::mat4 hornWorld = glm::translate(headWorld, glm::vec3(side * 0.09f, 0.075f, 0.025f));
            hornWorld = glm::rotate(hornWorld, glm::radians(-65.0f), glm::vec3(1, 0, 0));
            hornWorld = glm::rotate(hornWorld, glm::radians(side * 15.0f), glm::vec3(0, 1, 0));
            drawDragonPart(m_goldDetailMesh, glm::scale(hornWorld, glm::vec3(0.035f, 0.035f, 0.2f)), DragonMaterial::Gold, shadowPass);

            const glm::mat4 eyeWorld = glm::translate(headWorld, glm::vec3(side * 0.112f, 0.058f, -0.065f));
            drawDragonPart(m_eyeMesh, glm::scale(eyeWorld, glm::vec3(0.027f, 0.03f, 0.035f)), DragonMaterial::Eye, shadowPass);

            // Two overlapping thin ellipsoids make each whisker bend outward and forward.
            glm::mat4 whiskerWorld = glm::translate(snoutWorld, glm::vec3(side * 0.07f, 0, -0.06f));
            whiskerWorld = glm::rotate(whiskerWorld, glm::radians(side * 135.0f), glm::vec3(0, 1, 0));
            drawDragonPart(m_lightDetailMesh, glm::scale(glm::translate(whiskerWorld, glm::vec3(0, 0, 0.08f)), glm::vec3(0.008f, 0.008f, 0.09f)), DragonMaterial::Pale, shadowPass);
            glm::mat4 whiskerTipWorld = glm::translate(whiskerWorld, glm::vec3(0, 0, 0.16f));
            whiskerTipWorld = glm::rotate(whiskerTipWorld, glm::radians(side * 20.0f), glm::vec3(0, 1, 0));
            whiskerTipWorld = glm::rotate(whiskerTipWorld, glm::radians(-20.0f), glm::vec3(1, 0, 0));
            drawDragonPart(m_lightDetailMesh, glm::scale(glm::translate(whiskerTipWorld, glm::vec3(0, 0, 0.06f)), glm::vec3(0.0055f, 0.0055f, 0.07f)), DragonMaterial::Pale, shadowPass);
        }
    }

    void drawSegmentedDragon(bool shadowPass = false)
    {
        // Reference: Practical 1 — Intro to OpenGL, drawArm() and primitive geometry.
        // Reference: Practical 1 — Intro to OpenGL, hierarchical transformations.
        // Reference: Lecture 2 — Transformations and Projections.
        // Reference: Lecture 5 — Computer Animation, hierarchical transformations.
        const glm::mat4 headWorld = glm::translate(m_modelMatrix, glm::vec3(0, 0, -0.4f));
        drawDragonPart(m_headMesh, glm::scale(headWorld, glm::vec3(0.14f, 0.13f, 0.15f)), DragonMaterial::Jade, shadowPass);
        const glm::mat4 snoutWorld = glm::translate(headWorld, glm::vec3(0, -0.025f, -0.14f));
        drawDragonPart(m_headMesh, glm::scale(snoutWorld, glm::vec3(0.09f, 0.065f, 0.14f)), DragonMaterial::Jade, shadowPass);
        drawDragonHeadDetails(headWorld, snoutWorld, shadowPass);

        constexpr float jointSpacing = 0.11f;
        glm::mat4 jointWorld = glm::translate(m_modelMatrix, glm::vec3(0, 0, -0.32f));
        for (std::size_t joint = 0; joint < m_bodyJointAngles.size(); ++joint) {
            jointWorld = glm::rotate(jointWorld, m_bodyJointAngles[joint], glm::vec3(0, 1, 0));
            const float radius = 0.12f - 0.009f * static_cast<float>(joint);
            // Keep the joint frame unscaled; the capped segments overlap by 0.04 units.
            const glm::mat4 segmentWorld = glm::translate(jointWorld, glm::vec3(0, 0, jointSpacing * 0.5f));
            drawDragonPart(m_bodySegmentMesh, glm::scale(segmentWorld, glm::vec3(radius, radius, 0.15f)), DragonMaterial::Jade, shadowPass);
            // Reference: Lecture 5 — Computer Animation, hierarchical transformations.
            // B5.2: each dorsal spike is a leaf of its own unscaled segment frame.
            if (joint % 2 == 0) {
                const glm::mat4 spikeWorld = glm::rotate(glm::translate(segmentWorld, glm::vec3(0, radius * 0.8f, 0)), glm::radians(-65.0f), glm::vec3(1, 0, 0));
                const float spikeRadius = radius * 0.25f;
                drawDragonPart(m_goldDetailMesh, glm::scale(spikeWorld, glm::vec3(spikeRadius, spikeRadius, radius * 0.9f)), DragonMaterial::Gold, shadowPass);
            }
            if (joint + 1 < m_bodyJointAngles.size())
                jointWorld = glm::translate(jointWorld, glm::vec3(0, 0, jointSpacing));
        }
        // The tail inherits joint 8, with its base overlapping the last segment.
        const glm::mat4 tailWorld = glm::translate(jointWorld, glm::vec3(0, 0, jointSpacing - 0.015f));
        drawDragonPart(m_tailMesh, glm::scale(tailWorld, glm::vec3(0.057f, 0.057f, 0.18f)), DragonMaterial::Jade, shadowPass);
    }

    bool updateDragonFlight()
    {
        glm::vec3 movementDirection { 0.0f };
        if (m_flightMode == FlightMode::Autopilot) {
            // D1 implementation source: AI-generated with OpenAI Codex.
            // Reference: Lecture 5 — Computer Animation, arc-length re-parameterization.
            m_autopilotElapsedSeconds = std::fmod(m_autopilotElapsedSeconds + static_cast<double>(m_deltaTime), s_autopilotLoopSeconds);
            const double targetDistance = m_totalFlightLength * (m_autopilotElapsedSeconds / s_autopilotLoopSeconds);
            m_autopilotProgress = autopilotParameterAtDistance(targetDistance);
            const auto segmentIndex = static_cast<std::size_t>(m_autopilotProgress);
            const float t = m_autopilotProgress - static_cast<float>(segmentIndex);
            const CubicBezierSegment& segment = m_flightPath[segmentIndex];
            m_dragonPosition = evaluateCubicBezier(segment, t);
            movementDirection = evaluateCubicBezierTangent(segment, t);
        } else if (m_cameraMode != CameraMode::FreeCamera && !ImGui::GetIO().WantCaptureKeyboard) {
            // B9 implementation source: AI-generated with OpenAI Codex.
            // Reference: Lecture 2 — Transformations and Projections.
            // Reference: Exercise 5 — Physics Simulation, keyboard movement.
            const float steering = static_cast<float>(m_window.isKeyPressed(GLFW_KEY_A))
                - static_cast<float>(m_window.isKeyPressed(GLFW_KEY_D));
            const float turnSpeed = glm::radians(100.0f); // Radians per second.
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
                constexpr float movementSpeed = 1.0f; // World units per second.
                m_dragonPosition += glm::normalize(movementDirection) * movementSpeed * m_deltaTime;
            }
        }

        // Reference: Lecture 2 — Transformations and Projections.
        // Autopilot alone faces the tangent; Manual steering owns its current yaw.
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
    float m_autopilotProgress { 0.0f }; // Segment index + local t, retained for the existing GUI.
    double m_autopilotElapsedSeconds { 0.0 };
    static constexpr double s_autopilotLoopSeconds = 15.0;

    // D1 implementation source: AI-generated with OpenAI Codex.
    // Reference: Lecture 5 — Computer Animation, arc-length re-parameterization.
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
                previousPosition = position; // Shared endpoints contribute no extra interval.
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
        const auto& next = upper == m_flightArcLengths.end() ? m_flightArcLengths.back() : *upper;
        const auto& previous = *(upper - 1);
        const double intervalLength = next.distance - previous.distance;
        const double blend = intervalLength > 0.0 ? (distance - previous.distance) / intervalLength : 0.0;
        // Interpolate segment+t so a segment-end t=1 connects to the next segment's t=0.
        const double from = static_cast<double>(previous.segment) + previous.t;
        const double to = static_cast<double>(next.segment) + next.t;
        const float parameter = static_cast<float>(from + blend * (to - from));
        // Rounding near the lap end must not produce an out-of-range segment index.
        return std::clamp(parameter, 0.0f, std::nextafter(static_cast<float>(m_flightPath.size()), 0.0f));
    }


    void initializeBezierLines()
    {
        // Reference: Lecture 5 — Computer Animation, composite Bézier curves.
        std::vector<glm::vec3> positions;
        positions.reserve(m_flightPath.size() * s_bezierSamplesPerSegment);
        for (const CubicBezierSegment& segment : m_flightPath) {
            for (int sample = 0; sample < s_bezierSamplesPerSegment; ++sample) {
                const float t = static_cast<float>(sample) / static_cast<float>(s_bezierSamplesPerSegment - 1);
                positions.push_back(evaluateCubicBezier(segment, t));
            }
        }

        // Reference: Lecture 3 — Graphics Pipeline with Modern OpenGL.
        glGenVertexArrays(1, &m_bezierVao);
        glGenBuffers(1, &m_bezierVbo);
        glBindVertexArray(m_bezierVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_bezierVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(positions.size() * sizeof(glm::vec3)), positions.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(sizeof(glm::vec3)), nullptr);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    static constexpr GLsizei s_bezierSamplesPerSegment = 65;
    GLuint m_bezierVao { 0 };
    GLuint m_bezierVbo { 0 };
    bool m_showBezierPath { true };

    // Reference: Lecture 2 — Transformations and Projections.
    // Limits are expressed in world-space coordinates.
    static constexpr glm::vec3 s_minFlightPosition { -8.0f, 0.0f, -8.0f };
    static constexpr glm::vec3 s_maxFlightPosition { 8.0f, 6.0f, 8.0f };
    bool m_constrainFlightToTerrain { true };

    enum class CameraMode { FollowDragon, FreeCamera, TopDown };
    CameraMode m_cameraMode { CameraMode::FollowDragon };
    float m_followOrbitYaw { 0.0f }; // Offset relative to the dragon's yaw.
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

    // Shader for default rendering and for depth rendering
    Shader m_defaultShader;
    Shader m_shadowShader;
    static constexpr GLsizei s_shadowMapSize = 2048;
    static constexpr glm::vec3 s_directionalLight { 0.5f, 1.0f, 0.6f };
    GLuint m_shadowFramebuffer { 0 };
    GLuint m_shadowDepthTexture { 0 };
    glm::mat4 m_lightViewProjection { 1.0f };
    bool m_shadowMapReady { false };
    bool m_shadowMapping { true };
    bool m_shadowPcf { true };
    Shader m_bezierShader;

    std::vector<GPUMesh> m_meshes;
    GPUMesh m_groundMesh;
    GPUMesh m_bodySegmentMesh;
    GPUMesh m_headMesh;
    GPUMesh m_tailMesh;
    GPUMesh m_goldDetailMesh;
    GPUMesh m_lightDetailMesh;
    GPUMesh m_eyeMesh;
    int m_dragonModel { 1 }; // Segmented Dragon by default.
    std::array<float, 8> m_bodyJointAngles {}; // Effective local Y rotations, radians.
    std::array<float, 8> m_manualBodyJointAngles {}; // Saved independently of the wave.
    bool m_animateDragonBody { true };
    double m_bodyAnimationTime { 0.0 }; // Elapsed active animation time, seconds.
    float m_bodyAnimationBlend { 1.0f }; // Smooth re-entry from the manual configuration.
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
    int m_shadingModel { 1 }; // 0: C1 Blinn-Phong, 1: Cook-Torrance PBR.
    float m_jadeRoughness { 0.22f };
    float m_goldRoughness { 0.28f };
    float m_ambientStrength { 0.25f };
    float m_diffuseStrength { 0.75f };
    float m_specularStrength { 0.35f };
    float m_lightingShininess { 32.0f };
    float m_deltaTime { 0.0f }; // Elapsed seconds between frame starts.
    glm::vec3 m_dragonPosition { 0.0f }; // World-space position.
    float m_dragonYaw { 0.0f }; // Radians around world Y; zero preserves the imported pose.

    std::array<CubicBezierSegment, 3> m_flightPath {{
        { glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.2f, 0.0f),
            glm::vec3(2.5f, 1.0f, 2.25f), glm::vec3(2.0f, 1.0f, 3.0f) },
        { glm::vec3(2.0f, 1.0f, 3.0f), glm::vec3(1.5f, 1.0f, 3.75f),
            glm::vec3(-1.5f, 0.7f, 3.75f), glm::vec3(-2.0f, 0.5f, 3.0f) },
        { glm::vec3(-2.0f, 0.5f, 3.0f), glm::vec3(-2.5f, 0.3f, 2.25f),
            glm::vec3(-1.0f, -0.2f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f) }
    }};

    // Projection and view matrices for you to fill in and use
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
