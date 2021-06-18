#include "TestObject.h"
#include "../VK-nn/libs/ImageBuffer.h"
#include "../VK-nn/Vulkan/CommandPool.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../VK-nn/libs/tiny_obj_loader.h"

#include <vector>
#include <optional>
#include <unordered_map>

TestObject::TestObject(const std::shared_ptr<Vulkan::Device> dev)
{
  data = std::make_unique<Vulkan::StorageArray>(dev);
  texture = std::make_unique<Vulkan::ImageArray>(dev);
  sampler = std::make_unique<Vulkan::Sampler>(dev, Vulkan::SamplerConfig());
  rotation = glm::mat4(1.0f);
}

TestObject::~TestObject()
{

}

bool TestObject::LoadModel(const std::filesystem::path obj_file, const std::filesystem::path materials_directory)
{
  if (!std::filesystem::exists(obj_file) || !obj_file.has_filename() || obj_file.extension() != ".obj")
  {
    return false;
  }

  std::vector<uint32_t> indices;
  std::unordered_map<Vertex, uint32_t> vertices;
  std::vector<Vertex> vertices_buff;

  if (obj_file.extension() == ".obj")
  {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, obj_file.c_str(), materials_directory.c_str()))
    {
      return false;
    }

    for (const auto& shape : shapes)
    {
      for (const auto& index : shape.mesh.indices)
      {
        Vertex vertex = {};
        vertex.pos =
        {
          attrib.vertices[3 * index.vertex_index + 0],
          attrib.vertices[3 * index.vertex_index + 1],
          attrib.vertices[3 * index.vertex_index + 2]
        };

        vertex.normal =
        {
          attrib.normals[3 * index.normal_index + 0],
          attrib.normals[3 * index.normal_index + 1],
          attrib.normals[3 * index.normal_index + 2],
        };

        vertex.texCoord =
        {
          attrib.texcoords[2 * index.texcoord_index + 0],
          1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
        };

        vertex.color = { 1.0f, 0.0f, 1.0f };

        if (vertices.count(vertex) == 0)
        {
          vertices[vertex] = uint32_t(vertices_buff.size());
          vertices_buff.push_back(vertex);
        }

        indices.push_back(vertices[vertex]);
      }
    }
  }

  data->StartConfig(Vulkan::HostVisibleMemory::HostInvisible);
  data->AddBuffer(Vulkan::BufferConfig().SetType(Vulkan::StorageType::Vertex).AddSubBuffer(vertices_buff));
  data->AddBuffer(Vulkan::BufferConfig().SetType(Vulkan::StorageType::Index).AddSubBuffer(indices));
  if (data->EndConfig() != VK_SUCCESS)
  {
    return false;
  }

  Vulkan::StorageArray src_buffer(data->GetDevice());
  src_buffer.StartConfig(Vulkan::HostVisibleMemory::HostVisible);
  src_buffer.AddBuffer(Vulkan::BufferConfig().SetType(Vulkan::StorageType::Storage)
                      .AddSubBuffer(vertices_buff).AddSubBuffer(indices));
  if (src_buffer.EndConfig() != VK_SUCCESS || 
      src_buffer.SetSubBufferData(0, 0, vertices_buff) != VK_SUCCESS ||
      src_buffer.SetSubBufferData(0, 1, indices))
  {
    return false;
  }

  auto q_index = texture->GetDevice()->GetGraphicFamilyQueueIndex();
  if (!q_index.has_value())
  {
    return false;
  }

  VkBufferCopy copy_region = {};
  Vulkan::CommandPool pool(data->GetDevice(), q_index.value());
  
  copy_region.size = src_buffer.GetInfo(0).sub_buffers[0].size;
  copy_region.srcOffset = src_buffer.GetInfo(0).sub_buffers[0].offset;
  copy_region.dstOffset = data->GetInfo(0).sub_buffers[0].offset;
  pool.GetCommandBuffer(0)
      .BeginCommandBuffer()
      .CopyBufferToBuffer(src_buffer.GetInfo(0).buffer, data->GetInfo(0).buffer, {copy_region});
  copy_region.size = src_buffer.GetInfo(0).sub_buffers[1].size;
  copy_region.srcOffset = src_buffer.GetInfo(0).sub_buffers[1].offset;
  copy_region.dstOffset = data->GetInfo(1).sub_buffers[0].offset;
  pool.GetCommandBuffer(0)
      .CopyBufferToBuffer(src_buffer.GetInfo(0).buffer, data->GetInfo(1).buffer, {copy_region})
      .EndCommandBuffer();

  if (Vulkan::Fence f(texture->GetDevice()); f.IsValid() && pool.IsReady(0) && pool.ExecuteBuffer(0, f.GetFence()) == VK_SUCCESS)
  {
    f.Wait();
    return true;
  }

  return false;
}

