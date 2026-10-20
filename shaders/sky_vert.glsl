#version 410

uniform mat4 inverseProjection;
uniform mat3 cameraRotation;
out vec3 worldDirection;

void main()
{
    const vec2 corners[3] = vec2[](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));
    vec2 ndc = corners[gl_VertexID];
    vec4 viewPosition = inverseProjection * vec4(ndc, 1, 1);
    worldDirection = cameraRotation * viewPosition.xyz;
    gl_Position = vec4(ndc, 1, 1);
}
