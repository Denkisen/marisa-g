#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBuffer 
{
  mat4 model;
  mat4 view;
  mat4 proj;
  vec4 light;
} world;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragLight;

void main() 
{  
  vec3 Ld = {1.0f, 1.0f, 1.0f};
  gl_PointSize = 3.0;
  vec3 pos = inPosition + (gl_InstanceIndex * 2);
  vec4 eye = world.view * world.model * vec4(pos, 1.0);
  vec3 s = normalize(vec3(world.light - eye));
  mat3 normal_matrix = transpose(inverse(mat3(world.model)));
  vec3 norm = normalize(normal_matrix * inNormal);

  fragLight = Ld * max(dot(s, norm), 0.0);
  gl_Position = world.proj * eye;
  //vec3 cl = (inNormal + 1) / 2;
  fragColor = inColor;
  fragTexCoord = inTexCoord;
}