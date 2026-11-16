#pragma once

#include <sframe/result.h>
#include <sframe/sframe.h>

namespace SFRAME_NAMESPACE {

void
encode_uint(uint64_t val, output_bytes buffer);

class Header
{
public:
  const KeyID key_id;
  const Counter counter;

  Header(KeyID key_id_in, Counter counter_in);
  static Result<Header> parse(input_bytes buffer);

  input_bytes encoded() const { return _encoded; }
  size_t size() const { return _encoded.size(); }

  // Configuration byte plus 8-byte KID and CTR
  static constexpr size_t max_size = 1 + 8 + 8;

private:
  // Just the configuration byte
  static constexpr size_t min_size = 1;

  owned_bytes<max_size> _encoded;

  Header(KeyID key_id_in, Counter counter_in, input_bytes encoded_in);
};

} // namespace SFRAME_NAMESPACE
