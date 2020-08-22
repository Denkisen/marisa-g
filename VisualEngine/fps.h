#ifndef __CPU_MARISAG_FPS_H
#define __CPU_MARISAG_FPS_H

#include <chrono>
#include <iostream>

class Fps
{
private:
  std::chrono::_V2::system_clock::time_point start;
  std::chrono::_V2::system_clock::time_point end;
  size_t frames = 0;
public:
  Fps() = default;
  ~Fps() = default;
  void Start()
  {
    start = std::chrono::high_resolution_clock::now();
    frames = 0;
  }

  void Frame()
  {
    frames++;
  }

  float GetFps()
  {
    end = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(end - start).count();

    return frames / time;
  }
};


#endif