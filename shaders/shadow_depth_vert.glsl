#version 410

// Implementation source (D2): AI-generated with OpenAI Codex.
// Reference: Exercise 4 — Shadow Mapping, light-space depth pass.
layout(location = 0) in vec3 position;
uniform mat4 lightViewProjection;
uniform mat4 modelMatrix;

void main()
{
    gl_Position = lightViewProjection * modelMatrix * vec4(position, 1.0);
}
