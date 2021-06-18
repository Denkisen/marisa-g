#ifndef __CPU_MARISAG_SETTINGS_H
#define __CPU_MARISAG_SETTINGS_H

#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

enum class WindowMode_t
{
  Window,
  BorderlessWindow,
  FullScreenWindow
};

enum class PresentMode_t
{
  Mailbox = VK_PRESENT_MODE_MAILBOX_KHR,
  Immediate = VK_PRESENT_MODE_IMMEDIATE_KHR,
  DefaultFIFO = VK_PRESENT_MODE_FIFO_KHR,
  RelaxedFIFO = VK_PRESENT_MODE_FIFO_RELAXED_KHR
};

enum class MSAA_t
{
  None = VK_SAMPLE_COUNT_1_BIT,
  x2 = VK_SAMPLE_COUNT_2_BIT,
  x4 = VK_SAMPLE_COUNT_4_BIT,
  x8 = VK_SAMPLE_COUNT_8_BIT,
  x16 = VK_SAMPLE_COUNT_16_BIT,
  x32 = VK_SAMPLE_COUNT_32_BIT,
  x64 = VK_SAMPLE_COUNT_64_BIT
};

class Settings
{
private:
  std::string device_name = "";
  size_t height = 820;
  size_t width = 1280;
  WindowMode_t window_mode = WindowMode_t::Window;
  PresentMode_t present_mode = PresentMode_t::DefaultFIFO;
  MSAA_t multisampling = MSAA_t::x2;
public:
  Settings() = default;
  ~Settings() = default;
  void Load(const std::string file);
  void Save(const std::string file);

  std::string DeviceName() const { return device_name; }
  void DeviceName(const std::string name) { device_name = name; }

  size_t Height() const { return height; }
  void Height(const size_t val) { height = val; }

  size_t Widght() const { return width; }
  void Widght(const size_t val) { width = val; }

  WindowMode_t WindowMode() const { return window_mode; }
  void WindowMode(const WindowMode_t mode) { window_mode = mode; }

  PresentMode_t PresentMode() const { return present_mode; }
  void PresentMode(const PresentMode_t mode) { present_mode = mode; }

  MSAA_t Multisampling() const { return multisampling; }
  void Multisampling( const MSAA_t samples) { multisampling = samples; }
};

#endif