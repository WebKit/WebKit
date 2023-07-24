/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/webrtc_video_engine.h"

#include <stdio.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/match.h"
#include "absl/types/optional.h"
#include "api/media_stream_interface.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/call.h"
#include "media/engine/webrtc_media_engine.h"
#include "media/engine/webrtc_voice_engine.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "modules/video_coding/codecs/vp9/svc_config.h"
#include "modules/video_coding/svc/scalability_mode_util.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"

namespace cricket {

namespace {

using ::webrtc::ParseRtpPayloadType;
using ::webrtc::ParseRtpSsrc;

constexpr int64_t kUnsignaledSsrcCooldownMs = rtc::kNumMillisecsPerSec / 2;

// TODO(bugs.webrtc.org/13166): Remove AV1X when backwards compatibility is not
// needed.
constexpr char kAv1xCodecName[] = "AV1X";

const char* StreamTypeToString(
    webrtc::VideoSendStream::StreamStats::StreamType type) {
  switch (type) {
    case webrtc::VideoSendStream::StreamStats::StreamType::kMedia:
      return "kMedia";
    case webrtc::VideoSendStream::StreamStats::StreamType::kRtx:
      return "kRtx";
    case webrtc::VideoSendStream::StreamStats::StreamType::kFlexfec:
      return "kFlexfec";
  }
  return nullptr;
}

bool IsEnabled(const webrtc::FieldTrialsView& trials, absl::string_view name) {
  return absl::StartsWith(trials.Lookup(name), "Enabled");
}

bool IsDisabled(const webrtc::FieldTrialsView& trials, absl::string_view name) {
  return absl::StartsWith(trials.Lookup(name), "Disabled");
}

void AddDefaultFeedbackParams(VideoCodec* codec,
                              const webrtc::FieldTrialsView& trials) {
  // Don't add any feedback params for RED and ULPFEC.
  if (codec->name == kRedCodecName || codec->name == kUlpfecCodecName)
    return;
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamRemb, kParamValueEmpty));
  codec->AddFeedbackParam(
      FeedbackParam(kRtcpFbParamTransportCc, kParamValueEmpty));
  // Don't add any more feedback params for FLEXFEC.
  if (codec->name == kFlexfecCodecName)
    return;
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamCcm, kRtcpFbCcmParamFir));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kParamValueEmpty));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kRtcpFbNackParamPli));
  if (codec->name == kVp8CodecName &&
      IsEnabled(trials, "WebRTC-RtcpLossNotification")) {
    codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamLntf, kParamValueEmpty));
  }
}

// Helper function to determine whether a codec should use the [35, 63] range.
// Should be used when adding new codecs (or variants).
bool IsCodecValidForLowerRange(const VideoCodec& codec) {
  if (absl::EqualsIgnoreCase(codec.name, kFlexfecCodecName) ||
      absl::EqualsIgnoreCase(codec.name, kAv1CodecName) ||
      absl::EqualsIgnoreCase(codec.name, kAv1xCodecName)) {
    return true;
  } else if (absl::EqualsIgnoreCase(codec.name, kH264CodecName)) {
    std::string profile_level_id;
    std::string packetization_mode;

    if (codec.GetParam(kH264FmtpProfileLevelId, &profile_level_id)) {
      if (absl::StartsWithIgnoreCase(profile_level_id, "4d00")) {
        if (codec.GetParam(kH264FmtpPacketizationMode, &packetization_mode)) {
          return packetization_mode == "0";
        }
      }
      // H264 with YUV444.
      return absl::StartsWithIgnoreCase(profile_level_id, "f400");
    }
  } else if (absl::EqualsIgnoreCase(codec.name, kVp9CodecName)) {
    std::string profile_id;

    if (codec.GetParam(kVP9ProfileId, &profile_id)) {
      if (profile_id.compare("1") == 0 || profile_id.compare("3") == 0) {
        return true;
      }
    }
  }
  return false;
}

// This function will assign dynamic payload types (in the range [96, 127]
// and then [35, 63]) to the input codecs, and also add ULPFEC, RED, FlexFEC,
// and associated RTX codecs for recognized codecs (VP8, VP9, H264, and RED).
// It will also add default feedback params to the codecs.
// is_decoder_factory is needed to keep track of the implict assumption that any
// H264 decoder also supports constrained base line profile.
// Also, is_decoder_factory is used to decide whether FlexFEC video format
// should be advertised as supported.
// TODO(kron): Perhaps it is better to move the implicit knowledge to the place
// where codecs are negotiated.
template <class T>
std::vector<VideoCodec> GetPayloadTypesAndDefaultCodecs(
    const T* factory,
    bool is_decoder_factory,
    bool include_rtx,
    const webrtc::FieldTrialsView& trials) {
  if (!factory) {
    return {};
  }

  std::vector<webrtc::SdpVideoFormat> supported_formats =
      factory->GetSupportedFormats();
  if (is_decoder_factory) {
    AddH264ConstrainedBaselineProfileToSupportedFormats(&supported_formats);
  }

  if (supported_formats.empty())
    return std::vector<VideoCodec>();

  supported_formats.push_back(webrtc::SdpVideoFormat(kRedCodecName));
  supported_formats.push_back(webrtc::SdpVideoFormat(kUlpfecCodecName));

  // flexfec-03 is supported as
  // - receive codec unless WebRTC-FlexFEC-03-Advertised is disabled
  // - send codec if WebRTC-FlexFEC-03-Advertised is enabled
  if ((is_decoder_factory &&
       !IsDisabled(trials, "WebRTC-FlexFEC-03-Advertised")) ||
      (!is_decoder_factory &&
       IsEnabled(trials, "WebRTC-FlexFEC-03-Advertised"))) {
    webrtc::SdpVideoFormat flexfec_format(kFlexfecCodecName);
    // This value is currently arbitrarily set to 10 seconds. (The unit
    // is microseconds.) This parameter MUST be present in the SDP, but
    // we never use the actual value anywhere in our code however.
    // TODO(brandtr): Consider honouring this value in the sender and receiver.
    flexfec_format.parameters = {{kFlexfecFmtpRepairWindow, "10000000"}};
    supported_formats.push_back(flexfec_format);
  }

  // Due to interoperability issues with old Chrome/WebRTC versions that
  // ignore the [35, 63] range prefer the lower range for new codecs.
  static const int kFirstDynamicPayloadTypeLowerRange = 35;
  static const int kLastDynamicPayloadTypeLowerRange = 63;

  static const int kFirstDynamicPayloadTypeUpperRange = 96;
  static const int kLastDynamicPayloadTypeUpperRange = 127;
  int payload_type_upper = kFirstDynamicPayloadTypeUpperRange;
  int payload_type_lower = kFirstDynamicPayloadTypeLowerRange;

  std::vector<VideoCodec> output_codecs;
  for (const webrtc::SdpVideoFormat& format : supported_formats) {
    VideoCodec codec(format);
    bool isFecCodec = absl::EqualsIgnoreCase(codec.name, kUlpfecCodecName) ||
                      absl::EqualsIgnoreCase(codec.name, kFlexfecCodecName);

    // Check if we ran out of payload types.
    if (payload_type_lower > kLastDynamicPayloadTypeLowerRange) {
      // TODO(https://bugs.chromium.org/p/webrtc/issues/detail?id=12248):
      // return an error.
      RTC_LOG(LS_ERROR) << "Out of dynamic payload types [35,63] after "
                           "fallback from [96, 127], skipping the rest.";
      RTC_DCHECK_EQ(payload_type_upper, kLastDynamicPayloadTypeUpperRange);
      break;
    }

    // Lower range gets used for "new" codecs or when running out of payload
    // types in the upper range.
    if (IsCodecValidForLowerRange(codec) ||
        payload_type_upper >= kLastDynamicPayloadTypeUpperRange) {
      codec.id = payload_type_lower++;
    } else {
      codec.id = payload_type_upper++;
    }
    AddDefaultFeedbackParams(&codec, trials);
    output_codecs.push_back(codec);

    // Add associated RTX codec for non-FEC codecs.
    if (include_rtx) {
      if (!isFecCodec) {
        // Check if we ran out of payload types.
        if (payload_type_lower > kLastDynamicPayloadTypeLowerRange) {
          // TODO(https://bugs.chromium.org/p/webrtc/issues/detail?id=12248):
          // return an error.
          RTC_LOG(LS_ERROR) << "Out of dynamic payload types [35,63] after "
                               "fallback from [96, 127], skipping the rest.";
          RTC_DCHECK_EQ(payload_type_upper, kLastDynamicPayloadTypeUpperRange);
          break;
        }
        if (IsCodecValidForLowerRange(codec) ||
            payload_type_upper >= kLastDynamicPayloadTypeUpperRange) {
          output_codecs.push_back(
              VideoCodec::CreateRtxCodec(payload_type_lower++, codec.id));
        } else {
          output_codecs.push_back(
              VideoCodec::CreateRtxCodec(payload_type_upper++, codec.id));
        }
      }
    }
  }
  return output_codecs;
}

static std::string CodecVectorToString(const std::vector<VideoCodec>& codecs) {
  rtc::StringBuilder out;
  out << "{";
  for (size_t i = 0; i < codecs.size(); ++i) {
    out << codecs[i].ToString();
    if (i != codecs.size() - 1) {
      out << ", ";
    }
  }
  out << "}";
  return out.Release();
}

static bool ValidateCodecFormats(const std::vector<VideoCodec>& codecs) {
  bool has_video = false;
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (!codecs[i].ValidateCodecFormat()) {
      return false;
    }
    if (codecs[i].GetCodecType() == VideoCodec::CODEC_VIDEO) {
      has_video = true;
    }
  }
  if (!has_video) {
    RTC_LOG(LS_ERROR) << "Setting codecs without a video codec is invalid: "
                      << CodecVectorToString(codecs);
    return false;
  }
  return true;
}

static bool ValidateStreamParams(const StreamParams& sp) {
  if (sp.ssrcs.empty()) {
    RTC_LOG(LS_ERROR) << "No SSRCs in stream parameters: " << sp.ToString();
    return false;
  }

  std::vector<uint32_t> primary_ssrcs;
  sp.GetPrimarySsrcs(&primary_ssrcs);
  std::vector<uint32_t> rtx_ssrcs;
  sp.GetFidSsrcs(primary_ssrcs, &rtx_ssrcs);
  for (uint32_t rtx_ssrc : rtx_ssrcs) {
    bool rtx_ssrc_present = false;
    for (uint32_t sp_ssrc : sp.ssrcs) {
      if (sp_ssrc == rtx_ssrc) {
        rtx_ssrc_present = true;
        break;
      }
    }
    if (!rtx_ssrc_present) {
      RTC_LOG(LS_ERROR) << "RTX SSRC '" << rtx_ssrc
                        << "' missing from StreamParams ssrcs: "
                        << sp.ToString();
      return false;
    }
  }
  if (!rtx_ssrcs.empty() && primary_ssrcs.size() != rtx_ssrcs.size()) {
    RTC_LOG(LS_ERROR)
        << "RTX SSRCs exist, but don't cover all SSRCs (unsupported): "
        << sp.ToString();
    return false;
  }

  return true;
}

// Returns true if the given codec is disallowed from doing simulcast.
bool IsCodecDisabledForSimulcast(bool legacy_scalability_mode,
                                 webrtc::VideoCodecType codec_type) {
  if (legacy_scalability_mode && (codec_type == webrtc::kVideoCodecVP9 ||
                                  codec_type == webrtc::kVideoCodecAV1)) {
    return true;
  }

  return false;
}

// Returns its smallest positive argument. If neither argument is positive,
// returns an arbitrary nonpositive value.
int MinPositive(int a, int b) {
  if (a <= 0) {
    return b;
  }
  if (b <= 0) {
    return a;
  }
  return std::min(a, b);
}

bool IsLayerActive(const webrtc::RtpEncodingParameters& layer) {
  return layer.active &&
         (!layer.max_bitrate_bps || *layer.max_bitrate_bps > 0) &&
         (!layer.max_framerate || *layer.max_framerate > 0);
}

int NumActiveStreams(const webrtc::RtpParameters& rtp_parameters) {
  int res = 0;
  for (size_t i = 0; i < rtp_parameters.encodings.size(); ++i) {
    if (rtp_parameters.encodings[i].active) {
      ++res;
    }
  }
  return res;
}

absl::optional<int> NumSpatialLayersFromEncoding(
    const webrtc::RtpParameters& rtp_parameters,
    size_t idx) {
  if (idx >= rtp_parameters.encodings.size())
    return absl::nullopt;

  absl::optional<webrtc::ScalabilityMode> scalability_mode =
      webrtc::ScalabilityModeFromString(
          rtp_parameters.encodings[idx].scalability_mode.value_or(""));
  return scalability_mode
             ? absl::optional<int>(
                   ScalabilityModeToNumSpatialLayers(*scalability_mode))
             : absl::nullopt;
}

std::map<uint32_t, webrtc::VideoSendStream::StreamStats>
MergeInfoAboutOutboundRtpSubstreams(
    const std::map<uint32_t, webrtc::VideoSendStream::StreamStats>&
        substreams) {
  std::map<uint32_t, webrtc::VideoSendStream::StreamStats> rtp_substreams;
  // Add substreams for all RTP media streams.
  for (const auto& pair : substreams) {
    uint32_t ssrc = pair.first;
    const webrtc::VideoSendStream::StreamStats& substream = pair.second;
    switch (substream.type) {
      case webrtc::VideoSendStream::StreamStats::StreamType::kMedia:
        break;
      case webrtc::VideoSendStream::StreamStats::StreamType::kRtx:
      case webrtc::VideoSendStream::StreamStats::StreamType::kFlexfec:
        continue;
    }
    rtp_substreams.insert(std::make_pair(ssrc, substream));
  }
  // Complement the kMedia substream stats with the associated kRtx and kFlexfec
  // substream stats.
  for (const auto& pair : substreams) {
    switch (pair.second.type) {
      case webrtc::VideoSendStream::StreamStats::StreamType::kMedia:
        continue;
      case webrtc::VideoSendStream::StreamStats::StreamType::kRtx:
      case webrtc::VideoSendStream::StreamStats::StreamType::kFlexfec:
        break;
    }
    // The associated substream is an RTX or FlexFEC substream that is
    // referencing an RTP media substream.
    const webrtc::VideoSendStream::StreamStats& associated_substream =
        pair.second;
    RTC_DCHECK(associated_substream.referenced_media_ssrc.has_value());
    uint32_t media_ssrc = associated_substream.referenced_media_ssrc.value();
    if (substreams.find(media_ssrc) == substreams.end()) {
      RTC_LOG(LS_WARNING) << "Substream [ssrc: " << pair.first << ", type: "
                          << StreamTypeToString(associated_substream.type)
                          << "] is associated with a media ssrc (" << media_ssrc
                          << ") that does not have StreamStats. Ignoring its "
                          << "RTP stats.";
      continue;
    }
    webrtc::VideoSendStream::StreamStats& rtp_substream =
        rtp_substreams[media_ssrc];

    // We only merge `rtp_stats`. All other metrics are not applicable for RTX
    // and FlexFEC.
    // TODO(hbos): kRtx and kFlexfec stats should use a separate struct to make
    // it clear what is or is not applicable.
    rtp_substream.rtp_stats.Add(associated_substream.rtp_stats);
  }
  return rtp_substreams;
}

bool IsActiveFromEncodings(
    absl::optional<uint32_t> ssrc,
    const std::vector<webrtc::RtpEncodingParameters>& encodings) {
  if (ssrc.has_value()) {
    // Report the `active` value of a specific ssrc, or false if an encoding
    // with this ssrc does not exist.
    auto encoding_it = std::find_if(
        encodings.begin(), encodings.end(),
        [ssrc = ssrc.value()](const webrtc::RtpEncodingParameters& encoding) {
          return encoding.ssrc.has_value() && encoding.ssrc.value() == ssrc;
        });
    return encoding_it != encodings.end() ? encoding_it->active : false;
  }
  // If `ssrc` is not specified then any encoding being active counts as active.
  for (const auto& encoding : encodings) {
    if (encoding.active) {
      return true;
    }
  }
  return false;
}

bool IsScalabilityModeSupportedByCodec(
    const VideoCodec& codec,
    const std::string& scalability_mode,
    const webrtc::VideoSendStream::Config& config) {
  return config.encoder_settings.encoder_factory
      ->QueryCodecSupport(webrtc::SdpVideoFormat(codec.name, codec.params),
                          scalability_mode)
      .is_supported;
}

// Fallback to default value if the scalability mode is unset or unsupported by
// the codec.
void FallbackToDefaultScalabilityModeIfNotSupported(
    const VideoCodec& codec,
    const webrtc::VideoSendStream::Config& config,
    std::vector<webrtc::RtpEncodingParameters>& encodings) {
  if (!absl::c_any_of(encodings,
                      [](const webrtc::RtpEncodingParameters& encoding) {
                        return encoding.scalability_mode &&
                               !encoding.scalability_mode->empty();
                      })) {
    // Fallback is only enabled if the scalability mode is configured for any of
    // the encodings for now.
    return;
  }
  if (config.encoder_settings.encoder_factory == nullptr) {
    return;
  }
  for (auto& encoding : encodings) {
    RTC_LOG(LS_INFO) << "Encoding scalability_mode: "
                     << encoding.scalability_mode.value_or("-");
    if (!encoding.active && !encoding.scalability_mode.has_value()) {
      // Inactive encodings should not fallback since apps may only specify the
      // scalability mode of the first encoding when the others are inactive.
      continue;
    }
    if (!encoding.scalability_mode.has_value() ||
        !IsScalabilityModeSupportedByCodec(codec, *encoding.scalability_mode,
                                           config)) {
      encoding.scalability_mode = webrtc::kDefaultScalabilityModeStr;
      RTC_LOG(LS_INFO) << " -> " << *encoding.scalability_mode;
    }
  }
}

}  // namespace

// This constant is really an on/off, lower-level configurable NACK history
// duration hasn't been implemented.
static const int kNackHistoryMs = 1000;

static const int kDefaultRtcpReceiverReportSsrc = 1;

// Minimum time interval for logging stats.
static const int64_t kStatsLogIntervalMs = 10000;

std::map<uint32_t, webrtc::VideoSendStream::StreamStats>
MergeInfoAboutOutboundRtpSubstreamsForTesting(
    const std::map<uint32_t, webrtc::VideoSendStream::StreamStats>&
        substreams) {
  return MergeInfoAboutOutboundRtpSubstreams(substreams);
}

