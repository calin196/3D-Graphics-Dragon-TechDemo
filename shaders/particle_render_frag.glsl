#version 410

in vec2 glowCoordinate;
in float lifeFraction;
in float streamBrightness;
flat in int tailEmber;
layout(location = 0) out vec4 fragColor;

void main()
{
    float radius2 = dot(glowCoordinate, glowCoordinate);
    if (radius2 >= 1.0)
        discard;

    vec3 hot = vec3(1.0, 0.92, 0.55);
    vec3 gold = vec3(1.0, 0.56, 0.04);
    vec3 fire = vec3(1.0, 0.12, 0.012);
    vec3 color = mix(hot, gold, smoothstep(0.0, 0.35, lifeFraction));
    color = mix(color, fire, smoothstep(0.30, 0.85, lifeFraction));
    float horizontalWave = sin(9.0 * glowCoordinate.x + 8.0 * lifeFraction);
    float verticalWave = sin(7.0 * glowCoordinate.y - 12.0 * lifeFraction);
    float edge = radius2 + 0.06 * horizontalWave * verticalWave;
    float radialGlow = exp(-4.5 * radius2);
    float edgeFade = 1.0 - smoothstep(0.45, 1.0, edge);
    float glow = radialGlow * edgeFade;
    float fade = 1.0 - smoothstep(0.0, 1.0, lifeFraction);
    if (tailEmber == 1) {
        // Small emissive sparks need a brighter core than the translucent fire plume.
        vec3 emberGold = vec3(3.0, 1.35, 0.12);
        vec3 emberRed = vec3(1.8, 0.12, 0.015);
        vec3 emberColor = mix(emberGold, emberRed, lifeFraction);
        fragColor = vec4(emberColor, 0.65 * glow * fade);
    } else {
        fragColor = vec4(color, 0.12 * streamBrightness * glow * fade);
    }
}
