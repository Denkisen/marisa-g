#include "engine.h"

Vulkan::Instance VisualEngine::instance;
std::shared_ptr<Vulkan::Device> VisualEngine::device;
std::shared_ptr<Vulkan::SwapChain> VisualEngine::swapchain;
std::shared_ptr<Vulkan::RenderPass> VisualEngine::render_pass;
std::shared_ptr<Vulkan::GraphicPipeline> VisualEngine::g_pipeline;
std::shared_ptr<Vulkan::TransferArray<Vertex>> VisualEngine::input_vertex_array;
VkCommandPool VisualEngine::command_pool;
std::vector<VkCommandBuffer> VisualEngine::command_buffers;
std::vector<VkSemaphore> VisualEngine::image_available_semaphores;
std::vector<VkSemaphore> VisualEngine::render_finished_semaphores;
std::vector<VkFence> VisualEngine::in_queue_fences;
std::vector<VkFence> VisualEngine::images_in_process;
std::vector<Vulkan::ShaderInfo> VisualEngine::shader_infos;
size_t VisualEngine::height = 768;
size_t VisualEngine::width = 1024;
size_t VisualEngine::frames_in_pipeline = 10;
size_t VisualEngine::current_frame = 0;
GLFWwindow * VisualEngine::window = nullptr;
std::thread VisualEngine::event_handler_thread;

bool VisualEngine::resize_flag = false;

VisualEngine::VisualEngine()
{    
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(width, height, instance.AppName().c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, FrameBufferResizeCallback);

  device = std::make_shared<Vulkan::Device>(Vulkan::PhysicalDeviceType::Discrete, Vulkan::QueueType::DrawingType, window);
  swapchain = std::make_shared<Vulkan::SwapChain>(device);
  render_pass = std::make_shared<Vulkan::RenderPass>(device, swapchain);
  shader_infos.resize(2);
  shader_infos[0] = {"main", "bin/vert.spv", Vulkan::ShaderType::Vertex};
  shader_infos[1] = {"main", "bin/frag.spv", Vulkan::ShaderType::Fragment};

  g_pipeline = std::make_shared<Vulkan::GraphicPipeline>(device, swapchain, render_pass);
  command_pool = Vulkan::Supply::CreateCommandPool(device->GetDevice(), device->GetGraphicFamilyQueueIndex().value());

  std::vector<Vertex> vertices = 
  {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
  };

  std::vector<VertexDescription> vertex_descriptions = 
  {
    {offsetof(Vertex, pos), VK_FORMAT_R32G32_SFLOAT},
    {offsetof(Vertex, color), VK_FORMAT_R32G32B32_SFLOAT}
  };
  input_vertex_array = std::make_shared<Vulkan::TransferArray<Vertex>>(device, Vulkan::StorageType::Vertex);
  *input_vertex_array = vertices;

  std::vector<VkVertexInputBindingDescription> binding_description(1);
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
  GetVertexInputBindingDescription(0, vertex_descriptions, binding_description[0], attribute_descriptions);

  g_pipeline->SetShaderInfos(shader_infos);
  g_pipeline->SetVertexInputBindingDescription(binding_description, attribute_descriptions);

  WriteCommandBuffers();

  input_vertex_array->MoveData(command_pool);

  VkSemaphoreCreateInfo semaphore_info = {};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  image_available_semaphores.resize(frames_in_pipeline);
  render_finished_semaphores.resize(frames_in_pipeline);
  in_queue_fences.resize(frames_in_pipeline);
  images_in_process.resize(swapchain->GetImageViewsCount(), VK_NULL_HANDLE);

  for (size_t i = 0; i < frames_in_pipeline; ++i)
  {
    if (vkCreateSemaphore(device->GetDevice(), &semaphore_info, nullptr, &render_finished_semaphores[i]) != VK_SUCCESS)
      throw std::runtime_error("failed to create semaphores!");
    if (vkCreateSemaphore(device->GetDevice(), &semaphore_info, nullptr, &image_available_semaphores[i]) != VK_SUCCESS)
      throw std::runtime_error("failed to create semaphores!");
    if (vkCreateFence(device->GetDevice(), &fence_info, nullptr, &in_queue_fences[i]) != VK_SUCCESS)
      throw std::runtime_error("failed to create fence!");
  }

  event_handler_thread = std::thread(EventHadler);
}

