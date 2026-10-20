#version 410

layout(std140) uniform Material
{
    vec3 kd;
    vec3 ks;
    float shininess;
    float transparency;
};

uniform samplerCube environmentMap;
uniform vec3 cameraPosition;
uniform vec2 pondCenter;
uniform float waveTime;
uniform float waveStrength;
uniform vec3 lightDirection;
uniform vec3 sunColor;
uniform float sunIntensity;
uniform float dayAmbient;
uniform vec3 environmentTint;
uniform vec3 twilightColor;
uniform float twilightAmount;
uniform bool enableLighting;
uniform bool enableReflections;

uniform sampler2D shadowMap;
uniform mat4 lightViewProjection;
uniform bool enableShadows;
uniform bool enableShadowPcf;

uniform bool enableFireLight;
uniform bool enableFireShadows;
uniform sampler2D fireShadowMap;
uniform mat4 fireViewProjection;
uniform vec3 fireLightPosition;
uniform vec3 mouthGlowPosition;
uniform vec3 fireLightDirection;
uniform float fireLightIntensity;
uniform float fireLightRange;


in vec3 waterPosition;
in vec3 waterNormal;
in float shoreRadius;
layout(location = 0) out vec4 fragColor;

vec3 safeNormalize(vec3 value)
{
    return value / max(length(value), 1e-6);
}

float directionalVisibility(vec3 geometricNormal)
{
    if (!enableShadows)
        return 1.0;
    vec4 lightClipPosition = lightViewProjection * vec4(waterPosition, 1.0);
    if (abs(lightClipPosition.w) < 1e-6)
        return 1.0;
    vec3 shadowCoordinate = lightClipPosition.xyz / lightClipPosition.w * 0.5 + 0.5;
    if (any(lessThan(shadowCoordinate, vec3(0.0))) || any(greaterThan(shadowCoordinate, vec3(1.0))))
        return 1.0;

    vec3 surfaceToLight = safeNormalize(lightDirection);
    float NdotL = max(dot(geometricNormal, surfaceToLight), 0.0);
    float slopeNumerator = sqrt(max(1.0 - NdotL * NdotL, 0.0));
    float slopeDenominator = max(NdotL, 0.2);
    float surfaceSlope = slopeNumerator / slopeDenominator;
    float depthBias = clamp(0.0006 * surfaceSlope, 0.00005, 0.002);
    float receiverDepth = shadowCoordinate.z - depthBias;
    if (!enableShadowPcf) {
        float storedDepth = textureLod(shadowMap, shadowCoordinate.xy, 0.0).r;
        if (receiverDepth <= storedDepth) {
            return 1.0;
        } else {
            return 0.0;
        }
    }
    ivec2 shadowMapSize = textureSize(shadowMap, 0);
    vec2 texelSize = 1.0 / vec2(shadowMapSize);
    float visibleSamples = 0.0;
    for (int offsetY = -2; offsetY <= 2; ++offsetY) {
        for (int offsetX = -2; offsetX <= 2; ++offsetX) {
            vec2 sampleOffset = vec2(offsetX, offsetY) * texelSize;
            vec2 sampleUV = shadowCoordinate.xy + sampleOffset;
            float storedDepth = textureLod(shadowMap, sampleUV, 0.0).r;
            float sampleVisibility;
            if (receiverDepth <= storedDepth) {
                sampleVisibility = 1.0;
            } else {
                sampleVisibility = 0.0;
            }
            visibleSamples += sampleVisibility;
        }
    }
    return visibleSamples / 25.0;
}

vec3 fireRadiance(out vec3 L)
{
    L = vec3(0);
    if (!enableFireLight)
        return vec3(0);
    vec3 toLight = fireLightPosition - waterPosition;
    float lightDistance = length(toLight);
    L = toLight / max(lightDistance, 1e-6);
    if (lightDistance >= fireLightRange)
        return vec3(0);
    float outerConeCosine = cos(radians(22.0));
    float innerConeCosine = cos(radians(6.0));
    float coneAlignment = dot(-L, fireLightDirection);
    float coneWeight = smoothstep(outerConeCosine, innerConeCosine, coneAlignment);
    coneWeight *= coneWeight;
    float rangeFade = 1.0 - smoothstep(0.75 * fireLightRange, fireLightRange, lightDistance);
    vec3 fadedRadiance = vec3(1.0, 0.32, 0.055) * fireLightIntensity * coneWeight * rangeFade * rangeFade;
    float distanceAttenuation = 1.0 + 1.0 * lightDistance + 1.2 * lightDistance * lightDistance;
    return fadedRadiance / distanceAttenuation;
}

vec3 mouthGlow(vec3 N)
{
    if (!enableFireLight)
        return vec3(0);
    vec3 toMouth = mouthGlowPosition - waterPosition;
    float mouthDistance = length(toMouth);
    if (mouthDistance >= 1.2)
        return vec3(0);
    float distanceFade = max(1.0 - mouthDistance / 1.2, 0.0);
    vec3 mouthDirection = toMouth / max(mouthDistance, 1e-6);
    float surfaceFacing = 0.25 + 0.75 * max(dot(N, mouthDirection), 0.0);
    vec3 fadedGlow = vec3(1.0, 0.24, 0.045) * (0.18 * fireLightIntensity) * surfaceFacing * distanceFade * distanceFade;
    float distanceAttenuation = 1.0 + 4.0 * mouthDistance * mouthDistance;
    return fadedGlow / distanceAttenuation;
}

