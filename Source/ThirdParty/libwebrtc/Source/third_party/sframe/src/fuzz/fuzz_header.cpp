#include <cstddef>
#include <cstdint>
#include <tuple>

#include <header.h>

using namespace SFRAME_NAMESPACE;

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  auto input = input_bytes(data, size);
  std::ignore = Header::parse(input);
  return 0;
}
