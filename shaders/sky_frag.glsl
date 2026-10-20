#version 410

uniform samplerCube environmentMap;

uniform vec3 environmentTint;
uniform vec3 twilightColor;
uniform float twilightAmount;
in vec3 worldDirection;
layout(location = 0) out vec4 fragColor;

void main()
{

    vec3 direction = normalize(worldDirection);
    vec3 environmentColor = textureLod(environmentMap, direction, 0.0).rgb;
    vec3 linearColor = max(environmentColor, vec3(0)) * environmentTint;
    float horizonWeight = clamp(1.0 - abs(direction.y), 0.0, 1.0);
    float horizon = pow(horizonWeight, 4.0);
    linearColor = mix(linearColor, twilightColor, twilightAmount * horizon);
    vec3 lowRange = 12.92 * linearColor;
    vec3 highRange = 1.055 * pow(linearColor, vec3(1.0 / 2.4)) - 0.055;
    bvec3 useHighRange = greaterThan(linearColor, vec3(0.0031308));
    vec3 displayColor = mix(lowRange, highRange, useHighRange);
    fragColor = vec4(displayColor, 1);
}
