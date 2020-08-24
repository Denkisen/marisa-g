#ifndef __CPU_MARISAG_VISUAL_ENGINE_H
#define __CPU_MARISAG_VISUAL_ENGINE_H

#include "../VK-nn/Vulkan/Instance.h"
#include "../VK-nn/Vulkan/Device.h"
#include "../VK-nn/Vulkan/SwapChain.h"
#include "../VK-nn/Vulkan/GraphicPipeline.h"
#include "../VK-nn/Vulkan/RenderPass.h"
#include "../VK-nn/Vulkan/Descriptors.h"
#include "../VK-nn/Vulkan/Buffer.h"
#include "../VK-nn/Vulkan/CommandPool.h"
#include "../VK-nn/Vulkan/Image.h"
#include "../VK-nn/Vulkan/Sampler.h"
#include "../VK-nn/Vulkan/Object.h"
#include "../VK-nn/Vulkan/Vertex.h"

#include "fps.h"
#include "Settings.h"

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
};

class VisualEngine
{
private:
  Settings settings;
  Vulkan::Instance instance;

  std::shared_ptr<Vulkan::Device> device;
  std::shared_ptr<Vulkan::SwapChain> swapchain;
  std::shared_ptr<Vulkan::RenderPass> render_pass;
  std::shared_ptr<Vulkan::GraphicPipeline> g_pipeline;

  std::vector<std::shared_ptr<Vulkan::Buffer<World>>> world_uniform_buffers;

  std::shared_ptr<Vulkan::Descriptors> descriptors;
  std::shared_ptr<Vulkan::CommandPool> command_pool;
  std::vector<Vulkan::BufferLock> buffer_locks;

  std::shared_ptr<Vulkan::Object> girl;
  
  size_t frames_in_pipeline = 0;
  size_t current_frame = 0;
  Fps fps;
  GLFWwindow *window;
  std::thread event_handler_thread;
  std::vector<Vulkan::ShaderInfo> shader_infos;
  std::string exec_directory = "";
 
  std::vector<VkSemaphore> image_available_semaphores;
  std::vector<VkSemaphore> render_finished_semaphores;
  std::vector<VkFence> in_queue_fences;
  std::vector<VkFence> images_in_process;

  bool resize_flag;
  bool up_key_down = false;
  bool down_key_down = false;
  glm::vec3 girl_pos;

  void WriteCommandBuffers();
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