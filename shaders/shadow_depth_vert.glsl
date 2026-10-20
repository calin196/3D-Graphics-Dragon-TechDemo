#version 410

layout(location = 0) in vec3 position;
uniform mat4 lightViewProjection;
uniform mat4 modelMatrix;

layout(location = 3) in ivec2 boneIndices;
layout(location = 4) in vec2 boneWeights;
uniform bool useSkinning;
uniform mat4 boneMatrices[8];

layout(location = 5) in vec3 mouthOffset;
uniform bool useMouthMorph;
uniform float mouthOpen;

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
    if (useMouthMorph) {
        // Use the same vertex blend as the visible jaw for both shadow lights.
        morphedPosition += mouthOpen * mouthOffset;
    }
    vec4 localPosition = skin * vec4(morphedPosition, 1.0);
    gl_Position = lightViewProjection * modelMatrix * localPosition;
}
