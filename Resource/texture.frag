#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform texture2D testTexture;
layout(binding = 2) uniform sampler testSampler;

void main()
{
    outColor = texture(sampler2D(testTexture, texSampler), fragTexCoord);
}