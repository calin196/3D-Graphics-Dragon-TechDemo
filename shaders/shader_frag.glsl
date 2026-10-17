#version 410

layout(std140) uniform Material // Must match the GPUMaterial defined in src/mesh.h
{
    vec3 kd;
	vec3 ks;
	float shininess;
	float transparency;
};

uniform sampler2D colorMap;
uniform bool hasTexCoords;
uniform bool useMaterial;

// Implementation source (C4): AI-generated with OpenAI Codex.
// Reference: Lecture 6 — Texture Mapping, normal maps.
uniform sampler2D normalMap; // Linear vector data on texture unit 1.
uniform bool useNormalMap;
uniform float normalStrength;

// Implementation source (C1): AI-generated with OpenAI Codex.
// Reference: Lecture 4 — Materials and Shading.
// Exercise 3 provides the Blinn-Phong shader scaffold; halfway-vector math follows C1.
uniform bool enableLighting;
uniform vec3 lightDirection; // World-space surface-to-light direction; white light.
uniform vec3 cameraPosition;
uniform float ambientStrength;
uniform float diffuseStrength;
uniform float specularStrength;
uniform float lightingShininess; // Separate from the existing Material block.

// Implementation source (C2): AI-generated with OpenAI Codex.
// Reference: Lecture 9 — Advanced Materials, microfacet BRDF.
uniform int shadingModel; // 0: unchanged C1 Blinn-Phong; 1: metallic-roughness PBR.
uniform float pbrMetallic;
uniform float pbrRoughness;

// Implementation source (C6): AI-generated with OpenAI Codex.
// Reference: Lecture 6 — Texture Mapping; Lecture 9 — Advanced Materials.
uniform sampler2D roughnessMap; // Linear scalar data on unit 4.
uniform sampler2D metallicMap; // Linear scalar data on unit 5.
uniform bool usePbrPropertyMaps;

// Implementation source (C5): AI-generated with OpenAI Codex.
// Reference: Lecture 6 — Texture Mapping, environment maps.
uniform samplerCube environmentMap; // Linear radiance on texture unit 2.
uniform bool enableEnvironmentReflections; // Explicitly enabled only for gold draws in PBR.
uniform float reflectionStrength;
uniform float environmentMaxLod;

// Implementation source (D2): AI-generated with OpenAI Codex.
// Reference: Lecture 7 — Real-Time Shadows; Exercise 4 — Shadow Mapping.
uniform sampler2D shadowMap; // Raw depth on texture unit 3.
uniform mat4 lightViewProjection;
uniform bool enableShadows;
uniform bool enableShadowPcf;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

layout(location = 0) out vec4 fragColor;

// C2: r is perceptual roughness, clamped to [0.08, 1].
// GGX uses alpha = r*r and alphaSquared = r^4, not r^2, in D.
const float PI = 3.14159265359;

vec3 safeNormalize(vec3 value)
{
    return value / max(length(value), 1e-6);
}

// Compare depths before filtering: conventional 3x3 percentage-closer filtering.
float directionalVisibility(vec3 geometricNormal)
{
    if (!enableShadows)
        return 1.0;
    vec4 clip = lightViewProjection * vec4(fragPosition, 1.0);
    if (abs(clip.w) < 1e-6)
        return 1.0;
    vec3 projected = clip.xyz / clip.w * 0.5 + 0.5;
    if (any(lessThan(projected, vec3(0.0))) || any(greaterThan(projected, vec3(1.0))))
        return 1.0;
    // Bias follows the geometric surface slope, independently of the scale normal map.
    float NdotL = max(dot(geometricNormal, safeNormalize(lightDirection)), 0.0);
    float depth = projected.z - max(0.00005, 0.0003 * (1.0 - NdotL));
    if (!enableShadowPcf)
        return depth <= textureLod(shadowMap, projected.xy, 0.0).r ? 1.0 : 0.0;
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            visibility += depth <= textureLod(shadowMap, projected.xy + vec2(x, y) * texel, 0.0).r ? 1.0 : 0.0;
    return visibility / 9.0;
}