rtc::scoped_refptr<webrtc::VideoEncoderConfig::EncoderSpecificSettings>
WebRtcVideoChannel::WebRtcVideoSendStream::ConfigureVideoEncoderSettings(
    const VideoCodec& codec) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  bool is_screencast = parameters_.options.is_screencast.value_or(false);
  // No automatic resizing when using simulcast or screencast, or when
  // disabled by field trial flag.
  bool automatic_resize = !disable_automatic_resize_ && !is_screencast &&
                          (parameters_.config.rtp.ssrcs.size() == 1 ||
                           NumActiveStreams(rtp_parameters_) == 1);

  bool denoising;
  bool codec_default_denoising = false;
  if (is_screencast) {
    denoising = false;
  } else {
    // Use codec default if video_noise_reduction is unset.
    codec_default_denoising = !parameters_.options.video_noise_reduction;
    denoising = parameters_.options.video_noise_reduction.value_or(false);
  }

  if (absl::EqualsIgnoreCase(codec.name, kH264CodecName)) {
    return nullptr;
  }
  if (absl::EqualsIgnoreCase(codec.name, kVp8CodecName)) {
    webrtc::VideoCodecVP8 vp8_settings =
        webrtc::VideoEncoder::GetDefaultVp8Settings();
    vp8_settings.automaticResizeOn = automatic_resize;
    // VP8 denoising is enabled by default.
    vp8_settings.denoisingOn = codec_default_denoising ? true : denoising;
    return rtc::make_ref_counted<
        webrtc::VideoEncoderConfig::Vp8EncoderSpecificSettings>(vp8_settings);
  }
  if (absl::EqualsIgnoreCase(codec.name, kVp9CodecName)) {
    webrtc::VideoCodecVP9 vp9_settings =
        webrtc::VideoEncoder::GetDefaultVp9Settings();

    vp9_settings.numberOfSpatialLayers = std::min<unsigned char>(
        parameters_.config.rtp.ssrcs.size(), kConferenceMaxNumSpatialLayers);
    vp9_settings.numberOfTemporalLayers =
        std::min<unsigned char>(parameters_.config.rtp.ssrcs.size() > 1
                                    ? kConferenceDefaultNumTemporalLayers
                                    : 1,
                                kConferenceMaxNumTemporalLayers);

    // VP9 denoising is disabled by default.
    vp9_settings.denoisingOn = codec_default_denoising ? true : denoising;
    // Disable automatic resize if more than one spatial layer is requested.
    bool vp9_automatic_resize = automatic_resize;
    absl::optional<int> num_spatial_layers =
        NumSpatialLayersFromEncoding(rtp_parameters_, /*idx=*/0);
    if (num_spatial_layers && *num_spatial_layers > 1) {
      vp9_automatic_resize = false;
    }
    vp9_settings.automaticResizeOn = vp9_automatic_resize;
    if (!is_screencast) {
      webrtc::FieldTrialFlag interlayer_pred_experiment_enabled("Enabled");
      webrtc::FieldTrialEnum<webrtc::InterLayerPredMode> inter_layer_pred_mode(
          "inter_layer_pred_mode", webrtc::InterLayerPredMode::kOnKeyPic,
          {{"off", webrtc::InterLayerPredMode::kOff},
           {"on", webrtc::InterLayerPredMode::kOn},
           {"onkeypic", webrtc::InterLayerPredMode::kOnKeyPic}});
      webrtc::FieldTrialFlag force_flexible_mode("FlexibleMode");
      webrtc::ParseFieldTrial(
          {&interlayer_pred_experiment_enabled, &inter_layer_pred_mode,
           &force_flexible_mode},
          call_->trials().Lookup("WebRTC-Vp9InterLayerPred"));
      if (interlayer_pred_experiment_enabled) {
        vp9_settings.interLayerPred = inter_layer_pred_mode;
      } else {
        // Limit inter-layer prediction to key pictures by default.
        vp9_settings.interLayerPred = webrtc::InterLayerPredMode::kOnKeyPic;
      }
      vp9_settings.flexibleMode = force_flexible_mode.Get();
    } else {
      // Multiple spatial layers vp9 screenshare needs flexible mode.
      vp9_settings.flexibleMode = vp9_settings.numberOfSpatialLayers > 1;
      vp9_settings.interLayerPred = webrtc::InterLayerPredMode::kOn;
    }
    return rtc::make_ref_counted<
        webrtc::VideoEncoderConfig::Vp9EncoderSpecificSettings>(vp9_settings);
  }
  return nullptr;
}

WebRtcVideoEngine::WebRtcVideoEngine(
    std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
    std::unique_ptr<webrtc::VideoDecoderFactory> video_decoder_factory,
    const webrtc::FieldTrialsView& trials)
    : decoder_factory_(std::move(video_decoder_factory)),
      encoder_factory_(std::move(video_encoder_factory)),
      trials_(trials) {
  RTC_DLOG(LS_INFO) << "WebRtcVideoEngine::WebRtcVideoEngine()";
}

WebRtcVideoEngine::~WebRtcVideoEngine() {
  RTC_DLOG(LS_INFO) << "WebRtcVideoEngine::~WebRtcVideoEngine";
}

VideoMediaChannel* WebRtcVideoEngine::CreateMediaChannel(
    MediaChannel::Role role,
    webrtc::Call* call,
    const MediaConfig& config,
    const VideoOptions& options,
    const webrtc::CryptoOptions& crypto_options,
    webrtc::VideoBitrateAllocatorFactory* video_bitrate_allocator_factory) {
  RTC_LOG(LS_INFO) << "CreateMediaChannel. Options: " << options.ToString();
  return new WebRtcVideoChannel(role, call, config, options, crypto_options,
                                encoder_factory_.get(), decoder_factory_.get(),
                                video_bitrate_allocator_factory);
}
std::vector<VideoCodec> WebRtcVideoEngine::send_codecs(bool include_rtx) const {
  return GetPayloadTypesAndDefaultCodecs(encoder_factory_.get(),
                                         /*is_decoder_factory=*/false,
                                         include_rtx, trials_);
}

std::vector<VideoCodec> WebRtcVideoEngine::recv_codecs(bool include_rtx) const {
  return GetPayloadTypesAndDefaultCodecs(decoder_factory_.get(),
                                         /*is_decoder_factory=*/true,
                                         include_rtx, trials_);
}

std::vector<webrtc::RtpHeaderExtensionCapability>
WebRtcVideoEngine::GetRtpHeaderExtensions() const {
  std::vector<webrtc::RtpHeaderExtensionCapability> result;
  int id = 1;
  for (const auto& uri :
       {webrtc::RtpExtension::kTimestampOffsetUri,
        webrtc::RtpExtension::kAbsSendTimeUri,
        webrtc::RtpExtension::kVideoRotationUri,
        webrtc::RtpExtension::kTransportSequenceNumberUri,
        webrtc::RtpExtension::kPlayoutDelayUri,
        webrtc::RtpExtension::kVideoContentTypeUri,
        webrtc::RtpExtension::kVideoTimingUri,
        webrtc::RtpExtension::kColorSpaceUri, webrtc::RtpExtension::kMidUri,
        webrtc::RtpExtension::kRidUri, webrtc::RtpExtension::kRepairedRidUri}) {
    result.emplace_back(uri, id++, webrtc::RtpTransceiverDirection::kSendRecv);
  }
  result.emplace_back(webrtc::RtpExtension::kGenericFrameDescriptorUri00, id++,
                      IsEnabled(trials_, "WebRTC-GenericDescriptorAdvertised")
                          ? webrtc::RtpTransceiverDirection::kSendRecv
                          : webrtc::RtpTransceiverDirection::kStopped);
  result.emplace_back(
      webrtc::RtpExtension::kDependencyDescriptorUri, id++,
      IsEnabled(trials_, "WebRTC-DependencyDescriptorAdvertised")
          ? webrtc::RtpTransceiverDirection::kSendRecv
          : webrtc::RtpTransceiverDirection::kStopped);

  result.emplace_back(
      webrtc::RtpExtension::kVideoLayersAllocationUri, id++,
      IsEnabled(trials_, "WebRTC-VideoLayersAllocationAdvertised")
          ? webrtc::RtpTransceiverDirection::kSendRecv
          : webrtc::RtpTransceiverDirection::kStopped);

  result.emplace_back(
      webrtc::RtpExtension::kVideoFrameTrackingIdUri, id++,
      IsEnabled(trials_, "WebRTC-VideoFrameTrackingIdAdvertised")
          ? webrtc::RtpTransceiverDirection::kSendRecv
          : webrtc::RtpTransceiverDirection::kStopped);

  return result;
}

WebRtcVideoChannel::WebRtcVideoChannel(
    MediaChannel::Role role,
    webrtc::Call* call,
    const MediaConfig& config,
    const VideoOptions& options,
    const webrtc::CryptoOptions& crypto_options,
    webrtc::VideoEncoderFactory* encoder_factory,
    webrtc::VideoDecoderFactory* decoder_factory,
    webrtc::VideoBitrateAllocatorFactory* bitrate_allocator_factory)
    : VideoMediaChannel(role, call->network_thread(), config.enable_dscp),
      worker_thread_(call->worker_thread()),
      sending_(false),
      receiving_(false),
      call_(call),
      default_sink_(nullptr),
      video_config_(config.video),
      encoder_factory_(encoder_factory),
      decoder_factory_(decoder_factory),
      bitrate_allocator_factory_(bitrate_allocator_factory),
      default_send_options_(options),
      last_send_stats_log_ms_(-1),
      last_receive_stats_log_ms_(-1),
      discard_unknown_ssrc_packets_(
          IsEnabled(call_->trials(),
                    "WebRTC-Video-DiscardPacketsWithUnknownSsrc")),
      crypto_options_(crypto_options) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  rtcp_receiver_report_ssrc_ = kDefaultRtcpReceiverReportSsrc;
  recv_codecs_ = MapCodecs(GetPayloadTypesAndDefaultCodecs(
      decoder_factory_, /*is_decoder_factory=*/true,
      /*include_rtx=*/true, call_->trials()));
  recv_flexfec_payload_type_ =
      recv_codecs_.empty() ? 0 : recv_codecs_.front().flexfec_payload_type;
}

WebRtcVideoChannel::~WebRtcVideoChannel() {
  for (auto& kv : send_streams_)
    delete kv.second;
  for (auto& kv : receive_streams_)
    delete kv.second;
}

std::vector<WebRtcVideoChannel::VideoCodecSettings>
WebRtcVideoChannel::SelectSendVideoCodecs(
    const std::vector<VideoCodecSettings>& remote_mapped_codecs) const {
  std::vector<webrtc::SdpVideoFormat> sdp_formats =
      encoder_factory_ ? encoder_factory_->GetImplementations()
                       : std::vector<webrtc::SdpVideoFormat>();

  // The returned vector holds the VideoCodecSettings in term of preference.
  // They are orderd by receive codec preference first and local implementation
  // preference second.
  std::vector<VideoCodecSettings> encoders;
  for (const VideoCodecSettings& remote_codec : remote_mapped_codecs) {
    for (auto format_it = sdp_formats.begin();
         format_it != sdp_formats.end();) {
      // For H264, we will limit the encode level to the remote offered level
      // regardless if level asymmetry is allowed or not. This is strictly not
      // following the spec in https://tools.ietf.org/html/rfc6184#section-8.2.2
      // since we should limit the encode level to the lower of local and remote
      // level when level asymmetry is not allowed.
      if (format_it->IsSameCodec(
              {remote_codec.codec.name, remote_codec.codec.params})) {
        encoders.push_back(remote_codec);

        // To allow the VideoEncoderFactory to keep information about which
        // implementation to instantitate when CreateEncoder is called the two
        // parmeter sets are merged.
        encoders.back().codec.params.insert(format_it->parameters.begin(),
                                            format_it->parameters.end());

        format_it = sdp_formats.erase(format_it);
      } else {
        ++format_it;
      }
    }
  }

  return encoders;
}

bool WebRtcVideoChannel::NonFlexfecReceiveCodecsHaveChanged(
    std::vector<VideoCodecSettings> before,
    std::vector<VideoCodecSettings> after) {
  // The receive codec order doesn't matter, so we sort the codecs before
  // comparing. This is necessary because currently the
  // only way to change the send codec is to munge SDP, which causes
  // the receive codec list to change order, which causes the streams
  // to be recreates which causes a "blink" of black video.  In order
  // to support munging the SDP in this way without recreating receive
  // streams, we ignore the order of the received codecs so that
  // changing the order doesn't cause this "blink".
  auto comparison = [](const VideoCodecSettings& codec1,
                       const VideoCodecSettings& codec2) {
    return codec1.codec.id > codec2.codec.id;
  };
  absl::c_sort(before, comparison);
  absl::c_sort(after, comparison);

  // Changes in FlexFEC payload type are handled separately in
  // WebRtcVideoChannel::GetChangedRecvParameters, so disregard FlexFEC in the
  // comparison here.
  return !absl::c_equal(before, after,
                        VideoCodecSettings::EqualsDisregardingFlexfec);
}

bool WebRtcVideoChannel::GetChangedSendParameters(
    const VideoSendParameters& params,
    ChangedSendParameters* changed_params) const {
  if (!ValidateCodecFormats(params.codecs) ||
      !ValidateRtpExtensions(params.extensions, send_rtp_extensions_)) {
    return false;
  }

  std::vector<VideoCodecSettings> negotiated_codecs =
      SelectSendVideoCodecs(MapCodecs(params.codecs));

  // We should only fail here if send direction is enabled.
  if (params.is_stream_active && negotiated_codecs.empty()) {
    RTC_LOG(LS_ERROR) << "No video codecs supported.";
    return false;
  }

  // Never enable sending FlexFEC, unless we are in the experiment.
  if (!IsEnabled(call_->trials(), "WebRTC-FlexFEC-03")) {
    for (VideoCodecSettings& codec : negotiated_codecs)
      codec.flexfec_payload_type = -1;
  }

  if (negotiated_codecs_ != negotiated_codecs) {
    if (negotiated_codecs.empty()) {
      changed_params->send_codec = absl::nullopt;
    } else if (send_codec() != negotiated_codecs.front()) {
      changed_params->send_codec = negotiated_codecs.front();
    }
    changed_params->negotiated_codecs = std::move(negotiated_codecs);
  }

  // Handle RTP header extensions.
  if (params.extmap_allow_mixed != ExtmapAllowMixed()) {
    changed_params->extmap_allow_mixed = params.extmap_allow_mixed;
  }
  std::vector<webrtc::RtpExtension> filtered_extensions = FilterRtpExtensions(
      params.extensions, webrtc::RtpExtension::IsSupportedForVideo, true,
      call_->trials());
  if (send_rtp_extensions_ != filtered_extensions) {
    changed_params->rtp_header_extensions =
        absl::optional<std::vector<webrtc::RtpExtension>>(filtered_extensions);
  }

  if (params.mid != send_params_.mid) {
    changed_params->mid = params.mid;
  }

  // Handle max bitrate.
  if (params.max_bandwidth_bps != send_params_.max_bandwidth_bps &&
      params.max_bandwidth_bps >= -1) {
    // 0 or -1 uncaps max bitrate.
    // TODO(pbos): Reconsider how 0 should be treated. It is not mentioned as a
    // special value and might very well be used for stopping sending.
    changed_params->max_bandwidth_bps =
        params.max_bandwidth_bps == 0 ? -1 : params.max_bandwidth_bps;
  }

  // Handle conference mode.
  if (params.conference_mode != send_params_.conference_mode) {
    changed_params->conference_mode = params.conference_mode;
  }

  // Handle RTCP mode.
  if (params.rtcp.reduced_size != send_params_.rtcp.reduced_size) {
    changed_params->rtcp_mode = params.rtcp.reduced_size
                                    ? webrtc::RtcpMode::kReducedSize
                                    : webrtc::RtcpMode::kCompound;
  }

  return true;
}

bool WebRtcVideoChannel::SetSendParameters(const VideoSendParameters& params) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetSendParameters");
  RTC_LOG(LS_INFO) << "SetSendParameters: " << params.ToString();
  ChangedSendParameters changed_params;
  if (!GetChangedSendParameters(params, &changed_params)) {
    return false;
  }

  if (changed_params.negotiated_codecs) {
    for (const auto& send_codec : *changed_params.negotiated_codecs)
      RTC_LOG(LS_INFO) << "Negotiated codec: " << send_codec.codec.ToString();
  }

  send_params_ = params;
  return ApplyChangedParams(changed_params);
}

void WebRtcVideoChannel::RequestEncoderFallback() {
  if (!worker_thread_->IsCurrent()) {
    worker_thread_->PostTask(
        SafeTask(task_safety_.flag(), [this] { RequestEncoderFallback(); }));
    return;
  }

  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (negotiated_codecs_.size() <= 1) {
    RTC_LOG(LS_WARNING) << "Encoder failed but no fallback codec is available";
    return;
  }

  ChangedSendParameters params;
  params.negotiated_codecs = negotiated_codecs_;
  params.negotiated_codecs->erase(params.negotiated_codecs->begin());
  params.send_codec = params.negotiated_codecs->front();
  ApplyChangedParams(params);
}

void WebRtcVideoChannel::RequestEncoderSwitch(
    const webrtc::SdpVideoFormat& format,
    bool allow_default_fallback) {
  if (!worker_thread_->IsCurrent()) {
    worker_thread_->PostTask(
        SafeTask(task_safety_.flag(), [this, format, allow_default_fallback] {
          RequestEncoderSwitch(format, allow_default_fallback);
        }));
    return;
  }

  RTC_DCHECK_RUN_ON(&thread_checker_);

  for (const VideoCodecSettings& codec_setting : negotiated_codecs_) {
    if (format.IsSameCodec(
            {codec_setting.codec.name, codec_setting.codec.params})) {
      VideoCodecSettings new_codec_setting = codec_setting;
      for (const auto& kv : format.parameters) {
        new_codec_setting.codec.params[kv.first] = kv.second;
      }

      if (send_codec() == new_codec_setting) {
        // Already using this codec, no switch required.
        return;
      }

      ChangedSendParameters params;
      params.send_codec = new_codec_setting;
      ApplyChangedParams(params);
      return;
    }
  }

  RTC_LOG(LS_WARNING) << "Failed to switch encoder to: " << format.ToString()
                      << ". Is default fallback allowed: "
                      << allow_default_fallback;

  if (allow_default_fallback) {
    RequestEncoderFallback();
  }
}

