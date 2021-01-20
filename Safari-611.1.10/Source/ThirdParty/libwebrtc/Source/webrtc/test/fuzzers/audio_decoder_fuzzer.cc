/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/audio_decoder_fuzzer.h"

#include <limits>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_decoder.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
template <typename T, unsigned int B = sizeof(T)>
bool ParseInt(const uint8_t** data, size_t* remaining_size, T* value) {
  static_assert(std::numeric_limits<T>::is_integer, "Type must be an integer.");
  static_assert(sizeof(T) <= sizeof(uint64_t),
                "Cannot read wider than uint64_t.");
  static_assert(B <= sizeof(T), "T must be at least B bytes wide.");
  if (B > *remaining_size)
    return false;
  uint64_t val = ByteReader<uint64_t, B>::ReadBigEndian(*data);
  *data += B;
  *remaining_size -= B;
  *value = static_cast<T>(val);
  return true;
}
}  // namespace

// This function reads two bytes from the beginning of |data|, interprets them
// as the first packet length, and reads this many bytes if available. The
// payload is inserted into the decoder, and the process continues until no more
// data is available. Either AudioDecoder::Decode or
// AudioDecoder::DecodeRedundant is used, depending on the value of
// |decode_type|.
void FuzzAudioDecoder(DecoderFunctionType decode_type,
                      const uint8_t* data,
                      size_t size,
                      AudioDecoder* decoder,
                      int sample_rate_hz,
                      size_t max_decoded_bytes,
                      int16_t* decoded) {
  const uint8_t* data_ptr = data;
  size_t remaining_size = size;
  size_t packet_len;
  constexpr size_t kMaxNumFuzzedPackets = 200;
  for (size_t num_packets = 0; num_packets < kMaxNumFuzzedPackets;
       ++num_packets) {
    if (!(ParseInt<size_t, 2>(&data_ptr, &remaining_size, &packet_len) &&
          packet_len <= remaining_size)) {
      break;
    }
    AudioDecoder::SpeechType speech_type;
    switch (decode_type) {
      case DecoderFunctionType::kNormalDecode:
        decoder->Decode(data_ptr, packet_len, sample_rate_hz, max_decoded_bytes,
                        decoded, &speech_type);
        break;
      case DecoderFunctionType::kRedundantDecode:
        decoder->DecodeRedundant(data_ptr, packet_len, sample_rate_hz,
                                 max_decoded_bytes, decoded, &speech_type);
        break;
    }
    data_ptr += packet_len;
    remaining_size -= packet_len;
  }
}

}  // namespace webrtc
