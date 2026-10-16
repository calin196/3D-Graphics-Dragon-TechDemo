#version 410

uniform vec3 pathColor;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(pathColor, 1.0);
}
