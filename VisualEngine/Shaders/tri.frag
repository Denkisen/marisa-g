#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragLight;

layout(location = 0) out vec4 outColor;
layout(binding = 1) uniform sampler2D texSampler;

void main() {
  outColor = texture(texSampler, fragTexCoord);
  vec3 l = vec3(outColor) * fragLight;
  outColor = vec4(l, outColor[3]);
}