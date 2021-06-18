#include "engine.h"
#include "Vertex.h"

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
    if (!exec_fences.empty() && exec_fences[i] != VK_NULL_HANDLE)
      vkDestroyFence(device->GetDevice(), exec_fences[i], nullptr);
  }
}

VisualEngine::VisualEngine(int argc, char const *argv[])
{
  settings.Load("test.conf");
  PrepareWindow();
  
  exec_directory = Vulkan::Misc::GetExecDirectory(argv[0]);
  auto vertex_description = GetVertexDescription(0);

  if (!std::filesystem::exists(exec_directory))
    throw std::runtime_error("argv[0] is not a valid path.");

  surface = std::make_shared<Vulkan::Surface>(Vulkan::SurfaceConfig().SetAppTitle(Vulkan::Instance::AppName())
                                              .SetHeight(settings.Height()).SetWidght(settings.Widght()));
  surface->SetWindowUserPointer(this);
  surface->SetFramebufferSizeCallback(FrameBufferResizeCallback);
  surface->SetKeyCallback(KeyboardCallback);

  VkPhysicalDeviceFeatures device_features = {};
  device_features.geometryShader = VK_TRUE;

  device = std::make_shared<Vulkan::Device>(Vulkan::DeviceConfig().SetDeviceType(Vulkan::PhysicalDeviceType::Discrete)
                                            .SetQueueType(Vulkan::QueueType::DrawingType)
                                            .SetSurface(surface)
                                            .SetRequiredDeviceFeatures(device_features));
  
  descriptors = std::make_shared<Vulkan::Descriptors>(device);
  command_pool = std::make_shared<Vulkan::CommandPool>(device, device->GetGraphicFamilyQueueIndex().value());
  swapchain = std::make_shared<Vulkan::SwapChain>(device, Vulkan::SwapChainConfig()
                                                          .SetImagesCount(2)
                                                          .SetPresentMode((VkPresentModeKHR) settings.PresentMode()));
  storage_buffers = std::make_shared<Vulkan::StorageArray>(device);
  render_pass_bufers = std::make_shared<Vulkan::ImageArray>(device);
  render_pass = Vulkan::Helpers::CreateOneSubpassRenderPassMultisamplingDepth(device, swapchain, *render_pass_bufers.get(), (VkSampleCountFlagBits) settings.Multisampling());

  frames_in_pipeline = swapchain->GetImagesCount() + 1;
  storage_buffers->AddBuffer(Vulkan::BufferConfig()
                            .SetType(Vulkan::StorageType::Uniform)
                            .AddSubBufferRange(swapchain->GetImagesCount(), 1, sizeof(World)));
  storage_buffers->EndConfig();

  image_available_semaphores = std::make_unique<Vulkan::SemaphoreArray>(device);
  render_finished_semaphores = std::make_unique<Vulkan::SemaphoreArray>(device);

  girl = std::make_unique<TestObject>(device);
  //girl->LoadModel("Resources/Models/Torus/torus.obj", "Resources/Models/Torus/");
  girl->LoadModel("Resources/Models/girl/girl.obj", "Resources/Models/girl/");
  girl->LoadTexture("Resources/Models/girl/girl_mip.png", true);

  Vulkan::DescriptorInfo s_info = {};
  s_info.type = Vulkan::DescriptorType::ImageSamplerCombined;
  s_info.image_info.sampler = girl->GetSampler();
  s_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  s_info.image_info.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  s_info.image_info.image_view = girl->GetTextureInfo().image_view;
  s_info.offset = 0;
  s_info.size = 0;

  Vulkan::DescriptorInfo d_info = {};
  d_info.type = d_info.MapStorageType(Vulkan::StorageType::Uniform);
  d_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  for (size_t i = 0; i < swapchain->GetImagesCount(); ++i)
  {
    d_info.size = storage_buffers->GetInfo(0).sub_buffers[i].size;
    d_info.offset = storage_buffers->GetInfo(0).sub_buffers[i].offset;
    d_info.buffer_info.buffer = storage_buffers->GetInfo(0).buffer;
    descriptors->AddSetLayoutConfig(Vulkan::LayoutConfig()
                                    .AddBufferOrImage(d_info)
                                    .AddBufferOrImage(s_info));
  }

  descriptors->BuildAllSetLayoutConfigs();  

  Vulkan::GraphicPipelineConfig::InputBinding input_binding = {};
  input_binding.attribute_desc = vertex_description.second;
  input_binding.binding_desc = vertex_description.first;

  pipelines.AddPipeline(device, swapchain, render_pass, Vulkan::GraphicPipelineConfig()
                        .UseDepthBias(VK_TRUE)
                        .UseDepthTesting(VK_TRUE)
                        .AddShader(Vulkan::ShaderType::Vertex, exec_directory + "tri.vert.spv", "main")
                        .AddShader(Vulkan::ShaderType::Fragment, exec_directory + "tri.frag.spv", "main")
                        .AddInputBinding(input_binding)
                        .SetSamplesCount((VkSampleCountFlagBits) settings.Multisampling())
                        .AddDescriptorSetLayouts(descriptors->GetDescriptorSetLayouts())
                        .AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
                        .AddDynamicState(VK_DYNAMIC_STATE_SCISSOR)
                        .SetFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
                        .SetCullMode(VK_CULL_MODE_BACK_BIT)
                        .SetPolygonMode(VK_POLYGON_MODE_FILL)
                        .SetMinSampleShading(0.5)
                        .UseSampleShading(VK_TRUE));

  UpdateCommandBuffers();
  PrepareSyncPrimitives();
}

