
# Chinese Dragon — 3D Graphics Tech Demo

An interactive real-time 3D graphics application developed in C++ and OpenGL as part of the **3D Computer Graphics and Animation** course at Delft University of Technology (TU Delft).

The project features a procedurally modelled Chinese dragon flying through a 3D environment. It demonstrates graphics programming techniques including procedural geometry, animation, GPU skinning, advanced shading, lighting, shadow mapping, and GPU particle effects.

## Technologies

- **Language:** C++20
- **Graphics:** OpenGL 4.5, GLSL
- **Interface:** Dear ImGui
- **Libraries:** GLFW, GLM, GLAD
- **Build System:** CMake

## Main Features

### 1. Procedural Dragon Modelling and Animation

- Procedurally generated 3D dragon geometry, including detailed head, body, horns, whiskers, and tail.
- Hierarchical transformations for articulated body movement.
- Sinusoidal body animation with phase offsets.
- GPU linear blend skinning using an eight-bone skeleton.
- GPU blend shapes for animated jaw movement.
- Manual flight controls and smooth directional rotation.

### 2. Advanced Rendering

- Blinn-Phong and physically based rendering (PBR).
- Material textures, roughness and metallic property maps.
- Normal mapping for additional surface detail.
- Environment mapping and reflections.
- Shadow mapping with PCF filtering.
- Bloom post-processing.

### 3. Flight and Camera Systems

- Manual keyboard-controlled flight.
- Automatic flight along three connected cubic Bézier curves.
- Constant-speed autopilot using arc-length approximation.
- Three camera modes: Follow Dragon, Free Camera, and Top-Down.
- Interactive camera rotation and zoom.

### 4. Procedural Environment

- Procedurally generated terrain and volcano.
- Rocks, grass, animated lava, and water effects.
- Dynamic day/night lighting.
- Terrain collision detection and flight boundaries.
- Minimap displaying the environment and dragon position.

### 5. GPU Particle Effects

- Fire-breathing particle effect.
- Tail ember trail.
- GPU-based particle simulation and billboard rendering.
- Fire-generated lighting and shadows.

### 6. Interactive User Interface

An ImGui control panel provides real-time access to:

- Rendering and lighting settings.
- Animation controls and body joint angles.
- Flight modes and camera selection.
- Shadow, texture, reflection, and particle effects.
- Environment settings and visual effects.

## Controls

| Key | Action |
|-----|--------|
| W / S | Fly forward / backward |
| A / D | Turn left / right |
| Q / E | Descend / ascend |
| 1 | Toggle fire breathing |
| F1 | Follow Dragon camera |
| F2 | Free Camera |
| F3 | Top-Down camera |

**Camera controls:**

- Follow Dragon: hold the left mouse button to orbit; scroll to zoom.
- Free Camera: WASD to move, Q/E for vertical movement, and hold the left mouse button to look around.
- Manual dragon flight is paused while using Free Camera.
- Autopilot and additional graphics features can be enabled from the ImGui panel.

## Build and Run

### Requirements

- CMake 3.14 or newer
- C++20-compatible compiler
- OpenGL 4.5-compatible graphics hardware and drivers
- Visual Studio with C++ development tools (recommended on Windows)

### 1. Clone the repository

```bash
git clone https://github.com/calin196/3D-Graphics-Dragon-TechDemo.git
cd 3D-Graphics-Dragon-TechDemo
```

### 2. Configure and build

```bash
cmake -S . -B build
cmake --build build --config Debug
```

### 3. Run the application

On Windows with the Visual Studio CMake generator:

```powershell
.\build\Debug\Master_TechDemo.exe
```

The executable is named `Master_TechDemo`. Its exact location may differ depending on the selected CMake generator.

The repository includes the graphics framework, shader source files, and application resources required by the project.

## Project Structure

```text
3D-Graphics-Dragon-TechDemo/
├── framework/       # Graphics framework and dependencies
├── resources/       # Textures and other assets
├── shaders/         # GLSL shaders
├── src/             # C++ source code
├── CMakeLists.txt   # Build configuration
└── README.md        # Project documentation
```

## Academic Context

Developed as a technical demonstration project for the MSc Computer Science course **3D Computer Graphics and Animation (CS4515)** at TU Delft.

The project explores the practical implementation and integration of real-time rendering, mathematical animation, GPU programming, and interactive 3D application development.
