#version 410

layout(std140) uniform Material
{
    vec3 kd;
    vec3 ks;
    float shininess;
    float transparency;
};

uniform sampler2D colorMap;
uniform bool hasTexCoords;
uniform bool useMaterial;
uniform bool isLavaSurface;
uniform bool isLavaStream;
uniform float lavaFlowOffset;

uniform sampler2D normalMap;
uniform bool useNormalMap;
uniform float normalStrength;

uniform bool enableLighting;
uniform vec3 lightDirection;
uniform vec3 cameraPosition;

uniform vec3 sunColor;
uniform float sunIntensity;
uniform float dayAmbient;
uniform vec3 environmentTint;
uniform vec3 twilightColor;
uniform float twilightAmount;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float lightingShininess;

uniform int shadingModel;
uniform float pbrMetallic;
uniform float pbrRoughness;

uniform sampler2D roughnessMap;
uniform sampler2D metallicMap;
uniform bool usePbrPropertyMaps;

uniform samplerCube environmentMap;
uniform bool enableEnvironmentReflections;
uniform float reflectionStrength;
uniform float environmentMaxLod;

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

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

layout(location = 0) out vec4 fragColor;

const float PI = 3.14159265359;

vec3 safeNormalize(vec3 value)
{
    return value / max(length(value), 1e-6);
}

float directionalVisibility(vec3 geometricNormal)
{
    if (!enableShadows)
        return 1.0;
    vec4 lightClipPosition = lightViewProjection * vec4(fragPosition, 1.0);
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
    vec3 toLight = fireLightPosition - fragPosition;
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
    vec3 toMouth = mouthGlowPosition - fragPosition;
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
    vec4 lightClipPosition = fireViewProjection * vec4(fragPosition, 1);
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

float distributionGGX(float NdotH, float r)
{
    float alpha = r * r;
    float alphaSquared = alpha * alpha;

    float n2 = NdotH * NdotH;
    float denominator = (1.0 - n2) + alphaSquared * n2;
    float safeDenominator = max(PI * denominator * denominator, 1e-12);
    return alphaSquared / safeDenominator;
}

float geometrySchlickGGX(float NdotDirection, float r)
{

    float k = (r + 1.0) * (r + 1.0) / 8.0;
    float denominator = max(NdotDirection * (1.0 - k) + k, 1e-6);
    return NdotDirection / denominator;
}

float geometrySmith(float NdotV, float NdotL, float r)
{
    float viewMasking = geometrySchlickGGX(NdotV, r);
    float lightMasking = geometrySchlickGGX(NdotL, r);
    return viewMasking * lightMasking;
}

vec3 fresnelSchlick(float VdotH, vec3 F0)
{
    float grazingWeight = clamp(1.0 - VdotH, 0.0, 1.0);
    float fresnelWeight = pow(grazingWeight, 5.0);
    return F0 + (vec3(1.0) - F0) * fresnelWeight;
}

vec3 srgbToLinear(vec3 color)
{
    color = clamp(color, 0.0, 1.0);
    vec3 lowRange = color / 12.92;
    vec3 powerBase = (color + 0.055) / 1.055;
    vec3 highRange = pow(powerBase, vec3(2.4));
    bvec3 useHighRange = greaterThan(color, vec3(0.04045));
    return mix(lowRange, highRange, useHighRange);
}

vec3 linearToSrgb(vec3 color)
{
    color = max(color, vec3(0.0));
    vec3 lowRange = 12.92 * color;
    vec3 highRange = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    bvec3 useHighRange = greaterThan(color, vec3(0.0031308));
    return mix(lowRange, highRange, useHighRange);
}

vec3 cookTorrance(
    vec3 baseColor, vec3 N, vec3 L, vec3 V, float visibility, float metallic, float roughness, vec3 radiance)
{
    metallic = clamp(metallic, 0.0, 1.0);
    float r = clamp(roughness, 0.08, 1.0);
    float NdotL = clamp(dot(N, L), 0.0, 1.0);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);

    vec3 result = vec3(0.0);
    vec3 halfway = L + V;
    if (NdotL > 0.0 && NdotV > 0.0 && dot(halfway, halfway) > 1e-12) {
        vec3 H = safeNormalize(halfway);
        vec3 F0 = mix(vec3(0.04), baseColor, metallic);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);
        vec3 F = fresnelSchlick(VdotH, F0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float D = distributionGGX(NdotH, r);
        float G = geometrySmith(NdotV, NdotL, r);
        float specularDenominator = max(4.0 * NdotV * NdotL, 1e-6);
        vec3 specularBRDF = D * G * F / specularDenominator;
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 diffuseBRDF = kD * baseColor / PI;
        vec3 direct = (diffuseBRDF + specularBRDF) * radiance * NdotL;
        result += direct * visibility;
    }
    return result;
}

