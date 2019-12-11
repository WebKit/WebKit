/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_ACM2_RENT_A_CODEC_H_
#define MODULES_AUDIO_CODING_ACM2_RENT_A_CODEC_H_

#include <stddef.h>
#include <map>
#include <memory>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_encoder.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "modules/audio_coding/neteq/neteq_decoder_enum.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

struct CodecInst;
class LockedIsacBandwidthInfo;

namespace acm2 {

struct RentACodec {
  enum class CodecId {
#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
    kISAC,
#endif
#ifdef WEBRTC_CODEC_ISAC
    kISACSWB,
#endif
    // Mono
    kPCM16B,
    kPCM16Bwb,
    kPCM16Bswb32kHz,
    // Stereo
    kPCM16B_2ch,
    kPCM16Bwb_2ch,
    kPCM16Bswb32kHz_2ch,
    // Mono
    kPCMU,
    kPCMA,
    // Stereo
    kPCMU_2ch,
    kPCMA_2ch,
#ifdef WEBRTC_CODEC_ILBC
    kILBC,
#endif
    kG722,      // Mono
    kG722_2ch,  // Stereo
#ifdef WEBRTC_CODEC_OPUS
    kOpus,  // Mono and stereo
#endif
    kCNNB,
    kCNWB,
    kCNSWB,
#ifdef ENABLE_48000_HZ
    kCNFB,
#endif
    kAVT,
    kAVT16kHz,
    kAVT32kHz,
    kAVT48kHz,
#ifdef WEBRTC_CODEC_RED
    kRED,
#endif
    kNumCodecs,  // Implementation detail. Don't use.

// Set unsupported codecs to -1.
#if !defined(WEBRTC_CODEC_ISAC) && !defined(WEBRTC_CODEC_ISACFX)
    kISAC = -1,
#endif
#ifndef WEBRTC_CODEC_ISAC
    kISACSWB = -1,
#endif
    // 48 kHz not supported, always set to -1.
    kPCM16Bswb48kHz = -1,
#ifndef WEBRTC_CODEC_ILBC
    kILBC = -1,
#endif
#ifndef WEBRTC_CODEC_OPUS
    kOpus = -1,  // Mono and stereo
#endif
#ifndef WEBRTC_CODEC_RED
    kRED = -1,
#endif
#ifndef ENABLE_48000_HZ
    kCNFB = -1,
#endif

    kNone = -1
  };

  static inline size_t NumberOfCodecs() {
    return static_cast<size_t>(CodecId::kNumCodecs);
  }

  static inline absl::optional<int> CodecIndexFromId(CodecId codec_id) {
    const int i = static_cast<int>(codec_id);
    return i >= 0 && i < static_cast<int>(NumberOfCodecs())
               ? absl::optional<int>(i)
               : absl::nullopt;
  }

  static inline absl::optional<CodecId> CodecIdFromIndex(int codec_index) {
    return static_cast<size_t>(codec_index) < NumberOfCodecs()
               ? absl::optional<RentACodec::CodecId>(
                     static_cast<RentACodec::CodecId>(codec_index))
               : absl::nullopt;
  }

  static absl::optional<CodecId> CodecIdByParams(const char* payload_name,
                                                 int sampling_freq_hz,
                                                 size_t channels);
  static absl::optional<CodecInst> CodecInstById(CodecId codec_id);
  static absl::optional<CodecId> CodecIdByInst(const CodecInst& codec_inst);
  static absl::optional<CodecInst> CodecInstByParams(const char* payload_name,
                                                     int sampling_freq_hz,
                                                     size_t channels);

  static inline bool IsPayloadTypeValid(int payload_type) {
    return payload_type >= 0 && payload_type <= 127;
  }

  static absl::optional<NetEqDecoder> NetEqDecoderFromCodecId(
      CodecId codec_id,
      size_t num_channels);
};

}  // namespace acm2
}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_ACM2_RENT_A_CODEC_H_
