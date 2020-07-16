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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <optional>

class VisualEngine
{
private:
  static size_t frames_in_pipeline;
  static size_t current_frame;
  static Vulkan::Instance instance;
  static std::shared_ptr<Vulkan::Device> device;
  static std::shared_ptr<Vulkan::SwapChain> swapchain;
  static std::shared_ptr<Vulkan::RenderPass> render_pass;
  static std::shared_ptr<Vulkan::GraphicPipeline> g_pipeline;
  static size_t height;
  static size_t width;
  static GLFWwindow *window;
  static std::thread event_handler_thread;
  static VkCommandPool command_pool;
  static std::vector<VkCommandBuffer> command_buffers;
  static std::vector<VkSemaphore> image_available_semaphores;
  static std::vector<VkSemaphore> render_finished_semaphores;
  static std::vector<VkFence> in_queue_fences;
  static std::vector<VkFence> images_in_process;

  static void EventHadler();

  static void WriteCommandBuffers();
  static void DrawFrame();
public:
  VisualEngine();
  VisualEngine(const VisualEngine &obj) = delete;
  VisualEngine& operator= (const VisualEngine &obj) = delete;
  ~VisualEngine();
};

#endif