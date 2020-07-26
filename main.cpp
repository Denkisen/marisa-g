#include <iostream>
#include "VisualEngine/engine.h"

int main(int argc, char const *argv[])
{
  try
  {
    VisualEngine engine;
    engine.Start();
  } 
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
  }
  return 0;
}