bool WebRtcVideoChannel::ApplyChangedParams(
    const ChangedSendParameters& changed_params) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (changed_params.negotiated_codecs)
    negotiated_codecs_ = *changed_params.negotiated_codecs;

  if (changed_params.send_codec)
    send_codec() = changed_params.send_codec;

  if (changed_params.extmap_allow_mixed) {
    SetExtmapAllowMixed(*changed_params.extmap_allow_mixed);
  }
  if (changed_params.rtp_header_extensions) {
    send_rtp_extensions_ = *changed_params.rtp_header_extensions;
  }

  if (changed_params.send_codec || changed_params.max_bandwidth_bps) {
    if (send_params_.max_bandwidth_bps == -1) {
      // Unset the global max bitrate (max_bitrate_bps) if max_bandwidth_bps is
      // -1, which corresponds to no "b=AS" attribute in SDP. Note that the
      // global max bitrate may be set below in GetBitrateConfigForCodec, from
      // the codec max bitrate.
      // TODO(pbos): This should be reconsidered (codec max bitrate should
      // probably not affect global call max bitrate).
      bitrate_config_.max_bitrate_bps = -1;
    }

    if (send_codec()) {
      // TODO(holmer): Changing the codec parameters shouldn't necessarily mean
      // that we change the min/max of bandwidth estimation. Reevaluate this.
      bitrate_config_ = GetBitrateConfigForCodec(send_codec()->codec);
      if (!changed_params.send_codec) {
        // If the codec isn't changing, set the start bitrate to -1 which means
        // "unchanged" so that BWE isn't affected.
        bitrate_config_.start_bitrate_bps = -1;
      }
    }

    if (send_params_.max_bandwidth_bps >= 0) {
      // Note that max_bandwidth_bps intentionally takes priority over the
      // bitrate config for the codec. This allows FEC to be applied above the
      // codec target bitrate.
      // TODO(pbos): Figure out whether b=AS means max bitrate for this
      // WebRtcVideoChannel (in which case we're good), or per sender (SSRC),
      // in which case this should not set a BitrateConstraints but rather
      // reconfigure all senders.
      bitrate_config_.max_bitrate_bps = send_params_.max_bandwidth_bps == 0
                                            ? -1
                                            : send_params_.max_bandwidth_bps;
    }

    call_->GetTransportControllerSend()->SetSdpBitrateParameters(
        bitrate_config_);
  }

  for (auto& kv : send_streams_) {
    kv.second->SetSendParameters(changed_params);
  }
  if (role() == MediaChannel::Role::kBoth) {
    if (changed_params.send_codec || changed_params.rtcp_mode) {
      // Update receive feedback parameters from new codec or RTCP mode.
      if (send_codec()) {
        SetReceiverFeedbackParameters(
            HasLntf(send_codec()->codec), HasNack(send_codec()->codec),
            send_params_.rtcp.reduced_size ? webrtc::RtcpMode::kReducedSize
                                           : webrtc::RtcpMode::kCompound,
            send_codec()->rtx_time);
      }
    }
  } else {
    if (changed_params.send_codec || changed_params.rtcp_mode) {
      send_codec_changed_callback_();
    }
  }
  return true;
}

void WebRtcVideoChannel::SetReceiverFeedbackParameters(
    bool lntf_enabled,
    bool nack_enabled,
    webrtc::RtcpMode rtcp_mode,
    absl::optional<int> rtx_time) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  RTC_DCHECK(role() == MediaChannel::Role::kReceive ||
             role() == MediaChannel::Role::kBoth);
  // Update receive feedback parameters from new codec or RTCP mode.
  for (auto& kv : receive_streams_) {
    RTC_DCHECK(kv.second != nullptr);
    kv.second->SetFeedbackParameters(lntf_enabled, nack_enabled, rtcp_mode,
                                     rtx_time);
  }
  // Store for future creation of receive streams
  rtp_config_.lntf.enabled = lntf_enabled;
  if (nack_enabled) {
    rtp_config_.nack.rtp_history_ms = kNackHistoryMs;
  } else {
    rtp_config_.nack.rtp_history_ms = 0;
  }
  rtp_config_.rtcp_mode = rtcp_mode;
  // Note: There is no place in config to store rtx_time.
}

webrtc::RtpParameters WebRtcVideoChannel::GetRtpSendParameters(
    uint32_t ssrc) const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Attempting to get RTP send parameters for stream "
                           "with ssrc "
                        << ssrc << " which doesn't exist.";
    return webrtc::RtpParameters();
  }

  webrtc::RtpParameters rtp_params = it->second->GetRtpParameters();
  // Need to add the common list of codecs to the send stream-specific
  // RTP parameters.
  for (const VideoCodec& codec : send_params_.codecs) {
    if (send_codec() && send_codec()->codec.id == codec.id) {
      // Put the current send codec to the front of the codecs list.
      RTC_DCHECK_EQ(codec.name, send_codec()->codec.name);
      rtp_params.codecs.insert(rtp_params.codecs.begin(),
                               codec.ToCodecParameters());
    } else {
      rtp_params.codecs.push_back(codec.ToCodecParameters());
    }
  }

  return rtp_params;
}

webrtc::RTCError WebRtcVideoChannel::SetRtpSendParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters,
    webrtc::SetParametersCallback callback) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetRtpSendParameters");
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Attempting to set RTP send parameters for stream "
                         "with ssrc "
                      << ssrc << " which doesn't exist.";
    return webrtc::InvokeSetParametersCallback(
        callback, webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR));
  }

  // TODO(deadbeef): Handle setting parameters with a list of codecs in a
  // different order (which should change the send codec).
  webrtc::RtpParameters current_parameters = GetRtpSendParameters(ssrc);
  if (current_parameters.codecs != parameters.codecs) {
    RTC_DLOG(LS_ERROR) << "Using SetParameters to change the set of codecs "
                          "is not currently supported.";
    return webrtc::InvokeSetParametersCallback(
        callback, webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR));
  }

  if (!parameters.encodings.empty()) {
    // Note that these values come from:
    // https://tools.ietf.org/html/draft-ietf-tsvwg-rtcweb-qos-16#section-5
    // TODO(deadbeef): Change values depending on whether we are sending a
    // keyframe or non-keyframe.
    rtc::DiffServCodePoint new_dscp = rtc::DSCP_DEFAULT;
    switch (parameters.encodings[0].network_priority) {
      case webrtc::Priority::kVeryLow:
        new_dscp = rtc::DSCP_CS1;
        break;
      case webrtc::Priority::kLow:
        new_dscp = rtc::DSCP_DEFAULT;
        break;
      case webrtc::Priority::kMedium:
        new_dscp = rtc::DSCP_AF42;
        break;
      case webrtc::Priority::kHigh:
        new_dscp = rtc::DSCP_AF41;
        break;
    }
    SetPreferredDscp(new_dscp);
  }

  return it->second->SetRtpParameters(parameters, std::move(callback));
}

webrtc::RtpParameters WebRtcVideoChannel::GetRtpReceiveParameters(
    uint32_t ssrc) const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  webrtc::RtpParameters rtp_params;
  auto it = receive_streams_.find(ssrc);
  if (it == receive_streams_.end()) {
    RTC_LOG(LS_WARNING)
        << "Attempting to get RTP receive parameters for stream "
           "with SSRC "
        << ssrc << " which doesn't exist.";
    return webrtc::RtpParameters();
  }
  rtp_params = it->second->GetRtpParameters();
  rtp_params.header_extensions = recv_rtp_extensions_;

  // Add codecs, which any stream is prepared to receive.
  for (const VideoCodec& codec : recv_params_.codecs) {
    rtp_params.codecs.push_back(codec.ToCodecParameters());
  }

  return rtp_params;
}

webrtc::RtpParameters WebRtcVideoChannel::GetDefaultRtpReceiveParameters()
    const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  webrtc::RtpParameters rtp_params;
  if (!default_sink_) {
    // Getting parameters on a default, unsignaled video receive stream but
    // because we've not configured to receive such a stream, `encodings` is
    // empty.
    return rtp_params;
  }
  rtp_params.encodings.emplace_back();

  // Add codecs, which any stream is prepared to receive.
  for (const VideoCodec& codec : recv_params_.codecs) {
    rtp_params.codecs.push_back(codec.ToCodecParameters());
  }

  return rtp_params;
}

bool WebRtcVideoChannel::GetChangedRecvParameters(
    const VideoRecvParameters& params,
    ChangedRecvParameters* changed_params) const {
  if (!ValidateCodecFormats(params.codecs) ||
      !ValidateRtpExtensions(params.extensions, recv_rtp_extensions_)) {
    return false;
  }

  // Handle receive codecs.
  const std::vector<VideoCodecSettings> mapped_codecs =
      MapCodecs(params.codecs);
  if (mapped_codecs.empty()) {
    RTC_LOG(LS_ERROR)
        << "GetChangedRecvParameters called without any video codecs.";
    return false;
  }

  // Verify that every mapped codec is supported locally.
  if (params.is_stream_active) {
    const std::vector<VideoCodec> local_supported_codecs =
        GetPayloadTypesAndDefaultCodecs(decoder_factory_,
                                        /*is_decoder_factory=*/true,
                                        /*include_rtx=*/true, call_->trials());
    for (const VideoCodecSettings& mapped_codec : mapped_codecs) {
      if (!FindMatchingCodec(local_supported_codecs, mapped_codec.codec)) {
        RTC_LOG(LS_ERROR)
            << "GetChangedRecvParameters called with unsupported video codec: "
            << mapped_codec.codec.ToString();
        return false;
      }
    }
  }

  if (NonFlexfecReceiveCodecsHaveChanged(recv_codecs_, mapped_codecs)) {
    changed_params->codec_settings =
        absl::optional<std::vector<VideoCodecSettings>>(mapped_codecs);
  }

  // Handle RTP header extensions.
  std::vector<webrtc::RtpExtension> filtered_extensions = FilterRtpExtensions(
      params.extensions, webrtc::RtpExtension::IsSupportedForVideo, false,
      call_->trials());
  if (filtered_extensions != recv_rtp_extensions_) {
    changed_params->rtp_header_extensions =
        absl::optional<std::vector<webrtc::RtpExtension>>(filtered_extensions);
  }

  int flexfec_payload_type = mapped_codecs.front().flexfec_payload_type;
  if (flexfec_payload_type != recv_flexfec_payload_type_) {
    changed_params->flexfec_payload_type = flexfec_payload_type;
  }

  return true;
}

bool WebRtcVideoChannel::SetRecvParameters(const VideoRecvParameters& params) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetRecvParameters");
  RTC_LOG(LS_INFO) << "SetRecvParameters: " << params.ToString();
  ChangedRecvParameters changed_params;
  if (!GetChangedRecvParameters(params, &changed_params)) {
    return false;
  }
  if (changed_params.flexfec_payload_type) {
    RTC_DLOG(LS_INFO) << "Changing FlexFEC payload type (recv) from "
                      << recv_flexfec_payload_type_ << " to "
                      << *changed_params.flexfec_payload_type;
    recv_flexfec_payload_type_ = *changed_params.flexfec_payload_type;
  }
  if (changed_params.rtp_header_extensions) {
    recv_rtp_extensions_ = *changed_params.rtp_header_extensions;
    recv_rtp_extension_map_ =
        webrtc::RtpHeaderExtensionMap(recv_rtp_extensions_);
  }
  if (changed_params.codec_settings) {
    RTC_DLOG(LS_INFO) << "Changing recv codecs from "
                      << CodecSettingsVectorToString(recv_codecs_) << " to "
                      << CodecSettingsVectorToString(
                             *changed_params.codec_settings);
    recv_codecs_ = *changed_params.codec_settings;
  }

  for (auto& kv : receive_streams_) {
    kv.second->SetRecvParameters(changed_params);
  }
  recv_params_ = params;
  return true;
}

std::string WebRtcVideoChannel::CodecSettingsVectorToString(
    const std::vector<VideoCodecSettings>& codecs) {
  rtc::StringBuilder out;
  out << "{";
  for (size_t i = 0; i < codecs.size(); ++i) {
    out << codecs[i].codec.ToString();
    if (i != codecs.size() - 1) {
      out << ", ";
    }
  }
  out << "}";
  return out.Release();
}

void WebRtcVideoChannel::ExtractCodecInformation(
    rtc::ArrayView<const VideoCodecSettings> recv_codecs,
    std::map<int, int>& rtx_associated_payload_types,
    std::set<int>& raw_payload_types,
    std::vector<webrtc::VideoReceiveStreamInterface::Decoder>& decoders) {
  RTC_DCHECK(!recv_codecs.empty());
  RTC_DCHECK(rtx_associated_payload_types.empty());
  RTC_DCHECK(raw_payload_types.empty());
  RTC_DCHECK(decoders.empty());

  for (const VideoCodecSettings& recv_codec : recv_codecs) {
    decoders.emplace_back(
        webrtc::SdpVideoFormat(recv_codec.codec.name, recv_codec.codec.params),
        recv_codec.codec.id);
    rtx_associated_payload_types.emplace(recv_codec.rtx_payload_type,
                                         recv_codec.codec.id);
    if (recv_codec.codec.packetization == kPacketizationParamRaw) {
      raw_payload_types.insert(recv_codec.codec.id);
    }
  }
}

void WebRtcVideoChannel::SetReceiverReportSsrc(uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (ssrc == rtcp_receiver_report_ssrc_)
    return;

  rtcp_receiver_report_ssrc_ = ssrc;
  for (auto& [unused, receive_stream] : receive_streams_)
    receive_stream->SetLocalSsrc(ssrc);
}

bool WebRtcVideoChannel::GetSendCodec(VideoCodec* codec) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!send_codec()) {
    RTC_LOG(LS_VERBOSE) << "GetSendCodec: No send codec set.";
    return false;
  }
  *codec = send_codec()->codec;
  return true;
}

void WebRtcVideoChannel::SetReceive(bool receive) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetReceive");
  RTC_LOG(LS_VERBOSE) << "SetReceive: " << (receive ? "true" : "false");
  for (const auto& kv : receive_streams_) {
    if (receive) {
      kv.second->StartReceiveStream();
    } else {
      kv.second->StopReceiveStream();
    }
  }
  receiving_ = receive;
}

bool WebRtcVideoChannel::SetSend(bool send) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::SetSend");
  RTC_LOG(LS_VERBOSE) << "SetSend: " << (send ? "true" : "false");
  if (send && !send_codec()) {
    RTC_DLOG(LS_ERROR) << "SetSend(true) called before setting codec.";
    return false;
  }
  for (const auto& kv : send_streams_) {
    kv.second->SetSend(send);
  }
  sending_ = send;
  return true;
}

bool WebRtcVideoChannel::SetVideoSend(
    uint32_t ssrc,
    const VideoOptions* options,
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  TRACE_EVENT0("webrtc", "SetVideoSend");
  RTC_DCHECK(ssrc != 0);
  RTC_LOG(LS_INFO) << "SetVideoSend (ssrc= " << ssrc << ", options: "
                   << (options ? options->ToString() : "nullptr")
                   << ", source = " << (source ? "(source)" : "nullptr") << ")";

  const auto& kv = send_streams_.find(ssrc);
  if (kv == send_streams_.end()) {
    // Allow unknown ssrc only if source is null.
    RTC_CHECK(source == nullptr);
    RTC_LOG(LS_ERROR) << "No sending stream on ssrc " << ssrc;
    return false;
  }

  return kv->second->SetVideoSend(options, source);
}

bool WebRtcVideoChannel::ValidateSendSsrcAvailability(
    const StreamParams& sp) const {
  for (uint32_t ssrc : sp.ssrcs) {
    if (send_ssrcs_.find(ssrc) != send_ssrcs_.end()) {
      RTC_LOG(LS_ERROR) << "Send stream with SSRC '" << ssrc
                        << "' already exists.";
      return false;
    }
  }
  return true;
}

bool WebRtcVideoChannel::ValidateReceiveSsrcAvailability(
    const StreamParams& sp) const {
  for (uint32_t ssrc : sp.ssrcs) {
    if (receive_ssrcs_.find(ssrc) != receive_ssrcs_.end()) {
      RTC_LOG(LS_ERROR) << "Receive stream with SSRC '" << ssrc
                        << "' already exists.";
      return false;
    }
  }
  return true;
}

bool WebRtcVideoChannel::AddSendStream(const StreamParams& sp) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(role() == MediaChannel::Role::kSend ||
             role() == MediaChannel::Role::kBoth);

  RTC_LOG(LS_INFO) << "AddSendStream: " << sp.ToString();
  if (!ValidateStreamParams(sp))
    return false;

  if (!ValidateSendSsrcAvailability(sp))
    return false;

  for (uint32_t used_ssrc : sp.ssrcs)
    send_ssrcs_.insert(used_ssrc);

  webrtc::VideoSendStream::Config config(this);

  for (const RidDescription& rid : sp.rids()) {
    config.rtp.rids.push_back(rid.rid);
  }

  config.suspend_below_min_bitrate = video_config_.suspend_below_min_bitrate;
  config.periodic_alr_bandwidth_probing =
      video_config_.periodic_alr_bandwidth_probing;
  config.encoder_settings.experiment_cpu_load_estimator =
      video_config_.experiment_cpu_load_estimator;
  config.encoder_settings.encoder_factory = encoder_factory_;
  config.encoder_settings.bitrate_allocator_factory =
      bitrate_allocator_factory_;
  config.encoder_settings.encoder_switch_request_callback = this;
  config.crypto_options = crypto_options_;
  config.rtp.extmap_allow_mixed = ExtmapAllowMixed();
  config.rtcp_report_interval_ms = video_config_.rtcp_report_interval_ms;
  config.rtp.enable_send_packet_batching =
      video_config_.enable_send_packet_batching;

  WebRtcVideoSendStream* stream = new WebRtcVideoSendStream(
      call_, sp, std::move(config), default_send_options_,
      video_config_.enable_cpu_adaptation, bitrate_config_.max_bitrate_bps,
      send_codec(), send_rtp_extensions_, send_params_);

  uint32_t ssrc = sp.first_ssrc();
  RTC_DCHECK(ssrc != 0);
  send_streams_[ssrc] = stream;

  // If legacy kBoth mode, tell my receiver part about its SSRC.
  // In kSend mode, this is the responsibility of the caller.
  if (role() == MediaChannel::Role::kBoth) {
    if (rtcp_receiver_report_ssrc_ == kDefaultRtcpReceiverReportSsrc) {
      SetReceiverReportSsrc(ssrc);
    }
  }

  if (sending_) {
    stream->SetSend(true);
  }

  return true;
}

