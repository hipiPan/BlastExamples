#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 1000) uniform texture2D testTexture;
layout(binding = 3000) uniform sampler testSampler;

void main()
{
    outColor = texture(sampler2D(testTexture, testSampler), fragTexCoord);
}