#include <namespace.h>
#include <sframe/result.h>
#include <sframe/sframe.h>

#include <cstdint>
#include <string>
#include <vector>

using bytes = std::vector<uint8_t>;

bytes
from_hex(const std::string& hex);
std::string
to_hex(const SFRAME_NAMESPACE::input_bytes data);

template<typename T>
bytes
to_bytes(const T& range)
{
  return bytes(range.begin(), range.end());
}