void VisualEngine::Start()
{
  priv_frame_time = std::chrono::high_resolution_clock::now();
  event_handler_thread = std::thread(&VisualEngine::EventHadler, this);
  if (event_handler_thread.joinable())
    event_handler_thread.join();
}

void VisualEngine::EventHadler()
{
  while (!surface->IsWindowShouldClose()) 
  {
    surface->PollEvents();
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

  if (key == GLFW_KEY_LEFT && action == GLFW_PRESS)
    app->left_key_down = true;
  if (key == GLFW_KEY_LEFT && action == GLFW_RELEASE)
    app->left_key_down = false;

  if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS)
    app->right_key_down = true;
  if (key == GLFW_KEY_RIGHT && action == GLFW_RELEASE)
    app->right_key_down = false;
}

void VisualEngine::Draw(VisualEngine &obj)
{
  obj.DrawFrame();
}

void VisualEngine::UpdateCommandBuffers()
{
  auto descriptor_sets = descriptors->GetDescriptorSets();
  auto frame_buffers = render_pass->GetFrameBuffers();

  VkViewport port = {};
  port.x = 0.0f;
  port.y = 0.0f;
  port.width = (float) this->swapchain->GetExtent().width;
  port.height = (float) this->swapchain->GetExtent().height;
  port.minDepth = 0.0f;
  port.maxDepth = 1.0f;

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = swapchain->GetExtent();

  for (size_t i = 0; i < render_pass->GetFrameBuffers().size(); ++i)
  {
    command_pool->ResetCommandBuffer(i);
    command_pool->GetCommandBuffer(i)
                  .BeginCommandBuffer()
                  .BindVertexBuffers({girl->GetModelVerticesInfo().buffer}, {0}, 0, 1)
                  .BindIndexBuffer(girl->GetModelIndicesInfo().buffer, VK_INDEX_TYPE_UINT32, 0)
                  .SetViewport({port})
                  .SetScissor({scissor})
                  .BeginRenderPass(render_pass, i)
                  .BindPipeline(pipelines.GetPipeline(0), VK_PIPELINE_BIND_POINT_GRAPHICS)
                  .BindDescriptorSets(pipelines.GetLayout(0), VK_PIPELINE_BIND_POINT_GRAPHICS, descriptors->GetDescriptorSets(), 0, {})
                  .DrawIndexed(girl->GetModelIndicesInfo().sub_buffers[0].elements, 0, 0, 1, 0)
                  .EndRenderPass()
                  .EndCommandBuffer();
  }
}