vec3 dragonShadingNormal(vec3 N)
{
    vec3 positionDx = dFdx(fragPosition);
    vec3 positionDy = dFdy(fragPosition);
    vec2 uvDx = dFdx(fragTexCoord);
    vec2 uvDy = dFdy(fragTexCoord);
    if (!useNormalMap || normalStrength <= 0.0 || dot(N, N) < 0.5)
        return N;

    float determinant = uvDx.x * uvDy.y - uvDx.y * uvDy.x;
    float uvScale = length(uvDx) * length(uvDy);
    if (!(uvScale > 1e-20) || !(abs(determinant) > 1e-5 * uvScale))
        return N;

    vec3 T = (positionDx * uvDy.y - positionDy * uvDx.y) * sign(determinant);
    vec3 rawB = (positionDy * uvDx.x - positionDx * uvDy.x) * sign(determinant);
    T -= N * dot(N, T);
    float tangentLength2 = dot(T, T);
    float bitangentLength2 = dot(rawB, rawB);
    if (!(tangentLength2 > 1e-20) || !(bitangentLength2 > 1e-20))
        return N;
    T *= inversesqrt(tangentLength2);
    float handedness = dot(cross(N, T), rawB * inversesqrt(bitangentLength2));
    if (!(abs(handedness) > 1e-4))
        return N;
    vec3 B = sign(handedness) * cross(N, T);

    vec3 normalSample = texture(normalMap, fragTexCoord).xyz;
    vec3 tangentNormal = normalSample * 2.0 - 1.0;
    tangentNormal.xy *= normalStrength;
    vec3 mapped = mat3(T, B, N) * tangentNormal;
    float length2 = dot(mapped, mapped);
    if (length2 > 1e-12) {
        return mapped * inversesqrt(length2);
    } else {
        return N;
    }
}

vec3 environmentSpecular(vec3 baseColor, vec3 N, vec3 V, float metallic, float roughness)
{
    N = safeNormalize(N);
    V = safeNormalize(V);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    if (NdotV <= 0.0)
        return vec3(0.0);

    vec3 R = reflect(-V, N);
    metallic = clamp(metallic, 0.0, 1.0);
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 F = fresnelSchlick(NdotV, F0);

    float lod = clamp(roughness, 0.08, 1.0) * environmentMaxLod;
    vec3 environmentSample = textureLod(environmentMap, R, lod).rgb;
    vec3 radiance = environmentSample * environmentTint;
    float horizonWeight = clamp(1.0 - abs(R.y), 0.0, 1.0);
    float horizon = pow(horizonWeight, 4.0);
    radiance = mix(radiance, twilightColor, twilightAmount * horizon);
    return radiance * F * metallic * clamp(reflectionStrength, 0.0, 1.0);
}

