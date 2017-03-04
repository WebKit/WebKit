/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fuzzers/audio_decoder_fuzzer.h"

#include <limits>

#include "webrtc/api/audio_codecs/audio_decoder.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"

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
  while (ParseInt<size_t, 2>(&data_ptr, &remaining_size, &packet_len) &&
         packet_len <= remaining_size) {
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

// This function is similar to FuzzAudioDecoder, but also reads fuzzed data into
// RTP header values. The fuzzed data and values are sent to the decoder's
// IncomingPacket method.
void FuzzAudioDecoderIncomingPacket(const uint8_t* data,
                                    size_t size,
                                    AudioDecoder* decoder) {
  const uint8_t* data_ptr = data;
  size_t remaining_size = size;
  size_t packet_len;
  while (ParseInt<size_t, 2>(&data_ptr, &remaining_size, &packet_len)) {
    uint16_t rtp_sequence_number;
    if (!ParseInt(&data_ptr, &remaining_size, &rtp_sequence_number))
      break;
    uint32_t rtp_timestamp;
    if (!ParseInt(&data_ptr, &remaining_size, &rtp_timestamp))
      break;
    uint32_t arrival_timestamp;
    if (!ParseInt(&data_ptr, &remaining_size, &arrival_timestamp))
      break;
    if (remaining_size < packet_len)
      break;
    decoder->IncomingPacket(data_ptr, packet_len, rtp_sequence_number,
                            rtp_timestamp, arrival_timestamp);
    data_ptr += packet_len;
    remaining_size -= packet_len;
  }
}
}  // namespace webrtc
