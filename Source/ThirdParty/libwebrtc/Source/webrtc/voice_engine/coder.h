/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_CODER_H_
#define WEBRTC_VOICE_ENGINE_CODER_H_

#include <memory>

#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/acm2/codec_manager.h"
#include "webrtc/modules/audio_coding/acm2/rent_a_codec.h"
#include "webrtc/modules/audio_coding/include/audio_coding_module.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class AudioFrame;

class AudioCoder : public AudioPacketizationCallback {
 public:
  explicit AudioCoder(uint32_t instance_id);
  ~AudioCoder();

  int32_t SetEncodeCodec(const CodecInst& codec_inst);

  int32_t SetDecodeCodec(const CodecInst& codec_inst);

  int32_t Decode(AudioFrame* decoded_audio,
                 uint32_t samp_freq_hz,
                 const int8_t* incoming_payload,
                 size_t payload_length);

  int32_t PlayoutData(AudioFrame* decoded_audio, uint16_t samp_freq_hz);

  int32_t Encode(const AudioFrame& audio,
                 int8_t* encoded_data,
                 size_t* encoded_length_in_bytes);

 protected:
  int32_t SendData(FrameType frame_type,
                   uint8_t payload_type,
                   uint32_t time_stamp,
                   const uint8_t* payload_data,
                   size_t payload_size,
                   const RTPFragmentationHeader* fragmentation) override;

 private:
  std::unique_ptr<AudioCodingModule> acm_;
  acm2::CodecManager codec_manager_;
  acm2::RentACodec rent_a_codec_;

  CodecInst receive_codec_;

  uint32_t encode_timestamp_;
  int8_t* encoded_data_;
  size_t encoded_length_in_bytes_;

  uint32_t decode_timestamp_;
};
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_CODER_H_
