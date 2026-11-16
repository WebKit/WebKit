#include "header.h"

#include <algorithm>

namespace SFRAME_NAMESPACE {

static size_t
uint_size(uint64_t val)
{
  if (val < 0x08) {
    // Fits in the config byte
    return 0;
  }

  for (unsigned int i = 0; i < 8; i += 1) {
    if ((val >> (8 * i)) == 0) {
      return i;
    }
  }

  return 8;
}

void
encode_uint(uint64_t val, output_bytes buffer)
{
  size_t size = buffer.size();
  std::fill(buffer.begin(), buffer.end(), uint8_t(0));
  for (size_t i = 0; i < size && i < 8; i++) {
    buffer[size - i - 1] = uint8_t(val >> (8 * i));
  }
}

static Result<uint64_t>
decode_uint(input_bytes data)
{
  if (data.empty()) {
    return SFrameError(SFrameErrorType::invalid_parameter_error,
                       "Integer encoding is empty");
  }

  if (!data.empty() && data[0] == 0) {
    return SFrameError(SFrameErrorType::invalid_parameter_error,
                       "Integer is not minimally encoded");
  }

  uint64_t val = 0;
  for (size_t i = 0; i < data.size(); i++) {
    val = (val << 8) + static_cast<uint64_t>(data[i]);
  }
  return val;
}

struct ValueOrLength
{
  bool is_length = false;
  uint8_t value_or_length = 0;

  static ValueOrLength for_u64(uint64_t value)
  {
    if (value >= 0x08) {
      return { true, static_cast<uint8_t>(uint_size(value) - 1) };
    } else {
      return { false, static_cast<uint8_t>(value) };
    }
  }

  static ValueOrLength decode(uint8_t encoded)
  {
    const auto is_length = (encoded & 0x08) != 0;
    const auto value_or_length = (encoded & 0x07);
    return { is_length, static_cast<uint8_t>(value_or_length) };
  }

  uint8_t encode() const
  {
    return (((is_length) ? 1 : 0) << 3) | (value_or_length & 0x07);
  }

  size_t value_size() const
  {
    if (!is_length) {
      return 0;
    }

    return value_or_length + 1;
  }

  Result<std::tuple<uint64_t, input_bytes>> read(input_bytes data) const
  {
    if (!is_length) {
      // Nothing to read; value is already in config byte
      return std::make_tuple(uint64_t(value_or_length), data);
    }

    const auto size = value_size();
    SFRAME_VALUE_OR_RETURN(value, decode_uint(data.subspan(0, size)));
    const auto remaining = data.subspan(size);
    return std::make_tuple(value, remaining);
  }

private:
  ValueOrLength(bool is_length_in, uint8_t value_or_length_in)
    : is_length(is_length_in)
    , value_or_length(value_or_length_in)
  {
  }
};

struct ConfigByte
{
  ValueOrLength kid;
  ValueOrLength ctr;

  ConfigByte(uint64_t kid_in, uint64_t ctr_in)
    : kid(ValueOrLength::for_u64(kid_in))
    , ctr(ValueOrLength::for_u64(ctr_in))
  {
  }

  explicit ConfigByte(uint8_t encoded)
    : kid(ValueOrLength::decode(encoded >> 4))
    , ctr(ValueOrLength::decode(encoded & 0x0f))
  {
  }

  size_t encoded_size() const
  {
    return 1 + kid.value_size() + ctr.value_size();
  }

  uint8_t encode() const { return (kid.encode() << 4) | ctr.encode(); }
};

Header::Header(KeyID key_id_in, Counter counter_in)
  : key_id(key_id_in)
  , counter(counter_in)
{
  const auto cfg = ConfigByte{ key_id, counter };

  _encoded[0] = cfg.encode();
  _encoded.resize(cfg.encoded_size());

  const auto encoded = output_bytes(_encoded);
  const auto after_cfg = encoded.subspan(1);
  encode_uint(key_id, after_cfg.subspan(0, cfg.kid.value_size()));

  const auto after_kid = after_cfg.subspan(cfg.kid.value_size());
  encode_uint(counter, after_kid.subspan(0, cfg.ctr.value_size()));
}

Result<Header>
Header::parse(input_bytes buffer)
{
  if (buffer.size() < Header::min_size) {
    return SFrameError(SFrameErrorType::buffer_too_small_error,
                       "Ciphertext too small to decode header");
  }

  const auto cfg = ConfigByte{ buffer[0] };
  if (cfg.encoded_size() > buffer.size()) {
    return SFrameError(SFrameErrorType::buffer_too_small_error,
                       "Ciphertext too small to decode header");
  }

  const auto after_cfg = buffer.subspan(1);
  SFRAME_VALUE_OR_RETURN(kid_result, cfg.kid.read(after_cfg));
  const auto [key_id, after_kid] = kid_result;
  SFRAME_VALUE_OR_RETURN(ctr_result, cfg.ctr.read(after_kid));
  const auto [counter, _] = ctr_result;

  const auto encoded = buffer.subspan(0, cfg.encoded_size());

  return Header(key_id, counter, encoded);
}

Header::Header(KeyID key_id_in, Counter counter_in, input_bytes encoded_in)
  : key_id(key_id_in)
  , counter(counter_in)
  , _encoded(encoded_in)
{
}

#if 0
std::tuple<Header, input_bytes>
decode(input_bytes buffer)
{
  if (buffer.size() < Header::min_size) {
    throw buffer_too_small_error("Ciphertext too small to decode header");
  }

  const auto cfg = ConfigByte{ buffer[0] };
  const auto after_cfg = buffer.subspan(1);
  const auto [kid, after_kid] = cfg.kid.read(after_cfg);
  const auto [ctr, after_ctr] = cfg.ctr.read(after_kid);
  const auto header = Header{ KeyID(kid), Counter(ctr) };

  return { header, after_ctr };
}

size_t
Header::encode(output_bytes buffer) const
{
  if (buffer.size() < size()) {
    throw buffer_too_small_error("Buffer too small to encode header");
  }

  const auto cfg = ConfigByte{ key_id, counter };
  buffer[0] = cfg.encode();

  const auto after_cfg = buffer.subspan(1);
  encode_uint(key_id, after_cfg.subspan(0, cfg.kid.size()));

  const auto after_kid = after_cfg.subspan(cfg.kid.size());
  encode_uint(counter, after_kid.subspan(0, cfg.ctr.size()));

  return size();
}
#endif

} // namespace SFRAME_NAMESPACE
