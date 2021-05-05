#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform texture2D testTexture;
layout(binding = 2) uniform sampler testSampler;

layout(std140, binding = 0) uniform testUniformBuffer
{
    uniform float padding0;
    uniform float padding1;
    uniform float padding2;
};

void main()
{
    outColor = texture(sampler2D(testTexture, testSampler), fragTexCoord);
}