VisualEngine::~VisualEngine()
{
  event_handler_thread.join();

  for (size_t i = 0; i < frames_in_pipeline; ++i)
  {
    if (in_queue_fences[i] != VK_NULL_HANDLE)
      vkDestroyFence(device->GetDevice(), in_queue_fences[i], nullptr);
    if (image_available_semaphores[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(device->GetDevice(), image_available_semaphores[i], nullptr);    
    if (render_finished_semaphores[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(device->GetDevice(), render_finished_semaphores[i], nullptr);
  }

  if (command_pool != VK_NULL_HANDLE)
    vkDestroyCommandPool(device->GetDevice(), command_pool, nullptr);
  glfwDestroyWindow(window);
  glfwTerminate();
}

void VisualEngine::EventHadler()
{
  while (!glfwWindowShouldClose(window)) 
  {
    glfwPollEvents();
    DrawFrame();
  }
  vkDeviceWaitIdle(device->GetDevice());
}

void VisualEngine::FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
  auto app = reinterpret_cast<VisualEngine*>(glfwGetWindowUserPointer(window));
  app->resize_flag = true;
}

void VisualEngine::WriteCommandBuffers()
{
  if (!command_buffers.empty())
    vkFreeCommandBuffers(device->GetDevice(), command_pool, (uint32_t) command_buffers.size(), command_buffers.data());

  command_buffers = Vulkan::Supply::CreateCommandBuffers(device->GetDevice(), 
                                                        command_pool, 
                                                        render_pass->GetFrameBuffersCount(), 
                                                        VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  auto f_buffers = render_pass->GetFrameBuffers();

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  begin_info.pInheritanceInfo = nullptr;

  VkRenderPassBeginInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = render_pass->GetRenderPass();    
  render_pass_info.renderArea.offset = {0, 0};
  render_pass_info.renderArea.extent = swapchain->GetExtent();

  VkClearValue clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
  render_pass_info.clearValueCount = 1;
  render_pass_info.pClearValues = &clear_color;

  VkBuffer vertex_buffers[] = { input_vertex_array->GetDstBuffer() };
  VkDeviceSize offsets[] = { 0 };

  for (size_t i = 0; i < command_buffers.size(); ++i)
  {
    if (vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    render_pass_info.framebuffer = f_buffers[i];
    vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);
    vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline->GetPipeline());

    vkCmdDraw(command_buffers[i], input_vertex_array->GetElementsCount(), 1, 0, 0);
    vkCmdEndRenderPass(command_buffers[i]);
    if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) 
    {
      throw std::runtime_error("Failed to record command buffer!");
    }
  }
}

void VisualEngine::DrawFrame()
{
  uint32_t image_index = 0;
  vkWaitForFences(device->GetDevice(), 1, &in_queue_fences[current_frame], VK_TRUE, UINT64_MAX);
  VkResult res = vkAcquireNextImageKHR(device->GetDevice(), swapchain->GetSwapChain(), UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);

  if (res != VK_SUCCESS)
  {
    if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR || resize_flag)
    {
      int width = 0, height = 0;
      do
      {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
      } while (width == 0 || height == 0);
      
      vkDeviceWaitIdle(device->GetDevice());
      resize_flag = false;
      swapchain->ReBuildSwapChain();
      render_pass->ReBuildRenderPass();
      g_pipeline->ReBuildPipeline();

      WriteCommandBuffers();
      return;
    }
    else
      throw std::runtime_error("failed to acquire swap chain image!");
  }

  if (images_in_process[image_index] != VK_NULL_HANDLE) 
  {
    vkWaitForFences(device->GetDevice(), 1, &images_in_process[image_index], VK_TRUE, UINT64_MAX);
  }

  images_in_process[image_index] = in_queue_fences[image_index];

  VkSemaphore wait_semaphores[] = { image_available_semaphores[current_frame] };  
  VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };
  VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; 
  VkSwapchainKHR swapchains[] = { swapchain->GetSwapChain() };
  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffers[image_index];
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_semaphores;  

  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = signal_semaphores;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.pImageIndices = &image_index;
  present_info.pResults = nullptr;

  vkResetFences(device->GetDevice(), 1, &in_queue_fences[current_frame]);
  if (vkQueueSubmit(device->GetGraphicQueue(), 1, &submit_info, in_queue_fences[current_frame]) != VK_SUCCESS) 
  {
    throw std::runtime_error("failed to submit draw command buffer!");
  }
  vkQueuePresentKHR(device->GetPresentQueue(), &present_info);

  current_frame = (current_frame + 1) % frames_in_pipeline;
}

void VisualEngine::GetVertexInputBindingDescription(uint32_t binding, std::vector<VertexDescription> vertex_descriptions, VkVertexInputBindingDescription &out_binding_description, std::vector<VkVertexInputAttributeDescription> &out_attribute_descriptions)
{
  if (vertex_descriptions.empty())
    throw std::runtime_error("Vertex description is empty.");
    
  out_binding_description.binding = binding;
  out_binding_description.stride = sizeof(Vertex);
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