bool WebRtcVideoChannel::RemoveSendStream(uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_LOG(LS_INFO) << "RemoveSendStream: " << ssrc;

  WebRtcVideoSendStream* removed_stream;
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    return false;
  }

  for (uint32_t old_ssrc : it->second->GetSsrcs())
    send_ssrcs_.erase(old_ssrc);

  removed_stream = it->second;
  send_streams_.erase(it);

  // Switch receiver report SSRCs, the one in use is no longer valid.
  if (rtcp_receiver_report_ssrc_ == ssrc) {
    SetReceiverReportSsrc(send_streams_.empty() ? kDefaultRtcpReceiverReportSsrc
                                                : send_streams_.begin()->first);
  }

  delete removed_stream;

  return true;
}

void WebRtcVideoChannel::DeleteReceiveStream(
    WebRtcVideoChannel::WebRtcVideoReceiveStream* stream) {
  for (uint32_t old_ssrc : stream->GetSsrcs())
    receive_ssrcs_.erase(old_ssrc);
  delete stream;
}

bool WebRtcVideoChannel::AddRecvStream(const StreamParams& sp) {
  return AddRecvStream(sp, false);
}

bool WebRtcVideoChannel::AddRecvStream(const StreamParams& sp,
                                       bool default_stream) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  RTC_LOG(LS_INFO) << "AddRecvStream"
                   << (default_stream ? " (default stream)" : "") << ": "
                   << sp.ToString();
  if (!sp.has_ssrcs()) {
    // This is a StreamParam with unsignaled SSRCs. Store it, so it can be used
    // later when we know the SSRC on the first packet arrival.
    unsignaled_stream_params_ = sp;
    return true;
  }

  if (!ValidateStreamParams(sp))
    return false;

  for (uint32_t ssrc : sp.ssrcs) {
    // Remove running stream if this was a default stream.
    const auto& prev_stream = receive_streams_.find(ssrc);
    if (prev_stream != receive_streams_.end()) {
      if (default_stream || !prev_stream->second->IsDefaultStream()) {
        RTC_LOG(LS_ERROR) << "Receive stream for SSRC '" << ssrc
                          << "' already exists.";
        return false;
      }
      DeleteReceiveStream(prev_stream->second);
      receive_streams_.erase(prev_stream);
    }
  }

  if (!ValidateReceiveSsrcAvailability(sp))
    return false;

  for (uint32_t used_ssrc : sp.ssrcs)
    receive_ssrcs_.insert(used_ssrc);

  webrtc::VideoReceiveStreamInterface::Config config(this, decoder_factory_);
  webrtc::FlexfecReceiveStream::Config flexfec_config(this);
  ConfigureReceiverRtp(&config, &flexfec_config, sp);

  config.crypto_options = crypto_options_;
  config.enable_prerenderer_smoothing =
      video_config_.enable_prerenderer_smoothing;
  if (!sp.stream_ids().empty()) {
    config.sync_group = sp.stream_ids()[0];
  }

  if (unsignaled_frame_transformer_ && !config.frame_transformer)
    config.frame_transformer = unsignaled_frame_transformer_;

  auto receive_stream =
      new WebRtcVideoReceiveStream(call_, sp, std::move(config), default_stream,
                                   recv_codecs_, flexfec_config);
  if (receiving_) {
    receive_stream->StartReceiveStream();
  }
  receive_streams_[sp.first_ssrc()] = receive_stream;
  return true;
}

void WebRtcVideoChannel::ConfigureReceiverRtp(
    webrtc::VideoReceiveStreamInterface::Config* config,
    webrtc::FlexfecReceiveStream::Config* flexfec_config,
    const StreamParams& sp) const {
  uint32_t ssrc = sp.first_ssrc();

  config->rtp.remote_ssrc = ssrc;
  config->rtp.local_ssrc = rtcp_receiver_report_ssrc_;

  // TODO(pbos): This protection is against setting the same local ssrc as
  // remote which is not permitted by the lower-level API. RTCP requires a
  // corresponding sender SSRC. Figure out what to do when we don't have
  // (receive-only) or know a good local SSRC.
  if (config->rtp.remote_ssrc == config->rtp.local_ssrc) {
    if (config->rtp.local_ssrc != kDefaultRtcpReceiverReportSsrc) {
      config->rtp.local_ssrc = kDefaultRtcpReceiverReportSsrc;
    } else {
      config->rtp.local_ssrc = kDefaultRtcpReceiverReportSsrc + 1;
    }
  }

  if (role() == MediaChannel::Role::kBoth) {
    // Whether or not the receive stream sends reduced size RTCP is determined
    // by the send params.
    // TODO(deadbeef): Once we change "send_params" to "sender_params" and
    // "recv_params" to "receiver_params", we should get this out of
    // receiver_params_.
    config->rtp.rtcp_mode = send_params_.rtcp.reduced_size
                                ? webrtc::RtcpMode::kReducedSize
                                : webrtc::RtcpMode::kCompound;
    // rtx-time (RFC 4588) is a declarative attribute similar to rtcp-rsize and
    // determined by the sender / send codec.
    if (send_codec() && send_codec()->rtx_time) {
      config->rtp.nack.rtp_history_ms = *send_codec()->rtx_time;
    }
  } else {
    // The mode and rtx time is determined by a call to the configuration
    // function.
    config->rtp.rtcp_mode = rtp_config_.rtcp_mode;
  }

  sp.GetFidSsrc(ssrc, &config->rtp.rtx_ssrc);

  // TODO(brandtr): Generalize when we add support for multistream protection.
  flexfec_config->payload_type = recv_flexfec_payload_type_;
  if (!IsDisabled(call_->trials(), "WebRTC-FlexFEC-03-Advertised") &&
      sp.GetFecFrSsrc(ssrc, &flexfec_config->rtp.remote_ssrc)) {
    flexfec_config->protected_media_ssrcs = {ssrc};
    flexfec_config->rtp.local_ssrc = config->rtp.local_ssrc;
    flexfec_config->rtcp_mode = config->rtp.rtcp_mode;
  }
}

bool WebRtcVideoChannel::RemoveRecvStream(uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_LOG(LS_INFO) << "RemoveRecvStream: " << ssrc;

  auto stream = receive_streams_.find(ssrc);
  if (stream == receive_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Stream not found for ssrc: " << ssrc;
    return false;
  }
  DeleteReceiveStream(stream->second);
  receive_streams_.erase(stream);

  return true;
}

void WebRtcVideoChannel::ResetUnsignaledRecvStream() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_LOG(LS_INFO) << "ResetUnsignaledRecvStream.";
  unsignaled_stream_params_ = StreamParams();
  last_unsignalled_ssrc_creation_time_ms_ = absl::nullopt;

  // Delete any created default streams. This is needed to avoid SSRC collisions
  // in Call's RtpDemuxer, in the case that `this` has created a default video
  // receiver, and then some other WebRtcVideoChannel gets the SSRC signaled
  // in the corresponding Unified Plan "m=" section.
  auto it = receive_streams_.begin();
  while (it != receive_streams_.end()) {
    if (it->second->IsDefaultStream()) {
      DeleteReceiveStream(it->second);
      receive_streams_.erase(it++);
    } else {
      ++it;
    }
  }
}

bool WebRtcVideoChannel::SetLocalSsrc(const StreamParams& sp) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(role() == MediaChannel::Role::kReceive);
  if (rtcp_receiver_report_ssrc_ == kDefaultRtcpReceiverReportSsrc) {
    SetReceiverReportSsrc(sp.first_ssrc());
  }
  return true;
}

absl::optional<uint32_t> WebRtcVideoChannel::GetUnsignaledSsrc() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  absl::optional<uint32_t> ssrc;
  for (auto it = receive_streams_.begin(); it != receive_streams_.end(); ++it) {
    if (it->second->IsDefaultStream()) {
      ssrc.emplace(it->first);
      break;
    }
  }
  return ssrc;
}

void WebRtcVideoChannel::OnDemuxerCriteriaUpdatePending() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  ++demuxer_criteria_id_;
}

void WebRtcVideoChannel::OnDemuxerCriteriaUpdateComplete() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  ++demuxer_criteria_completed_id_;
}

bool WebRtcVideoChannel::SetSink(
    uint32_t ssrc,
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_LOG(LS_INFO) << "SetSink: ssrc:" << ssrc << " "
                   << (sink ? "(ptr)" : "nullptr");

  auto it = receive_streams_.find(ssrc);
  if (it == receive_streams_.end()) {
    return false;
  }

  it->second->SetSink(sink);
  return true;
}

void WebRtcVideoChannel::SetDefaultSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_LOG(LS_INFO) << "SetDefaultSink: " << (sink ? "(ptr)" : "nullptr");
  default_sink_ = sink;
}

bool WebRtcVideoChannel::GetSendStats(VideoMediaSendInfo* info) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::GetSendStats");

  info->Clear();
  if (send_streams_.empty()) {
    return true;
  }

  // Log stats periodically.
  bool log_stats = false;
  int64_t now_ms = rtc::TimeMillis();
  if (last_send_stats_log_ms_ == -1 ||
      now_ms - last_send_stats_log_ms_ > kStatsLogIntervalMs) {
    last_send_stats_log_ms_ = now_ms;
    log_stats = true;
  }

  info->Clear();
  FillSenderStats(info, log_stats);
  FillSendCodecStats(info);
  // TODO(holmer): We should either have rtt available as a metric on
  // VideoSend/ReceiveStreams, or we should remove rtt from VideoSenderInfo.
  webrtc::Call::Stats stats = call_->GetStats();
  if (stats.rtt_ms != -1) {
    for (size_t i = 0; i < info->senders.size(); ++i) {
      info->senders[i].rtt_ms = stats.rtt_ms;
    }
    for (size_t i = 0; i < info->aggregated_senders.size(); ++i) {
      info->aggregated_senders[i].rtt_ms = stats.rtt_ms;
    }
  }

  if (log_stats)
    RTC_LOG(LS_INFO) << stats.ToString(now_ms);

  return true;
}
bool WebRtcVideoChannel::GetReceiveStats(VideoMediaReceiveInfo* info) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel::GetReceiveStats");

  info->Clear();
  if (receive_streams_.empty()) {
    return true;
  }

  // Log stats periodically.
  bool log_stats = false;
  int64_t now_ms = rtc::TimeMillis();
  if (last_receive_stats_log_ms_ == -1 ||
      now_ms - last_receive_stats_log_ms_ > kStatsLogIntervalMs) {
    last_receive_stats_log_ms_ = now_ms;
    log_stats = true;
  }

  FillReceiverStats(info, log_stats);
  FillReceiveCodecStats(info);

  return true;
}

void WebRtcVideoChannel::FillSenderStats(VideoMediaSendInfo* video_media_info,
                                         bool log_stats) {
  for (const auto& it : send_streams_) {
    auto infos = it.second->GetPerLayerVideoSenderInfos(log_stats);
    if (infos.empty())
      continue;
    video_media_info->aggregated_senders.push_back(
        it.second->GetAggregatedVideoSenderInfo(infos));
    for (auto&& info : infos) {
      video_media_info->senders.push_back(info);
    }
  }
}

void WebRtcVideoChannel::FillReceiverStats(
    VideoMediaReceiveInfo* video_media_info,
    bool log_stats) {
  for (const auto& it : receive_streams_) {
    video_media_info->receivers.push_back(
        it.second->GetVideoReceiverInfo(log_stats));
  }
}

void WebRtcVideoChannel::FillBitrateInfo(BandwidthEstimationInfo* bwe_info) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  for (const auto& it : send_streams_) {
    it.second->FillBitrateInfo(bwe_info);
  }
}

void WebRtcVideoChannel::FillSendCodecStats(
    VideoMediaSendInfo* video_media_info) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!send_codec()) {
    return;
  }
  // Note: since RTP stats don't account for RTX and FEC separately (see
  // https://w3c.github.io/webrtc-stats/#dom-rtcstatstype-outbound-rtp)
  // we can omit the codec information for those here and only insert the
  // primary codec that is being used to send here.
  video_media_info->send_codecs.insert(std::make_pair(
      send_codec()->codec.id, send_codec()->codec.ToCodecParameters()));
}

void WebRtcVideoChannel::FillReceiveCodecStats(
    VideoMediaReceiveInfo* video_media_info) {
  for (const auto& receiver : video_media_info->receivers) {
    auto codec =
        absl::c_find_if(recv_params_.codecs, [&receiver](const VideoCodec& c) {
          return receiver.codec_payload_type &&
                 *receiver.codec_payload_type == c.id;
        });
    if (codec != recv_params_.codecs.end()) {
      video_media_info->receive_codecs.insert(
          std::make_pair(codec->id, codec->ToCodecParameters()));
    }
  }
}

void WebRtcVideoChannel::OnPacketReceived(
    const webrtc::RtpPacketReceived& packet) {
  // Note: the network_thread_checker may refer to the worker thread if the two
  // threads are combined, but this is either always true or always false
  // depending on configuration set at object initialization.
  RTC_DCHECK_RUN_ON(&network_thread_checker_);

  // TODO(crbug.com/1373439): Stop posting to the worker thread when the
  // combined network/worker project launches.
  if (webrtc::TaskQueueBase::Current() != worker_thread_) {
    worker_thread_->PostTask(
        SafeTask(task_safety_.flag(), [this, packet = packet]() mutable {
          RTC_DCHECK_RUN_ON(&thread_checker_);
          ProcessReceivedPacket(std::move(packet));
        }));
  } else {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    ProcessReceivedPacket(packet);
  }
}

bool WebRtcVideoChannel::MaybeCreateDefaultReceiveStream(
    const webrtc::RtpPacketReceived& packet) {
  if (discard_unknown_ssrc_packets_) {
    return false;
  }

  if (packet.PayloadType() == recv_flexfec_payload_type_) {
    return false;
  }

  // Ignore unknown ssrcs if there is a demuxer criteria update pending.
  // During a demuxer update we may receive ssrcs that were recently
  // removed or we may receve ssrcs that were recently configured for a
  // different video channel.
  if (demuxer_criteria_id_ != demuxer_criteria_completed_id_) {
    return false;
  }

  // See if this payload_type is registered as one that usually gets its
  // own SSRC (RTX) or at least is safe to drop either way (FEC). If it
  // is, and it wasn't handled above by DeliverPacket, that means we don't
  // know what stream it associates with, and we shouldn't ever create an
  // implicit channel for these.
  bool is_rtx_payload = false;
  for (auto& codec : recv_codecs_) {
    if (packet.PayloadType() == codec.ulpfec.red_rtx_payload_type ||
        packet.PayloadType() == codec.ulpfec.ulpfec_payload_type) {
      return false;
    }

    if (packet.PayloadType() == codec.rtx_payload_type) {
      is_rtx_payload = true;
      break;
    }
  }

  if (is_rtx_payload) {
    // As we don't support receiving simulcast there can only be one RTX
    // stream, which will be associated with unsignaled media stream.
    absl::optional<uint32_t> current_default_ssrc = GetUnsignaledSsrc();
    if (current_default_ssrc) {
      FindReceiveStream(*current_default_ssrc)->UpdateRtxSsrc(packet.Ssrc());
    } else {
      // Received unsignaled RTX packet before a media packet. Create a default
      // stream with a "random" SSRC and the RTX SSRC from the packet.  The
      // stream will be recreated on the first media packet, unless we are
      // extremely lucky and used the right media SSRC.
      ReCreateDefaultReceiveStream(/*ssrc =*/14795, /*rtx_ssrc=*/packet.Ssrc());
    }
    return true;
  } else {
    // Ignore unknown ssrcs if we recently created an unsignalled receive
    // stream since this shouldn't happen frequently. Getting into a state
    // of creating decoders on every packet eats up processing time (e.g.
    // https://crbug.com/1069603) and this cooldown prevents that.
    if (last_unsignalled_ssrc_creation_time_ms_.has_value()) {
      int64_t now_ms = rtc::TimeMillis();
      if (now_ms - last_unsignalled_ssrc_creation_time_ms_.value() <
          kUnsignaledSsrcCooldownMs) {
        // We've already created an unsignalled ssrc stream within the last
        // 0.5 s, ignore with a warning.
        RTC_LOG(LS_WARNING)
            << "Another unsignalled ssrc packet arrived shortly after the "
            << "creation of an unsignalled ssrc stream. Dropping packet.";
        return false;
      }
    }
  }
  // RTX SSRC not yet known.
  ReCreateDefaultReceiveStream(packet.Ssrc(), absl::nullopt);
  last_unsignalled_ssrc_creation_time_ms_ = rtc::TimeMillis();
  return true;
}

void WebRtcVideoChannel::ReCreateDefaultReceiveStream(
    uint32_t ssrc,
    absl::optional<uint32_t> rtx_ssrc) {
  RTC_DCHECK_RUN_ON(&thread_checker_);

  absl::optional<uint32_t> default_recv_ssrc = GetUnsignaledSsrc();
  if (default_recv_ssrc) {
    RTC_LOG(LS_INFO) << "Destroying old default receive stream for SSRC="
                     << ssrc << ".";
    RemoveRecvStream(*default_recv_ssrc);
  }

  StreamParams sp = unsignaled_stream_params();
  sp.ssrcs.push_back(ssrc);
  if (rtx_ssrc) {
    sp.AddFidSsrc(ssrc, *rtx_ssrc);
  }
  RTC_LOG(LS_INFO) << "Creating default receive stream for SSRC=" << ssrc
                   << ".";
  if (!AddRecvStream(sp, /*default_stream=*/true)) {
    RTC_LOG(LS_WARNING) << "Could not create default receive stream.";
  }

  // SSRC 0 returns default_recv_base_minimum_delay_ms.
  const int unsignaled_ssrc = 0;
  int default_recv_base_minimum_delay_ms =
      GetBaseMinimumPlayoutDelayMs(unsignaled_ssrc).value_or(0);
  // Set base minimum delay if it was set before for the default receive
  // stream.
  SetBaseMinimumPlayoutDelayMs(ssrc, default_recv_base_minimum_delay_ms);
  SetSink(ssrc, default_sink_);
}

void WebRtcVideoChannel::OnPacketSent(const rtc::SentPacket& sent_packet) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  // TODO(tommi): We shouldn't need to go through call_ to deliver this
  // notification. We should already have direct access to
  // video_send_delay_stats_ and transport_send_ptr_ via `stream_`.
  // So we should be able to remove OnSentPacket from Call and handle this per
  // channel instead. At the moment Call::OnSentPacket calls OnSentPacket for
  // the video stats, for all sent packets, including audio, which causes
  // unnecessary lookups.
  call_->OnSentPacket(sent_packet);
}