float distributionGGX(float NdotH, float r)
{
    float alpha = r * r;
    float alphaSquared = alpha * alpha;
    // This form avoids subtracting nearly equal values when NdotH is near 1.
    float n2 = NdotH * NdotH;
    float denominator = (1.0 - n2) + alphaSquared * n2;
    return alphaSquared / max(PI * denominator * denominator, 1e-12);
}

float geometrySchlickGGX(float NdotDirection, float r)
{
    // Standard direct-light Schlick remap k=(r+1)^2/8, using the same r as D.
    float k = (r + 1.0) * (r + 1.0) / 8.0;
    return NdotDirection / max(NdotDirection * (1.0 - k) + k, 1e-6);
}

float geometrySmith(float NdotV, float NdotL, float r)
{
    return geometrySchlickGGX(NdotV, r) * geometrySchlickGGX(NdotL, r);
}

vec3 fresnelSchlick(float VdotH, vec3 F0)
{
    return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// Existing textures use GL_RGB/RGBA, not sRGB formats; FRAMEBUFFER_SRGB is disabled.
// Treat authored colors/samples as sRGB. Convert only PBR, preserving C1/unlit output.
vec3 srgbToLinear(vec3 color)
{
    color = clamp(color, 0.0, 1.0);
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)),
        greaterThan(color, vec3(0.04045)));
}

vec3 linearToSrgb(vec3 color)
{
    color = max(color, vec3(0.0));
    return mix(12.92 * color, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055,
        greaterThan(color, vec3(0.0031308)));
}

vec3 cookTorrance(vec3 baseColor, vec3 N, vec3 L, vec3 V, float visibility, float metallic, float roughness)
{
    metallic = clamp(metallic, 0.0, 1.0);
    float r = clamp(roughness, 0.08, 1.0);
    float NdotL = clamp(dot(N, L), 0.0, 1.0);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    // Modest readability fill, not physically accurate environment lighting or BRDF diffuse.
    vec3 result = 0.08 * baseColor;
    vec3 halfway = L + V;
    if (NdotL > 0.0 && NdotV > 0.0 && dot(halfway, halfway) > 1e-12) {
        vec3 H = safeNormalize(halfway);
        vec3 F0 = mix(vec3(0.04), baseColor, metallic);
        vec3 F = fresnelSchlick(clamp(dot(V, H), 0.0, 1.0), F0);
        float D = distributionGGX(clamp(dot(N, H), 0.0, 1.0), r);
        float G = geometrySmith(NdotV, NdotL, r);
        vec3 specularBRDF = D * G * F / max(4.0 * NdotV * NdotL, 1e-6);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 diffuseBRDF = kD * baseColor / PI;
        // Single white directional light: fixed linear radiance, independent of camera.
        vec3 radiance = vec3(3.0);
        vec3 direct = (diffuseBRDF + specularBRDF) * radiance * NdotL;
        result += direct * visibility; // Ambient above and C5 reflections remain unshadowed.
    }
    return result;
}

