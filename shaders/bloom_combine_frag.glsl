#version 410

uniform sampler2D scene;
uniform sampler2D bloom;
uniform float strength;
in vec2 uv;
layout(location = 0) out vec4 fragColor;

vec3 decode(vec3 color)
{
    color = max(color, vec3(0.0));
    vec3 lowRange = color / 12.92;
    vec3 powerBase = (color + 0.055) / 1.055;
    vec3 highRange = pow(powerBase, vec3(2.4));
    bvec3 useHighRange = greaterThan(color, vec3(0.04045));
    return mix(lowRange, highRange, useHighRange);
}

vec3 encode(vec3 color)
{
    color = max(color, vec3(0.0));
    vec3 lowRange = 12.92 * color;
    vec3 highRange = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    bvec3 useHighRange = greaterThan(color, vec3(0.0031308));
    return mix(lowRange, highRange, useHighRange);
}

void main()
{
    vec3 sceneSample = texture(scene, uv).rgb;
    vec3 base = decode(sceneSample);
    vec3 glow = texture(bloom, uv).rgb;
    float bloomStrength = max(strength, 0.0);
    vec3 combinedColor = base + bloomStrength * glow;
    vec3 displayColor = encode(combinedColor);
    fragColor = vec4(displayColor, 1.0);
}
