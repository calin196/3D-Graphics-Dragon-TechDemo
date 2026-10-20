#version 410

layout(location = 0) in vec3 position;
layout(location = 2) in vec2 texCoord;

uniform mat4 viewProjection;
uniform vec2 pondCenter;
uniform vec2 pondRadii;
uniform float waveTime;
uniform float waveStrength;

out vec3 waterPosition;
out vec3 waterNormal;
out float shoreRadius;

void main()
{
    vec2 localPosition = position.xz - pondCenter;
    vec2 firstDirection = vec2(4.5, 2.0);
    vec2 secondDirection = vec2(-3.0, 7.0);
    vec2 thirdDirection = vec2(10.0, -5.0);
    float firstPhase = dot(localPosition, firstDirection) - waveTime;
    float secondPhase = dot(localPosition, secondDirection) - 0.7 * waveTime;
    float thirdPhase = dot(localPosition, thirdDirection) - 1.3 * waveTime;
    float wave = sin(firstPhase) + 0.5 * sin(secondPhase) + 0.25 * sin(thirdPhase);
    vec2 waveGradient = cos(firstPhase) * firstDirection;
    waveGradient += 0.5 * cos(secondPhase) * secondDirection;
    waveGradient += 0.25 * cos(thirdPhase) * thirdDirection;

    // Taper displacement to zero at the bank so waves do not expose the floor.
    float edgeDistance = max(0.0, 1.0 - texCoord.x * texCoord.x);
    float shoreFade = edgeDistance * edgeDistance;
    vec2 radiusGradient = localPosition / (pondRadii * pondRadii);
    vec2 fadeGradient = -4.0 * edgeDistance * radiusGradient;
    float waveAmplitude = waveStrength / 1.75;
    vec2 heightGradient = waveAmplitude * (shoreFade * waveGradient + wave * fadeGradient);
    waterNormal = normalize(vec3(-heightGradient.x, 1.0, -heightGradient.y));
    waterPosition = position;
    waterPosition.y += waveAmplitude * shoreFade * wave;
    shoreRadius = texCoord.x;
    gl_Position = viewProjection * vec4(waterPosition, 1.0);
}
