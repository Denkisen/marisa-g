#include "engine.h"

#include <filesystem>

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

  glfwDestroyWindow(window);
  glfwTerminate();
}

VisualEngine::VisualEngine(int argc, char const *argv[])
{
  glfwInit();
  PrepareWindow();

  exec_directory = Vulkan::Supply::GetExecDirectory(argv[0]);

  if (!std::filesystem::exists(exec_directory))
    throw std::runtime_error("argv[0] is not a valid path.");

  std::vector<Vulkan::VertexDescription> vertex_descriptions = 
  {
    {offsetof(Vertex, pos), VK_FORMAT_R32G32_SFLOAT},
    {offsetof(Vertex, color), VK_FORMAT_R32G32B32_SFLOAT},
    {offsetof(Vertex, texCoord), VK_FORMAT_R32G32_SFLOAT}
  };

  std::vector<VkVertexInputBindingDescription> binding_description(1);
  std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

  device = std::make_shared<Vulkan::Device>(Vulkan::PhysicalDeviceType::Discrete, Vulkan::QueueType::DrawingAndComputeType, window);
  swapchain = std::make_shared<Vulkan::SwapChain>(device);
  render_pass = std::make_shared<Vulkan::RenderPass>(device, swapchain);
  g_pipeline = std::make_shared<Vulkan::GraphicPipeline>(device, swapchain, render_pass);

  input_vertex_array_src = std::make_shared<Vulkan::Buffer<Vertex>>(device, Vulkan::StorageType::Vertex, Vulkan::HostVisibleMemory::HostVisible);
  input_index_array_src = std::make_shared<Vulkan::Buffer<uint16_t>>(device, Vulkan::StorageType::Index, Vulkan::HostVisibleMemory::HostVisible); 

  input_vertex_array_dst = std::make_shared<Vulkan::Buffer<Vertex>>(device, Vulkan::StorageType::Vertex, Vulkan::HostVisibleMemory::HostInvisible); 
  input_index_array_dst = std::make_shared<Vulkan::Buffer<uint16_t>>(device, Vulkan::StorageType::Index, Vulkan::HostVisibleMemory::HostInvisible);

  texture_data.Load(exec_directory + "../Resources/wall-texture.jpg", 4);
  texture_data.Resize(512, 512);
  texture_image = std::make_shared<Vulkan::Image>(device, texture_data.Width(), 
                                                  texture_data.Height(), 
                                                  Vulkan::ImageTiling::Optimal, 
                                                  Vulkan::HostVisibleMemory::HostInvisible,
                                                  Vulkan::ImageType::Sampled);
  samlper = std::make_shared<Vulkan::Sampler>(device);

  texture_buffer = std::make_shared<Vulkan::Buffer<uint8_t>>(device, Vulkan::StorageType::Storage, Vulkan::HostVisibleMemory::HostVisible);
  *texture_buffer = texture_data.Canvas();

  descriptors = std::make_shared<Vulkan::Descriptors>(device);
  command_pool = std::make_shared<Vulkan::CommandPool>(device, device->GetGraphicFamilyQueueIndex().value());

  frames_in_pipeline = swapchain->GetImageViewsCount() + 5;

  for (size_t i = 0; i < swapchain->GetImageViewsCount(); ++i)
  {
    world_uniform_buffers.push_back(std::make_shared<Vulkan::Buffer<World>>(device, Vulkan::StorageType::Uniform, Vulkan::HostVisibleMemory::HostVisible));
    world_uniform_buffers[i]->AllocateBuffer(1);
    descriptors->ClearDescriptorSetLayout(i);
    descriptors->Add(i, world_uniform_buffers[i], VK_SHADER_STAGE_VERTEX_BIT, 0);
    descriptors->Add(i, texture_image, samlper, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
  }

  descriptors->BuildAll();

  PrepareShaders();
  g_pipeline->SetShaderInfos(shader_infos); 
 
  Vulkan::Supply::GetVertexInputBindingDescription<Vertex>(0, vertex_descriptions, binding_description[0], attribute_descriptions);
  g_pipeline->SetVertexInputBindingDescription(binding_description, attribute_descriptions);
  g_pipeline->SetDescriptorsSetLayouts(descriptors->GetDescriptorSetLayouts());

  const std::vector<Vertex> vertices = 
  {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
  };

  const std::vector<uint16_t> indices = 
  {
    0, 1, 2, 2, 3, 0
  };

  *input_vertex_array_src = vertices;
  *input_vertex_array_dst = vertices;
  *input_index_array_src = indices;
  *input_index_array_dst = indices;

  VkBufferCopy copy_region = {};
  copy_region.srcOffset = 0;
  copy_region.dstOffset = 0;

  VkBufferImageCopy image_region = {};
  image_region.bufferOffset = 0;
  image_region.bufferRowLength = 0;
  image_region.bufferImageHeight = 0;

  image_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_region.imageSubresource.mipLevel = 0;
  image_region.imageSubresource.baseArrayLayer = 0;
  image_region.imageSubresource.layerCount = 1;

  image_region.imageOffset = {0, 0, 0};
  image_region.imageExtent = 
  {
    (uint32_t) texture_image->Width(),
    (uint32_t) texture_image->Height(),
    1
  };

  command_pool->BeginCommandBuffer(0);

  copy_region.size = std::min(input_vertex_array_src->BufferLength(), input_vertex_array_src->BufferLength());
  command_pool->CopyBuffer(0, input_vertex_array_src, input_vertex_array_dst, {copy_region});

  copy_region.size = std::min(input_index_array_src->BufferLength(), input_index_array_src->BufferLength());
  command_pool->CopyBuffer(0, input_index_array_src, input_index_array_dst, {copy_region});
  command_pool->CopyBufferToImage(0, texture_buffer, texture_image, {image_region});
  command_pool->EndCommandBuffer(0);
  command_pool->ExecuteBuffer(0);


  WriteCommandBuffers();
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
  auto descriptor_sets = descriptors->GetDescriptorSets();
  auto frame_buffers = render_pass->GetFrameBuffers();

  for (size_t i = 0; i < render_pass->GetFrameBuffersCount(); ++i)
  {
    command_pool->BeginCommandBuffer(i, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    command_pool->BindVertexBuffers(i, { input_vertex_array_dst->GetBuffer() }, { 0 }, 0, 1);
    command_pool->BindIndexBuffer(i, input_index_array_dst->GetBuffer(), VK_INDEX_TYPE_UINT16, 0);
    command_pool->BeginRenderPass(i, render_pass->GetRenderPass(), frame_buffers[i], swapchain->GetExtent(), { 0, 0 });
    command_pool->BindPipeline(i, g_pipeline->GetPipeline(), VK_PIPELINE_BIND_POINT_GRAPHICS);
    command_pool->BindDescriptorSets(i, g_pipeline->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS, { descriptor_sets[i] }, {}, 0);
    command_pool->DrawIndexed(i, input_index_array_dst->ItemsCount(), 0, 0, 1, 0);
    command_pool->EndRenderPass(i);
    command_pool->EndCommandBuffer(i);
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
  submit_info.pCommandBuffers = &(*command_pool)[image_index];
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
  shader_infos[0] = {"main", exec_directory + "tri.vert.spv", Vulkan::ShaderType::Vertex};
  shader_infos[1] = {"main", exec_directory + "tri.frag.spv", Vulkan::ShaderType::Fragment};
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