void WebRtcVideoChannel::OnReadyToSend(bool ready) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  RTC_LOG(LS_VERBOSE) << "OnReadyToSend: " << (ready ? "Ready." : "Not ready.");
  call_->SignalChannelNetworkState(
      webrtc::MediaType::VIDEO,
      ready ? webrtc::kNetworkUp : webrtc::kNetworkDown);
}

void WebRtcVideoChannel::OnNetworkRouteChanged(
    absl::string_view transport_name,
    const rtc::NetworkRoute& network_route) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  worker_thread_->PostTask(SafeTask(
      task_safety_.flag(),
      [this, name = std::string(transport_name), route = network_route] {
        RTC_DCHECK_RUN_ON(&thread_checker_);
        webrtc::RtpTransportControllerSendInterface* transport =
            call_->GetTransportControllerSend();
        transport->OnNetworkRouteChanged(name, route);
        transport->OnTransportOverheadChanged(route.packet_overhead);
      }));
}

void WebRtcVideoChannel::SetInterface(MediaChannelNetworkInterface* iface) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  MediaChannel::SetInterface(iface);
  // Set the RTP recv/send buffer to a bigger size.
  MediaChannel::SetOption(MediaChannelNetworkInterface::ST_RTP,
                          rtc::Socket::OPT_RCVBUF, kVideoRtpRecvBufferSize);

  // Speculative change to increase the outbound socket buffer size.
  // In b/15152257, we are seeing a significant number of packets discarded
  // due to lack of socket buffer space, although it's not yet clear what the
  // ideal value should be.
  const std::string group_name_send_buf_size =
      call_->trials().Lookup("WebRTC-SendBufferSizeBytes");
  int send_buffer_size = kVideoRtpSendBufferSize;
  if (!group_name_send_buf_size.empty() &&
      (sscanf(group_name_send_buf_size.c_str(), "%d", &send_buffer_size) != 1 ||
       send_buffer_size <= 0)) {
    RTC_LOG(LS_WARNING) << "Invalid send buffer size: "
                        << group_name_send_buf_size;
    send_buffer_size = kVideoRtpSendBufferSize;
  }

  MediaChannel::SetOption(MediaChannelNetworkInterface::ST_RTP,
                          rtc::Socket::OPT_SNDBUF, send_buffer_size);
}

void WebRtcVideoChannel::SetFrameDecryptor(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  auto matching_stream = receive_streams_.find(ssrc);
  if (matching_stream != receive_streams_.end()) {
    matching_stream->second->SetFrameDecryptor(frame_decryptor);
  }
}

void WebRtcVideoChannel::SetFrameEncryptor(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  auto matching_stream = send_streams_.find(ssrc);
  if (matching_stream != send_streams_.end()) {
    matching_stream->second->SetFrameEncryptor(frame_encryptor);
  } else {
    RTC_LOG(LS_ERROR) << "No stream found to attach frame encryptor";
  }
}

void WebRtcVideoChannel::SetEncoderSelector(
    uint32_t ssrc,
    webrtc::VideoEncoderFactory::EncoderSelectorInterface* encoder_selector) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  auto matching_stream = send_streams_.find(ssrc);
  if (matching_stream != send_streams_.end()) {
    matching_stream->second->SetEncoderSelector(encoder_selector);
  } else {
    RTC_LOG(LS_ERROR) << "No stream found to attach encoder selector";
  }
}

void WebRtcVideoChannel::SetVideoCodecSwitchingEnabled(bool enabled) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  allow_codec_switching_ = enabled;
  if (allow_codec_switching_) {
    RTC_LOG(LS_INFO) << "Encoder switching enabled.";
  }
}

bool WebRtcVideoChannel::SetBaseMinimumPlayoutDelayMs(uint32_t ssrc,
                                                      int delay_ms) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  absl::optional<uint32_t> default_ssrc = GetUnsignaledSsrc();

  // SSRC of 0 represents the default receive stream.
  if (ssrc == 0) {
    default_recv_base_minimum_delay_ms_ = delay_ms;
  }

  if (ssrc == 0 && !default_ssrc) {
    return true;
  }

  if (ssrc == 0 && default_ssrc) {
    ssrc = default_ssrc.value();
  }

  auto stream = receive_streams_.find(ssrc);
  if (stream != receive_streams_.end()) {
    stream->second->SetBaseMinimumPlayoutDelayMs(delay_ms);
    return true;
  } else {
    RTC_LOG(LS_ERROR) << "No stream found to set base minimum playout delay";
    return false;
  }
}

absl::optional<int> WebRtcVideoChannel::GetBaseMinimumPlayoutDelayMs(
    uint32_t ssrc) const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  // SSRC of 0 represents the default receive stream.
  if (ssrc == 0) {
    return default_recv_base_minimum_delay_ms_;
  }

  auto stream = receive_streams_.find(ssrc);
  if (stream != receive_streams_.end()) {
    return stream->second->GetBaseMinimumPlayoutDelayMs();
  } else {
    RTC_LOG(LS_ERROR) << "No stream found to get base minimum playout delay";
    return absl::nullopt;
  }
}

std::vector<webrtc::RtpSource> WebRtcVideoChannel::GetSources(
    uint32_t ssrc) const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  auto it = receive_streams_.find(ssrc);
  if (it == receive_streams_.end()) {
    // TODO(bugs.webrtc.org/9781): Investigate standard compliance
    // with sources for streams that has been removed.
    RTC_LOG(LS_ERROR) << "Attempting to get contributing sources for SSRC:"
                      << ssrc << " which doesn't exist.";
    return {};
  }
  return it->second->GetSources();
}

bool WebRtcVideoChannel::SendRtp(const uint8_t* data,
                                 size_t len,
                                 const webrtc::PacketOptions& options) {
  MediaChannel::SendRtp(data, len, options);
  return true;
}

bool WebRtcVideoChannel::SendRtcp(const uint8_t* data, size_t len) {
  MediaChannel::SendRtcp(data, len);
  return true;
}

WebRtcVideoChannel::WebRtcVideoSendStream::VideoSendStreamParameters::
    VideoSendStreamParameters(
        webrtc::VideoSendStream::Config config,
        const VideoOptions& options,
        int max_bitrate_bps,
        const absl::optional<VideoCodecSettings>& codec_settings)
    : config(std::move(config)),
      options(options),
      max_bitrate_bps(max_bitrate_bps),
      conference_mode(false),
      codec_settings(codec_settings) {}

WebRtcVideoChannel::WebRtcVideoSendStream::WebRtcVideoSendStream(
    webrtc::Call* call,
    const StreamParams& sp,
    webrtc::VideoSendStream::Config config,
    const VideoOptions& options,
    bool enable_cpu_overuse_detection,
    int max_bitrate_bps,
    const absl::optional<VideoCodecSettings>& codec_settings,
    const absl::optional<std::vector<webrtc::RtpExtension>>& rtp_extensions,
    // TODO(deadbeef): Don't duplicate information between send_params,
    // rtp_extensions, options, etc.
    const VideoSendParameters& send_params)
    : worker_thread_(call->worker_thread()),
      ssrcs_(sp.ssrcs),
      ssrc_groups_(sp.ssrc_groups),
      call_(call),
      enable_cpu_overuse_detection_(enable_cpu_overuse_detection),
      source_(nullptr),
      stream_(nullptr),
      parameters_(std::move(config), options, max_bitrate_bps, codec_settings),
      rtp_parameters_(CreateRtpParametersWithEncodings(sp)),
      sending_(false),
      disable_automatic_resize_(
          IsEnabled(call->trials(), "WebRTC-Video-DisableAutomaticResize")) {
  // Maximum packet size may come in RtpConfig from external transport, for
  // example from QuicTransportInterface implementation, so do not exceed
  // given max_packet_size.
  parameters_.config.rtp.max_packet_size =
      std::min<size_t>(parameters_.config.rtp.max_packet_size, kVideoMtu);
  parameters_.conference_mode = send_params.conference_mode;

  sp.GetPrimarySsrcs(&parameters_.config.rtp.ssrcs);

  // ValidateStreamParams should prevent this from happening.
  RTC_CHECK(!parameters_.config.rtp.ssrcs.empty());
  rtp_parameters_.encodings[0].ssrc = parameters_.config.rtp.ssrcs[0];

  // RTX.
  sp.GetFidSsrcs(parameters_.config.rtp.ssrcs,
                 &parameters_.config.rtp.rtx.ssrcs);

  // FlexFEC SSRCs.
  // TODO(brandtr): This code needs to be generalized when we add support for
  // multistream protection.
  if (IsEnabled(call_->trials(), "WebRTC-FlexFEC-03")) {
    uint32_t flexfec_ssrc;
    bool flexfec_enabled = false;
    for (uint32_t primary_ssrc : parameters_.config.rtp.ssrcs) {
      if (sp.GetFecFrSsrc(primary_ssrc, &flexfec_ssrc)) {
        if (flexfec_enabled) {
          RTC_LOG(LS_INFO)
              << "Multiple FlexFEC streams in local SDP, but "
                 "our implementation only supports a single FlexFEC "
                 "stream. Will not enable FlexFEC for proposed "
                 "stream with SSRC: "
              << flexfec_ssrc << ".";
          continue;
        }

        flexfec_enabled = true;
        parameters_.config.rtp.flexfec.ssrc = flexfec_ssrc;
        parameters_.config.rtp.flexfec.protected_media_ssrcs = {primary_ssrc};
      }
    }
  }

  parameters_.config.rtp.c_name = sp.cname;
  if (rtp_extensions) {
    parameters_.config.rtp.extensions = *rtp_extensions;
    rtp_parameters_.header_extensions = *rtp_extensions;
  }
  parameters_.config.rtp.rtcp_mode = send_params.rtcp.reduced_size
                                         ? webrtc::RtcpMode::kReducedSize
                                         : webrtc::RtcpMode::kCompound;
  parameters_.config.rtp.mid = send_params.mid;
  rtp_parameters_.rtcp.reduced_size = send_params.rtcp.reduced_size;

  if (codec_settings) {
    SetCodec(*codec_settings);
  }
}

WebRtcVideoChannel::WebRtcVideoSendStream::~WebRtcVideoSendStream() {
  if (stream_ != NULL) {
    call_->DestroyVideoSendStream(stream_);
  }
}

bool WebRtcVideoChannel::WebRtcVideoSendStream::SetVideoSend(
    const VideoOptions* options,
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source) {
  TRACE_EVENT0("webrtc", "WebRtcVideoSendStream::SetVideoSend");
  RTC_DCHECK_RUN_ON(&thread_checker_);

  if (options) {
    VideoOptions old_options = parameters_.options;
    parameters_.options.SetAll(*options);
    if (parameters_.options.is_screencast.value_or(false) !=
            old_options.is_screencast.value_or(false) &&
        parameters_.codec_settings) {
      // If screen content settings change, we may need to recreate the codec
      // instance so that the correct type is used.

      SetCodec(*parameters_.codec_settings);
      // Mark screenshare parameter as being updated, then test for any other
      // changes that may require codec reconfiguration.
      old_options.is_screencast = options->is_screencast;
    }
    if (parameters_.options != old_options) {
      ReconfigureEncoder(nullptr);
    }
  }

  if (source_ && stream_) {
    stream_->SetSource(nullptr, webrtc::DegradationPreference::DISABLED);
  }
  // Switch to the new source.
  source_ = source;
  if (source && stream_) {
    stream_->SetSource(source_, GetDegradationPreference());
  }
  return true;
}

webrtc::DegradationPreference
WebRtcVideoChannel::WebRtcVideoSendStream::GetDegradationPreference() const {
  // Do not adapt resolution for screen content as this will likely
  // result in blurry and unreadable text.
  // `this` acts like a VideoSource to make sure SinkWants are handled on the
  // correct thread.
  if (!enable_cpu_overuse_detection_) {
    return webrtc::DegradationPreference::DISABLED;
  }

  webrtc::DegradationPreference degradation_preference;
  if (rtp_parameters_.degradation_preference.has_value()) {
    degradation_preference = *rtp_parameters_.degradation_preference;
  } else {
    if (parameters_.options.content_hint ==
        webrtc::VideoTrackInterface::ContentHint::kFluid) {
      degradation_preference =
          webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
    } else if (parameters_.options.is_screencast.value_or(false) ||
               parameters_.options.content_hint ==
                   webrtc::VideoTrackInterface::ContentHint::kDetailed ||
               parameters_.options.content_hint ==
                   webrtc::VideoTrackInterface::ContentHint::kText) {
      degradation_preference =
          webrtc::DegradationPreference::MAINTAIN_RESOLUTION;
    } else if (IsEnabled(call_->trials(), "WebRTC-Video-BalancedDegradation")) {
      // Standard wants balanced by default, but it needs to be tuned first.
      degradation_preference = webrtc::DegradationPreference::BALANCED;
    } else {
      // Keep MAINTAIN_FRAMERATE by default until BALANCED has been tuned for
      // all codecs and launched.
      degradation_preference =
          webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
    }
  }

  return degradation_preference;
}

const std::vector<uint32_t>&
WebRtcVideoChannel::WebRtcVideoSendStream::GetSsrcs() const {
  return ssrcs_;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::SetCodec(
    const VideoCodecSettings& codec_settings) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  FallbackToDefaultScalabilityModeIfNotSupported(
      codec_settings.codec, parameters_.config, rtp_parameters_.encodings);

  parameters_.encoder_config = CreateVideoEncoderConfig(codec_settings.codec);
  RTC_DCHECK_GT(parameters_.encoder_config.number_of_streams, 0);

  parameters_.config.rtp.payload_name = codec_settings.codec.name;
  parameters_.config.rtp.payload_type = codec_settings.codec.id;
  parameters_.config.rtp.raw_payload =
      codec_settings.codec.packetization == kPacketizationParamRaw;
  parameters_.config.rtp.ulpfec = codec_settings.ulpfec;
  parameters_.config.rtp.flexfec.payload_type =
      codec_settings.flexfec_payload_type;

  // Set RTX payload type if RTX is enabled.
  if (!parameters_.config.rtp.rtx.ssrcs.empty()) {
    if (codec_settings.rtx_payload_type == -1) {
      RTC_LOG(LS_WARNING)
          << "RTX SSRCs configured but there's no configured RTX "
             "payload type. Ignoring.";
      parameters_.config.rtp.rtx.ssrcs.clear();
    } else {
      parameters_.config.rtp.rtx.payload_type = codec_settings.rtx_payload_type;
    }
  }

  const bool has_lntf = HasLntf(codec_settings.codec);
  parameters_.config.rtp.lntf.enabled = has_lntf;
  parameters_.config.encoder_settings.capabilities.loss_notification = has_lntf;

  parameters_.config.rtp.nack.rtp_history_ms =
      HasNack(codec_settings.codec) ? kNackHistoryMs : 0;

  parameters_.codec_settings = codec_settings;

  // TODO(bugs.webrtc.org/8830): Avoid recreation, it should be enough to call
  // ReconfigureEncoder.
  RTC_LOG(LS_INFO) << "RecreateWebRtcStream (send) because of SetCodec.";
  RecreateWebRtcStream();
}

void WebRtcVideoChannel::WebRtcVideoSendStream::SetSendParameters(
    const ChangedSendParameters& params) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  // `recreate_stream` means construction-time parameters have changed and the
  // sending stream needs to be reset with the new config.
  bool recreate_stream = false;
  if (params.rtcp_mode) {
    parameters_.config.rtp.rtcp_mode = *params.rtcp_mode;
    rtp_parameters_.rtcp.reduced_size =
        parameters_.config.rtp.rtcp_mode == webrtc::RtcpMode::kReducedSize;
    recreate_stream = true;
  }
  if (params.extmap_allow_mixed) {
    parameters_.config.rtp.extmap_allow_mixed = *params.extmap_allow_mixed;
    recreate_stream = true;
  }
  if (params.rtp_header_extensions) {
    parameters_.config.rtp.extensions = *params.rtp_header_extensions;
    rtp_parameters_.header_extensions = *params.rtp_header_extensions;
    recreate_stream = true;
  }
  if (params.mid) {
    parameters_.config.rtp.mid = *params.mid;
    recreate_stream = true;
  }
  if (params.max_bandwidth_bps) {
    parameters_.max_bitrate_bps = *params.max_bandwidth_bps;
    ReconfigureEncoder(nullptr);
  }
  if (params.conference_mode) {
    parameters_.conference_mode = *params.conference_mode;
  }

  // Set codecs and options.
  if (params.send_codec) {
    SetCodec(*params.send_codec);
    recreate_stream = false;  // SetCodec has already recreated the stream.
  } else if (params.conference_mode && parameters_.codec_settings) {
    SetCodec(*parameters_.codec_settings);
    recreate_stream = false;  // SetCodec has already recreated the stream.
  }
  if (recreate_stream) {
    RTC_LOG(LS_INFO)
        << "RecreateWebRtcStream (send) because of SetSendParameters";
    RecreateWebRtcStream();
  }
}

webrtc::RTCError WebRtcVideoChannel::WebRtcVideoSendStream::SetRtpParameters(
    const webrtc::RtpParameters& new_parameters,
    webrtc::SetParametersCallback callback) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  // This is checked higher in the stack (RtpSender), so this is only checking
  // for users accessing the private APIs or tests, not specification
  // conformance.
  // TODO(orphis): Migrate tests to later make this a DCHECK only
  webrtc::RTCError error = CheckRtpParametersInvalidModificationAndValues(
      rtp_parameters_, new_parameters);
  if (!error.ok()) {
    // Error is propagated to the callback at a higher level
    return error;
  }

  bool new_param = false;
  for (size_t i = 0; i < rtp_parameters_.encodings.size(); ++i) {
    if ((new_parameters.encodings[i].min_bitrate_bps !=
         rtp_parameters_.encodings[i].min_bitrate_bps) ||
        (new_parameters.encodings[i].max_bitrate_bps !=
         rtp_parameters_.encodings[i].max_bitrate_bps) ||
        (new_parameters.encodings[i].max_framerate !=
         rtp_parameters_.encodings[i].max_framerate) ||
        (new_parameters.encodings[i].scale_resolution_down_by !=
         rtp_parameters_.encodings[i].scale_resolution_down_by) ||
        (new_parameters.encodings[i].num_temporal_layers !=
         rtp_parameters_.encodings[i].num_temporal_layers) ||
        (new_parameters.encodings[i].requested_resolution !=
         rtp_parameters_.encodings[i].requested_resolution) ||
        (new_parameters.encodings[i].scalability_mode !=
         rtp_parameters_.encodings[i].scalability_mode)) {
      new_param = true;
      break;
    }
  }

  bool new_degradation_preference = false;
  if (new_parameters.degradation_preference !=
      rtp_parameters_.degradation_preference) {
    new_degradation_preference = true;
  }

  // Some fields (e.g. bitrate priority) only need to update the bitrate
  // allocator which is updated via ReconfigureEncoder (however, note that the
  // actual encoder should only be reconfigured if needed).
  bool reconfigure_encoder =
      new_param || (new_parameters.encodings[0].bitrate_priority !=
                    rtp_parameters_.encodings[0].bitrate_priority);

  // Note that the simulcast encoder adapter relies on the fact that layers
  // de/activation triggers encoder reinitialization.
  bool new_send_state = false;
  for (size_t i = 0; i < rtp_parameters_.encodings.size(); ++i) {
    bool new_active = IsLayerActive(new_parameters.encodings[i]);
    bool old_active = IsLayerActive(rtp_parameters_.encodings[i]);
    if (new_active != old_active) {
      new_send_state = true;
    }
  }
  rtp_parameters_ = new_parameters;
  // Codecs are currently handled at the WebRtcVideoChannel level.
  rtp_parameters_.codecs.clear();
  if (reconfigure_encoder || new_send_state) {
    // Callback responsibility is delegated to ReconfigureEncoder()
    ReconfigureEncoder(std::move(callback));
    callback = nullptr;
  }
  if (new_send_state) {
    UpdateSendState();
  }
  if (new_degradation_preference) {
    if (source_ && stream_) {
      stream_->SetSource(source_, GetDegradationPreference());
    }
  }
  return webrtc::InvokeSetParametersCallback(callback, webrtc::RTCError::OK());
}

