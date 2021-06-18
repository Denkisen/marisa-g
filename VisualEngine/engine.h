#ifndef __CPU_MARISAG_VISUAL_ENGINE_H
#define __CPU_MARISAG_VISUAL_ENGINE_H

#include "../VK-nn/Vulkan/Instance.h"
#include "../VK-nn/Vulkan/Device.h"
#include "../VK-nn/Vulkan/Surface.h"
#include "../VK-nn/Vulkan/SwapChain.h"
#include "../VK-nn/Vulkan/Pipelines.h"
#include "../VK-nn/Vulkan/RenderPass.h"
#include "../VK-nn/Vulkan/Descriptors.h"
#include "../VK-nn/Vulkan/StorageArray.h"
#include "../VK-nn/Vulkan/ImageArray.h"
#include "../VK-nn/Vulkan/CommandPool.h"
#include "../VK-nn/Vulkan/Sampler.h"
#include "../VK-nn/Vulkan/Fence.h"
#include "../VK-nn/Vulkan/Semaphore.h"

#include "Settings.h"
#include "TestObject.h"

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <thread>

struct World 
{
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
  glm::vec4 light;
};

class VisualEngine
{
private:
  Settings settings;

  std::shared_ptr<Vulkan::Device> device;
  std::shared_ptr<Vulkan::SwapChain> swapchain;
  std::shared_ptr<Vulkan::Surface> surface;
  std::shared_ptr<Vulkan::RenderPass> render_pass;
  Vulkan::Pipelines pipelines;

  std::shared_ptr<Vulkan::StorageArray> storage_buffers;
  std::shared_ptr<Vulkan::ImageArray> render_pass_bufers;
  std::shared_ptr<Vulkan::Descriptors> descriptors;
  std::shared_ptr<Vulkan::CommandPool> command_pool;

  std::unique_ptr<TestObject> girl;
  
  size_t frames_in_pipeline = 0;
  size_t current_frame = 0;
  std::chrono::_V2::system_clock::time_point priv_frame_time;
  std::thread event_handler_thread;
  std::string exec_directory = "";
 
  std::unique_ptr<Vulkan::SemaphoreArray> image_available_semaphores;
  std::unique_ptr<Vulkan::SemaphoreArray> render_finished_semaphores;
  std::vector<VkFence> exec_fences;
  std::vector<VkFence> in_process;

  bool resize_flag;
  bool up_key_down = false;
  bool down_key_down = false;
  bool left_key_down = false;
  bool right_key_down = false;

  void UpdateCommandBuffers();
  void DrawFrame();
  void EventHadler();
  void PrepareShaders();
  void PrepareWindow();
  void PrepareSyncPrimitives();
  void ReBuildPipelines();
  void UpdateWorldUniformBuffers(uint32_t image_index);

  static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);  
  static void KeyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void Draw(VisualEngine &obj);
public:
  VisualEngine() = delete;
  VisualEngine(int argc, char const *argv[]);
  VisualEngine(const VisualEngine &obj) = delete;
  VisualEngine& operator= (const VisualEngine &obj) = delete;
  void Start();
  ~VisualEngine();
};

#endif