void main()
{
    vec3 N;
    if (dot(fragNormal, fragNormal) > 0.0) {
        N = normalize(fragNormal);
    } else {
        N = vec3(0.0);
    }

    if (isLavaSurface) {
        vec2 lavaUV = fragTexCoord;
        if (isLavaStream) {
            lavaUV.x *= 0.35;
            lavaUV.y -= lavaFlowOffset;
        }
        vec4 lavaFrame = texture(colorMap, lavaUV);
        float moltenAmount = lavaFrame.a;
        // Sand-colored crust receives daylight; only the molten cracks are emissive.
        vec3 crustColor = vec3(0.66, 0.59, 0.45);
        float daylight = max(dot(N, safeNormalize(lightDirection)), 0.0);
        vec3 crustLight = vec3(0.25 * dayAmbient) + sunColor * sunIntensity * daylight * 0.75;
        if (!enableLighting)
            crustLight = vec3(1.0);
        vec3 crust = crustColor * crustLight;
        if (isLavaStream) {
            float edgeFade = smoothstep(0.0, 0.15, fragTexCoord.x);
            edgeFade *= 1.0 - smoothstep(0.85, 1.0, fragTexCoord.x);
            moltenAmount = max(moltenAmount, 0.35) * edgeFade;
            crust = vec3(0.20, 0.055, 0.012) * crustLight;
        }
        vec3 molten = lavaFrame.rgb * 2.8;
        fragColor = vec4(mix(crust, molten, moltenAmount), 1.0);
        return;
    }

    vec3 surfaceColor;
    if (hasTexCoords)
        surfaceColor = texture(colorMap, fragTexCoord).rgb;
    else if (useMaterial)
        surfaceColor = kd;
    else
        surfaceColor = N;

    if (!enableLighting) {
        fragColor = vec4(surfaceColor, 1.0);
        return;
    }

    float visibility = directionalVisibility(N);
    vec3 fireL;
    vec3 fire = fireRadiance(fireL);
    float fireShadow = 1.0;
    if (dot(fire, fire) > 0.0) {
        fireShadow = fireVisibility(N, fireL);
    }
    N = dragonShadingNormal(N);

    vec3 glow = mouthGlow(N);
    if (shadingModel == 1) {

        float roughness = clamp(pbrRoughness, 0.08, 1.0);
        float metallic = clamp(pbrMetallic, 0.0, 1.0);
        if (usePbrPropertyMaps) {
            float roughnessSample = texture(roughnessMap, fragTexCoord).r;
            roughness = clamp(roughnessSample, 0.08, 1.0);
            float metallicSample = texture(metallicMap, fragTexCoord).r;
            metallic = clamp(metallicSample, 0.0, 1.0);
        }
        vec3 linearColor = srgbToLinear(surfaceColor);
        vec3 V = safeNormalize(cameraPosition - fragPosition);
        vec3 ambientContribution = (0.08 * dayAmbient + glow / PI) * linearColor;
        vec3 sunDirection = safeNormalize(lightDirection);
        vec3 sunRadiance = 3.0 * sunColor * sunIntensity;
        vec3 sunContribution =
            cookTorrance(linearColor, N, sunDirection, V, visibility, metallic, roughness, sunRadiance);
        vec3 lit = ambientContribution + sunContribution;
        if (dot(fire, fire) > 0.0)
            lit += cookTorrance(linearColor, N, fireL, V, fireShadow, metallic, roughness, fire);
        if (enableEnvironmentReflections && reflectionStrength > 0.0 && metallic > 0.0)
            lit += environmentSpecular(linearColor, N, cameraPosition - fragPosition, metallic, roughness);
        fragColor = vec4(linearToSrgb(lit), 1.0);
        return;
    }

    vec3 L = normalize(lightDirection);
    float diffuseFactor = max(dot(N, L), 0.0);
    float specularFactor = 0.0;
    if (diffuseFactor > 0.0) {
        vec3 towardCamera = cameraPosition - fragPosition;
        vec3 V = towardCamera / max(length(towardCamera), 1e-6);
        vec3 halfway = L + V;

        if (dot(halfway, halfway) > 1e-8) {
            vec3 H = normalize(halfway);
            specularFactor = pow(max(dot(N, H), 0.0), lightingShininess);
        }
    }

    vec3 direct = diffuseStrength * diffuseFactor * surfaceColor + vec3(specularStrength * specularFactor);
    vec3 color = (ambientStrength * dayAmbient + glow) * surfaceColor + visibility * sunColor * sunIntensity * direct;
    if (dot(fire, fire) > 0.0) {
        float diffuse = max(dot(N, fireL), 0.0);
        vec3 halfway = fireL + safeNormalize(cameraPosition - fragPosition);
        float specular = 0.0;
        if (diffuse > 0.0 && dot(halfway, halfway) > 1e-8) {
            vec3 halfwayDirection = safeNormalize(halfway);
            float specularAlignment = max(dot(N, halfwayDirection), 0.0);
            specular = pow(specularAlignment, lightingShininess);
        }
        vec3 fireDiffuse = diffuseStrength * diffuse * surfaceColor;
        vec3 fireSpecular = vec3(specularStrength * specular);
        vec3 fireContribution = fireShadow * (fire / 3.0) * (fireDiffuse + fireSpecular);
        color += fireContribution;
    }
    fragColor = vec4(color, 1.0);
}
