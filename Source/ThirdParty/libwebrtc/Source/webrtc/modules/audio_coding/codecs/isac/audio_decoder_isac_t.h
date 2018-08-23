/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_DECODER_ISAC_T_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_DECODER_ISAC_T_H_

#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_decoder.h"
#include "modules/audio_coding/codecs/isac/locked_bandwidth_info.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

template <typename T>
class AudioDecoderIsacT final : public AudioDecoder {
 public:
  explicit AudioDecoderIsacT(int sample_rate_hz);
  AudioDecoderIsacT(int sample_rate_hz,
                    const rtc::scoped_refptr<LockedIsacBandwidthInfo>& bwinfo);
  ~AudioDecoderIsacT() override;

  bool HasDecodePlc() const override;
  size_t DecodePlc(size_t num_frames, int16_t* decoded) override;
  void Reset() override;
  int IncomingPacket(const uint8_t* payload,
                     size_t payload_len,
                     uint16_t rtp_sequence_number,
                     uint32_t rtp_timestamp,
                     uint32_t arrival_timestamp) override;
  int ErrorCode() override;
  int SampleRateHz() const override;
  size_t Channels() const override;
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  typename T::instance_type* isac_state_;
  int sample_rate_hz_;
  rtc::scoped_refptr<LockedIsacBandwidthInfo> bwinfo_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AudioDecoderIsacT);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_CODECS_ISAC_AUDIO_DECODER_ISAC_T_H_