// C4: reconstruct +U tangent / +V bitangent in world space from screen derivatives.
// This derivative-based implementation is AI-generated, not copied from a practical.
vec3 dragonShadingNormal(vec3 N)
{
    vec3 dx = dFdx(fragPosition);
    vec3 dy = dFdy(fragPosition);
    vec2 uvDx = dFdx(fragTexCoord);
    vec2 uvDy = dFdy(fragTexCoord);
    if (!useNormalMap || normalStrength <= 0.0 || dot(N, N) < 0.5)
        return N;

    float determinant = uvDx.x * uvDy.y - uvDx.y * uvDy.x;
    float uvScale = length(uvDx) * length(uvDy);
    if (!(uvScale > 1e-20) || !(abs(determinant) > 1e-5 * uvScale))
        return N;
    // Only the determinant sign is needed before normalization; avoid dividing by it.
    vec3 T = (dx * uvDy.y - dy * uvDx.y) * sign(determinant);
    vec3 rawB = (dy * uvDx.x - dx * uvDy.x) * sign(determinant);
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

    // RGB [0,1] -> tangent XYZ [-1,1]; no sRGB decoding for vectors.
    vec3 tangentNormal = texture(normalMap, fragTexCoord).xyz * 2.0 - 1.0;
    tangentNormal.xy *= normalStrength;
    vec3 mapped = mat3(T, B, N) * tangentNormal;
    float length2 = dot(mapped, mapped);
    return length2 > 1e-12 ? mapped * inversesqrt(length2) : N;
}

// C5: basic environment specular approximation, separate from the C2 direct light.
// Reference: Lecture 9 — Advanced Materials, Fresnel reflectance.
vec3 environmentSpecular(vec3 baseColor, vec3 N, vec3 V, float metallic, float roughness)
{
    N = safeNormalize(N);
    V = safeNormalize(V);
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    if (NdotV <= 0.0)
        return vec3(0.0);
    // V points toward the world-space eye; the incident ray is -V.
    vec3 R = reflect(-V, N);
    metallic = clamp(metallic, 0.0, 1.0);
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    vec3 F = fresnelSchlick(NdotV, F0);
    // Ordinary mipmaps approximate roughness blur, NOT a GGX-prefiltered IBL map.
    float lod = clamp(roughness, 0.08, 1.0) * environmentMaxLod;
    vec3 radiance = textureLod(environmentMap, R, lod).rgb;
    return radiance * F * metallic * clamp(reflectionStrength, 0.0, 1.0);
}

void main()
{
    vec3 N = dot(fragNormal, fragNormal) > 0.0 ? normalize(fragNormal) : vec3(0.0);
    // Preserve the existing texture / material / signed-normal color selection.
    vec3 surfaceColor;
    if (hasTexCoords)     surfaceColor = texture(colorMap, fragTexCoord).rgb;
    else if (useMaterial) surfaceColor = kd;
    else                  surfaceColor = N;

    if (!enableLighting) {
        fragColor = vec4(surfaceColor, 1.0);
        return;
    }

    float visibility = directionalVisibility(N); // Use geometric N for bias before C4 perturbation.
    N = dragonShadingNormal(N); // Shared lighting normal; surface/debug colors above stay geometric.

    if (shadingModel == 1) {
        // Resolve material properties once; direct light and C5 reflections share them.
        float roughness = clamp(pbrRoughness, 0.08, 1.0);
        float metallic = clamp(pbrMetallic, 0.0, 1.0);
        if (usePbrPropertyMaps) {
            roughness = clamp(texture(roughnessMap, fragTexCoord).r, 0.08, 1.0);
            metallic = clamp(texture(metallicMap, fragTexCoord).r, 0.0, 1.0);
        }
        vec3 linearColor = srgbToLinear(surfaceColor);
        vec3 lit = cookTorrance(linearColor, N, safeNormalize(lightDirection),
            safeNormalize(cameraPosition - fragPosition), visibility, metallic, roughness);
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
        // Avoid normalizing zero when the eye and light directions oppose.
        if (dot(halfway, halfway) > 1e-8) {
            vec3 H = normalize(halfway);
            specularFactor = pow(max(dot(N, H), 0.0), lightingShininess);
        }
    }

    vec3 color = (ambientStrength + diffuseStrength * diffuseFactor) * surfaceColor
        + vec3(specularStrength * specularFactor);
    if (visibility < 1.0)
        color = ambientStrength * surfaceColor + visibility * (diffuseStrength * diffuseFactor * surfaceColor
            + vec3(specularStrength * specularFactor));
    fragColor = vec4(color, 1.0);
}
