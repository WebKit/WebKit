/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_NETEQ_DECODER_ENUM_H_
#define MODULES_AUDIO_CODING_NETEQ_NETEQ_DECODER_ENUM_H_

#include "api/audio_codecs/audio_format.h"
#include "api/optional.h"

namespace webrtc {

enum class NetEqDecoder {
  kDecoderPCMu,
  kDecoderPCMa,
  kDecoderPCMu_2ch,
  kDecoderPCMa_2ch,
  kDecoderILBC,
  kDecoderISAC,
  kDecoderISACswb,
  kDecoderPCM16B,
  kDecoderPCM16Bwb,
  kDecoderPCM16Bswb32kHz,
  kDecoderPCM16Bswb48kHz,
  kDecoderPCM16B_2ch,
  kDecoderPCM16Bwb_2ch,
  kDecoderPCM16Bswb32kHz_2ch,
  kDecoderPCM16Bswb48kHz_2ch,
  kDecoderPCM16B_5ch,
  kDecoderG722,
  kDecoderG722_2ch,
  kDecoderRED,
  kDecoderAVT,
  kDecoderAVT16kHz,
  kDecoderAVT32kHz,
  kDecoderAVT48kHz,
  kDecoderCNGnb,
  kDecoderCNGwb,
  kDecoderCNGswb32kHz,
  kDecoderCNGswb48kHz,
  kDecoderArbitrary,
  kDecoderOpus,
  kDecoderOpus_2ch,
};

rtc::Optional<SdpAudioFormat> NetEqDecoderToSdpAudioFormat(NetEqDecoder nd);

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_NETEQ_NETEQ_DECODER_ENUM_H_
