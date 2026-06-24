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
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/audio_codecs/audio_format.h"
#include "api/field_trials_view.h"
#include "api/media_types.h"
#include "api/payload_type.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/codec.h"
#include "media/base/codec_comparators.h"
#include "media/base/codec_list.h"
#include "media/base/media_constants.h"
#include "media/base/media_engine.h"
#include "pc/codec_configuration.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"

namespace webrtc {

namespace {

std::vector<CodecConfiguration> CollectAudioCodecConfigurations(
    const std::vector<AudioCodecSpec>& specs,
    bool add_auxiliary_codecs) {
  std::vector<CodecConfiguration> out;

  // Only generate CN payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_cn = {{8000, false}};
  // Only generate telephone-event payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_dtmf = {{8000, false},
                                                          {48000, false}};

  for (const AudioCodecSpec& spec : specs) {
    if (absl::EqualsIgnoreCase(spec.format.name, kRedCodecName)) {
      continue;
    }

    CodecConfiguration config;
    config.codec = CreateAudioCodec(spec.format);
    if (spec.info.supports_network_adaption) {
      config.codec.AddFeedbackParam(
          FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
    }

    if (add_auxiliary_codecs) {
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
    }

    if (config.codec.name == kOpusCodecName) {
      // Audio RED is handled by the engine, not the factory, and is always
      // available for Opus.
      config.resiliency.red = true;
    }
    out.push_back(config);
  }

  if (add_auxiliary_codecs) {
    // Add CN codecs after "proper" audio codecs.
    for (const auto& cn : generate_cn) {
      if (cn.second) {
        CodecConfiguration cn_config;
        cn_config.codec = CreateAudioCodec({kCnCodecName, cn.first, 1});
        out.push_back(cn_config);
      }
    }

    // Add telephone-event codecs last.
    for (const auto& dtmf : generate_dtmf) {
      if (dtmf.second) {
        CodecConfiguration dtmf_config;
        dtmf_config.codec = CreateAudioCodec({kDtmfCodecName, dtmf.first, 1});
        out.push_back(dtmf_config);
      }
    }
  }
  return out;
}

std::vector<CodecConfiguration> AudioCodecConfigurationsFromFactory(
    const VoiceEngineInterface& voice,
    bool is_sender) {
  RTC_DCHECK(!is_sender || voice.encoder_factory()) << "No encoder factory";
  RTC_DCHECK(is_sender || voice.decoder_factory()) << "No decoder factory";
  return CollectAudioCodecConfigurations(
      is_sender ? voice.encoder_factory()->GetSupportedEncoders()
                : voice.decoder_factory()->GetSupportedDecoders(),
      voice.NeedsAuxiliaryCodecsAdded());
}

std::vector<CodecConfiguration> CollectVideoCodecConfigurations(
    const std::vector<SdpVideoFormat>& formats,
    bool rtx_enabled,
    bool add_auxiliary_codecs,
    const FieldTrialsView& trials) {
  if (formats.empty()) {
    return {};
  }

  bool has_red = false;
  bool has_ulpfec = false;
  bool has_flexfec = false;
  bool has_rtx = false;

  for (const SdpVideoFormat& format : formats) {
    if (absl::EqualsIgnoreCase(format.name, kRedCodecName)) {
      has_red = true;
    } else if (absl::EqualsIgnoreCase(format.name, kUlpfecCodecName)) {
      has_ulpfec = true;
    } else if (absl::EqualsIgnoreCase(format.name, kFlexfecCodecName)) {
      has_flexfec = true;
    } else if (absl::EqualsIgnoreCase(format.name, kRtxCodecName)) {
      has_rtx = true;
    }
  }

  std::vector<CodecConfiguration> out;
  for (const SdpVideoFormat& format : formats) {
    Codec codec = CreateVideoCodec(format);
    if (codec.IsResiliencyCodec()) {
      continue;
    }

    AddDefaultFeedbackParams(&codec, trials);

    CodecConfiguration config;
    config.codec = codec;
    if (rtx_enabled && (has_rtx || add_auxiliary_codecs)) {
      Codec::ResiliencyType resiliency_type = codec.GetResiliencyType();
      if (resiliency_type != Codec::ResiliencyType::kFlexfec &&
          resiliency_type != Codec::ResiliencyType::kUlpfec) {
        config.resiliency.rtx = true;
      }
    }
    config.resiliency.red = has_red;
    config.resiliency.ulpfec = has_ulpfec;
    if (trials.IsEnabled("WebRTC-FlexFEC-03-Advertised") ||
        trials.IsEnabled("WebRTC-FlexFEC-03")) {
      config.resiliency.flexfec = has_flexfec;
    }
    out.push_back(config);
  }
  return out;
}

std::vector<CodecConfiguration> VideoCodecConfigurationsFromFactory(
    const VideoEngineInterface& video,
    bool is_sender,
    bool rtx_enabled,
    const FieldTrialsView& trials) {
  return CollectVideoCodecConfigurations(
      video.GetSupportedFormats(!is_sender), rtx_enabled,
      video.NeedsAuxiliaryCodecsAdded(), trials);
}

Codecs CodecsFromConfigurations(
    const std::vector<CodecConfiguration>& configurations,
    MediaType type,
    bool rtx_enabled) {
  Codecs out;
  flat_set<std::string> shared_added;
  bool video_red_needed = false;
  bool video_ulpfec_needed = false;
  bool video_flexfec_needed = false;

  for (const auto& config : configurations) {
    out.push_back(config.codec);
    if (type == MediaType::AUDIO) {
      if (config.resiliency.red && shared_added.insert(kRedCodecName).second) {
        out.push_back(CreateAudioCodec({kRedCodecName, 48000, 2}));
      }
    } else {
      if (config.resiliency.rtx) {
        out.push_back(CreateVideoCodec(PayloadType::NotSet(), kRtxCodecName));
      }
      if (config.resiliency.red) {
        video_red_needed = true;
      }
      if (config.resiliency.ulpfec) {
        video_ulpfec_needed = true;
      }
      if (config.resiliency.flexfec) {
        video_flexfec_needed = true;
      }
    }
  }

  // Add video resiliency codecs at the end, in the order: RED, FEC
  if (type == MediaType::VIDEO) {
    if (video_red_needed) {
      out.push_back(CreateVideoCodec(kRedCodecName));
      if (rtx_enabled) {
        out.push_back(CreateVideoCodec(PayloadType::NotSet(), kRtxCodecName));
      }
    }
    if (video_ulpfec_needed) {
      out.push_back(CreateVideoCodec(kUlpfecCodecName));
    }
    if (video_flexfec_needed) {
      out.push_back(CreateVideoCodec(kFlexfecCodecName));
    }
  }
  return out;
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
                 bool rtx_enabled) {
  const VoiceEngineInterface& voice = media_engine->voice();
  const VideoEngineInterface& video = media_engine->video();
  // Use current mechanisms for getting codecs from media engine.
  return (type == MediaType::AUDIO)
             ? (is_sender ? voice.LegacySendCodecs() : voice.LegacyRecvCodecs())
             : GetLegacyVideoCodecs(video, is_sender, rtx_enabled);
}

}  // namespace

TypedCodecVendor::TypedCodecVendor(const MediaEngineInterface* absl_nonnull
                                       media_engine,
                                   MediaType type,
                                   bool is_sender,
                                   bool rtx_enabled,
                                   const FieldTrialsView& trials) {
  RTC_DCHECK(media_engine != nullptr);

  if (trials.IsEnabled("WebRTC-PayloadTypesInTransport")) {
    if (type == MediaType::AUDIO) {
      configurations_ =
          AudioCodecConfigurationsFromFactory(media_engine->voice(), is_sender);
    } else {
      configurations_ = VideoCodecConfigurationsFromFactory(
          media_engine->video(), is_sender, rtx_enabled, trials);
    }
    codecs_ = CodecList::CreateFromTrustedData(
        CodecsFromConfigurations(configurations_, type, rtx_enabled));
  } else {
    codecs_ = CodecList::CreateFromTrustedData(
        GetCodecs(media_engine, type, is_sender, rtx_enabled));
  }
}

void TypedCodecVendor::SetRawPacketization(const Codec& codec) {
  for (CodecConfiguration& config : configurations_) {
    if (MatchesWithCodecRules(config.codec, codec)) {
      config.codec.packetization = kPacketizationParamRaw;
    }
  }
  for (Codec& c : codecs_.writable_codecs()) {
    if (MatchesWithCodecRules(c, codec)) {
      c.packetization = kPacketizationParamRaw;
    }
  }
}

}  // namespace webrtc
