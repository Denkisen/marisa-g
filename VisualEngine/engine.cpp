#include "engine.h"

#include <filesystem>
#include <optional>
#include <chrono>

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
  settings.Load("test.conf");
  PrepareWindow();
  
  exec_directory = Vulkan::Supply::GetExecDirectory(argv[0]);
  auto vertex_description = Vulkan::GetVertexDescription(0);

  if (!std::filesystem::exists(exec_directory))
    throw std::runtime_error("argv[0] is not a valid path.");

  device = std::make_shared<Vulkan::Device>(Vulkan::PhysicalDeviceType::Discrete, Vulkan::QueueType::DrawingAndComputeType, window);
  swapchain = std::make_shared<Vulkan::SwapChain>(device, (VkPresentModeKHR) settings.PresentMode());
  render_pass = std::make_shared<Vulkan::RenderPass>(device, swapchain);
  g_pipeline = std::make_shared<Vulkan::GraphicPipeline>(device, swapchain, render_pass);
  descriptors = std::make_shared<Vulkan::Descriptors>(device);
  command_pool = std::make_shared<Vulkan::CommandPool>(device, device->GetGraphicFamilyQueueIndex().value());

  frames_in_pipeline = swapchain->GetImageViewsCount() + 5;

  buffer_locks.resize(render_pass->GetFrameBuffersCount());
  for (size_t i = 0; i < buffer_locks.size(); ++i)
    buffer_locks[i] = command_pool->OrderBufferLock();

  girl = std::make_shared<Vulkan::Object>(device, command_pool);
  girl->LoadModel("Resources/Models/girl/girl.obj", "Resources/Models/girl/");
  girl->LoadTexture("Resources/Models/girl/girl_mip.png", true);
  girl_pos = {0.0f, 0.0f, 0.0f};

  for (size_t i = 0; i < swapchain->GetImageViewsCount(); ++i)
  {
    world_uniform_buffers.push_back(std::make_shared<Vulkan::Buffer<World>>(device, Vulkan::StorageType::Uniform, Vulkan::HostVisibleMemory::HostVisible));
    world_uniform_buffers[i]->AllocateBuffer(1);
    descriptors->ClearDescriptorSetLayout(i);
    descriptors->Add(i, 0, world_uniform_buffers[i], VK_SHADER_STAGE_VERTEX_BIT);
    descriptors->Add(i, 1, girl->GetTextureImage(), girl->GetSampler(), VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  descriptors->BuildAll();

  PrepareShaders();
  g_pipeline->SetShaderInfos(shader_infos); 
  g_pipeline->SetDescriptorsSetLayouts(descriptors->GetDescriptorSetLayouts());
  g_pipeline->SetVertexInputBindingDescription({vertex_description.first}, vertex_description.second);
  g_pipeline->UseDepthTesting(VK_TRUE);
  g_pipeline->SetSamplesCount((VkSampleCountFlagBits) settings.Multisampling());
  render_pass->SetSamplesCount((VkSampleCountFlagBits) settings.Multisampling());
  ReBuildPipelines();

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

void VisualEngine::KeyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  auto app = reinterpret_cast<VisualEngine*>(glfwGetWindowUserPointer(window));
  if (key == GLFW_KEY_UP && action == GLFW_PRESS)
    app->up_key_down = true;
  if (key == GLFW_KEY_UP && action == GLFW_RELEASE)
    app->up_key_down = false;

  if (key == GLFW_KEY_DOWN && action == GLFW_PRESS)
    app->down_key_down = true;
  if (key == GLFW_KEY_DOWN && action == GLFW_RELEASE)
    app->down_key_down = false;
}

void VisualEngine::Draw(VisualEngine &obj)
{
  obj.fps.Start();
  obj.DrawFrame();
}

void VisualEngine::WriteCommandBuffers()
{
  auto descriptor_sets = descriptors->GetDescriptorSets();
  auto frame_buffers = render_pass->GetFrameBuffers();
  for (size_t i = 0; i < buffer_locks.size(); ++i)
  {
    command_pool->BeginCommandBuffer(buffer_locks[i], VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    command_pool->BindVertexBuffers(buffer_locks[i], { girl->GetVertexBuffer()->GetBuffer() }, { 0 }, 0, 1);
    command_pool->BindIndexBuffer(buffer_locks[i], girl->GetIndexBuffer()->GetBuffer(), VK_INDEX_TYPE_UINT32, 0);
    command_pool->BeginRenderPass(buffer_locks[i], render_pass, i, { 0, 0 });
    command_pool->BindPipeline(buffer_locks[i], g_pipeline->GetPipeline(), VK_PIPELINE_BIND_POINT_GRAPHICS);
    command_pool->BindDescriptorSets(buffer_locks[i], g_pipeline->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS, { descriptor_sets[i] }, {}, 0);
    command_pool->DrawIndexed(buffer_locks[i], girl->GetIndexBuffer()->ItemsCount(), 0, 0, 1, 0);
    command_pool->EndRenderPass(buffer_locks[i]);
    command_pool->EndCommandBuffer(buffer_locks[i]);
  }
}

void VisualEngine::UpdateWorldUniformBuffers(uint32_t image_index)
{
  static auto start_time = std::chrono::high_resolution_clock::now();
  auto current_time = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();
  World bf = {};
  if (up_key_down)
  {
    girl_pos.x -= 0.01 * time;
    girl_pos.z -= 0.01 * time;
  }
  if (down_key_down)
  {
    girl_pos.x += 0.01 * time;
    girl_pos.z += 0.01 * time;
  }
  bf.model = glm::translate(glm::mat4(1.0f), girl_pos); 
  bf.model = glm::rotate(bf.model, time * glm::radians(35.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  bf.view = glm::lookAt(glm::vec3(7.0f, 7.0f, 7.0f), glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  bf.proj = glm::perspective(glm::radians(45.0f), swapchain->GetExtent().width / (float) swapchain->GetExtent().height, 0.1f, 100.0f);
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
  submit_info.pCommandBuffers = &(*command_pool)[buffer_locks[image_index]];
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
  fps.Frame();
  if (current_frame == frames_in_pipeline - 1)
  {
    std::string title = instance.AppName() + " FPS:" + std::to_string(fps.GetFps());
    glfwSetWindowTitle(window, title.c_str());
    fps.Start();
  }

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

  window = glfwCreateWindow(settings.Widght(), settings.Height(), instance.AppName().c_str(), nullptr, nullptr);

  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, FrameBufferResizeCallback);
  glfwSetKeyCallback(window, KeyboardCallback);
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

