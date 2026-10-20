#version 410

uniform sampler2D positionAge;
uniform sampler2D velocityLifetime;
uniform mat4 viewProjection;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform bool breathActive;
uniform bool tailActive;
out vec2 glowCoordinate;
out float lifeFraction;
out float streamBrightness;
flat out int tailEmber;

void main()
{
    int stateWidth = textureSize(positionAge, 0).x;
    int stateColumn = gl_InstanceID % stateWidth;
    int stateRow = gl_InstanceID / stateWidth;
    ivec2 stateCell = ivec2(stateColumn, stateRow);
    vec4 positionAgeState = texelFetch(positionAge, stateCell, 0);
    float lifetime = texelFetch(velocityLifetime, stateCell, 0).w;
    const vec2 corners[4] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
    glowCoordinate = corners[gl_VertexID];
    bool isTailEmber = gl_InstanceID >= 1024;
    tailEmber = 0;
    if (isTailEmber) {
        tailEmber = 1;
    }
    bool isMouthStream = gl_InstanceID % 10 < 6;
    if (isMouthStream) {
        streamBrightness = 0.85;
    } else {
        streamBrightness = 1.6;
    }
    if (isTailEmber) {
        streamBrightness = 0.8;
    }
    float safeLifetime = max(lifetime, 0.0001);
    float relativeAge = positionAgeState.w / safeLifetime;
    lifeFraction = clamp(relativeAge, 0.0, 1.0);
    bool streamEnabled = breathActive;
    if (isTailEmber) {
        streamEnabled = tailActive;

    }
    if (!streamEnabled || lifetime <= 0.0 || positionAgeState.w < 0.0 || positionAgeState.w >= lifetime) {
        gl_Position = vec4(2, 2, 2, 1);
        return;
    }

    float baseRadius;
    if (isMouthStream) {
        baseRadius = 0.050;
    } else {
        baseRadius = 0.026;
    }
    if (isTailEmber) {
        baseRadius = 0.018;
    }
    float particleVariation = fract(float(gl_InstanceID) * 0.618034);
    float radiusVariation = 1.0 + 0.45 * particleVariation;
    float radius = baseRadius * radiusVariation;
    float shrinkRate = 0.75;
    if (isTailEmber) {
        shrinkRate = 0.35;
    }
    radius *= (1.0 - shrinkRate * lifeFraction);
    vec3 horizontalOffset = glowCoordinate.x * cameraRight;
    vec3 verticalOffset = glowCoordinate.y * cameraUp;
    vec3 billboardOffset = radius * (horizontalOffset + verticalOffset);
    vec3 worldPosition = positionAgeState.xyz + billboardOffset;
    gl_Position = viewProjection * vec4(worldPosition, 1);
}
