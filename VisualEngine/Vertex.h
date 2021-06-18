#ifndef __VISUALENGINE_VERTEX_H
#define __VISUALENGINE_VERTEX_H

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>

struct alignas(16) Vertex
{
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;
  glm::vec3 normal;

  bool operator==(const Vertex& other) const
  {
    return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
  }
};

struct VertexDescription
{
  uint32_t offset = 0; // offset in bytes of struct member
  VkFormat format = VK_FORMAT_R32G32_SFLOAT; // struct member format
};

template <class T>
void GetVertexInputBindingDescription(uint32_t binding, std::vector<VertexDescription> vertex_descriptions, VkVertexInputBindingDescription& out_binding_description, std::vector<VkVertexInputAttributeDescription>& out_attribute_descriptions)
{
  if (vertex_descriptions.empty()) return;

  out_binding_description.binding = binding;
  out_binding_description.stride = sizeof(T);
  out_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  out_attribute_descriptions.resize(vertex_descriptions.size());
  for (size_t i = 0; i < out_attribute_descriptions.size(); ++i)
  {
    out_attribute_descriptions[i].binding = binding;
    out_attribute_descriptions[i].location = i;
    out_attribute_descriptions[i].format = vertex_descriptions[i].format;
    out_attribute_descriptions[i].offset = vertex_descriptions[i].offset;
  }
}

std::pair<VkVertexInputBindingDescription, std::vector<VkVertexInputAttributeDescription>> GetVertexDescription(uint32_t binding);

template <class T>
inline void hash_combine(std::size_t & s, const T & v)
{
  std::hash<T> h;
  s^= h(v) + 0x9e3779b9 + (s<< 6) + (s>> 2);
}

namespace std 
{
  template<> struct hash<Vertex> 
  {
    size_t operator()(Vertex const& vertex) const 
    {
      size_t res = 0;
      hash_combine(res, hash<glm::vec3>()(vertex.pos));
      hash_combine(res, hash<glm::vec3>()(vertex.color));
      hash_combine(res, hash<glm::vec2>()(vertex.texCoord));
      hash_combine(res, hash<glm::vec3>()(vertex.normal));

      return res;
    }
  };
}

#endif