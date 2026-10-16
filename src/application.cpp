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
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

// Implementation source (B5.1): AI-generated with OpenAI Codex.
// Reference: Lecture 3 — Graphics Pipeline.
Mesh createDragonRevolvedMesh(const std::vector<glm::vec2>& profile, const glm::vec3& color)
{
    // Profile entries are (radius, Z); eight radial sides surround the local Z axis.
    Mesh mesh {};
    mesh.material.kd = color;
    constexpr unsigned int sides = 8;
    auto addTriangle = [&mesh](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        const glm::vec3 cross = glm::cross(b - a, c - a);
        if (glm::dot(cross, cross) < 1e-12f)
            return; // At a pole, one half of a ring quad collapses.
        const glm::vec3 normal = glm::normalize(cross);
        const unsigned int first = static_cast<unsigned int>(mesh.vertices.size());
        mesh.vertices.push_back({ a, normal, { 0, 0 } });
        mesh.vertices.push_back({ b, normal, { 0, 0 } });
        mesh.vertices.push_back({ c, normal, { 0, 0 } });
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
            addTriangle(a, b, c);
            addTriangle(a, c, d);
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
        , m_bodySegmentMesh(createDragonRevolvedMesh({ { 0, -0.5f }, { 1, -0.35f }, { 1, 0.35f }, { 0, 0.5f } }, glm::vec3(0.12f, 0.55f, 0.3f)))
        , m_headMesh(createDragonRevolvedMesh({ { 0, -1 }, { 0.7071f, -0.7071f }, { 1, 0 }, { 0.7071f, 0.7071f }, { 0, 1 } }, glm::vec3(0.18f, 0.65f, 0.36f)))
        , m_tailMesh(createDragonRevolvedMesh({ { 0, 0 }, { 1, 0 }, { 0, 1 } }, glm::vec3(0.12f, 0.55f, 0.3f)))
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

        m_meshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/dragon.obj");

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "Shaders/shadow_frag.glsl");
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
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(groundNormalMatrix));
                m_texture.bind(GL_TEXTURE0);
                glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), m_groundMesh.hasTextureCoords());
                glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
                m_groundMesh.draw(m_defaultShader);
            }

            if (m_dragonModel == 1) {
                drawSegmentedDragon();
            } else {
                for (GPUMesh& mesh : m_meshes) {
                    m_defaultShader.bind();
                    glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                    //Uncomment this line when you use the modelMatrix (or fragmentPosition)
                    //glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
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

    void drawDragonPart(GPUMesh& mesh, const glm::mat4& modelMatrix)
    {
        const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * modelMatrix;
        const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(modelMatrix));
        m_defaultShader.bind();
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
        glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
        // The existing shader optimizes out modelMatrix because fragPosition is unused.
        glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
        mesh.draw(m_defaultShader);
    }

    // Implementation source (B5.2): AI-generated with OpenAI Codex.
    // Reference: Practical 1 — Intro to OpenGL, primitive geometry.
    // Reference: Lecture 2 — Transformations and Projections.
    void drawDragonHeadDetails(const glm::mat4& headWorld, const glm::mat4& snoutWorld)
    {
        const glm::mat4 jawWorld = glm::translate(snoutWorld, glm::vec3(0, -0.045f, -0.015f));
        drawDragonPart(m_lightDetailMesh, glm::scale(jawWorld, glm::vec3(0.085f, 0.028f, 0.125f)));
        for (float side : { -1.0f, 1.0f }) {
            glm::mat4 hornWorld = glm::translate(headWorld, glm::vec3(side * 0.09f, 0.075f, 0.025f));
            hornWorld = glm::rotate(hornWorld, glm::radians(-65.0f), glm::vec3(1, 0, 0));
            hornWorld = glm::rotate(hornWorld, glm::radians(side * 15.0f), glm::vec3(0, 1, 0));
            drawDragonPart(m_goldDetailMesh, glm::scale(hornWorld, glm::vec3(0.035f, 0.035f, 0.2f)));

            const glm::mat4 eyeWorld = glm::translate(headWorld, glm::vec3(side * 0.112f, 0.058f, -0.065f));
            drawDragonPart(m_eyeMesh, glm::scale(eyeWorld, glm::vec3(0.027f, 0.03f, 0.035f)));

            // Two overlapping thin ellipsoids make each whisker bend outward and forward.
            glm::mat4 whiskerWorld = glm::translate(snoutWorld, glm::vec3(side * 0.07f, 0, -0.06f));
            whiskerWorld = glm::rotate(whiskerWorld, glm::radians(side * 135.0f), glm::vec3(0, 1, 0));
            drawDragonPart(m_lightDetailMesh, glm::scale(glm::translate(whiskerWorld, glm::vec3(0, 0, 0.08f)), glm::vec3(0.008f, 0.008f, 0.09f)));
            glm::mat4 whiskerTipWorld = glm::translate(whiskerWorld, glm::vec3(0, 0, 0.16f));
            whiskerTipWorld = glm::rotate(whiskerTipWorld, glm::radians(side * 20.0f), glm::vec3(0, 1, 0));
            whiskerTipWorld = glm::rotate(whiskerTipWorld, glm::radians(-20.0f), glm::vec3(1, 0, 0));
            drawDragonPart(m_lightDetailMesh, glm::scale(glm::translate(whiskerTipWorld, glm::vec3(0, 0, 0.06f)), glm::vec3(0.0055f, 0.0055f, 0.07f)));
        }
    }

    void drawSegmentedDragon()
    {
        // Reference: Practical 1 — Intro to OpenGL, drawArm() and primitive geometry.
        // Reference: Practical 1 — Intro to OpenGL, hierarchical transformations.
        // Reference: Lecture 2 — Transformations and Projections.
        // Reference: Lecture 5 — Computer Animation, hierarchical transformations.
        const glm::mat4 headWorld = glm::translate(m_modelMatrix, glm::vec3(0, 0, -0.4f));
        drawDragonPart(m_headMesh, glm::scale(headWorld, glm::vec3(0.14f, 0.13f, 0.15f)));
        const glm::mat4 snoutWorld = glm::translate(headWorld, glm::vec3(0, -0.025f, -0.14f));
        drawDragonPart(m_headMesh, glm::scale(snoutWorld, glm::vec3(0.09f, 0.065f, 0.14f)));
        drawDragonHeadDetails(headWorld, snoutWorld);

        constexpr float jointSpacing = 0.11f;
        glm::mat4 jointWorld = glm::translate(m_modelMatrix, glm::vec3(0, 0, -0.32f));
        for (std::size_t joint = 0; joint < m_bodyJointAngles.size(); ++joint) {
            jointWorld = glm::rotate(jointWorld, m_bodyJointAngles[joint], glm::vec3(0, 1, 0));
            const float radius = 0.12f - 0.009f * static_cast<float>(joint);
            // Keep the joint frame unscaled; the capped segments overlap by 0.04 units.
            const glm::mat4 segmentWorld = glm::translate(jointWorld, glm::vec3(0, 0, jointSpacing * 0.5f));
            drawDragonPart(m_bodySegmentMesh, glm::scale(segmentWorld, glm::vec3(radius, radius, 0.15f)));
            // Reference: Lecture 5 — Computer Animation, hierarchical transformations.
            // B5.2: each dorsal spike is a leaf of its own unscaled segment frame.
            if (joint % 2 == 0) {
                const glm::mat4 spikeWorld = glm::rotate(glm::translate(segmentWorld, glm::vec3(0, radius * 0.8f, 0)), glm::radians(-65.0f), glm::vec3(1, 0, 0));
                const float spikeRadius = radius * 0.25f;
                drawDragonPart(m_goldDetailMesh, glm::scale(spikeWorld, glm::vec3(spikeRadius, spikeRadius, radius * 0.9f)));
            }
            if (joint + 1 < m_bodyJointAngles.size())
                jointWorld = glm::translate(jointWorld, glm::vec3(0, 0, jointSpacing));
        }
        // The tail inherits joint 8, with its base overlapping the last segment.
        const glm::mat4 tailWorld = glm::translate(jointWorld, glm::vec3(0, 0, jointSpacing - 0.015f));
        drawDragonPart(m_tailMesh, glm::scale(tailWorld, glm::vec3(0.057f, 0.057f, 0.18f)));
    }

    bool updateDragonFlight()
    {
        glm::vec3 movementDirection { 0.0f };
        if (m_flightMode == FlightMode::Autopilot) {
            // Reference: Lecture 5 — Computer Animation, composite Bézier curves.
            constexpr float secondsPerSegment = 5.0f;
            m_autopilotProgress = std::fmod(m_autopilotProgress + m_deltaTime / secondsPerSegment, static_cast<float>(m_flightPath.size()));
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
    float m_autopilotProgress { 0.0f };

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
    bool m_showGround { true };
    bool m_useMaterial { true };
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