bool TestObject::LoadTexture(const std::filesystem::path image_file, const bool enable_mip_levels)
{
  if (!std::filesystem::exists(image_file) || !image_file.has_filename())
  {
    return false;
  }

  ImageBuffer image(image_file.string());
  size_t tex_w = enable_mip_levels ? ((size_t) image.Width() * 2) / 3 : (size_t) image.Width();
  size_t tex_h = (size_t) image.Height();

  texture->StartConfig();
  texture->AddImage(Vulkan::ImageConfig()
                    .PreallocateMipLevels(enable_mip_levels)
                    .SetSize(tex_h, tex_w, image.Channels())
                    .SetMemoryAccess(Vulkan::HostVisibleMemory::HostInvisible)
                    .SetSamplesCount(VK_SAMPLE_COUNT_1_BIT)
                    .SetTiling(Vulkan::ImageTiling::Optimal)
                    .SetType(Vulkan::ImageType::Sampled)
                    .SetFormat(VK_FORMAT_R8G8B8A8_SRGB));
  if (texture->EndConfig() != VK_SUCCESS)
  {
    return false;
  }

  sampler = std::make_unique<Vulkan::Sampler>(sampler->GetDevice(), Vulkan::SamplerConfig().SetLODMax(texture->GetInfo(0).image_info.mipLevels));
  Vulkan::StorageArray src_buffer(data->GetDevice());

  {
    std::vector<uint8_t> raw_data = enable_mip_levels ? image.GetMipLevelsBuffer() : image.Canvas();
    src_buffer.StartConfig();
    src_buffer.AddBuffer(Vulkan::BufferConfig().SetType(Vulkan::StorageType::Storage)
      .AddSubBuffer(raw_data.size(), sizeof(decltype(raw_data)::value_type)));
    if (src_buffer.EndConfig() != VK_SUCCESS)
    {
      return false;
    }

    if (src_buffer.SetBufferData(0, raw_data) != VK_SUCCESS)
    {
      return false;
    }
  }

  std::vector<VkBufferImageCopy> image_regions(texture->GetInfo(0).image_info.mipLevels);
  uint32_t buffer_offset = 0;
  for (size_t i = 0; i < image_regions.size(); ++i)
  {
    image_regions[i].bufferOffset = buffer_offset;
    image_regions[i].bufferRowLength = (uint32_t)tex_w;
    image_regions[i].bufferImageHeight = (uint32_t)tex_h;

    image_regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_regions[i].imageSubresource.mipLevel = i;
    image_regions[i].imageSubresource.baseArrayLayer = 0;
    image_regions[i].imageSubresource.layerCount = 1;

    image_regions[i].imageOffset = { 0, 0, 0 };
    image_regions[i].imageExtent =
    {
      (uint32_t)tex_w,
      (uint32_t)tex_h,
      1
    };

    buffer_offset += tex_w * tex_h * image.Channels();
    if (tex_w > 1) tex_w /= 2;
    if (tex_h > 1) tex_h /= 2;
  }

  auto q_index = texture->GetDevice()->GetGraphicFamilyQueueIndex();
  if (!q_index.has_value())
  {
    return false;
  }

  Vulkan::CommandPool pool(texture->GetDevice(), q_index.value());

  pool.GetCommandBuffer(0)
      .BeginCommandBuffer()
      .ImageLayoutTransition(*texture, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, true)
      .CopyBufferToImage(src_buffer.GetInfo(0).buffer, *texture, 0, image_regions)
      .ImageLayoutTransition(*texture, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, true)
      .EndCommandBuffer();

  if (Vulkan::Fence f(texture->GetDevice()); f.IsValid() && pool.IsReady(0) && pool.ExecuteBuffer(0, f.GetFence()) == VK_SUCCESS)
  {
    f.Wait();
    return true;
  }

  return false;
}

glm::mat4 TestObject::ObjectTransforations()
{
  glm::mat4 result = glm::mat4(1.0f);
  result = glm::translate(glm::mat4(1.0f), position);
  //result = glm::rotate(result, glm::orientedAngle(glm::normalize(direction), glm::vec3(0.0f, 0.0f, -1.0f), up_axis), up_axis);
  return result * rotation;
}

void TestObject::SetPosition(const glm::vec3 pos)
{
  position = pos;
}

void TestObject::SetDirection(const glm::vec3 dir)
{
  direction = dir;
}

void TestObject::Move(const glm::vec3 pos_offset)
{
  position += pos_offset;
}

void TestObject::Rotate(const float x_angle, const float y_angle, const float z_angle)
{
  rotation = glm::rotate(rotation, x_angle, {1.0f, 0.0f, 0.0f});
  rotation = glm::rotate(rotation, y_angle, {0.0f, 1.0f, 0.0f});
  rotation = glm::rotate(rotation, z_angle, {0.0f, 0.0f, 1.0f});
  // direction = glm::rotateX(direction, glm::radians(x_angle));
  // direction = glm::rotateY(direction, glm::radians(y_angle));
  // direction = glm::rotateZ(direction, glm::radians(z_angle));
}