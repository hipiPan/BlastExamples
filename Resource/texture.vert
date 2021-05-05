#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(std140, binding = 0) uniform testUniformBuffer
{
    uniform float padding0;
    uniform float padding1;
    uniform float padding2;
};

void main() 
{
    gl_Position = vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}