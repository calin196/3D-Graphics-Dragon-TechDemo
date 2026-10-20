#version 410

uniform sampler2D scene;
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

vec3 bright(vec2 coordinate)
{

    vec3 sceneSample = texture(scene, coordinate).rgb;
    vec3 color = decode(sceneSample);
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    const float threshold = 1.0, knee = 0.2;
    float soft = clamp(luminance - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float fraction = max(luminance - threshold, soft) / max(luminance, 1e-6);

    return min(color * fraction, vec3(12.0));
}

void main()
{
    ivec2 sceneSize = textureSize(scene, 0);
    vec2 offset = 0.5 / vec2(sceneSize);
    vec2 lowerLeftUV = uv + vec2(-offset.x, -offset.y);
    vec2 lowerRightUV = uv + vec2(offset.x, -offset.y);
    vec2 upperLeftUV = uv + vec2(-offset.x, offset.y);
    vec2 upperRightUV = uv + vec2(offset.x, offset.y);
    vec3 lowerLeft = bright(lowerLeftUV);
    vec3 lowerRight = bright(lowerRightUV);
    vec3 upperLeft = bright(upperLeftUV);
    vec3 upperRight = bright(upperRightUV);
    vec3 color = lowerLeft + lowerRight + upperLeft + upperRight;
    fragColor = vec4(color * 0.25, 1.0);
}