float fireVisibility(vec3 geometricNormal, vec3 L)
{
    if (!enableFireShadows)
        return 1.0;
    vec4 lightClipPosition = fireViewProjection * vec4(waterPosition, 1);
    if (lightClipPosition.w <= 0.0)
        return 1.0;
    vec3 shadowCoordinate = lightClipPosition.xyz / lightClipPosition.w * 0.5 + 0.5;
    if (any(lessThan(shadowCoordinate, vec3(0))) || any(greaterThan(shadowCoordinate, vec3(1))))
        return 1.0;
    float NdotL = max(dot(geometricNormal, L), 0.0);
    float depthBias = max(0.00008, 0.0003 * (1.0 - NdotL));
    float receiverDepth = shadowCoordinate.z - depthBias;
    if (!enableShadowPcf) {
        float storedDepth = textureLod(fireShadowMap, shadowCoordinate.xy, 0.0).r;
        if (receiverDepth <= storedDepth) {
            return 1.0;
        } else {
            return 0.0;
        }
    }
    ivec2 shadowMapSize = textureSize(fireShadowMap, 0);
    vec2 texelSize = 1.0 / vec2(shadowMapSize);
    float visibleSamples = 0.0;
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            vec2 sampleOffset = vec2(offsetX, offsetY) * texelSize;
            vec2 sampleUV = shadowCoordinate.xy + sampleOffset;
            float storedDepth = textureLod(fireShadowMap, sampleUV, 0.0).r;
            float sampleVisibility;
            if (receiverDepth <= storedDepth) {
                sampleVisibility = 1.0;
            } else {
                sampleVisibility = 0.0;
            }
            visibleSamples += sampleVisibility;
        }
    }
    return visibleSamples / 9.0;
}


void main()
{
    vec3 N = normalize(waterNormal);
    vec3 towardEye = cameraPosition - waterPosition;
    vec3 V = towardEye / max(length(towardEye), 0.0001);
    vec3 L = normalize(lightDirection);
    float diffuse = max(dot(N, L), 0.0);
    float shallowWater = smoothstep(0.25, 0.95, shoreRadius);
    vec3 waterColor = kd * mix(0.45, 1.0, shallowWater);
    vec3 color = waterColor;
    float sunVisibility = 1.0;
    if (enableLighting) {
        // Use the undisplaced surface orientation for bias; waves still set receiver position.
        const vec3 geometricNormal = vec3(0.0, 1.0, 0.0);
        sunVisibility = directionalVisibility(geometricNormal);
        vec3 ambient = waterColor * (0.12 + 0.65 * dayAmbient);
        vec3 direct = waterColor * sunColor * sunIntensity * diffuse;
        color = ambient + direct * sunVisibility;
        color += waterColor * mouthGlow(N);
        vec3 halfway = L + V;
        if (dot(halfway, halfway) > 0.000001 && diffuse > 0.0) {
            vec3 H = normalize(halfway);
            float highlight = pow(max(dot(N, H), 0.0), 96.0);
            color += sunColor * sunIntensity * highlight * 1.8 * sunVisibility;
        }
        vec3 fireL;
        vec3 fire = fireRadiance(fireL);
        if (dot(fire, fire) > 0.0) {
            float fireDiffuse = max(dot(N, fireL), 0.0);
            float fireHighlight = 0.0;
            vec3 fireHalfway = fireL + V;
            if (dot(fireHalfway, fireHalfway) > 0.000001 && fireDiffuse > 0.0) {
                vec3 fireH = normalize(fireHalfway);
                fireHighlight = pow(max(dot(N, fireH), 0.0), 96.0);
            }
            float fireShadow = fireVisibility(geometricNormal, fireL);
            vec3 fireResponse = waterColor * fireDiffuse + vec3(1.8 * fireHighlight);
            color += fire * fireResponse * fireShadow;
        }
    }
    if (enableReflections) {
        // Reuse the sky cubemap; this does not render scene objects into a reflection pass.
        vec3 reflectedDirection = reflect(-V, N);
        vec3 environment = textureLod(environmentMap, reflectedDirection, 1.0).rgb;
        environment *= environmentTint;
        float horizonWeight = pow(clamp(1.0 - abs(reflectedDirection.y), 0.0, 1.0), 4.0);
        environment = mix(environment, twilightColor, twilightAmount * horizonWeight);
        float viewAlignment = clamp(dot(N, V), 0.0, 1.0);
        float fresnel = 0.02 + 0.98 * pow(1.0 - viewAlignment, 5.0);
        color = mix(color, environment, fresnel);
    }
    vec2 localPosition = waterPosition.xz - pondCenter;
    float rippleBend = 0.65 * sin(dot(localPosition, vec2(-2.0, 6.0)) - 0.7 * waveTime);
    float ripplePhase = dot(localPosition, vec2(9.0, 3.0)) + rippleBend - waveTime;
    float rippleCrest = smoothstep(0.80, 0.99, sin(ripplePhase));
    float rippleStrength = clamp(waveStrength / 0.018, 0.0, 2.0);
    float rippleFade = 1.0 - smoothstep(0.78, 1.0, shoreRadius);
    vec3 rippleColor = vec3(0.025, 0.06, 0.065) * (0.05 + 0.95 * dayAmbient * sunVisibility);
    color += rippleColor * rippleCrest * rippleStrength * rippleFade;
    float shoreline = smoothstep(0.90, 1.0, shoreRadius);
    vec3 bankColor = vec3(0.24, 0.39, 0.34) * (0.12 + 0.35 * dayAmbient + 0.30 * dayAmbient * sunVisibility);
    color = mix(color, bankColor, shoreline * 0.65);

    vec3 lowRange = 12.92 * color;
    vec3 highRange = 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    vec3 displayColor = mix(lowRange, highRange, greaterThan(color, vec3(0.0031308)));
    fragColor = vec4(displayColor, 1.0);
}
