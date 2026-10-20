#version 410

uniform mat4 mvpMatrix;
uniform mat4 modelMatrix;

uniform mat3 normalModelMatrix;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;

layout(location = 3) in ivec2 boneIndices;
layout(location = 4) in vec2 boneWeights;
uniform bool useSkinning;
uniform mat4 boneMatrices[8];

layout(location = 5) in vec3 mouthOffset;
uniform bool useMouthMorph;
uniform float mouthOpen;
uniform mat3 mouthNormalMatrix;

mat4 skinTransform()
{
    mat4 firstBoneContribution = boneWeights.x * boneMatrices[boneIndices.x];
    mat4 secondBoneContribution = boneWeights.y * boneMatrices[boneIndices.y];
    return firstBoneContribution + secondBoneContribution;
}

void main()
{
    mat4 skin = mat4(1.0);
    if (useSkinning)
        skin = skinTransform();
    vec3 morphedPosition = position;
    vec3 localNormal = normal;
    if (useMouthMorph) {
        // Blend closed/open vertex targets before the existing head/world transformations.
        morphedPosition += mouthOpen * mouthOffset;
        localNormal = normalize(mouthNormalMatrix * normal);
    }
    vec4 localPosition = skin * vec4(morphedPosition, 1.0);
    if (useSkinning) {
        mat3 skinLinearTransform = mat3(skin);
        mat3 skinNormalTransform = transpose(inverse(skinLinearTransform));
        localNormal = normalize(skinNormalTransform * localNormal);
    }
    gl_Position = mvpMatrix * localPosition;

    fragPosition = (modelMatrix * localPosition).xyz;
    fragNormal = normalModelMatrix * localNormal;
    fragTexCoord = texCoord;
}
