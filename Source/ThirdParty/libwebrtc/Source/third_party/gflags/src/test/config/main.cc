#include <iostream>
#include <gflags/gflags.h>

DEFINE_string(message, "Hello World!", "The message to print");

int main(int argc, char **argv)
{
  gflags::SetUsageMessage("Test CMake configuration of gflags library (gflags-config.cmake)");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::cout << FLAGS_message << std::endl;
  return 0;
}