webrtc::RtpParameters
WebRtcVideoChannel::WebRtcVideoSendStream::GetRtpParameters() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return rtp_parameters_;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::SetFrameEncryptor(
    rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  parameters_.config.frame_encryptor = frame_encryptor;
  if (stream_) {
    RTC_LOG(LS_INFO)
        << "RecreateWebRtcStream (send) because of SetFrameEncryptor, ssrc="
        << parameters_.config.rtp.ssrcs[0];
    RecreateWebRtcStream();
  }
}

void WebRtcVideoChannel::WebRtcVideoSendStream::SetEncoderSelector(
    webrtc::VideoEncoderFactory::EncoderSelectorInterface* encoder_selector) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  parameters_.config.encoder_selector = encoder_selector;
  if (stream_) {
    RTC_LOG(LS_INFO)
        << "RecreateWebRtcStream (send) because of SetEncoderSelector, ssrc="
        << parameters_.config.rtp.ssrcs[0];
    RecreateWebRtcStream();
  }
}

void WebRtcVideoChannel::WebRtcVideoSendStream::UpdateSendState() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (sending_) {
    RTC_DCHECK(stream_ != nullptr);
    size_t num_layers = rtp_parameters_.encodings.size();
    if (parameters_.encoder_config.number_of_streams == 1) {
      // SVC is used. Only one simulcast layer is present.
      num_layers = 1;
    }
    std::vector<bool> active_layers(num_layers);
    for (size_t i = 0; i < num_layers; ++i) {
      active_layers[i] = IsLayerActive(rtp_parameters_.encodings[i]);
    }
    if (parameters_.encoder_config.number_of_streams == 1 &&
        rtp_parameters_.encodings.size() > 1) {
      // SVC is used.
      // The only present simulcast layer should be active if any of the
      // configured SVC layers is active.
      active_layers[0] =
          absl::c_any_of(rtp_parameters_.encodings,
                         [](const auto& encoding) { return encoding.active; });
    }
    // This updates what simulcast layers are sending, and possibly starts
    // or stops the VideoSendStream.
    stream_->StartPerRtpStream(active_layers);
  } else {
    if (stream_ != nullptr) {
      stream_->Stop();
    }
  }
}

webrtc::VideoEncoderConfig
WebRtcVideoChannel::WebRtcVideoSendStream::CreateVideoEncoderConfig(
    const VideoCodec& codec) const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  webrtc::VideoEncoderConfig encoder_config;
  encoder_config.codec_type = webrtc::PayloadStringToCodecType(codec.name);
  encoder_config.video_format =
      webrtc::SdpVideoFormat(codec.name, codec.params);

  bool is_screencast = parameters_.options.is_screencast.value_or(false);
  if (is_screencast) {
    encoder_config.min_transmit_bitrate_bps =
        1000 * parameters_.options.screencast_min_bitrate_kbps.value_or(0);
    encoder_config.content_type =
        webrtc::VideoEncoderConfig::ContentType::kScreen;
  } else {
    encoder_config.min_transmit_bitrate_bps = 0;
    encoder_config.content_type =
        webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo;
  }

  // By default, the stream count for the codec configuration should match the
  // number of negotiated ssrcs but this may be capped below depending on the
  // `legacy_scalability_mode` and codec used.
  encoder_config.number_of_streams = parameters_.config.rtp.ssrcs.size();
  bool legacy_scalability_mode = true;
  // TODO(https://crbug.com/webrtc/14884): This is only used as a kill-switch
  // in case of serious bugs - when this reaches Stable, delete the field trial.
  if (!call_->trials().IsDisabled("WebRTC-AllowDisablingLegacyScalability")) {
    for (const webrtc::RtpEncodingParameters& encoding :
         rtp_parameters_.encodings) {
      if (encoding.scalability_mode.has_value() &&
          encoding.scale_resolution_down_by.has_value()) {
        legacy_scalability_mode = false;
        break;
      }
    }
  }
  // Maybe limit the number of simulcast layers depending on
  // `legacy_scalability_mode`, codec types (VP9/AV1). This path only exists
  // for backwards compatibility and will one day be deleted. If you want SVC,
  // please specify with the `scalability_mode` API instead amd disabling all
  // but one encoding.
  if (IsCodecDisabledForSimulcast(legacy_scalability_mode,
                                  encoder_config.codec_type)) {
    encoder_config.number_of_streams = 1;
  }

  // parameters_.max_bitrate comes from the max bitrate set at the SDP
  // (m-section) level with the attribute "b=AS." Note that we override this
  // value below if the RtpParameters max bitrate set with
  // RtpSender::SetParameters has a lower value.
  int stream_max_bitrate = parameters_.max_bitrate_bps;
  // When simulcast is enabled (when there are multiple encodings),
  // encodings[i].max_bitrate_bps will be enforced by
  // encoder_config.simulcast_layers[i].max_bitrate_bps. Otherwise, it's
  // enforced by stream_max_bitrate, taking the minimum of the two maximums
  // (one coming from SDP, the other coming from RtpParameters).
  if (rtp_parameters_.encodings[0].max_bitrate_bps &&
      rtp_parameters_.encodings.size() == 1) {
    stream_max_bitrate =
        MinPositive(*(rtp_parameters_.encodings[0].max_bitrate_bps),
                    parameters_.max_bitrate_bps);
  }

  // The codec max bitrate comes from the "x-google-max-bitrate" parameter
  // attribute set in the SDP for a specific codec. As done in
  // WebRtcVideoChannel::SetSendParameters, this value does not override the
  // stream max_bitrate set above.
  int codec_max_bitrate_kbps;
  if (codec.GetParam(kCodecParamMaxBitrate, &codec_max_bitrate_kbps) &&
      stream_max_bitrate == -1) {
    stream_max_bitrate = codec_max_bitrate_kbps * 1000;
  }
  encoder_config.max_bitrate_bps = stream_max_bitrate;

  // The encoder config's default bitrate priority is set to 1.0,
  // unless it is set through the sender's encoding parameters.
  // The bitrate priority, which is used in the bitrate allocation, is done
  // on a per sender basis, so we use the first encoding's value.
  encoder_config.bitrate_priority =
      rtp_parameters_.encodings[0].bitrate_priority;

  // Application-controlled state is held in the encoder_config's
  // simulcast_layers. Currently this is used to control which simulcast layers
  // are active and for configuring the min/max bitrate and max framerate.
  // The encoder_config's simulcast_layers is also used for non-simulcast (when
  // there is a single layer).
  RTC_DCHECK_GE(rtp_parameters_.encodings.size(),
                encoder_config.number_of_streams);
  RTC_DCHECK_GT(encoder_config.number_of_streams, 0);

  // Copy all provided constraints.
  encoder_config.simulcast_layers.resize(rtp_parameters_.encodings.size());
  for (size_t i = 0; i < encoder_config.simulcast_layers.size(); ++i) {
    encoder_config.simulcast_layers[i].active =
        rtp_parameters_.encodings[i].active;
    encoder_config.simulcast_layers[i].scalability_mode =
        webrtc::ScalabilityModeFromString(
            rtp_parameters_.encodings[i].scalability_mode.value_or(""));
    if (rtp_parameters_.encodings[i].min_bitrate_bps) {
      encoder_config.simulcast_layers[i].min_bitrate_bps =
          *rtp_parameters_.encodings[i].min_bitrate_bps;
    }
    if (rtp_parameters_.encodings[i].max_bitrate_bps) {
      encoder_config.simulcast_layers[i].max_bitrate_bps =
          *rtp_parameters_.encodings[i].max_bitrate_bps;
    }
    if (rtp_parameters_.encodings[i].max_framerate) {
      encoder_config.simulcast_layers[i].max_framerate =
          *rtp_parameters_.encodings[i].max_framerate;
    }
    if (rtp_parameters_.encodings[i].scale_resolution_down_by) {
      encoder_config.simulcast_layers[i].scale_resolution_down_by =
          *rtp_parameters_.encodings[i].scale_resolution_down_by;
    }
    if (rtp_parameters_.encodings[i].num_temporal_layers) {
      encoder_config.simulcast_layers[i].num_temporal_layers =
          *rtp_parameters_.encodings[i].num_temporal_layers;
    }
    encoder_config.simulcast_layers[i].requested_resolution =
        rtp_parameters_.encodings[i].requested_resolution;
  }

  encoder_config.legacy_conference_mode = parameters_.conference_mode;

  encoder_config.is_quality_scaling_allowed =
      !disable_automatic_resize_ && !is_screencast &&
      (parameters_.config.rtp.ssrcs.size() == 1 ||
       NumActiveStreams(rtp_parameters_) == 1);

  // Ensure frame dropping is always enabled.
  encoder_config.frame_drop_enabled = true;

  int max_qp = kDefaultQpMax;
  codec.GetParam(kCodecParamMaxQuantization, &max_qp);
  encoder_config.max_qp = max_qp;

  return encoder_config;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::ReconfigureEncoder(
    webrtc::SetParametersCallback callback) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (!stream_) {
    // The webrtc::VideoSendStream `stream_` has not yet been created but other
    // parameters has changed.
    webrtc::InvokeSetParametersCallback(callback, webrtc::RTCError::OK());
    return;
  }

  RTC_DCHECK_GT(parameters_.encoder_config.number_of_streams, 0);

  RTC_CHECK(parameters_.codec_settings);
  VideoCodecSettings codec_settings = *parameters_.codec_settings;

  FallbackToDefaultScalabilityModeIfNotSupported(
      codec_settings.codec, parameters_.config, rtp_parameters_.encodings);

  // Latest config, with and without encoder specfic settings.
  webrtc::VideoEncoderConfig encoder_config =
      CreateVideoEncoderConfig(codec_settings.codec);
  encoder_config.encoder_specific_settings =
      ConfigureVideoEncoderSettings(codec_settings.codec);
  webrtc::VideoEncoderConfig encoder_config_with_specifics =
      encoder_config.Copy();
  encoder_config.encoder_specific_settings = nullptr;

  // When switching between legacy SVC (3 encodings interpreted as 1 stream with
  // 3 spatial layers) and the standard API (3 encodings = 3 streams and spatial
  // layers specified by `scalability_mode`), the number of streams can change.
  bool num_streams_changed = parameters_.encoder_config.number_of_streams !=
                             encoder_config.number_of_streams;
  bool scalability_mode_used = !codec_settings.codec.scalability_modes.empty();
  bool scalability_modes = absl::c_any_of(
      rtp_parameters_.encodings,
      [](const auto& e) { return e.scalability_mode.has_value(); });

  parameters_.encoder_config = std::move(encoder_config);

  if (num_streams_changed && (scalability_mode_used != scalability_modes)) {
    // The app is switching between legacy and standard modes, recreate instead
    // of reconfiguring to avoid number of streams not matching in lower layers.
    RecreateWebRtcStream();
    webrtc::InvokeSetParametersCallback(callback, webrtc::RTCError::OK());
    return;
  }

  stream_->ReconfigureVideoEncoder(std::move(encoder_config_with_specifics),
                                   std::move(callback));
}

void WebRtcVideoChannel::WebRtcVideoSendStream::SetSend(bool send) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  sending_ = send;
  UpdateSendState();
}

std::vector<VideoSenderInfo>
WebRtcVideoChannel::WebRtcVideoSendStream::GetPerLayerVideoSenderInfos(
    bool log_stats) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  VideoSenderInfo common_info;
  if (parameters_.codec_settings) {
    common_info.codec_name = parameters_.codec_settings->codec.name;
    common_info.codec_payload_type = parameters_.codec_settings->codec.id;
  }
  std::vector<VideoSenderInfo> infos;
  webrtc::VideoSendStream::Stats stats;
  if (stream_ == nullptr) {
    for (uint32_t ssrc : parameters_.config.rtp.ssrcs) {
      common_info.add_ssrc(ssrc);
    }
    infos.push_back(common_info);
    return infos;
  } else {
    stats = stream_->GetStats();
    if (log_stats)
      RTC_LOG(LS_INFO) << stats.ToString(rtc::TimeMillis());

    // Metrics that are in common for all substreams.
    common_info.adapt_changes = stats.number_of_cpu_adapt_changes;
    common_info.adapt_reason =
        stats.cpu_limited_resolution ? ADAPTREASON_CPU : ADAPTREASON_NONE;
    common_info.has_entered_low_resolution = stats.has_entered_low_resolution;

    // Get bandwidth limitation info from stream_->GetStats().
    // Input resolution (output from video_adapter) can be further scaled down
    // or higher video layer(s) can be dropped due to bitrate constraints.
    // Note, adapt_changes only include changes from the video_adapter.
    if (stats.bw_limited_resolution)
      common_info.adapt_reason |= ADAPTREASON_BANDWIDTH;

    common_info.quality_limitation_reason = stats.quality_limitation_reason;
    common_info.quality_limitation_durations_ms =
        stats.quality_limitation_durations_ms;
    common_info.quality_limitation_resolution_changes =
        stats.quality_limitation_resolution_changes;
    common_info.encoder_implementation_name = stats.encoder_implementation_name;
    common_info.target_bitrate = stats.target_media_bitrate_bps;
    common_info.ssrc_groups = ssrc_groups_;
    common_info.frames = stats.frames;
    common_info.framerate_input = stats.input_frame_rate;
    common_info.avg_encode_ms = stats.avg_encode_time_ms;
    common_info.encode_usage_percent = stats.encode_usage_percent;
    common_info.nominal_bitrate = stats.media_bitrate_bps;
    common_info.content_type = stats.content_type;
    common_info.aggregated_framerate_sent = stats.encode_frame_rate;
    common_info.aggregated_huge_frames_sent = stats.huge_frames_sent;
    common_info.power_efficient_encoder = stats.power_efficient_encoder;

    // The normal case is that substreams are present, handled below. But if
    // substreams are missing (can happen before negotiated/connected where we
    // have no stats yet) a single outbound-rtp is created representing any and
    // all layers.
    if (stats.substreams.empty()) {
      for (uint32_t ssrc : parameters_.config.rtp.ssrcs) {
        common_info.add_ssrc(ssrc);
      }
      common_info.active =
          IsActiveFromEncodings(absl::nullopt, rtp_parameters_.encodings);
      common_info.framerate_sent = stats.encode_frame_rate;
      common_info.frames_encoded = stats.frames_encoded;
      common_info.total_encode_time_ms = stats.total_encode_time_ms;
      common_info.total_encoded_bytes_target = stats.total_encoded_bytes_target;
      common_info.frames_sent = stats.frames_encoded;
      common_info.huge_frames_sent = stats.huge_frames_sent;
      infos.push_back(common_info);
      return infos;
    }
  }
  // Merge `stats.substreams`, which may contain additional SSRCs for RTX or
  // Flexfec, with media SSRCs. This results in a set of substreams that match
  // with the outbound-rtp stats objects.
  auto outbound_rtp_substreams =
      MergeInfoAboutOutboundRtpSubstreams(stats.substreams);
  // If SVC is used, one stream is configured but multiple encodings exist. This
  // is not spec-compliant, but it is how we've implemented SVC so this affects
  // how the RTP stream's "active" value is determined.
  bool is_svc = (parameters_.encoder_config.number_of_streams == 1 &&
                 rtp_parameters_.encodings.size() > 1);
  for (const auto& pair : outbound_rtp_substreams) {
    auto info = common_info;
    uint32_t ssrc = pair.first;
    info.add_ssrc(ssrc);
    info.rid = parameters_.config.rtp.GetRidForSsrc(ssrc);
    info.active = IsActiveFromEncodings(
        !is_svc ? absl::optional<uint32_t>(ssrc) : absl::nullopt,
        rtp_parameters_.encodings);
    auto stream_stats = pair.second;
    RTC_DCHECK_EQ(stream_stats.type,
                  webrtc::VideoSendStream::StreamStats::StreamType::kMedia);
    info.payload_bytes_sent = stream_stats.rtp_stats.transmitted.payload_bytes;
    info.header_and_padding_bytes_sent =
        stream_stats.rtp_stats.transmitted.header_bytes +
        stream_stats.rtp_stats.transmitted.padding_bytes;
    info.packets_sent = stream_stats.rtp_stats.transmitted.packets;
    info.total_packet_send_delay +=
        stream_stats.rtp_stats.transmitted.total_packet_delay;
    info.send_frame_width = stream_stats.width;
    info.send_frame_height = stream_stats.height;
    info.key_frames_encoded = stream_stats.frame_counts.key_frames;
    info.framerate_sent = stream_stats.encode_frame_rate;
    info.frames_encoded = stream_stats.frames_encoded;
    info.frames_sent = stream_stats.frames_encoded;
    info.retransmitted_bytes_sent =
        stream_stats.rtp_stats.retransmitted.payload_bytes;
    info.retransmitted_packets_sent =
        stream_stats.rtp_stats.retransmitted.packets;
    info.firs_received = stream_stats.rtcp_packet_type_counts.fir_packets;
    info.nacks_received = stream_stats.rtcp_packet_type_counts.nack_packets;
    info.plis_received = stream_stats.rtcp_packet_type_counts.pli_packets;
    if (stream_stats.report_block_data.has_value()) {
      info.packets_lost = stream_stats.report_block_data->cumulative_lost();
      info.fraction_lost = stream_stats.report_block_data->fraction_lost();
      info.report_block_datas.push_back(*stream_stats.report_block_data);
    }
    info.qp_sum = stream_stats.qp_sum;
    info.total_encode_time_ms = stream_stats.total_encode_time_ms;
    info.total_encoded_bytes_target = stream_stats.total_encoded_bytes_target;
    info.huge_frames_sent = stream_stats.huge_frames_sent;
    info.scalability_mode = stream_stats.scalability_mode;
    infos.push_back(info);
  }
  return infos;
}

