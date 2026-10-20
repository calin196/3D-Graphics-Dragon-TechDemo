#version 410

uniform sampler2D image;
uniform bool horizontal;
in vec2 uv;
layout(location = 0) out vec4 fragColor;

void main()
{
    const float weights[5] = float[5](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);
    ivec2 imageSize = textureSize(image, 0);
    vec2 texelSize = 1.0 / vec2(imageSize);
    vec2 axis;
    if (horizontal) {
        axis = vec2(texelSize.x, 0.0);
    } else {
        axis = vec2(0.0, texelSize.y);
    }
    vec3 result = texture(image, uv).rgb * weights[0];
    for (int i = 1; i < 5; ++i) {
        vec2 sampleOffset = axis * float(i);
        vec2 positiveUV = uv + sampleOffset;
        vec3 positiveColor = texture(image, positiveUV).rgb;
        result += positiveColor * weights[i];
        vec2 negativeUV = uv - sampleOffset;
        vec3 negativeColor = texture(image, negativeUV).rgb;
        result += negativeColor * weights[i];
    }
    fragColor = vec4(result, 1.0);
}
