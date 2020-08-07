#include "engine.h"

VisualEngine::~VisualEngine()
{
#ifdef DEBUG
      std::cout << __func__ << std::endl;
#endif

  for (size_t i = 0; i < frames_in_pipeline; ++i)
  {
    if (!in_queue_fences.empty() && in_queue_fences[i] != VK_NULL_HANDLE)
      vkDestroyFence(device->GetDevice(), in_queue_fences[i], nullptr);
    if (!image_available_semaphores.empty() && image_available_semaphores[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(device->GetDevice(), image_available_semaphores[i], nullptr);    
    if (!render_finished_semaphores.empty() && render_finished_semaphores[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(device->GetDevice(), render_finished_semaphores[i], nullptr);
  }

  if (command_pool != VK_NULL_HANDLE)
    vkDestroyCommandPool(device->GetDevice(), command_pool, nullptr);
  glfwDestroyWindow(window);
  glfwTerminate();
}

VisualEngine::VisualEngine()
{
  glfwInit();
  PrepareWindow();

  std::vector<Vulkan::VertexDescription> vertex_descriptions = 
  {
    {offsetof(Vertex, pos), VK_FORMAT_R32G32_SFLOAT},
    {offsetof(Vertex, color), VK_FORMAT_R32G32B32_SFLOAT}
  };

  std::vector<VkVertexInputBindingDescription> binding_description(1);
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

  device = std::make_shared<Vulkan::Device>(Vulkan::PhysicalDeviceType::Discrete, Vulkan::QueueType::DrawingType, window);
  swapchain = std::make_shared<Vulkan::SwapChain>(device);
  render_pass = std::make_shared<Vulkan::RenderPass>(device, swapchain);
  g_pipeline = std::make_shared<Vulkan::GraphicPipeline>(device, swapchain, render_pass);

  input_vertex_array = std::make_shared<Vulkan::TransferArray<Vertex>>(device, Vulkan::StorageType::Vertex); 
  input_index_array = std::make_shared<Vulkan::TransferArray<uint16_t>>(device, Vulkan::StorageType::Index); 

  descriptors = std::make_shared<Vulkan::Descriptors>(device);
  command_pool = Vulkan::Supply::CreateCommandPool(device->GetDevice(), device->GetGraphicFamilyQueueIndex().value());

  frames_in_pipeline = swapchain->GetImageViewsCount() + 5;

  std::vector<std::shared_ptr<Vulkan::IStorage>> buffs;
  for (size_t i = 0; i < swapchain->GetImageViewsCount(); ++i)
  {
    world_uniform_buffers.push_back(std::make_shared<Vulkan::UniformBuffer<World>>(device));
    buffs.push_back(world_uniform_buffers[world_uniform_buffers.size() - 1]);
  }

  descriptors->Add(buffs, VK_SHADER_STAGE_VERTEX_BIT, true);
  descriptors->Build();
  PrepareShaders();
  g_pipeline->SetShaderInfos(shader_infos); 
 
  Vulkan::Supply::GetVertexInputBindingDescription<Vertex>(0, vertex_descriptions, binding_description[0], attribute_descriptions);
  g_pipeline->SetVertexInputBindingDescription(binding_description, attribute_descriptions);
  g_pipeline->SetDescriptorsSetLayouts(descriptors->GetDescriptorSetLayout(0));

  WriteCommandBuffers();
  const std::vector<Vertex> vertices = 
  {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
  };

  const std::vector<uint16_t> indices = 
  {
    0, 1, 2, 2, 3, 0
  };

  *input_vertex_array = vertices;
  input_vertex_array->MoveData(command_pool);

  *input_index_array = indices;
  input_index_array->MoveData(command_pool);

  PrepareSyncPrimitives();
}

void VisualEngine::Start()
{
  event_handler_thread = std::thread(&VisualEngine::EventHadler, this);
  if (event_handler_thread.joinable())
    event_handler_thread.join();
}

void VisualEngine::EventHadler()
{
  while (!glfwWindowShouldClose(window)) 
  {
    glfwPollEvents();
    Draw(*this);
  }
  vkDeviceWaitIdle(device->GetDevice());
}

void VisualEngine::FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
  auto app = reinterpret_cast<VisualEngine*>(glfwGetWindowUserPointer(window));
  app->resize_flag = true;
}

void VisualEngine::Draw(VisualEngine &obj)
{
  obj.DrawFrame();
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

  VkBuffer vertex_buffers[] = { input_vertex_array->GetBuffer() };
  VkDeviceSize offsets[] = { 0 };
  std::vector<VkDescriptorSet> descriptor_sets = descriptors->GetDescriptorSet(0);

  for (size_t i = 0; i < command_buffers.size(); ++i)
  {
    if (vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    render_pass_info.framebuffer = f_buffers[i];
    vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(command_buffers[i], input_index_array->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
    vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
  
    vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline->GetPipeline());

    vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline->GetPipelineLayout(), 0, 1, &descriptor_sets[i], 0, nullptr);
    vkCmdDrawIndexed(command_buffers[i], input_index_array->GetElementsCount(), 1, 0, 0, 0);
    vkCmdEndRenderPass(command_buffers[i]);
    if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) 
    {
      throw std::runtime_error("Failed to record command buffer!");
    }
  }
}

void VisualEngine::UpdateWorldUniformBuffers(uint32_t image_index)
{
  static auto start_time = std::chrono::high_resolution_clock::now();
  auto current_time = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
  World bf = {};
  bf.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  bf.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  bf.proj = glm::perspective(glm::radians(45.0f), swapchain->GetExtent().width / (float) swapchain->GetExtent().height, 0.1f, 10.0f);
  bf.proj[1][1] *= -1;
  *world_uniform_buffers[image_index] = bf;
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
      ReBuildPipelines();
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

  UpdateWorldUniformBuffers(image_index);

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

void VisualEngine::PrepareShaders()
{
  shader_infos.resize(2);
  shader_infos[0] = {"main", "tri.vert.spv", Vulkan::ShaderType::Vertex};
  shader_infos[1] = {"main", "tri.frag.spv", Vulkan::ShaderType::Fragment};
}

void VisualEngine::PrepareWindow()
{
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window = glfwCreateWindow(width, height, instance.AppName().c_str(), nullptr, nullptr);

  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, FrameBufferResizeCallback);
}

void VisualEngine::PrepareSyncPrimitives()
{
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
}

void VisualEngine::ReBuildPipelines()
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
}