VideoSenderInfo
WebRtcVideoChannel::WebRtcVideoSendStream::GetAggregatedVideoSenderInfo(
    const std::vector<VideoSenderInfo>& infos) const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_CHECK(!infos.empty());
  if (infos.size() == 1) {
    return infos[0];
  }
  VideoSenderInfo info = infos[0];
  info.local_stats.clear();
  for (uint32_t ssrc : parameters_.config.rtp.ssrcs) {
    info.add_ssrc(ssrc);
  }
  info.framerate_sent = info.aggregated_framerate_sent;
  info.huge_frames_sent = info.aggregated_huge_frames_sent;

  for (size_t i = 1; i < infos.size(); i++) {
    info.key_frames_encoded += infos[i].key_frames_encoded;
    info.payload_bytes_sent += infos[i].payload_bytes_sent;
    info.header_and_padding_bytes_sent +=
        infos[i].header_and_padding_bytes_sent;
    info.packets_sent += infos[i].packets_sent;
    info.total_packet_send_delay += infos[i].total_packet_send_delay;
    info.retransmitted_bytes_sent += infos[i].retransmitted_bytes_sent;
    info.retransmitted_packets_sent += infos[i].retransmitted_packets_sent;
    info.packets_lost += infos[i].packets_lost;
    if (infos[i].send_frame_width > info.send_frame_width)
      info.send_frame_width = infos[i].send_frame_width;
    if (infos[i].send_frame_height > info.send_frame_height)
      info.send_frame_height = infos[i].send_frame_height;
    info.firs_received += infos[i].firs_received;
    info.nacks_received += infos[i].nacks_received;
    info.plis_received += infos[i].plis_received;
    if (infos[i].report_block_datas.size())
      info.report_block_datas.push_back(infos[i].report_block_datas[0]);
    if (infos[i].qp_sum) {
      if (!info.qp_sum) {
        info.qp_sum = 0;
      }
      info.qp_sum = *info.qp_sum + *infos[i].qp_sum;
    }
    info.frames_encoded += infos[i].frames_encoded;
    info.frames_sent += infos[i].frames_sent;
    info.total_encode_time_ms += infos[i].total_encode_time_ms;
    info.total_encoded_bytes_target += infos[i].total_encoded_bytes_target;
  }
  return info;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::FillBitrateInfo(
    BandwidthEstimationInfo* bwe_info) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (stream_ == NULL) {
    return;
  }
  webrtc::VideoSendStream::Stats stats = stream_->GetStats();
  for (const auto& it : stats.substreams) {
    bwe_info->transmit_bitrate += it.second.total_bitrate_bps;
    bwe_info->retransmit_bitrate += it.second.retransmit_bitrate_bps;
  }
  bwe_info->target_enc_bitrate += stats.target_media_bitrate_bps;
  bwe_info->actual_enc_bitrate += stats.media_bitrate_bps;
}

void WebRtcVideoChannel::WebRtcVideoSendStream::
    SetEncoderToPacketizerFrameTransformer(
        rtc::scoped_refptr<webrtc::FrameTransformerInterface>
            frame_transformer) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  parameters_.config.frame_transformer = std::move(frame_transformer);
  if (stream_)
    RecreateWebRtcStream();
}

void WebRtcVideoChannel::WebRtcVideoSendStream::RecreateWebRtcStream() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (stream_ != NULL) {
    call_->DestroyVideoSendStream(stream_);
  }

  RTC_CHECK(parameters_.codec_settings);
  RTC_DCHECK_EQ((parameters_.encoder_config.content_type ==
                 webrtc::VideoEncoderConfig::ContentType::kScreen),
                parameters_.options.is_screencast.value_or(false))
      << "encoder content type inconsistent with screencast option";
  parameters_.encoder_config.encoder_specific_settings =
      ConfigureVideoEncoderSettings(parameters_.codec_settings->codec);

  webrtc::VideoSendStream::Config config = parameters_.config.Copy();
  if (!config.rtp.rtx.ssrcs.empty() && config.rtp.rtx.payload_type == -1) {
    RTC_LOG(LS_WARNING) << "RTX SSRCs configured but there's no configured RTX "
                           "payload type the set codec. Ignoring RTX.";
    config.rtp.rtx.ssrcs.clear();
  }
  if (parameters_.encoder_config.number_of_streams == 1) {
    // SVC is used instead of simulcast. Remove unnecessary SSRCs.
    if (config.rtp.ssrcs.size() > 1) {
      config.rtp.ssrcs.resize(1);
      if (config.rtp.rtx.ssrcs.size() > 1) {
        config.rtp.rtx.ssrcs.resize(1);
      }
    }
  }
  stream_ = call_->CreateVideoSendStream(std::move(config),
                                         parameters_.encoder_config.Copy());

  parameters_.encoder_config.encoder_specific_settings = NULL;

  // Calls stream_->StartPerRtpStream() to start the VideoSendStream
  // if necessary conditions are met.
  UpdateSendState();

  // Attach the source after starting the send stream to prevent frames from
  // being injected into a not-yet initializated video stream encoder.
  if (source_) {
    stream_->SetSource(source_, GetDegradationPreference());
  }
}

void WebRtcVideoChannel::WebRtcVideoSendStream::GenerateKeyFrame(
    const std::vector<std::string>& rids) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (stream_ != NULL) {
    stream_->GenerateKeyFrame(rids);
  } else {
    RTC_LOG(LS_WARNING)
        << "Absent send stream; ignoring request to generate keyframe.";
  }
}

WebRtcVideoChannel::WebRtcVideoReceiveStream::WebRtcVideoReceiveStream(
    webrtc::Call* call,
    const StreamParams& sp,
    webrtc::VideoReceiveStreamInterface::Config config,
    bool default_stream,
    const std::vector<VideoCodecSettings>& recv_codecs,
    const webrtc::FlexfecReceiveStream::Config& flexfec_config)
    : call_(call),
      stream_params_(sp),
      stream_(NULL),
      default_stream_(default_stream),
      config_(std::move(config)),
      flexfec_config_(flexfec_config),
      flexfec_stream_(nullptr),
      sink_(NULL),
      first_frame_timestamp_(-1),
      estimated_remote_start_ntp_time_ms_(0),
      receiving_(false) {
  RTC_DCHECK(config_.decoder_factory);
  RTC_DCHECK(config_.decoders.empty())
      << "Decoder info is supplied via `recv_codecs`";

  ExtractCodecInformation(recv_codecs, config_.rtp.rtx_associated_payload_types,
                          config_.rtp.raw_payload_types, config_.decoders);
  const VideoCodecSettings& codec = recv_codecs.front();
  config_.rtp.ulpfec_payload_type = codec.ulpfec.ulpfec_payload_type;
  config_.rtp.red_payload_type = codec.ulpfec.red_payload_type;
  config_.rtp.lntf.enabled = HasLntf(codec.codec);
  config_.rtp.nack.rtp_history_ms = HasNack(codec.codec) ? kNackHistoryMs : 0;
  if (codec.rtx_time && config_.rtp.nack.rtp_history_ms != 0) {
    config_.rtp.nack.rtp_history_ms = *codec.rtx_time;
  }

  config_.rtp.rtcp_xr.receiver_reference_time_report = HasRrtr(codec.codec);

  if (codec.ulpfec.red_rtx_payload_type != -1) {
    config_.rtp
        .rtx_associated_payload_types[codec.ulpfec.red_rtx_payload_type] =
        codec.ulpfec.red_payload_type;
  }

  config_.renderer = this;
  flexfec_config_.payload_type = flexfec_config.payload_type;

  CreateReceiveStream();
}

WebRtcVideoChannel::WebRtcVideoReceiveStream::~WebRtcVideoReceiveStream() {
  call_->DestroyVideoReceiveStream(stream_);
  if (flexfec_stream_)
    call_->DestroyFlexfecReceiveStream(flexfec_stream_);
}

webrtc::VideoReceiveStreamInterface&
WebRtcVideoChannel::WebRtcVideoReceiveStream::stream() {
  RTC_DCHECK(stream_);
  return *stream_;
}

webrtc::FlexfecReceiveStream*
WebRtcVideoChannel::WebRtcVideoReceiveStream::flexfec_stream() {
  return flexfec_stream_;
}

const std::vector<uint32_t>&
WebRtcVideoChannel::WebRtcVideoReceiveStream::GetSsrcs() const {
  return stream_params_.ssrcs;
}

std::vector<webrtc::RtpSource>
WebRtcVideoChannel::WebRtcVideoReceiveStream::GetSources() {
  RTC_DCHECK(stream_);
  return stream_->GetSources();
}

webrtc::RtpParameters
WebRtcVideoChannel::WebRtcVideoReceiveStream::GetRtpParameters() const {
  webrtc::RtpParameters rtp_parameters;

  std::vector<uint32_t> primary_ssrcs;
  stream_params_.GetPrimarySsrcs(&primary_ssrcs);
  for (uint32_t ssrc : primary_ssrcs) {
    rtp_parameters.encodings.emplace_back();
    rtp_parameters.encodings.back().ssrc = ssrc;
  }

  rtp_parameters.rtcp.reduced_size =
      config_.rtp.rtcp_mode == webrtc::RtcpMode::kReducedSize;

  return rtp_parameters;
}

