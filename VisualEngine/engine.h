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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <optional>

struct Vertex
{
  float pos[2];
  float color[3];
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
  size_t height = 768;
  size_t width = 1024;
  GLFWwindow *window;
  std::thread event_handler_thread;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> command_buffers;
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