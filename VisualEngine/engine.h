#ifndef __CPU_MARISAG_VISUAL_ENGINE_H
#define __CPU_MARISAG_VISUAL_ENGINE_H

#include "../VK-nn/Vulkan/Instance.h"
#include "../VK-nn/Vulkan/Device.h"
#include "../VK-nn/Vulkan/Array.h"
#include "../VK-nn/Vulkan/UniformBuffer.h"
#include "../VK-nn/Vulkan/Offload.h"
#include "../VK-nn/Vulkan/SwapChain.h"
#include "../VK-nn/Vulkan/GraphicPipeline.h"
#include "../VK-nn/Vulkan/RenderPass.h"
#include "../VK-nn/Vulkan/TransferArray.h"
#include "../VK-nn/Vulkan/Descriptors.h"
#include "../VK-nn/Vulkan/IStorage.h"
#include "../VK-nn/Vulkan/CommandPool.h"
#include "../VK-nn/libs/ImageBuffer.h"


#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <optional>
#include <chrono>

struct Vertex
{
  glm::vec2 pos;
  glm::vec3 color;
};

struct World 
{
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

class VisualEngine
{
private:
  size_t frames_in_pipeline = 0;
  size_t current_frame = 0;
  Vulkan::Instance instance;
  std::shared_ptr<Vulkan::Device> device;
  std::shared_ptr<Vulkan::SwapChain> swapchain;
  std::shared_ptr<Vulkan::RenderPass> render_pass;
  std::shared_ptr<Vulkan::GraphicPipeline> g_pipeline;
  std::shared_ptr<Vulkan::TransferArray<Vertex>> input_vertex_array;
  std::shared_ptr<Vulkan::TransferArray<uint16_t>> input_index_array;
  std::vector<std::shared_ptr<Vulkan::UniformBuffer<World>>> world_uniform_buffers;
  std::shared_ptr<Vulkan::Descriptors> descriptors;
  std::shared_ptr<Vulkan::CommandPool> command_pool;
  size_t height = 768;
  size_t width = 1024;
  GLFWwindow *window;
  std::thread event_handler_thread;
  std::vector<VkSemaphore> image_available_semaphores;
  std::vector<VkSemaphore> render_finished_semaphores;
  std::vector<VkFence> in_queue_fences;
  std::vector<VkFence> images_in_process;
  std::vector<Vulkan::ShaderInfo> shader_infos;
  bool resize_flag;

  void WriteCommandBuffers();
  void DrawFrame();
  void EventHadler();
  void PrepareShaders();
  void PrepareWindow();
  void PrepareSyncPrimitives();
  void ReBuildPipelines();
  void UpdateWorldUniformBuffers(uint32_t image_index);

  static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);  
  static void Draw(VisualEngine &obj);
public:
  VisualEngine();
  VisualEngine(const VisualEngine &obj) = delete;
  VisualEngine& operator= (const VisualEngine &obj) = delete;
  void Start();
  ~VisualEngine();
};

#endif