bool WebRtcVideoChannel::WebRtcVideoReceiveStream::ReconfigureCodecs(
    const std::vector<VideoCodecSettings>& recv_codecs) {
  RTC_DCHECK(stream_);
  RTC_DCHECK(!recv_codecs.empty());

  std::map<int, int> rtx_associated_payload_types;
  std::set<int> raw_payload_types;
  std::vector<webrtc::VideoReceiveStreamInterface::Decoder> decoders;
  ExtractCodecInformation(recv_codecs, rtx_associated_payload_types,
                          raw_payload_types, decoders);

  const auto& codec = recv_codecs.front();

  if (config_.rtp.red_payload_type != codec.ulpfec.red_payload_type ||
      config_.rtp.ulpfec_payload_type != codec.ulpfec.ulpfec_payload_type) {
    config_.rtp.ulpfec_payload_type = codec.ulpfec.ulpfec_payload_type;
    config_.rtp.red_payload_type = codec.ulpfec.red_payload_type;
    stream_->SetProtectionPayloadTypes(config_.rtp.red_payload_type,
                                       config_.rtp.ulpfec_payload_type);
  }

  const bool has_lntf = HasLntf(codec.codec);
  if (config_.rtp.lntf.enabled != has_lntf) {
    config_.rtp.lntf.enabled = has_lntf;
    stream_->SetLossNotificationEnabled(has_lntf);
  }

  int new_history_ms = config_.rtp.nack.rtp_history_ms;
  const int rtp_history_ms = HasNack(codec.codec) ? kNackHistoryMs : 0;
  if (rtp_history_ms != config_.rtp.nack.rtp_history_ms) {
    new_history_ms = rtp_history_ms;
  }

  // The rtx-time parameter can be used to override the hardcoded default for
  // the NACK buffer length.
  if (codec.rtx_time && new_history_ms != 0) {
    new_history_ms = *codec.rtx_time;
  }

  if (config_.rtp.nack.rtp_history_ms != new_history_ms) {
    config_.rtp.nack.rtp_history_ms = new_history_ms;
    stream_->SetNackHistory(webrtc::TimeDelta::Millis(new_history_ms));
  }

  const bool has_rtr = HasRrtr(codec.codec);
  if (has_rtr != config_.rtp.rtcp_xr.receiver_reference_time_report) {
    config_.rtp.rtcp_xr.receiver_reference_time_report = has_rtr;
    stream_->SetRtcpXr(config_.rtp.rtcp_xr);
  }

  if (codec.ulpfec.red_rtx_payload_type != -1) {
    rtx_associated_payload_types[codec.ulpfec.red_rtx_payload_type] =
        codec.ulpfec.red_payload_type;
  }

  if (config_.rtp.rtx_associated_payload_types !=
      rtx_associated_payload_types) {
    stream_->SetAssociatedPayloadTypes(rtx_associated_payload_types);
    rtx_associated_payload_types.swap(config_.rtp.rtx_associated_payload_types);
  }

  bool recreate_needed = false;

  if (raw_payload_types != config_.rtp.raw_payload_types) {
    raw_payload_types.swap(config_.rtp.raw_payload_types);
    recreate_needed = true;
  }

  if (decoders != config_.decoders) {
    decoders.swap(config_.decoders);
    recreate_needed = true;
  }

  return recreate_needed;
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetFeedbackParameters(
    bool lntf_enabled,
    bool nack_enabled,
    webrtc::RtcpMode rtcp_mode,
    absl::optional<int> rtx_time) {
  RTC_DCHECK(stream_);

  if (config_.rtp.rtcp_mode != rtcp_mode) {
    config_.rtp.rtcp_mode = rtcp_mode;
    stream_->SetRtcpMode(rtcp_mode);

    flexfec_config_.rtcp_mode = rtcp_mode;
    if (flexfec_stream_) {
      flexfec_stream_->SetRtcpMode(rtcp_mode);
    }
  }

  config_.rtp.lntf.enabled = lntf_enabled;
  stream_->SetLossNotificationEnabled(lntf_enabled);

  int nack_history_ms = nack_enabled ? rtx_time.value_or(kNackHistoryMs) : 0;
  config_.rtp.nack.rtp_history_ms = nack_history_ms;
  stream_->SetNackHistory(webrtc::TimeDelta::Millis(nack_history_ms));
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetFlexFecPayload(
    int payload_type) {
  // TODO(bugs.webrtc.org/11993, tommi): See if it is better to always have a
  // flexfec stream object around and instead of recreating the video stream,
  // reconfigure the flexfec object from within the rtp callback (soon to be on
  // the network thread).
  if (flexfec_stream_) {
    if (flexfec_stream_->payload_type() == payload_type) {
      RTC_DCHECK_EQ(flexfec_config_.payload_type, payload_type);
      return;
    }

    flexfec_config_.payload_type = payload_type;
    flexfec_stream_->SetPayloadType(payload_type);

    if (payload_type == -1) {
      stream_->SetFlexFecProtection(nullptr);
      call_->DestroyFlexfecReceiveStream(flexfec_stream_);
      flexfec_stream_ = nullptr;
    }
  } else if (payload_type != -1) {
    flexfec_config_.payload_type = payload_type;
    if (flexfec_config_.IsCompleteAndEnabled()) {
      flexfec_stream_ = call_->CreateFlexfecReceiveStream(flexfec_config_);
      stream_->SetFlexFecProtection(flexfec_stream_);
    }
  } else {
    // Noop. No flexfec stream exists and "new" payload_type == -1.
    RTC_DCHECK(!flexfec_config_.IsCompleteAndEnabled());
    flexfec_config_.payload_type = payload_type;
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetRecvParameters(
    const ChangedRecvParameters& params) {
  RTC_DCHECK(stream_);
  bool video_needs_recreation = false;
  if (params.codec_settings) {
    video_needs_recreation = ReconfigureCodecs(*params.codec_settings);
  }

  if (params.flexfec_payload_type)
    SetFlexFecPayload(*params.flexfec_payload_type);

  if (video_needs_recreation) {
    RecreateReceiveStream();
  } else {
    RTC_DLOG_F(LS_INFO) << "No receive stream recreate needed.";
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::RecreateReceiveStream() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(stream_);
  absl::optional<int> base_minimum_playout_delay_ms;
  absl::optional<webrtc::VideoReceiveStreamInterface::RecordingState>
      recording_state;
  if (stream_) {
    base_minimum_playout_delay_ms = stream_->GetBaseMinimumPlayoutDelayMs();
    recording_state = stream_->SetAndGetRecordingState(
        webrtc::VideoReceiveStreamInterface::RecordingState(),
        /*generate_key_frame=*/false);
    call_->DestroyVideoReceiveStream(stream_);
    stream_ = nullptr;
  }

  if (flexfec_stream_) {
    call_->DestroyFlexfecReceiveStream(flexfec_stream_);
    flexfec_stream_ = nullptr;
  }

  CreateReceiveStream();

  if (base_minimum_playout_delay_ms) {
    stream_->SetBaseMinimumPlayoutDelayMs(
        base_minimum_playout_delay_ms.value());
  }
  if (recording_state) {
    stream_->SetAndGetRecordingState(std::move(*recording_state),
                                     /*generate_key_frame=*/false);
  }
  if (receiving_) {
    StartReceiveStream();
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::CreateReceiveStream() {
  RTC_DCHECK(!stream_);
  RTC_DCHECK(!flexfec_stream_);
  if (flexfec_config_.IsCompleteAndEnabled()) {
    flexfec_stream_ = call_->CreateFlexfecReceiveStream(flexfec_config_);
  }

  webrtc::VideoReceiveStreamInterface::Config config = config_.Copy();
  config.rtp.protected_by_flexfec = (flexfec_stream_ != nullptr);
  config.rtp.packet_sink_ = flexfec_stream_;
  stream_ = call_->CreateVideoReceiveStream(std::move(config));
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::StartReceiveStream() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  receiving_ = true;
  stream_->Start();
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::StopReceiveStream() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  receiving_ = false;
  stream_->Stop();
  RecreateReceiveStream();
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::OnFrame(
    const webrtc::VideoFrame& frame) {
  webrtc::MutexLock lock(&sink_lock_);

  int64_t time_now_ms = rtc::TimeMillis();
  if (first_frame_timestamp_ < 0)
    first_frame_timestamp_ = time_now_ms;
  int64_t elapsed_time_ms = time_now_ms - first_frame_timestamp_;
  if (frame.ntp_time_ms() > 0)
    estimated_remote_start_ntp_time_ms_ = frame.ntp_time_ms() - elapsed_time_ms;

  if (sink_ == NULL) {
    RTC_LOG(LS_WARNING)
        << "VideoReceiveStreamInterface not connected to a VideoSink.";
    return;
  }

  sink_->OnFrame(frame);
}

bool WebRtcVideoChannel::WebRtcVideoReceiveStream::IsDefaultStream() const {
  return default_stream_;
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetFrameDecryptor(
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
  config_.frame_decryptor = frame_decryptor;
  if (stream_) {
    RTC_LOG(LS_INFO)
        << "Setting FrameDecryptor (recv) because of SetFrameDecryptor, "
           "remote_ssrc="
        << config_.rtp.remote_ssrc;
    stream_->SetFrameDecryptor(frame_decryptor);
  }
}

bool WebRtcVideoChannel::WebRtcVideoReceiveStream::SetBaseMinimumPlayoutDelayMs(
    int delay_ms) {
  return stream_ ? stream_->SetBaseMinimumPlayoutDelayMs(delay_ms) : false;
}

int WebRtcVideoChannel::WebRtcVideoReceiveStream::GetBaseMinimumPlayoutDelayMs()
    const {
  return stream_ ? stream_->GetBaseMinimumPlayoutDelayMs() : 0;
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  webrtc::MutexLock lock(&sink_lock_);
  sink_ = sink;
}

VideoReceiverInfo
WebRtcVideoChannel::WebRtcVideoReceiveStream::GetVideoReceiverInfo(
    bool log_stats) {
  VideoReceiverInfo info;
  info.ssrc_groups = stream_params_.ssrc_groups;
  info.add_ssrc(config_.rtp.remote_ssrc);
  webrtc::VideoReceiveStreamInterface::Stats stats = stream_->GetStats();
  info.decoder_implementation_name = stats.decoder_implementation_name;
  info.power_efficient_decoder = stats.power_efficient_decoder;
  if (stats.current_payload_type != -1) {
    info.codec_payload_type = stats.current_payload_type;
    auto decoder_it = absl::c_find_if(config_.decoders, [&](const auto& d) {
      return d.payload_type == stats.current_payload_type;
    });
    if (decoder_it != config_.decoders.end())
      info.codec_name = decoder_it->video_format.name;
  }
  info.payload_bytes_received = stats.rtp_stats.packet_counter.payload_bytes;
  info.header_and_padding_bytes_received =
      stats.rtp_stats.packet_counter.header_bytes +
      stats.rtp_stats.packet_counter.padding_bytes;
  info.packets_received = stats.rtp_stats.packet_counter.packets;
  info.packets_lost = stats.rtp_stats.packets_lost;
  info.jitter_ms = stats.rtp_stats.jitter / (kVideoCodecClockrate / 1000);

  info.framerate_received = stats.network_frame_rate;
  info.framerate_decoded = stats.decode_frame_rate;
  info.framerate_output = stats.render_frame_rate;
  info.frame_width = stats.width;
  info.frame_height = stats.height;

  {
    webrtc::MutexLock frame_cs(&sink_lock_);
    info.capture_start_ntp_time_ms = estimated_remote_start_ntp_time_ms_;
  }

  info.decode_ms = stats.decode_ms;
  info.max_decode_ms = stats.max_decode_ms;
  info.current_delay_ms = stats.current_delay_ms;
  info.target_delay_ms = stats.target_delay_ms;
  info.jitter_buffer_ms = stats.jitter_buffer_ms;
  info.jitter_buffer_delay_seconds = stats.jitter_buffer_delay_seconds;
  info.jitter_buffer_emitted_count = stats.jitter_buffer_emitted_count;
  info.min_playout_delay_ms = stats.min_playout_delay_ms;
  info.render_delay_ms = stats.render_delay_ms;
  info.frames_received =
      stats.frame_counts.key_frames + stats.frame_counts.delta_frames;
  info.frames_dropped = stats.frames_dropped;
  info.frames_decoded = stats.frames_decoded;
  info.key_frames_decoded = stats.frame_counts.key_frames;
  info.frames_rendered = stats.frames_rendered;
  info.qp_sum = stats.qp_sum;
  info.total_decode_time = stats.total_decode_time;
  info.total_processing_delay = stats.total_processing_delay;
  info.total_assembly_time = stats.total_assembly_time;
  info.frames_assembled_from_multiple_packets =
      stats.frames_assembled_from_multiple_packets;
  info.last_packet_received_timestamp_ms =
      stats.rtp_stats.last_packet_received_timestamp_ms;
  info.estimated_playout_ntp_timestamp_ms =
      stats.estimated_playout_ntp_timestamp_ms;
  info.first_frame_received_to_decoded_ms =
      stats.first_frame_received_to_decoded_ms;
  info.total_inter_frame_delay = stats.total_inter_frame_delay;
  info.total_squared_inter_frame_delay = stats.total_squared_inter_frame_delay;
  info.interframe_delay_max_ms = stats.interframe_delay_max_ms;
  info.freeze_count = stats.freeze_count;
  info.pause_count = stats.pause_count;
  info.total_freezes_duration_ms = stats.total_freezes_duration_ms;
  info.total_pauses_duration_ms = stats.total_pauses_duration_ms;

  info.content_type = stats.content_type;

  info.firs_sent = stats.rtcp_packet_type_counts.fir_packets;
  info.plis_sent = stats.rtcp_packet_type_counts.pli_packets;
  info.nacks_sent = stats.rtcp_packet_type_counts.nack_packets;
  // TODO(bugs.webrtc.org/10662): Add stats for LNTF.

  info.timing_frame_info = stats.timing_frame_info;

  if (stats.rtx_rtp_stats.has_value()) {
    info.retransmitted_packets_received =
        stats.rtx_rtp_stats->packet_counter.packets;
    info.retransmitted_bytes_received =
        stats.rtx_rtp_stats->packet_counter.payload_bytes;
    // RTX information gets added to primary counters.
    info.payload_bytes_received +=
        stats.rtx_rtp_stats->packet_counter.payload_bytes;
    info.header_and_padding_bytes_received +=
        stats.rtx_rtp_stats->packet_counter.header_bytes +
        stats.rtx_rtp_stats->packet_counter.padding_bytes;
    info.packets_received += stats.rtx_rtp_stats->packet_counter.packets;
  }

  if (log_stats)
    RTC_LOG(LS_INFO) << stats.ToString(rtc::TimeMillis());

  return info;
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::
    SetRecordableEncodedFrameCallback(
        std::function<void(const webrtc::RecordableEncodedFrame&)> callback) {
  if (stream_) {
    stream_->SetAndGetRecordingState(
        webrtc::VideoReceiveStreamInterface::RecordingState(
            std::move(callback)),
        /*generate_key_frame=*/true);
  } else {
    RTC_LOG(LS_ERROR) << "Absent receive stream; ignoring setting encoded "
                         "frame sink";
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::
    ClearRecordableEncodedFrameCallback() {
  if (stream_) {
    stream_->SetAndGetRecordingState(
        webrtc::VideoReceiveStreamInterface::RecordingState(),
        /*generate_key_frame=*/false);
  } else {
    RTC_LOG(LS_ERROR) << "Absent receive stream; ignoring clearing encoded "
                         "frame sink";
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::GenerateKeyFrame() {
  if (stream_) {
    stream_->GenerateKeyFrame();
  } else {
    RTC_LOG(LS_ERROR)
        << "Absent receive stream; ignoring key frame generation request.";
  }
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::
    SetDepacketizerToDecoderFrameTransformer(
        rtc::scoped_refptr<webrtc::FrameTransformerInterface>
            frame_transformer) {
  config_.frame_transformer = frame_transformer;
  if (stream_)
    stream_->SetDepacketizerToDecoderFrameTransformer(frame_transformer);
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::SetLocalSsrc(uint32_t ssrc) {
  config_.rtp.local_ssrc = ssrc;
  call_->OnLocalSsrcUpdated(stream(), ssrc);
  if (flexfec_stream_)
    call_->OnLocalSsrcUpdated(*flexfec_stream_, ssrc);
}

void WebRtcVideoChannel::WebRtcVideoReceiveStream::UpdateRtxSsrc(
    uint32_t ssrc) {
  stream_->UpdateRtxSsrc(ssrc);
}

WebRtcVideoChannel::VideoCodecSettings::VideoCodecSettings()
    : flexfec_payload_type(-1), rtx_payload_type(-1) {}

bool WebRtcVideoChannel::VideoCodecSettings::operator==(
    const WebRtcVideoChannel::VideoCodecSettings& other) const {
  return codec == other.codec && ulpfec == other.ulpfec &&
         flexfec_payload_type == other.flexfec_payload_type &&
         rtx_payload_type == other.rtx_payload_type &&
         rtx_time == other.rtx_time;
}

bool WebRtcVideoChannel::VideoCodecSettings::EqualsDisregardingFlexfec(
    const WebRtcVideoChannel::VideoCodecSettings& a,
    const WebRtcVideoChannel::VideoCodecSettings& b) {
  return a.codec == b.codec && a.ulpfec == b.ulpfec &&
         a.rtx_payload_type == b.rtx_payload_type && a.rtx_time == b.rtx_time;
}

bool WebRtcVideoChannel::VideoCodecSettings::operator!=(
    const WebRtcVideoChannel::VideoCodecSettings& other) const {
  return !(*this == other);
}

std::vector<WebRtcVideoChannel::VideoCodecSettings>
WebRtcVideoChannel::MapCodecs(const std::vector<VideoCodec>& codecs) {
  if (codecs.empty()) {
    return {};
  }

  std::vector<VideoCodecSettings> video_codecs;
  std::map<int, VideoCodec::CodecType> payload_codec_type;
  // `rtx_mapping` maps video payload type to rtx payload type.
  std::map<int, int> rtx_mapping;
  std::map<int, int> rtx_time_mapping;

  webrtc::UlpfecConfig ulpfec_config;
  absl::optional<int> flexfec_payload_type;

  for (const VideoCodec& in_codec : codecs) {
    const int payload_type = in_codec.id;

    if (payload_codec_type.find(payload_type) != payload_codec_type.end()) {
      RTC_LOG(LS_ERROR) << "Payload type already registered: "
                        << in_codec.ToString();
      return {};
    }
    payload_codec_type[payload_type] = in_codec.GetCodecType();

    switch (in_codec.GetCodecType()) {
      case VideoCodec::CODEC_RED: {
        if (ulpfec_config.red_payload_type != -1) {
          RTC_LOG(LS_ERROR)
              << "Duplicate RED codec: ignoring PT=" << payload_type
              << " in favor of PT=" << ulpfec_config.red_payload_type
              << " which was specified first.";
          break;
        }
        ulpfec_config.red_payload_type = payload_type;
        break;
      }

      case VideoCodec::CODEC_ULPFEC: {
        if (ulpfec_config.ulpfec_payload_type != -1) {
          RTC_LOG(LS_ERROR)
              << "Duplicate ULPFEC codec: ignoring PT=" << payload_type
              << " in favor of PT=" << ulpfec_config.ulpfec_payload_type
              << " which was specified first.";
          break;
        }
        ulpfec_config.ulpfec_payload_type = payload_type;
        break;
      }

      case VideoCodec::CODEC_FLEXFEC: {
        if (flexfec_payload_type) {
          RTC_LOG(LS_ERROR)
              << "Duplicate FLEXFEC codec: ignoring PT=" << payload_type
              << " in favor of PT=" << *flexfec_payload_type
              << " which was specified first.";
          break;
        }
        flexfec_payload_type = payload_type;
        break;
      }

      case VideoCodec::CODEC_RTX: {
        int associated_payload_type;
        if (!in_codec.GetParam(kCodecParamAssociatedPayloadType,
                               &associated_payload_type) ||
            !IsValidRtpPayloadType(associated_payload_type)) {
          RTC_LOG(LS_ERROR)
              << "RTX codec with invalid or no associated payload type: "
              << in_codec.ToString();
          return {};
        }
        int rtx_time;
        if (in_codec.GetParam(kCodecParamRtxTime, &rtx_time) && rtx_time > 0) {
          rtx_time_mapping[associated_payload_type] = rtx_time;
        }
        rtx_mapping[associated_payload_type] = payload_type;
        break;
      }

      case VideoCodec::CODEC_VIDEO: {
        video_codecs.emplace_back();
        video_codecs.back().codec = in_codec;
        break;
      }
    }
  }

  // One of these codecs should have been a video codec. Only having FEC
  // parameters into this code is a logic error.
  RTC_DCHECK(!video_codecs.empty());

  for (const auto& entry : rtx_mapping) {
    const int associated_payload_type = entry.first;
    const int rtx_payload_type = entry.second;
    auto it = payload_codec_type.find(associated_payload_type);
    if (it == payload_codec_type.end()) {
      RTC_LOG(LS_ERROR) << "RTX codec (PT=" << rtx_payload_type
                        << ") mapped to PT=" << associated_payload_type
                        << " which is not in the codec list.";
      return {};
    }
    const VideoCodec::CodecType associated_codec_type = it->second;
    if (associated_codec_type != VideoCodec::CODEC_VIDEO &&
        associated_codec_type != VideoCodec::CODEC_RED) {
      RTC_LOG(LS_ERROR)
          << "RTX PT=" << rtx_payload_type
          << " not mapped to regular video codec or RED codec (PT="
          << associated_payload_type << ").";
      return {};
    }

    if (associated_payload_type == ulpfec_config.red_payload_type) {
      ulpfec_config.red_rtx_payload_type = rtx_payload_type;
    }
  }

  for (VideoCodecSettings& codec_settings : video_codecs) {
    const int payload_type = codec_settings.codec.id;
    codec_settings.ulpfec = ulpfec_config;
    codec_settings.flexfec_payload_type = flexfec_payload_type.value_or(-1);
    auto it = rtx_mapping.find(payload_type);
    if (it != rtx_mapping.end()) {
      const int rtx_payload_type = it->second;
      codec_settings.rtx_payload_type = rtx_payload_type;

      auto rtx_time_it = rtx_time_mapping.find(payload_type);
      if (rtx_time_it != rtx_time_mapping.end()) {
        const int rtx_time = rtx_time_it->second;
        if (rtx_time < kNackHistoryMs) {
          codec_settings.rtx_time = rtx_time;
        } else {
          codec_settings.rtx_time = kNackHistoryMs;
        }
      }
    }
  }

  return video_codecs;
}

WebRtcVideoChannel::WebRtcVideoReceiveStream*
WebRtcVideoChannel::FindReceiveStream(uint32_t ssrc) {
  if (ssrc == 0) {
    absl::optional<uint32_t> default_ssrc = GetUnsignaledSsrc();
    if (!default_ssrc) {
      return nullptr;
    }
    ssrc = *default_ssrc;
  }
  auto it = receive_streams_.find(ssrc);
  if (it != receive_streams_.end()) {
    return it->second;
  }
  return nullptr;
}

// RTC_RUN_ON(worker_thread_)
void WebRtcVideoChannel::ProcessReceivedPacket(
    webrtc::RtpPacketReceived packet) {
  // TODO(bugs.webrtc.org/11993): This code is very similar to what
  // WebRtcVoiceMediaChannel::OnPacketReceived does. For maintainability and
  // consistency it would be good to move the interaction with call_->Receiver()
  // to a common implementation and provide a callback on the worker thread
  // for the exception case (DELIVERY_UNKNOWN_SSRC) and how retry is attempted.
  // TODO(bugs.webrtc.org/7135): extensions in `packet` is currently set
  // in RtpTransport and does not neccessarily include extensions specific
  // to this channel/MID. Also see comment in
  // BaseChannel::MaybeUpdateDemuxerAndRtpExtensions_w.
  // It would likely be good if extensions where merged per BUNDLE and
  // applied directly in RtpTransport::DemuxPacket;
  packet.IdentifyExtensions(recv_rtp_extension_map_);
  packet.set_payload_type_frequency(webrtc::kVideoPayloadTypeFrequency);
  if (!packet.arrival_time().IsFinite()) {
    packet.set_arrival_time(webrtc::Timestamp::Micros(rtc::TimeMicros()));
  }

  call_->Receiver()->DeliverRtpPacket(
      webrtc::MediaType::VIDEO, std::move(packet),
      absl::bind_front(&WebRtcVideoChannel::MaybeCreateDefaultReceiveStream,
                       this));
}

void WebRtcVideoChannel::SetRecordableEncodedFrameCallback(
    uint32_t ssrc,
    std::function<void(const webrtc::RecordableEncodedFrame&)> callback) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  WebRtcVideoReceiveStream* stream = FindReceiveStream(ssrc);
  if (stream) {
    stream->SetRecordableEncodedFrameCallback(std::move(callback));
  } else {
    RTC_LOG(LS_ERROR) << "Absent receive stream; ignoring setting encoded "
                         "frame sink for ssrc "
                      << ssrc;
  }
}

void WebRtcVideoChannel::ClearRecordableEncodedFrameCallback(uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  WebRtcVideoReceiveStream* stream = FindReceiveStream(ssrc);
  if (stream) {
    stream->ClearRecordableEncodedFrameCallback();
  } else {
    RTC_LOG(LS_ERROR) << "Absent receive stream; ignoring clearing encoded "
                         "frame sink for ssrc "
                      << ssrc;
  }
}

void WebRtcVideoChannel::RequestRecvKeyFrame(uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  WebRtcVideoReceiveStream* stream = FindReceiveStream(ssrc);
  if (stream) {
    return stream->GenerateKeyFrame();
  } else {
    RTC_LOG(LS_ERROR)
        << "Absent receive stream; ignoring key frame generation for ssrc "
        << ssrc;
  }
}

void WebRtcVideoChannel::GenerateSendKeyFrame(
    uint32_t ssrc,
    const std::vector<std::string>& rids) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  auto it = send_streams_.find(ssrc);
  if (it != send_streams_.end()) {
    it->second->GenerateKeyFrame(rids);
  } else {
    RTC_LOG(LS_ERROR)
        << "Absent send stream; ignoring key frame generation for ssrc "
        << ssrc;
  }
}

void WebRtcVideoChannel::SetEncoderToPacketizerFrameTransformer(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  auto matching_stream = send_streams_.find(ssrc);
  if (matching_stream != send_streams_.end()) {
    matching_stream->second->SetEncoderToPacketizerFrameTransformer(
        std::move(frame_transformer));
  }
}

void WebRtcVideoChannel::SetDepacketizerToDecoderFrameTransformer(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer) {
  RTC_DCHECK(frame_transformer);
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (ssrc == 0) {
    // If the receiver is unsignaled, save the frame transformer and set it
    // when the stream is associated with an ssrc.
    unsignaled_frame_transformer_ = std::move(frame_transformer);
    return;
  }

  auto matching_stream = receive_streams_.find(ssrc);
  if (matching_stream != receive_streams_.end()) {
    matching_stream->second->SetDepacketizerToDecoderFrameTransformer(
        std::move(frame_transformer));
  }
}

}  // namespace cricket