void VisualEngine::UpdateWorldUniformBuffers(uint32_t image_index)
{
  auto current_time = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - priv_frame_time).count();
  
  //surface->SetWindowTitle(Vulkan::Instance::AppName() + " FPS:" + std::to_string(1.0 / time));
  World bf = {};
  bf.light = {10.0f, 10.0f, 10.0f, 1.0f};
  float x = 0;
  float y = 0;
  float z = 0;
  if (up_key_down)
  {
    //girl->Move(glm::vec3(-30.0f * time, 0.0, -30.0f * time));
    x = time * -1.0f;
  }
  if (down_key_down)
  {
    //girl->Move(glm::vec3(30.0f * time, 0.0, 30.0f * time));
    x = time * 1.0f;
  }
  if (left_key_down)
  {
    y = time * 1.0f;
  }
  if (right_key_down)
  {
    y = time * -1.0f;
  }
  girl->Rotate(x, y, z);
  bf.model = girl->ObjectTransforations();
  bf.view = glm::lookAt(glm::vec3(10.0f, 10.0f, 10.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  bf.proj = glm::perspective(glm::radians(45.0f), swapchain->GetExtent().width / (float) swapchain->GetExtent().height, 0.1f, 100.0f);
  bf.proj[1][1] *= -1;

  storage_buffers->SetSubBufferData(0, image_index, std::vector<World>{bf});
  priv_frame_time = std::chrono::high_resolution_clock::now();
}

void VisualEngine::DrawFrame()
{
  uint32_t image_index = 0;
  vkWaitForFences(device->GetDevice(), 1, &exec_fences[current_frame], VK_TRUE, UINT64_MAX);
  VkResult res = vkAcquireNextImageKHR(device->GetDevice(), swapchain->GetSwapChain(), UINT64_MAX, (*image_available_semaphores)[current_frame], VK_NULL_HANDLE, &image_index);

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

  if (in_process[image_index] != VK_NULL_HANDLE) 
  {
    vkWaitForFences(device->GetDevice(), 1, &in_process[image_index], VK_TRUE, UINT64_MAX);
  }

  in_process[image_index] = exec_fences[current_frame];

  UpdateWorldUniformBuffers(image_index);

  std::vector<VkSemaphore> wait_semaphores = { (*image_available_semaphores)[current_frame] };  
  std::vector<VkSemaphore> signal_semaphores = { (*render_finished_semaphores)[current_frame] };
  std::vector<VkPipelineStageFlags> wait_stages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; 
  VkSwapchainKHR swapchains[] = { swapchain->GetSwapChain() };

  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = (uint32_t) signal_semaphores.size();
  present_info.pWaitSemaphores = signal_semaphores.data();
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.pImageIndices = &image_index;
  present_info.pResults = nullptr;

  vkResetFences(device->GetDevice(), 1, &exec_fences[current_frame]);
  command_pool->ExecuteBuffer(image_index, exec_fences[current_frame], signal_semaphores, wait_stages, wait_semaphores);
  vkQueuePresentKHR(device->GetPresentQueue(), &present_info);

  current_frame = (current_frame + 1) % frames_in_pipeline;
}

void VisualEngine::PrepareShaders()
{

}

void VisualEngine::PrepareWindow()
{

}

void VisualEngine::PrepareSyncPrimitives()
{
  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  exec_fences.resize(frames_in_pipeline);
  in_process.resize(swapchain->GetImagesCount(), VK_NULL_HANDLE);

  for (size_t i = 0; i < frames_in_pipeline; ++i)
  {
    image_available_semaphores->Add();
    render_finished_semaphores->Add();

    if (vkCreateFence(device->GetDevice(), &fence_info, nullptr, &exec_fences[i]) != VK_SUCCESS)
      throw std::runtime_error("failed to create fences!");
  }
}

void VisualEngine::ReBuildPipelines()
{
  std::pair<int32_t, int32_t> size = {0, 0};
  do
  {
    size = surface->GetFramebufferSize();
    surface->WaitEvents();
  } while (size.first == 0 || size.second == 0);
      
  vkDeviceWaitIdle(device->GetDevice());
  resize_flag = false;
  swapchain->ReCreate();
  render_pass = Vulkan::Helpers::CreateOneSubpassRenderPassMultisamplingDepth(device, swapchain, *render_pass_bufers.get(), (VkSampleCountFlagBits) settings.Multisampling());

  UpdateCommandBuffers();
}

