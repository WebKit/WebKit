/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/rent_a_codec.h"

#include <memory>
#include <utility>

#include "rtc_base/logging.h"
#include "modules/audio_coding/acm2/acm_codec_database.h"

namespace webrtc {
namespace acm2 {

absl::optional<RentACodec::CodecId> RentACodec::CodecIdByParams(
    const char* payload_name,
    int sampling_freq_hz,
    size_t channels) {
  return CodecIdFromIndex(
      ACMCodecDB::CodecId(payload_name, sampling_freq_hz, channels));
}

absl::optional<CodecInst> RentACodec::CodecInstById(CodecId codec_id) {
  absl::optional<int> mi = CodecIndexFromId(codec_id);
  return mi ? absl::optional<CodecInst>(ACMCodecDB::database_[*mi])
            : absl::nullopt;
}

absl::optional<RentACodec::CodecId> RentACodec::CodecIdByInst(
    const CodecInst& codec_inst) {
  return CodecIdFromIndex(ACMCodecDB::CodecNumber(codec_inst));
}

absl::optional<CodecInst> RentACodec::CodecInstByParams(
    const char* payload_name,
    int sampling_freq_hz,
    size_t channels) {
  absl::optional<CodecId> codec_id =
      CodecIdByParams(payload_name, sampling_freq_hz, channels);
  if (!codec_id)
    return absl::nullopt;
  absl::optional<CodecInst> ci = CodecInstById(*codec_id);
  RTC_DCHECK(ci);

  // Keep the number of channels from the function call. For most codecs it
  // will be the same value as in default codec settings, but not for all.
  ci->channels = channels;

  return ci;
}

absl::optional<NetEqDecoder> RentACodec::NetEqDecoderFromCodecId(
    CodecId codec_id,
    size_t num_channels) {
  absl::optional<int> i = CodecIndexFromId(codec_id);
  if (!i)
    return absl::nullopt;
  const NetEqDecoder ned = ACMCodecDB::neteq_decoders_[*i];
  return (ned == NetEqDecoder::kDecoderOpus && num_channels == 2)
             ? NetEqDecoder::kDecoderOpus_2ch
             : ned;
}

}  // namespace acm2
}  // namespace webrtc
