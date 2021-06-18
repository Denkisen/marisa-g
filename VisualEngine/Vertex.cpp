#include "Vertex.h"

std::pair<VkVertexInputBindingDescription, std::vector<VkVertexInputAttributeDescription>> GetVertexDescription(uint32_t binding)
{
  std::pair<VkVertexInputBindingDescription, std::vector<VkVertexInputAttributeDescription>> result;
  std::vector<VertexDescription> vertex_descriptions =
  {
    {offsetof(Vertex, pos), VK_FORMAT_R32G32B32_SFLOAT},
    {offsetof(Vertex, color), VK_FORMAT_R32G32B32_SFLOAT},
    {offsetof(Vertex, texCoord), VK_FORMAT_R32G32_SFLOAT},
    {offsetof(Vertex, normal), VK_FORMAT_R32G32B32_SFLOAT}
  };
  GetVertexInputBindingDescription<Vertex>(binding, vertex_descriptions, result.first, result.second);

  return result;
}
