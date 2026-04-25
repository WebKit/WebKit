/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/typed_codec_vendor.h"

#include <functional>
#include <map>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/audio_codecs/audio_format.h"
#include "api/field_trials_view.h"
#include "api/media_types.h"
#include "media/base/codec.h"
#include "media/base/codec_list.h"
#include "media/base/media_constants.h"
#include "media/base/media_engine.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

// Create the voice codecs. Do not allocate payload types at this time.
std::vector<Codec> CollectAudioCodecs(
    const std::vector<AudioCodecSpec>& specs) {
  std::vector<Codec> out;

  // Only generate CN payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_cn = {{8000, false}};
  // Only generate telephone-event payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_dtmf = {{8000, false},
                                                          {48000, false}};

  for (const auto& spec : specs) {
    Codec codec = CreateAudioCodec(spec.format);
    if (spec.info.supports_network_adaption) {
      codec.AddFeedbackParam(
          FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
    }

    if (spec.info.allow_comfort_noise) {
      // Generate a CN entry if the decoder allows it and we support the
      // clockrate.
      auto cn = generate_cn.find(spec.format.clockrate_hz);
      if (cn != generate_cn.end()) {
        cn->second = true;
      }
    }

    // Generate a telephone-event entry if we support the clockrate.
    auto dtmf = generate_dtmf.find(spec.format.clockrate_hz);
    if (dtmf != generate_dtmf.end()) {
      dtmf->second = true;
    }

    out.push_back(codec);

    // TODO(hta):  Don't assign RED codecs until we know that the PT for Opus
    // is final
    if (codec.name == kOpusCodecName) {
      // We don't know the PT to put into the RED fmtp parameter yet.
      // Leave it out.
      Codec red_codec = CreateAudioCodec({kRedCodecName, 48000, 2});
      out.push_back(red_codec);
    }
  }

  // Add CN codecs after "proper" audio codecs.
  for (const auto& cn : generate_cn) {
    if (cn.second) {
      Codec cn_codec = CreateAudioCodec({kCnCodecName, cn.first, 1});
      out.push_back(cn_codec);
    }
  }

  // Add telephone-event codecs last.
  for (const auto& dtmf : generate_dtmf) {
    if (dtmf.second) {
      Codec dtmf_codec = CreateAudioCodec({kDtmfCodecName, dtmf.first, 1});
      out.push_back(dtmf_codec);
    }
  }
  return out;
}

Codecs AudioCodecsFromFactory(const VoiceEngineInterface& voice,
                              bool is_sender) {
  RTC_DCHECK(!is_sender || voice.encoder_factory()) << "No encoder factory";
  RTC_DCHECK(is_sender || voice.decoder_factory()) << "No decoder factory";
  return CollectAudioCodecs(
      is_sender ? voice.encoder_factory()->GetSupportedEncoders()
                : voice.decoder_factory()->GetSupportedDecoders());
}

Codecs GetLegacyVideoCodecs(const VideoEngineInterface& video,
                            bool is_sender,
                            bool rtx_enabled) {
  return is_sender ? video.LegacySendCodecs(rtx_enabled)
                   : video.LegacyRecvCodecs(rtx_enabled);
}

Codecs GetCodecs(const MediaEngineInterface* media_engine,
                 MediaType type,
                 bool is_sender,
                 bool rtx_enabled,
                 const FieldTrialsView& trials) {
  const VoiceEngineInterface& voice = media_engine->voice();
  const VideoEngineInterface& video = media_engine->video();
  if (trials.IsEnabled("WebRTC-PayloadTypesInTransport")) {
    // Use legacy mechanisms for getting codecs from video engine.
    // TODO: https://issues.webrtc.org/360058654 - apply late assign to video.
    return (type == MediaType::AUDIO)
               ? AudioCodecsFromFactory(voice, is_sender)
               : GetLegacyVideoCodecs(video, is_sender, rtx_enabled);
  }

  // Use current mechanisms for getting codecs from media engine.
  return (type == MediaType::AUDIO)
             ? (is_sender ? voice.LegacySendCodecs() : voice.LegacyRecvCodecs())
             : GetLegacyVideoCodecs(video, is_sender, rtx_enabled);
}

}  // namespace

TypedCodecVendor::TypedCodecVendor(const MediaEngineInterface* media_engine,
                                   MediaType type,
                                   bool is_sender,
                                   bool rtx_enabled,
                                   const FieldTrialsView& trials)
    : codecs_(CodecList::CreateFromTrustedData(
          GetCodecs(media_engine, type, is_sender, rtx_enabled, trials))) {}

}  // namespace webrtc
