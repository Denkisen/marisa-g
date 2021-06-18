#ifndef __VISUALENGINE_TESTOBJECT_H
#define __VISUALENGINE_TESTOBJECT_H

#include "../VK-nn/Vulkan/ImageArray.h"
#include "../VK-nn/Vulkan/StorageArray.h"
#include "../VK-nn/Vulkan/Sampler.h"
#include "../VK-nn/Vulkan/Fence.h"
#include "Vertex.h"

#include <filesystem>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>

class TestObject
{
private:
  std::unique_ptr<Vulkan::Sampler> sampler;
  std::unique_ptr<Vulkan::ImageArray> texture;
  std::unique_ptr<Vulkan::StorageArray> data;

  glm::vec3 position = { 0.0f, 0.0f, 0.0f };
  glm::vec3 up_axis = { 0.0f, 1.0f, 0.0f };
  glm::vec3 direction = { 0.0f, 0.0f, -1.0f };
  glm::mat4 rotation;
public:
  TestObject() = delete;
  TestObject(const TestObject &obj) = delete;
  TestObject(TestObject &&obj) = delete;
  TestObject &operator=(const TestObject &obj) = delete;
  TestObject &operator=(TestObject &&obj) = delete;
  TestObject(const std::shared_ptr<Vulkan::Device> dev);
  ~TestObject();
  bool LoadModel(const std::filesystem::path obj_file, const std::filesystem::path materials_directory = "");
  bool LoadTexture(const std::filesystem::path image_file, const bool enable_mip_levels);
  VkSampler GetSampler() const { return sampler->GetSampler(); }
  Vulkan::image_t GetTextureInfo() const { return texture->GetInfo(0); }
  Vulkan::buffer_t GetModelVerticesInfo() const { return data->GetInfo(0); }
  Vulkan::buffer_t GetModelIndicesInfo() const { return data->GetInfo(1); }
  glm::mat4 ObjectTransforations();
  void SetPosition(const glm::vec3 pos);
  void SetDirection(const glm::vec3 dir);
  void Move(const glm::vec3 pos_offset);
  void Rotate(const float x_angle, const float y_angle, const float z_angle);
};

#endif
