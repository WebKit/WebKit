/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef HAVE_WEBRTC_VOICE

#include "media/engine/webrtcvoiceengine.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/call/audio_sink.h"
#include "api/media_transport_interface.h"
#include "media/base/audiosource.h"
#include "media/base/mediaconstants.h"
#include "media/base/streamparams.h"
#include "media/engine/adm_helpers.h"
#include "media/engine/apm_helpers.h"
#include "media/engine/payload_type_mapper.h"
#include "media/engine/webrtcmediaengine.h"
#include "modules/audio_device/audio_device_impl.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/byteorder.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/strings/audio_format_to_string.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/third_party/base64/base64.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace cricket {
namespace {

constexpr size_t kMaxUnsignaledRecvStreams = 4;

constexpr int kNackRtpHistoryMs = 5000;

// For SendSideBwe, Opus bitrate should be in the range between 6000 and 32000.
const int kOpusMinBitrateBps = 6000;
const int kOpusBitrateFbBps = 32000;

const int kMinTelephoneEventCode = 0;  // RFC4733 (Section 2.3.1)
const int kMaxTelephoneEventCode = 255;

const int kMinPayloadType = 0;
const int kMaxPayloadType = 127;

class ProxySink : public webrtc::AudioSinkInterface {
 public:
  explicit ProxySink(AudioSinkInterface* sink) : sink_(sink) {
    RTC_DCHECK(sink);
  }

  void OnData(const Data& audio) override { sink_->OnData(audio); }

 private:
  webrtc::AudioSinkInterface* sink_;
};

bool ValidateStreamParams(const StreamParams& sp) {
  if (sp.ssrcs.empty()) {
    RTC_DLOG(LS_ERROR) << "No SSRCs in stream parameters: " << sp.ToString();
    return false;
  }
  if (sp.ssrcs.size() > 1) {
    RTC_DLOG(LS_ERROR) << "Multiple SSRCs in stream parameters: "
                       << sp.ToString();
    return false;
  }
  return true;
}

// Dumps an AudioCodec in RFC 2327-ish format.
std::string ToString(const AudioCodec& codec) {
  rtc::StringBuilder ss;
  ss << codec.name << "/" << codec.clockrate << "/" << codec.channels;
  if (!codec.params.empty()) {
    ss << " {";
    for (const auto& param : codec.params) {
      ss << " " << param.first << "=" << param.second;
    }
    ss << " }";
  }
  ss << " (" << codec.id << ")";
  return ss.Release();
}

bool IsCodec(const AudioCodec& codec, const char* ref_name) {
  return absl::EqualsIgnoreCase(codec.name, ref_name);
}

bool FindCodec(const std::vector<AudioCodec>& codecs,
               const AudioCodec& codec,
               AudioCodec* found_codec) {
  for (const AudioCodec& c : codecs) {
    if (c.Matches(codec)) {
      if (found_codec != NULL) {
        *found_codec = c;
      }
      return true;
    }
  }
  return false;
}

bool VerifyUniquePayloadTypes(const std::vector<AudioCodec>& codecs) {
  if (codecs.empty()) {
    return true;
  }
  std::vector<int> payload_types;
  for (const AudioCodec& codec : codecs) {
    payload_types.push_back(codec.id);
  }
  std::sort(payload_types.begin(), payload_types.end());
  auto it = std::unique(payload_types.begin(), payload_types.end());
  return it == payload_types.end();
}

absl::optional<std::string> GetAudioNetworkAdaptorConfig(
    const AudioOptions& options) {
  if (options.audio_network_adaptor && *options.audio_network_adaptor &&
      options.audio_network_adaptor_config) {
    // Turn on audio network adaptor only when |options_.audio_network_adaptor|
    // equals true and |options_.audio_network_adaptor_config| has a value.
    return options.audio_network_adaptor_config;
  }
  return absl::nullopt;
}

// |max_send_bitrate_bps| is the bitrate from "b=" in SDP.
// |rtp_max_bitrate_bps| is the bitrate from RtpSender::SetParameters.
absl::optional<int> ComputeSendBitrate(int max_send_bitrate_bps,
                                       absl::optional<int> rtp_max_bitrate_bps,
                                       const webrtc::AudioCodecSpec& spec) {
  // If application-configured bitrate is set, take minimum of that and SDP
  // bitrate.
  const int bps =
      rtp_max_bitrate_bps
          ? webrtc::MinPositive(max_send_bitrate_bps, *rtp_max_bitrate_bps)
          : max_send_bitrate_bps;
  if (bps <= 0) {
    return spec.info.default_bitrate_bps;
  }

  if (bps < spec.info.min_bitrate_bps) {
    // If codec is not multi-rate and |bps| is less than the fixed bitrate then
    // fail. If codec is not multi-rate and |bps| exceeds or equal the fixed
    // bitrate then ignore.
    RTC_LOG(LS_ERROR) << "Failed to set codec " << spec.format.name
                      << " to bitrate " << bps << " bps"
                      << ", requires at least " << spec.info.min_bitrate_bps
                      << " bps.";
    return absl::nullopt;
  }

  if (spec.info.HasFixedBitrate()) {
    return spec.info.default_bitrate_bps;
  } else {
    // If codec is multi-rate then just set the bitrate.
    return std::min(bps, spec.info.max_bitrate_bps);
  }
}

}  // namespace

WebRtcVoiceEngine::WebRtcVoiceEngine(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>& encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
    rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing)
    : adm_(adm),
      encoder_factory_(encoder_factory),
      decoder_factory_(decoder_factory),
      audio_mixer_(audio_mixer),
      apm_(audio_processing) {
  // This may be called from any thread, so detach thread checkers.
  worker_thread_checker_.DetachFromThread();
  signal_thread_checker_.DetachFromThread();
  RTC_LOG(LS_INFO) << "WebRtcVoiceEngine::WebRtcVoiceEngine";
  RTC_DCHECK(decoder_factory);
  RTC_DCHECK(encoder_factory);
  RTC_DCHECK(audio_processing);
  // The rest of our initialization will happen in Init.
}

WebRtcVoiceEngine::~WebRtcVoiceEngine() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "WebRtcVoiceEngine::~WebRtcVoiceEngine";
  if (initialized_) {
    StopAecDump();

    // Stop AudioDevice.
    adm()->StopPlayout();
    adm()->StopRecording();
    adm()->RegisterAudioCallback(nullptr);
    adm()->Terminate();
  }
}

void WebRtcVoiceEngine::Init() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "WebRtcVoiceEngine::Init";

  // TaskQueue expects to be created/destroyed on the same thread.
  low_priority_worker_queue_.reset(
      new rtc::TaskQueue("rtc-low-prio", rtc::TaskQueue::Priority::LOW));

  // Load our audio codec lists.
  RTC_LOG(LS_INFO) << "Supported send codecs in order of preference:";
  send_codecs_ = CollectCodecs(encoder_factory_->GetSupportedEncoders());
  for (const AudioCodec& codec : send_codecs_) {
    RTC_LOG(LS_INFO) << ToString(codec);
  }

  RTC_LOG(LS_INFO) << "Supported recv codecs in order of preference:";
  recv_codecs_ = CollectCodecs(decoder_factory_->GetSupportedDecoders());
  for (const AudioCodec& codec : recv_codecs_) {
    RTC_LOG(LS_INFO) << ToString(codec);
  }

#if defined(WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE)
  // No ADM supplied? Create a default one.
  if (!adm_) {
    adm_ = webrtc::AudioDeviceModule::Create(
        webrtc::AudioDeviceModule::kPlatformDefaultAudio);
  }
#endif  // WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE
  RTC_CHECK(adm());
  webrtc::adm_helpers::Init(adm());
  webrtc::apm_helpers::Init(apm());

  // Set up AudioState.
  {
    webrtc::AudioState::Config config;
    if (audio_mixer_) {
      config.audio_mixer = audio_mixer_;
    } else {
      config.audio_mixer = webrtc::AudioMixerImpl::Create();
    }
    config.audio_processing = apm_;
    config.audio_device_module = adm_;
    audio_state_ = webrtc::AudioState::Create(config);
  }

  // Connect the ADM to our audio path.
  adm()->RegisterAudioCallback(audio_state()->audio_transport());

  // Save the default AGC configuration settings. This must happen before
  // calling ApplyOptions or the default will be overwritten.
  default_agc_config_ = webrtc::apm_helpers::GetAgcConfig(apm());

  // Set default engine options.
  {
    AudioOptions options;
    options.echo_cancellation = true;
    options.auto_gain_control = true;
    options.noise_suppression = true;
    options.highpass_filter = true;
    options.stereo_swapping = false;
    options.audio_jitter_buffer_max_packets = 50;
    options.audio_jitter_buffer_fast_accelerate = false;
    options.audio_jitter_buffer_min_delay_ms = 0;
    options.typing_detection = true;
    options.experimental_agc = false;
    options.extended_filter_aec = false;
    options.delay_agnostic_aec = false;
    options.experimental_ns = false;
    options.residual_echo_detector = true;
    bool error = ApplyOptions(options);
    RTC_DCHECK(error);
  }

  initialized_ = true;
}

rtc::scoped_refptr<webrtc::AudioState> WebRtcVoiceEngine::GetAudioState()
    const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return audio_state_;
}

VoiceMediaChannel* WebRtcVoiceEngine::CreateMediaChannel(
    webrtc::Call* call,
    const MediaConfig& config,
    const AudioOptions& options,
    const webrtc::CryptoOptions& crypto_options) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return new WebRtcVoiceMediaChannel(this, config, options, crypto_options,
                                     call);
}

bool WebRtcVoiceEngine::ApplyOptions(const AudioOptions& options_in) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "WebRtcVoiceEngine::ApplyOptions: "
                   << options_in.ToString();
  AudioOptions options = options_in;  // The options are modified below.

  // Set and adjust echo canceller options.
  // kEcConference is AEC with high suppression.
  webrtc::EcModes ec_mode = webrtc::kEcConference;

#if defined(WEBRTC_IOS)
  if (options.ios_force_software_aec_HACK &&
      *options.ios_force_software_aec_HACK) {
    // EC may be forced on for a device known to have non-functioning platform
    // AEC.
    options.echo_cancellation = true;
    options.extended_filter_aec = true;
    RTC_LOG(LS_WARNING)
        << "Force software AEC on iOS. May conflict with platform AEC.";
  } else {
    // On iOS, VPIO provides built-in EC.
    options.echo_cancellation = false;
    options.extended_filter_aec = false;
    RTC_LOG(LS_INFO) << "Always disable AEC on iOS. Use built-in instead.";
  }
#elif defined(WEBRTC_ANDROID)
  ec_mode = webrtc::kEcAecm;
  options.extended_filter_aec = false;
#endif

  // Delay Agnostic AEC automatically turns on EC if not set except on iOS
  // where the feature is not supported.
  bool use_delay_agnostic_aec = false;
#if !defined(WEBRTC_IOS)
  if (options.delay_agnostic_aec) {
    use_delay_agnostic_aec = *options.delay_agnostic_aec;
    if (use_delay_agnostic_aec) {
      options.echo_cancellation = true;
      options.extended_filter_aec = true;
      ec_mode = webrtc::kEcConference;
    }
  }
#endif

// Set and adjust noise suppressor options.
#if defined(WEBRTC_IOS)
  // On iOS, VPIO provides built-in NS.
  options.noise_suppression = false;
  options.typing_detection = false;
  options.experimental_ns = false;
  RTC_LOG(LS_INFO) << "Always disable NS on iOS. Use built-in instead.";
#elif defined(WEBRTC_ANDROID)
  options.typing_detection = false;
  options.experimental_ns = false;
#endif

// Set and adjust gain control options.
#if defined(WEBRTC_IOS)
  // On iOS, VPIO provides built-in AGC.
  options.auto_gain_control = false;
  options.experimental_agc = false;
  RTC_LOG(LS_INFO) << "Always disable AGC on iOS. Use built-in instead.";
#elif defined(WEBRTC_ANDROID)
  options.experimental_agc = false;
#endif

#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
  // Turn off the gain control if specified by the field trial.
  // The purpose of the field trial is to reduce the amount of resampling
  // performed inside the audio processing module on mobile platforms by
  // whenever possible turning off the fixed AGC mode and the high-pass filter.
  // (https://bugs.chromium.org/p/webrtc/issues/detail?id=6181).
  if (webrtc::field_trial::IsEnabled(
          "WebRTC-Audio-MinimizeResamplingOnMobile")) {
    options.auto_gain_control = false;
    RTC_LOG(LS_INFO) << "Disable AGC according to field trial.";
    if (!(options.noise_suppression.value_or(false) ||
          options.echo_cancellation.value_or(false))) {
      // If possible, turn off the high-pass filter.
      RTC_LOG(LS_INFO)
          << "Disable high-pass filter in response to field trial.";
      options.highpass_filter = false;
    }
  }
#endif

  if (options.echo_cancellation) {
    // Check if platform supports built-in EC. Currently only supported on
    // Android and in combination with Java based audio layer.
    // TODO(henrika): investigate possibility to support built-in EC also
    // in combination with Open SL ES audio.
    const bool built_in_aec = adm()->BuiltInAECIsAvailable();
    if (built_in_aec) {
      // Built-in EC exists on this device and use_delay_agnostic_aec is not
      // overriding it. Enable/Disable it according to the echo_cancellation
      // audio option.
      const bool enable_built_in_aec =
          *options.echo_cancellation && !use_delay_agnostic_aec;
      if (adm()->EnableBuiltInAEC(enable_built_in_aec) == 0 &&
          enable_built_in_aec) {
        // Disable internal software EC if built-in EC is enabled,
        // i.e., replace the software EC with the built-in EC.
        options.echo_cancellation = false;
        RTC_LOG(LS_INFO)
            << "Disabling EC since built-in EC will be used instead";
      }
    }
    webrtc::apm_helpers::SetEcStatus(apm(), *options.echo_cancellation,
                                     ec_mode);
  }

  if (options.auto_gain_control) {
    bool built_in_agc_avaliable = adm()->BuiltInAGCIsAvailable();
    if (built_in_agc_avaliable) {
      if (adm()->EnableBuiltInAGC(*options.auto_gain_control) == 0 &&
          *options.auto_gain_control) {
        // Disable internal software AGC if built-in AGC is enabled,
        // i.e., replace the software AGC with the built-in AGC.
        options.auto_gain_control = false;
        RTC_LOG(LS_INFO)
            << "Disabling AGC since built-in AGC will be used instead";
      }
    }
    webrtc::apm_helpers::SetAgcStatus(apm(), *options.auto_gain_control);
  }

  if (options.tx_agc_target_dbov || options.tx_agc_digital_compression_gain ||
      options.tx_agc_limiter) {
    // Override default_agc_config_. Generally, an unset option means "leave
    // the VoE bits alone" in this function, so we want whatever is set to be
    // stored as the new "default". If we didn't, then setting e.g.
    // tx_agc_target_dbov would reset digital compression gain and limiter
    // settings.
    default_agc_config_.targetLeveldBOv = options.tx_agc_target_dbov.value_or(
        default_agc_config_.targetLeveldBOv);
    default_agc_config_.digitalCompressionGaindB =
        options.tx_agc_digital_compression_gain.value_or(
            default_agc_config_.digitalCompressionGaindB);
    default_agc_config_.limiterEnable =
        options.tx_agc_limiter.value_or(default_agc_config_.limiterEnable);
    webrtc::apm_helpers::SetAgcConfig(apm(), default_agc_config_);
  }

  if (options.noise_suppression) {
    if (adm()->BuiltInNSIsAvailable()) {
      bool builtin_ns = *options.noise_suppression;
      if (adm()->EnableBuiltInNS(builtin_ns) == 0 && builtin_ns) {
        // Disable internal software NS if built-in NS is enabled,
        // i.e., replace the software NS with the built-in NS.
        options.noise_suppression = false;
        RTC_LOG(LS_INFO)
            << "Disabling NS since built-in NS will be used instead";
      }
    }
    webrtc::apm_helpers::SetNsStatus(apm(), *options.noise_suppression);
  }

  if (options.stereo_swapping) {
    RTC_LOG(LS_INFO) << "Stereo swapping enabled? " << *options.stereo_swapping;
    audio_state()->SetStereoChannelSwapping(*options.stereo_swapping);
  }

  if (options.audio_jitter_buffer_max_packets) {
    RTC_LOG(LS_INFO) << "NetEq capacity is "
                     << *options.audio_jitter_buffer_max_packets;
    audio_jitter_buffer_max_packets_ =
        std::max(20, *options.audio_jitter_buffer_max_packets);
  }
  if (options.audio_jitter_buffer_fast_accelerate) {
    RTC_LOG(LS_INFO) << "NetEq fast mode? "
                     << *options.audio_jitter_buffer_fast_accelerate;
    audio_jitter_buffer_fast_accelerate_ =
        *options.audio_jitter_buffer_fast_accelerate;
  }
  if (options.audio_jitter_buffer_min_delay_ms) {
    RTC_LOG(LS_INFO) << "NetEq minimum delay is "
                     << *options.audio_jitter_buffer_min_delay_ms;
    audio_jitter_buffer_min_delay_ms_ =
        *options.audio_jitter_buffer_min_delay_ms;
  }

  if (options.typing_detection) {
    RTC_LOG(LS_INFO) << "Typing detection is enabled? "
                     << *options.typing_detection;
    webrtc::apm_helpers::SetTypingDetectionStatus(apm(),
                                                  *options.typing_detection);
  }

  webrtc::Config config;

  if (options.delay_agnostic_aec)
    delay_agnostic_aec_ = options.delay_agnostic_aec;
  if (delay_agnostic_aec_) {
    RTC_LOG(LS_INFO) << "Delay agnostic aec is enabled? "
                     << *delay_agnostic_aec_;
    config.Set<webrtc::DelayAgnostic>(
        new webrtc::DelayAgnostic(*delay_agnostic_aec_));
  }

  if (options.extended_filter_aec) {
    extended_filter_aec_ = options.extended_filter_aec;
  }
  if (extended_filter_aec_) {
    RTC_LOG(LS_INFO) << "Extended filter aec is enabled? "
                     << *extended_filter_aec_;
    config.Set<webrtc::ExtendedFilter>(
        new webrtc::ExtendedFilter(*extended_filter_aec_));
  }

  if (options.experimental_ns) {
    experimental_ns_ = options.experimental_ns;
  }
  if (experimental_ns_) {
    RTC_LOG(LS_INFO) << "Experimental ns is enabled? " << *experimental_ns_;
    config.Set<webrtc::ExperimentalNs>(
        new webrtc::ExperimentalNs(*experimental_ns_));
  }

  webrtc::AudioProcessing::Config apm_config = apm()->GetConfig();

  if (options.highpass_filter) {
    apm_config.high_pass_filter.enabled = *options.highpass_filter;
  }

  if (options.residual_echo_detector) {
    apm_config.residual_echo_detector.enabled = *options.residual_echo_detector;
  }

  apm()->SetExtraOptions(config);
  apm()->ApplyConfig(apm_config);
  return true;
}

const std::vector<AudioCodec>& WebRtcVoiceEngine::send_codecs() const {
  RTC_DCHECK(signal_thread_checker_.CalledOnValidThread());
  return send_codecs_;
}

const std::vector<AudioCodec>& WebRtcVoiceEngine::recv_codecs() const {
  RTC_DCHECK(signal_thread_checker_.CalledOnValidThread());
  return recv_codecs_;
}

RtpCapabilities WebRtcVoiceEngine::GetCapabilities() const {
  RTC_DCHECK(signal_thread_checker_.CalledOnValidThread());
  RtpCapabilities capabilities;
  capabilities.header_extensions.push_back(
      webrtc::RtpExtension(webrtc::RtpExtension::kAudioLevelUri,
                           webrtc::RtpExtension::kAudioLevelDefaultId));
  if (webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe") &&
      !webrtc::field_trial::IsEnabled("WebRTC-Audio-ABWENoTWCC")) {
    capabilities.header_extensions.push_back(webrtc::RtpExtension(
        webrtc::RtpExtension::kTransportSequenceNumberUri,
        webrtc::RtpExtension::kTransportSequenceNumberDefaultId));
  }
  // TODO(bugs.webrtc.org/4050): Add MID header extension as capability once MID
  // demuxing is completed.
  // capabilities.header_extensions.push_back(webrtc::RtpExtension(
  //     webrtc::RtpExtension::kMidUri, webrtc::RtpExtension::kMidDefaultId));
  return capabilities;
}

void WebRtcVoiceEngine::RegisterChannel(WebRtcVoiceMediaChannel* channel) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(channel);
  channels_.push_back(channel);
}

void WebRtcVoiceEngine::UnregisterChannel(WebRtcVoiceMediaChannel* channel) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto it = std::find(channels_.begin(), channels_.end(), channel);
  RTC_DCHECK(it != channels_.end());
  channels_.erase(it);
}

bool WebRtcVoiceEngine::StartAecDump(rtc::PlatformFile file,
                                     int64_t max_size_bytes) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto aec_dump = webrtc::AecDumpFactory::Create(
      file, max_size_bytes, low_priority_worker_queue_.get());
  if (!aec_dump) {
    return false;
  }
  apm()->AttachAecDump(std::move(aec_dump));
  return true;
}

void WebRtcVoiceEngine::StartAecDump(const std::string& filename) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());

  auto aec_dump = webrtc::AecDumpFactory::Create(
      filename, -1, low_priority_worker_queue_.get());
  if (aec_dump) {
    apm()->AttachAecDump(std::move(aec_dump));
  }
}

void WebRtcVoiceEngine::StopAecDump() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  apm()->DetachAecDump();
}

webrtc::AudioDeviceModule* WebRtcVoiceEngine::adm() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(adm_);
  return adm_.get();
}

webrtc::AudioProcessing* WebRtcVoiceEngine::apm() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(apm_);
  return apm_.get();
}

webrtc::AudioState* WebRtcVoiceEngine::audio_state() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(audio_state_);
  return audio_state_.get();
}

AudioCodecs WebRtcVoiceEngine::CollectCodecs(
    const std::vector<webrtc::AudioCodecSpec>& specs) const {
  PayloadTypeMapper mapper;
  AudioCodecs out;

  // Only generate CN payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_cn = {
      {8000, false}, {16000, false}, {32000, false}};
  // Only generate telephone-event payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_dtmf = {
      {8000, false}, {16000, false}, {32000, false}, {48000, false}};

  auto map_format = [&mapper](const webrtc::SdpAudioFormat& format,
                              AudioCodecs* out) {
    absl::optional<AudioCodec> opt_codec = mapper.ToAudioCodec(format);
    if (opt_codec) {
      if (out) {
        out->push_back(*opt_codec);
      }
    } else {
      RTC_LOG(LS_ERROR) << "Unable to assign payload type to format: "
                        << rtc::ToString(format);
    }

    return opt_codec;
  };

  for (const auto& spec : specs) {
    // We need to do some extra stuff before adding the main codecs to out.
    absl::optional<AudioCodec> opt_codec = map_format(spec.format, nullptr);
    if (opt_codec) {
      AudioCodec& codec = *opt_codec;
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
    }
  }

  // Add CN codecs after "proper" audio codecs.
  for (const auto& cn : generate_cn) {
    if (cn.second) {
      map_format({kCnCodecName, cn.first, 1}, &out);
    }
  }

  // Add telephone-event codecs last.
  for (const auto& dtmf : generate_dtmf) {
    if (dtmf.second) {
      map_format({kDtmfCodecName, dtmf.first, 1}, &out);
    }
  }

  return out;
}

class WebRtcVoiceMediaChannel::WebRtcAudioSendStream
    : public AudioSource::Sink {
 public:
  WebRtcAudioSendStream(
      uint32_t ssrc,
      const std::string& mid,
      const std::string& c_name,
      const std::string track_id,
      const absl::optional<webrtc::AudioSendStream::Config::SendCodecSpec>&
          send_codec_spec,
      bool extmap_allow_mixed,
      const std::vector<webrtc::RtpExtension>& extensions,
      int max_send_bitrate_bps,
      int rtcp_report_interval_ms,
      const absl::optional<std::string>& audio_network_adaptor_config,
      webrtc::Call* call,
      webrtc::Transport* send_transport,
      webrtc::MediaTransportInterface* media_transport,
      const rtc::scoped_refptr<webrtc::AudioEncoderFactory>& encoder_factory,
      const absl::optional<webrtc::AudioCodecPairId> codec_pair_id,
      rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor,
      const webrtc::CryptoOptions& crypto_options)
      : call_(call),
        config_(send_transport, media_transport),
        send_side_bwe_with_overhead_(
            webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
        max_send_bitrate_bps_(max_send_bitrate_bps),
        rtp_parameters_(CreateRtpParametersWithOneEncoding()) {
    RTC_DCHECK(call);
    RTC_DCHECK(encoder_factory);
    config_.rtp.ssrc = ssrc;
    config_.rtp.mid = mid;
    config_.rtp.c_name = c_name;
    config_.rtp.extmap_allow_mixed = extmap_allow_mixed;
    config_.rtp.extensions = extensions;
    config_.has_dscp = rtp_parameters_.encodings[0].network_priority !=
                       webrtc::kDefaultBitratePriority;
    config_.audio_network_adaptor_config = audio_network_adaptor_config;
    config_.encoder_factory = encoder_factory;
    config_.codec_pair_id = codec_pair_id;
    config_.track_id = track_id;
    config_.frame_encryptor = frame_encryptor;
    config_.crypto_options = crypto_options;
    config_.rtcp_report_interval_ms = rtcp_report_interval_ms;
    rtp_parameters_.encodings[0].ssrc = ssrc;
    rtp_parameters_.rtcp.cname = c_name;
    rtp_parameters_.header_extensions = extensions;

    if (send_codec_spec) {
      UpdateSendCodecSpec(*send_codec_spec);
    }

    stream_ = call_->CreateAudioSendStream(config_);
  }

  ~WebRtcAudioSendStream() override {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    ClearSource();
    call_->DestroyAudioSendStream(stream_);
  }

  void SetSendCodecSpec(
      const webrtc::AudioSendStream::Config::SendCodecSpec& send_codec_spec) {
    UpdateSendCodecSpec(send_codec_spec);
    ReconfigureAudioSendStream();
  }

  void SetRtpExtensions(const std::vector<webrtc::RtpExtension>& extensions) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.rtp.extensions = extensions;
    rtp_parameters_.header_extensions = extensions;
    ReconfigureAudioSendStream();
  }

  void SetExtmapAllowMixed(bool extmap_allow_mixed) {
    config_.rtp.extmap_allow_mixed = extmap_allow_mixed;
    ReconfigureAudioSendStream();
  }

  void SetMid(const std::string& mid) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    if (config_.rtp.mid == mid) {
      return;
    }
    config_.rtp.mid = mid;
    ReconfigureAudioSendStream();
  }

  void SetFrameEncryptor(
      rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.frame_encryptor = frame_encryptor;
    ReconfigureAudioSendStream();
  }

  void SetAudioNetworkAdaptorConfig(
      const absl::optional<std::string>& audio_network_adaptor_config) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    if (config_.audio_network_adaptor_config == audio_network_adaptor_config) {
      return;
    }
    config_.audio_network_adaptor_config = audio_network_adaptor_config;
    UpdateAllowedBitrateRange();
    ReconfigureAudioSendStream();
  }

  bool SetMaxSendBitrate(int bps) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(config_.send_codec_spec);
    RTC_DCHECK(audio_codec_spec_);
    auto send_rate = ComputeSendBitrate(
        bps, rtp_parameters_.encodings[0].max_bitrate_bps, *audio_codec_spec_);

    if (!send_rate) {
      return false;
    }

    max_send_bitrate_bps_ = bps;

    if (send_rate != config_.send_codec_spec->target_bitrate_bps) {
      config_.send_codec_spec->target_bitrate_bps = send_rate;
      ReconfigureAudioSendStream();
    }
    return true;
  }

  bool SendTelephoneEvent(int payload_type,
                          int payload_freq,
                          int event,
                          int duration_ms) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    return stream_->SendTelephoneEvent(payload_type, payload_freq, event,
                                       duration_ms);
  }

  void SetSend(bool send) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    send_ = send;
    UpdateSendState();
  }

  void SetMuted(bool muted) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    stream_->SetMuted(muted);
    muted_ = muted;
  }

  bool muted() const {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    return muted_;
  }

  webrtc::AudioSendStream::Stats GetStats(bool has_remote_tracks) const {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    return stream_->GetStats(has_remote_tracks);
  }

  // Starts the sending by setting ourselves as a sink to the AudioSource to
  // get data callbacks.
  // This method is called on the libjingle worker thread.
  // TODO(xians): Make sure Start() is called only once.
  void SetSource(AudioSource* source) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(source);
    if (source_) {
      RTC_DCHECK(source_ == source);
      return;
    }
    source->SetSink(this);
    source_ = source;
    UpdateSendState();
  }

  // Stops sending by setting the sink of the AudioSource to nullptr. No data
  // callback will be received after this method.
  // This method is called on the libjingle worker thread.
  void ClearSource() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    if (source_) {
      source_->SetSink(nullptr);
      source_ = nullptr;
    }
    UpdateSendState();
  }

  // AudioSource::Sink implementation.
  // This method is called on the audio thread.
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override {
    RTC_DCHECK_EQ(16, bits_per_sample);
    RTC_CHECK_RUNS_SERIALIZED(&audio_capture_race_checker_);
    RTC_DCHECK(stream_);
    std::unique_ptr<webrtc::AudioFrame> audio_frame(new webrtc::AudioFrame());
    audio_frame->UpdateFrame(
        audio_frame->timestamp_, static_cast<const int16_t*>(audio_data),
        number_of_frames, sample_rate, audio_frame->speech_type_,
        audio_frame->vad_activity_, number_of_channels);
    stream_->SendAudioData(std::move(audio_frame));
  }

  // Callback from the |source_| when it is going away. In case Start() has
  // never been called, this callback won't be triggered.
  void OnClose() override {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    // Set |source_| to nullptr to make sure no more callback will get into
    // the source.
    source_ = nullptr;
    UpdateSendState();
  }

  const webrtc::RtpParameters& rtp_parameters() const {
    return rtp_parameters_;
  }

  webrtc::RTCError SetRtpParameters(const webrtc::RtpParameters& parameters) {
    webrtc::RTCError error = ValidateRtpParameters(rtp_parameters_, parameters);
    if (!error.ok()) {
      return error;
    }

    absl::optional<int> send_rate;
    if (audio_codec_spec_) {
      send_rate = ComputeSendBitrate(max_send_bitrate_bps_,
                                     parameters.encodings[0].max_bitrate_bps,
                                     *audio_codec_spec_);
      if (!send_rate) {
        return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR);
      }
    }

    const absl::optional<int> old_rtp_max_bitrate =
        rtp_parameters_.encodings[0].max_bitrate_bps;
    double old_priority = rtp_parameters_.encodings[0].bitrate_priority;
    double old_dscp = rtp_parameters_.encodings[0].network_priority;
    rtp_parameters_ = parameters;
    config_.bitrate_priority = rtp_parameters_.encodings[0].bitrate_priority;
    config_.has_dscp = (rtp_parameters_.encodings[0].network_priority !=
                        webrtc::kDefaultBitratePriority);

    bool reconfigure_send_stream =
        (rtp_parameters_.encodings[0].max_bitrate_bps != old_rtp_max_bitrate) ||
        (rtp_parameters_.encodings[0].bitrate_priority != old_priority) ||
        (rtp_parameters_.encodings[0].network_priority != old_dscp);
    if (rtp_parameters_.encodings[0].max_bitrate_bps != old_rtp_max_bitrate) {
      // Update the bitrate range.
      if (send_rate) {
        config_.send_codec_spec->target_bitrate_bps = send_rate;
      }
      UpdateAllowedBitrateRange();
    }
    if (reconfigure_send_stream) {
      ReconfigureAudioSendStream();
    }

    rtp_parameters_.rtcp.cname = config_.rtp.c_name;
    rtp_parameters_.rtcp.reduced_size = false;

    // parameters.encodings[0].active could have changed.
    UpdateSendState();
    return webrtc::RTCError::OK();
  }

 private:
  void UpdateSendState() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    RTC_DCHECK_EQ(1UL, rtp_parameters_.encodings.size());
    if (send_ && source_ != nullptr && rtp_parameters_.encodings[0].active) {
      stream_->Start();
    } else {  // !send || source_ = nullptr
      stream_->Stop();
    }
  }

  void UpdateAllowedBitrateRange() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    const bool is_opus =
        config_.send_codec_spec &&
        absl::EqualsIgnoreCase(config_.send_codec_spec->format.name,
                               kOpusCodecName);
    if (is_opus && webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe")) {
      config_.min_bitrate_bps = kOpusMinBitrateBps;

      // This means that when RtpParameters is reset, we may change the
      // encoder's bit rate immediately (through ReconfigureAudioSendStream()),
      // meanwhile change the cap to the output of BWE.
      config_.max_bitrate_bps =
          rtp_parameters_.encodings[0].max_bitrate_bps
              ? *rtp_parameters_.encodings[0].max_bitrate_bps
              : kOpusBitrateFbBps;

      // TODO(mflodman): Keep testing this and set proper values.
      // Note: This is an early experiment currently only supported by Opus.
      if (send_side_bwe_with_overhead_) {
        const int max_packet_size_ms =
            WEBRTC_OPUS_SUPPORT_120MS_PTIME ? 120 : 60;

        // OverheadPerPacket = Ipv4(20B) + UDP(8B) + SRTP(10B) + RTP(12)
        constexpr int kOverheadPerPacket = 20 + 8 + 10 + 12;

        int min_overhead_bps =
            kOverheadPerPacket * 8 * 1000 / max_packet_size_ms;

        // We assume that |config_.max_bitrate_bps| before the next line is
        // a hard limit on the payload bitrate, so we add min_overhead_bps to
        // it to ensure that, when overhead is deducted, the payload rate
        // never goes beyond the limit.
        // Note: this also means that if a higher overhead is forced, we
        // cannot reach the limit.
        // TODO(minyue): Reconsider this when the signaling to BWE is done
        // through a dedicated API.
        config_.max_bitrate_bps += min_overhead_bps;

        // In contrast to max_bitrate_bps, we let min_bitrate_bps always be
        // reachable.
        config_.min_bitrate_bps += min_overhead_bps;
      }
    }
  }

  void UpdateSendCodecSpec(
      const webrtc::AudioSendStream::Config::SendCodecSpec& send_codec_spec) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.send_codec_spec = send_codec_spec;
    auto info =
        config_.encoder_factory->QueryAudioEncoder(send_codec_spec.format);
    RTC_DCHECK(info);
    // If a specific target bitrate has been set for the stream, use that as
    // the new default bitrate when computing send bitrate.
    if (send_codec_spec.target_bitrate_bps) {
      info->default_bitrate_bps = std::max(
          info->min_bitrate_bps,
          std::min(info->max_bitrate_bps, *send_codec_spec.target_bitrate_bps));
    }

    audio_codec_spec_.emplace(
        webrtc::AudioCodecSpec{send_codec_spec.format, *info});

    config_.send_codec_spec->target_bitrate_bps = ComputeSendBitrate(
        max_send_bitrate_bps_, rtp_parameters_.encodings[0].max_bitrate_bps,
        *audio_codec_spec_);

    UpdateAllowedBitrateRange();
  }

  void ReconfigureAudioSendStream() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    stream_->Reconfigure(config_);
  }

  rtc::ThreadChecker worker_thread_checker_;
  rtc::RaceChecker audio_capture_race_checker_;
  webrtc::Call* call_ = nullptr;
  webrtc::AudioSendStream::Config config_;
  const bool send_side_bwe_with_overhead_;
  // The stream is owned by WebRtcAudioSendStream and may be reallocated if
  // configuration changes.
  webrtc::AudioSendStream* stream_ = nullptr;

  // Raw pointer to AudioSource owned by LocalAudioTrackHandler.
  // PeerConnection will make sure invalidating the pointer before the object
  // goes away.
  AudioSource* source_ = nullptr;
  bool send_ = false;
  bool muted_ = false;
  int max_send_bitrate_bps_;
  webrtc::RtpParameters rtp_parameters_;
  absl::optional<webrtc::AudioCodecSpec> audio_codec_spec_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcAudioSendStream);
};

class WebRtcVoiceMediaChannel::WebRtcAudioReceiveStream {
 public:
  WebRtcAudioReceiveStream(
      uint32_t remote_ssrc,
      uint32_t local_ssrc,
      bool use_transport_cc,
      bool use_nack,
      const std::vector<std::string>& stream_ids,
      const std::vector<webrtc::RtpExtension>& extensions,
      webrtc::Call* call,
      webrtc::Transport* rtcp_send_transport,
      webrtc::MediaTransportInterface* media_transport,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
      const std::map<int, webrtc::SdpAudioFormat>& decoder_map,
      absl::optional<webrtc::AudioCodecPairId> codec_pair_id,
      size_t jitter_buffer_max_packets,
      bool jitter_buffer_fast_accelerate,
      int jitter_buffer_min_delay_ms,
      rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor,
      const webrtc::CryptoOptions& crypto_options)
      : call_(call), config_() {
    RTC_DCHECK(call);
    config_.rtp.remote_ssrc = remote_ssrc;
    config_.rtp.local_ssrc = local_ssrc;
    config_.rtp.transport_cc = use_transport_cc;
    config_.rtp.nack.rtp_history_ms = use_nack ? kNackRtpHistoryMs : 0;
    config_.rtp.extensions = extensions;
    config_.rtcp_send_transport = rtcp_send_transport;
    config_.media_transport = media_transport;
    config_.jitter_buffer_max_packets = jitter_buffer_max_packets;
    config_.jitter_buffer_fast_accelerate = jitter_buffer_fast_accelerate;
    config_.jitter_buffer_min_delay_ms = jitter_buffer_min_delay_ms;
    if (!stream_ids.empty()) {
      config_.sync_group = stream_ids[0];
    }
    config_.decoder_factory = decoder_factory;
    config_.decoder_map = decoder_map;
    config_.codec_pair_id = codec_pair_id;
    config_.frame_decryptor = frame_decryptor;
    config_.crypto_options = crypto_options;
    RecreateAudioReceiveStream();
  }

  ~WebRtcAudioReceiveStream() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    call_->DestroyAudioReceiveStream(stream_);
  }

  void SetFrameDecryptor(
      rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.frame_decryptor = frame_decryptor;
    RecreateAudioReceiveStream();
  }

  void SetLocalSsrc(uint32_t local_ssrc) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.rtp.local_ssrc = local_ssrc;
    ReconfigureAudioReceiveStream();
  }

  void SetUseTransportCcAndRecreateStream(bool use_transport_cc,
                                          bool use_nack) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.rtp.transport_cc = use_transport_cc;
    config_.rtp.nack.rtp_history_ms = use_nack ? kNackRtpHistoryMs : 0;
    ReconfigureAudioReceiveStream();
  }

  void SetRtpExtensionsAndRecreateStream(
      const std::vector<webrtc::RtpExtension>& extensions) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.rtp.extensions = extensions;
    RecreateAudioReceiveStream();
  }

  // Set a new payload type -> decoder map.
  void SetDecoderMap(const std::map<int, webrtc::SdpAudioFormat>& decoder_map) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.decoder_map = decoder_map;
    ReconfigureAudioReceiveStream();
  }

  void MaybeRecreateAudioReceiveStream(
      const std::vector<std::string>& stream_ids) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    std::string sync_group;
    if (!stream_ids.empty()) {
      sync_group = stream_ids[0];
    }
    if (config_.sync_group != sync_group) {
      RTC_LOG(LS_INFO) << "Recreating AudioReceiveStream for SSRC="
                       << config_.rtp.remote_ssrc
                       << " because of sync group change.";
      config_.sync_group = sync_group;
      RecreateAudioReceiveStream();
    }
  }

  webrtc::AudioReceiveStream::Stats GetStats() const {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    return stream_->GetStats();
  }

  void SetRawAudioSink(std::unique_ptr<webrtc::AudioSinkInterface> sink) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    // Need to update the stream's sink first; once raw_audio_sink_ is
    // reassigned, whatever was in there before is destroyed.
    stream_->SetSink(sink.get());
    raw_audio_sink_ = std::move(sink);
  }

  void SetOutputVolume(double volume) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    output_volume_ = volume;
    stream_->SetGain(volume);
  }

  void SetPlayout(bool playout) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    if (playout) {
      stream_->Start();
    } else {
      stream_->Stop();
    }
    playout_ = playout;
  }

  std::vector<webrtc::RtpSource> GetSources() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    return stream_->GetSources();
  }

  webrtc::RtpParameters GetRtpParameters() const {
    webrtc::RtpParameters rtp_parameters;
    rtp_parameters.encodings.emplace_back();
    rtp_parameters.encodings[0].ssrc = config_.rtp.remote_ssrc;
    rtp_parameters.header_extensions = config_.rtp.extensions;

    return rtp_parameters;
  }

 private:
  void RecreateAudioReceiveStream() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    if (stream_) {
      call_->DestroyAudioReceiveStream(stream_);
    }
    stream_ = call_->CreateAudioReceiveStream(config_);
    RTC_CHECK(stream_);
    stream_->SetGain(output_volume_);
    SetPlayout(playout_);
    stream_->SetSink(raw_audio_sink_.get());
  }

  void ReconfigureAudioReceiveStream() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    stream_->Reconfigure(config_);
  }

  rtc::ThreadChecker worker_thread_checker_;
  webrtc::Call* call_ = nullptr;
  webrtc::AudioReceiveStream::Config config_;
  // The stream is owned by WebRtcAudioReceiveStream and may be reallocated if
  // configuration changes.
  webrtc::AudioReceiveStream* stream_ = nullptr;
  bool playout_ = false;
  float output_volume_ = 1.0;
  std::unique_ptr<webrtc::AudioSinkInterface> raw_audio_sink_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcAudioReceiveStream);
};

WebRtcVoiceMediaChannel::WebRtcVoiceMediaChannel(
    WebRtcVoiceEngine* engine,
    const MediaConfig& config,
    const AudioOptions& options,
    const webrtc::CryptoOptions& crypto_options,
    webrtc::Call* call)
    : VoiceMediaChannel(config),
      engine_(engine),
      call_(call),
      audio_config_(config.audio),
      crypto_options_(crypto_options) {
  RTC_LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::WebRtcVoiceMediaChannel";
  RTC_DCHECK(call);
  engine->RegisterChannel(this);
  SetOptions(options);
}

WebRtcVoiceMediaChannel::~WebRtcVoiceMediaChannel() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::~WebRtcVoiceMediaChannel";
  // TODO(solenberg): Should be able to delete the streams directly, without
  //                  going through RemoveNnStream(), once stream objects handle
  //                  all (de)configuration.
  while (!send_streams_.empty()) {
    RemoveSendStream(send_streams_.begin()->first);
  }
  while (!recv_streams_.empty()) {
    RemoveRecvStream(recv_streams_.begin()->first);
  }
  engine()->UnregisterChannel(this);
}

rtc::DiffServCodePoint WebRtcVoiceMediaChannel::PreferredDscp() const {
  return preferred_dscp_;
}

bool WebRtcVoiceMediaChannel::SetSendParameters(
    const AudioSendParameters& params) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::SetSendParameters");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "WebRtcVoiceMediaChannel::SetSendParameters: "
                   << params.ToString();
  // TODO(pthatcher): Refactor this to be more clean now that we have
  // all the information at once.

  if (!SetSendCodecs(params.codecs)) {
    return false;
  }

  if (!ValidateRtpExtensions(params.extensions)) {
    return false;
  }

  if (ExtmapAllowMixed() != params.extmap_allow_mixed) {
    SetExtmapAllowMixed(params.extmap_allow_mixed);
    for (auto& it : send_streams_) {
      it.second->SetExtmapAllowMixed(params.extmap_allow_mixed);
    }
  }

  std::vector<webrtc::RtpExtension> filtered_extensions = FilterRtpExtensions(
      params.extensions, webrtc::RtpExtension::IsSupportedForAudio, true);
  if (send_rtp_extensions_ != filtered_extensions) {
    send_rtp_extensions_.swap(filtered_extensions);
    for (auto& it : send_streams_) {
      it.second->SetRtpExtensions(send_rtp_extensions_);
    }
  }
  if (!params.mid.empty()) {
    mid_ = params.mid;
    for (auto& it : send_streams_) {
      it.second->SetMid(params.mid);
    }
  }

  if (!SetMaxSendBitrate(params.max_bandwidth_bps)) {
    return false;
  }
  return SetOptions(params.options);
}

bool WebRtcVoiceMediaChannel::SetRecvParameters(
    const AudioRecvParameters& params) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::SetRecvParameters");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "WebRtcVoiceMediaChannel::SetRecvParameters: "
                   << params.ToString();
  // TODO(pthatcher): Refactor this to be more clean now that we have
  // all the information at once.

  if (!SetRecvCodecs(params.codecs)) {
    return false;
  }

  if (!ValidateRtpExtensions(params.extensions)) {
    return false;
  }
  std::vector<webrtc::RtpExtension> filtered_extensions = FilterRtpExtensions(
      params.extensions, webrtc::RtpExtension::IsSupportedForAudio, false);
  if (recv_rtp_extensions_ != filtered_extensions) {
    recv_rtp_extensions_.swap(filtered_extensions);
    for (auto& it : recv_streams_) {
      it.second->SetRtpExtensionsAndRecreateStream(recv_rtp_extensions_);
    }
  }
  return true;
}

webrtc::RtpParameters WebRtcVoiceMediaChannel::GetRtpSendParameters(
    uint32_t ssrc) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Attempting to get RTP send parameters for stream "
                        << "with ssrc " << ssrc << " which doesn't exist.";
    return webrtc::RtpParameters();
  }

  webrtc::RtpParameters rtp_params = it->second->rtp_parameters();
  // Need to add the common list of codecs to the send stream-specific
  // RTP parameters.
  for (const AudioCodec& codec : send_codecs_) {
    rtp_params.codecs.push_back(codec.ToCodecParameters());
  }
  return rtp_params;
}

webrtc::RTCError WebRtcVoiceMediaChannel::SetRtpSendParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Attempting to set RTP send parameters for stream "
                        << "with ssrc " << ssrc << " which doesn't exist.";
    return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR);
  }

  // TODO(deadbeef): Handle setting parameters with a list of codecs in a
  // different order (which should change the send codec).
  webrtc::RtpParameters current_parameters = GetRtpSendParameters(ssrc);
  if (current_parameters.codecs != parameters.codecs) {
    RTC_DLOG(LS_ERROR) << "Using SetParameters to change the set of codecs "
                       << "is not currently supported.";
    return webrtc::RTCError(webrtc::RTCErrorType::UNSUPPORTED_PARAMETER);
  }

  if (!parameters.encodings.empty()) {
    auto& priority = parameters.encodings[0].network_priority;
    rtc::DiffServCodePoint new_dscp = rtc::DSCP_DEFAULT;
    if (priority == 0.5 * webrtc::kDefaultBitratePriority) {
      new_dscp = rtc::DSCP_CS1;
    } else if (priority == 1.0 * webrtc::kDefaultBitratePriority) {
      new_dscp = rtc::DSCP_DEFAULT;
    } else if (priority == 2.0 * webrtc::kDefaultBitratePriority) {
      new_dscp = rtc::DSCP_EF;
    } else if (priority == 4.0 * webrtc::kDefaultBitratePriority) {
      new_dscp = rtc::DSCP_EF;
    } else {
      RTC_LOG(LS_WARNING) << "Received invalid send network priority: "
                          << priority;
      return webrtc::RTCError(webrtc::RTCErrorType::INVALID_RANGE);
    }

    if (new_dscp != preferred_dscp_) {
      preferred_dscp_ = new_dscp;
      MediaChannel::UpdateDscp();
    }
  }

  // TODO(minyue): The following legacy actions go into
  // |WebRtcAudioSendStream::SetRtpParameters()| which is called at the end,
  // though there are two difference:
  // 1. |WebRtcVoiceMediaChannel::SetChannelSendParameters()| only calls
  // |SetSendCodec| while |WebRtcAudioSendStream::SetRtpParameters()| calls
  // |SetSendCodecs|. The outcome should be the same.
  // 2. AudioSendStream can be recreated.

  // Codecs are handled at the WebRtcVoiceMediaChannel level.
  webrtc::RtpParameters reduced_params = parameters;
  reduced_params.codecs.clear();
  return it->second->SetRtpParameters(reduced_params);
}

webrtc::RtpParameters WebRtcVoiceMediaChannel::GetRtpReceiveParameters(
    uint32_t ssrc) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  webrtc::RtpParameters rtp_params;
  // SSRC of 0 represents the default receive stream.
  if (ssrc == 0) {
    if (!default_sink_) {
      RTC_LOG(LS_WARNING)
          << "Attempting to get RTP parameters for the default, "
             "unsignaled audio receive stream, but not yet "
             "configured to receive such a stream.";
      return rtp_params;
    }
    rtp_params.encodings.emplace_back();
  } else {
    auto it = recv_streams_.find(ssrc);
    if (it == recv_streams_.end()) {
      RTC_LOG(LS_WARNING)
          << "Attempting to get RTP receive parameters for stream "
          << "with ssrc " << ssrc << " which doesn't exist.";
      return webrtc::RtpParameters();
    }
    rtp_params = it->second->GetRtpParameters();
  }

  for (const AudioCodec& codec : recv_codecs_) {
    rtp_params.codecs.push_back(codec.ToCodecParameters());
  }
  return rtp_params;
}

bool WebRtcVoiceMediaChannel::SetRtpReceiveParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  // SSRC of 0 represents the default receive stream.
  if (ssrc == 0) {
    if (!default_sink_) {
      RTC_LOG(LS_WARNING)
          << "Attempting to set RTP parameters for the default, "
             "unsignaled audio receive stream, but not yet "
             "configured to receive such a stream.";
      return false;
    }
  } else {
    auto it = recv_streams_.find(ssrc);
    if (it == recv_streams_.end()) {
      RTC_LOG(LS_WARNING)
          << "Attempting to set RTP receive parameters for stream "
          << "with ssrc " << ssrc << " which doesn't exist.";
      return false;
    }
  }

  webrtc::RtpParameters current_parameters = GetRtpReceiveParameters(ssrc);
  if (current_parameters != parameters) {
    RTC_DLOG(LS_ERROR) << "Changing the RTP receive parameters is currently "
                       << "unsupported.";
    return false;
  }
  return true;
}

bool WebRtcVoiceMediaChannel::SetOptions(const AudioOptions& options) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "Setting voice channel options: " << options.ToString();

  // We retain all of the existing options, and apply the given ones
  // on top.  This means there is no way to "clear" options such that
  // they go back to the engine default.
  options_.SetAll(options);
  if (!engine()->ApplyOptions(options_)) {
    RTC_LOG(LS_WARNING)
        << "Failed to apply engine options during channel SetOptions.";
    return false;
  }

  absl::optional<std::string> audio_network_adaptor_config =
      GetAudioNetworkAdaptorConfig(options_);
  for (auto& it : send_streams_) {
    it.second->SetAudioNetworkAdaptorConfig(audio_network_adaptor_config);
  }

  RTC_LOG(LS_INFO) << "Set voice channel options. Current options: "
                   << options_.ToString();
  return true;
}

bool WebRtcVoiceMediaChannel::SetRecvCodecs(
    const std::vector<AudioCodec>& codecs) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());

  // Set the payload types to be used for incoming media.
  RTC_LOG(LS_INFO) << "Setting receive voice codecs.";

  if (!VerifyUniquePayloadTypes(codecs)) {
    RTC_LOG(LS_ERROR) << "Codec payload types overlap.";
    return false;
  }

  // Create a payload type -> SdpAudioFormat map with all the decoders. Fail
  // unless the factory claims to support all decoders.
  std::map<int, webrtc::SdpAudioFormat> decoder_map;
  for (const AudioCodec& codec : codecs) {
    // Log a warning if a codec's payload type is changing. This used to be
    // treated as an error. It's abnormal, but not really illegal.
    AudioCodec old_codec;
    if (FindCodec(recv_codecs_, codec, &old_codec) &&
        old_codec.id != codec.id) {
      RTC_LOG(LS_WARNING) << codec.name << " mapped to a second payload type ("
                          << codec.id << ", was already mapped to "
                          << old_codec.id << ")";
    }
    auto format = AudioCodecToSdpAudioFormat(codec);
    if (!IsCodec(codec, "cn") && !IsCodec(codec, "telephone-event") &&
        !engine()->decoder_factory_->IsSupportedDecoder(format)) {
      RTC_LOG(LS_ERROR) << "Unsupported codec: " << rtc::ToString(format);
      return false;
    }
    // We allow adding new codecs but don't allow changing the payload type of
    // codecs that are already configured since we might already be receiving
    // packets with that payload type. See RFC3264, Section 8.3.2.
    // TODO(deadbeef): Also need to check for clashes with previously mapped
    // payload types, and not just currently mapped ones. For example, this
    // should be illegal:
    // 1. {100: opus/48000/2, 101: ISAC/16000}
    // 2. {100: opus/48000/2}
    // 3. {100: opus/48000/2, 101: ISAC/32000}
    // Though this check really should happen at a higher level, since this
    // conflict could happen between audio and video codecs.
    auto existing = decoder_map_.find(codec.id);
    if (existing != decoder_map_.end() && !existing->second.Matches(format)) {
      RTC_LOG(LS_ERROR) << "Attempting to use payload type " << codec.id
                        << " for " << codec.name
                        << ", but it is already used for "
                        << existing->second.name;
      return false;
    }
    decoder_map.insert({codec.id, std::move(format)});
  }

  if (decoder_map == decoder_map_) {
    // There's nothing new to configure.
    return true;
  }

  if (playout_) {
    // Receive codecs can not be changed while playing. So we temporarily
    // pause playout.
    ChangePlayout(false);
  }

  decoder_map_ = std::move(decoder_map);
  for (auto& kv : recv_streams_) {
    kv.second->SetDecoderMap(decoder_map_);
  }
  recv_codecs_ = codecs;

  if (desired_playout_ && !playout_) {
    ChangePlayout(desired_playout_);
  }
  return true;
}

// Utility function called from SetSendParameters() to extract current send
// codec settings from the given list of codecs (originally from SDP). Both send
// and receive streams may be reconfigured based on the new settings.
bool WebRtcVoiceMediaChannel::SetSendCodecs(
    const std::vector<AudioCodec>& codecs) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  dtmf_payload_type_ = absl::nullopt;
  dtmf_payload_freq_ = -1;

  // Validate supplied codecs list.
  for (const AudioCodec& codec : codecs) {
    // TODO(solenberg): Validate more aspects of input - that payload types
    //                  don't overlap, remove redundant/unsupported codecs etc -
    //                  the same way it is done for RtpHeaderExtensions.
    if (codec.id < kMinPayloadType || codec.id > kMaxPayloadType) {
      RTC_LOG(LS_WARNING) << "Codec payload type out of range: "
                          << ToString(codec);
      return false;
    }
  }

  // Find PT of telephone-event codec with lowest clockrate, as a fallback, in
  // case we don't have a DTMF codec with a rate matching the send codec's, or
  // if this function returns early.
  std::vector<AudioCodec> dtmf_codecs;
  for (const AudioCodec& codec : codecs) {
    if (IsCodec(codec, kDtmfCodecName)) {
      dtmf_codecs.push_back(codec);
      if (!dtmf_payload_type_ || codec.clockrate < dtmf_payload_freq_) {
        dtmf_payload_type_ = codec.id;
        dtmf_payload_freq_ = codec.clockrate;
      }
    }
  }

  // Scan through the list to figure out the codec to use for sending.
  absl::optional<webrtc::AudioSendStream::Config::SendCodecSpec>
      send_codec_spec;
  webrtc::BitrateConstraints bitrate_config;
  absl::optional<webrtc::AudioCodecInfo> voice_codec_info;
  for (const AudioCodec& voice_codec : codecs) {
    if (!(IsCodec(voice_codec, kCnCodecName) ||
          IsCodec(voice_codec, kDtmfCodecName) ||
          IsCodec(voice_codec, kRedCodecName))) {
      webrtc::SdpAudioFormat format(voice_codec.name, voice_codec.clockrate,
                                    voice_codec.channels, voice_codec.params);

      voice_codec_info = engine()->encoder_factory_->QueryAudioEncoder(format);
      if (!voice_codec_info) {
        RTC_LOG(LS_WARNING) << "Unknown codec " << ToString(voice_codec);
        continue;
      }

      send_codec_spec = webrtc::AudioSendStream::Config::SendCodecSpec(
          voice_codec.id, format);
      if (voice_codec.bitrate > 0) {
        send_codec_spec->target_bitrate_bps = voice_codec.bitrate;
      }
      send_codec_spec->transport_cc_enabled = HasTransportCc(voice_codec);
      send_codec_spec->nack_enabled = HasNack(voice_codec);
      bitrate_config = GetBitrateConfigForCodec(voice_codec);
      break;
    }
  }

  if (!send_codec_spec) {
    return false;
  }

  RTC_DCHECK(voice_codec_info);
  if (voice_codec_info->allow_comfort_noise) {
    // Loop through the codecs list again to find the CN codec.
    // TODO(solenberg): Break out into a separate function?
    for (const AudioCodec& cn_codec : codecs) {
      if (IsCodec(cn_codec, kCnCodecName) &&
          cn_codec.clockrate == send_codec_spec->format.clockrate_hz &&
          cn_codec.channels == voice_codec_info->num_channels) {
        if (cn_codec.channels != 1) {
          RTC_LOG(LS_WARNING)
              << "CN #channels " << cn_codec.channels << " not supported.";
        } else if (cn_codec.clockrate != 8000 && cn_codec.clockrate != 16000 &&
                   cn_codec.clockrate != 32000) {
          RTC_LOG(LS_WARNING)
              << "CN frequency " << cn_codec.clockrate << " not supported.";
        } else {
          send_codec_spec->cng_payload_type = cn_codec.id;
        }
        break;
      }
    }

    // Find the telephone-event PT exactly matching the preferred send codec.
    for (const AudioCodec& dtmf_codec : dtmf_codecs) {
      if (dtmf_codec.clockrate == send_codec_spec->format.clockrate_hz) {
        dtmf_payload_type_ = dtmf_codec.id;
        dtmf_payload_freq_ = dtmf_codec.clockrate;
        break;
      }
    }
  }

  if (send_codec_spec_ != send_codec_spec) {
    send_codec_spec_ = std::move(send_codec_spec);
    // Apply new settings to all streams.
    for (const auto& kv : send_streams_) {
      kv.second->SetSendCodecSpec(*send_codec_spec_);
    }
  } else {
    // If the codec isn't changing, set the start bitrate to -1 which means
    // "unchanged" so that BWE isn't affected.
    bitrate_config.start_bitrate_bps = -1;
  }
  call_->GetTransportControllerSend()->SetSdpBitrateParameters(bitrate_config);

  // Check if the transport cc feedback or NACK status has changed on the
  // preferred send codec, and in that case reconfigure all receive streams.
  if (recv_transport_cc_enabled_ != send_codec_spec_->transport_cc_enabled ||
      recv_nack_enabled_ != send_codec_spec_->nack_enabled) {
    RTC_LOG(LS_INFO) << "Recreate all the receive streams because the send "
                        "codec has changed.";
    recv_transport_cc_enabled_ = send_codec_spec_->transport_cc_enabled;
    recv_nack_enabled_ = send_codec_spec_->nack_enabled;
    for (auto& kv : recv_streams_) {
      kv.second->SetUseTransportCcAndRecreateStream(recv_transport_cc_enabled_,
                                                    recv_nack_enabled_);
    }
  }

  send_codecs_ = codecs;
  return true;
}

void WebRtcVoiceMediaChannel::SetPlayout(bool playout) {
  desired_playout_ = playout;
  return ChangePlayout(desired_playout_);
}

void WebRtcVoiceMediaChannel::ChangePlayout(bool playout) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::ChangePlayout");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  if (playout_ == playout) {
    return;
  }

  for (const auto& kv : recv_streams_) {
    kv.second->SetPlayout(playout);
  }
  playout_ = playout;
}

void WebRtcVoiceMediaChannel::SetSend(bool send) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::SetSend");
  if (send_ == send) {
    return;
  }

  // Apply channel specific options, and initialize the ADM for recording (this
  // may take time on some platforms, e.g. Android).
  if (send) {
    engine()->ApplyOptions(options_);

    // InitRecording() may return an error if the ADM is already recording.
    if (!engine()->adm()->RecordingIsInitialized() &&
        !engine()->adm()->Recording()) {
      if (engine()->adm()->InitRecording() != 0) {
        RTC_LOG(LS_WARNING) << "Failed to initialize recording";
      }
    }
  }

  // Change the settings on each send channel.
  for (auto& kv : send_streams_) {
    kv.second->SetSend(send);
  }

  send_ = send;
}

bool WebRtcVoiceMediaChannel::SetAudioSend(uint32_t ssrc,
                                           bool enable,
                                           const AudioOptions* options,
                                           AudioSource* source) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  // TODO(solenberg): The state change should be fully rolled back if any one of
  //                  these calls fail.
  if (!SetLocalSource(ssrc, source)) {
    return false;
  }
  if (!MuteStream(ssrc, !enable)) {
    return false;
  }
  if (enable && options) {
    return SetOptions(*options);
  }
  return true;
}

bool WebRtcVoiceMediaChannel::AddSendStream(const StreamParams& sp) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::AddSendStream");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "AddSendStream: " << sp.ToString();

  uint32_t ssrc = sp.first_ssrc();
  RTC_DCHECK(0 != ssrc);

  if (send_streams_.find(ssrc) != send_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Stream already exists with ssrc " << ssrc;
    return false;
  }

  absl::optional<std::string> audio_network_adaptor_config =
      GetAudioNetworkAdaptorConfig(options_);
  WebRtcAudioSendStream* stream = new WebRtcAudioSendStream(
      ssrc, mid_, sp.cname, sp.id, send_codec_spec_, ExtmapAllowMixed(),
      send_rtp_extensions_, max_send_bitrate_bps_,
      audio_config_.rtcp_report_interval_ms, audio_network_adaptor_config,
      call_, this, media_transport(), engine()->encoder_factory_,
      codec_pair_id_, nullptr, crypto_options_);
  send_streams_.insert(std::make_pair(ssrc, stream));

  // At this point the stream's local SSRC has been updated. If it is the first
  // send stream, make sure that all the receive streams are updated with the
  // same SSRC in order to send receiver reports.
  if (send_streams_.size() == 1) {
    receiver_reports_ssrc_ = ssrc;
    for (const auto& kv : recv_streams_) {
      // TODO(solenberg): Allow applications to set the RTCP SSRC of receive
      // streams instead, so we can avoid reconfiguring the streams here.
      kv.second->SetLocalSsrc(ssrc);
    }
  }

  send_streams_[ssrc]->SetSend(send_);
  return true;
}

bool WebRtcVoiceMediaChannel::RemoveSendStream(uint32_t ssrc) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::RemoveSendStream");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "RemoveSendStream: " << ssrc;

  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Try to remove stream with ssrc " << ssrc
                        << " which doesn't exist.";
    return false;
  }

  it->second->SetSend(false);

  // TODO(solenberg): If we're removing the receiver_reports_ssrc_ stream, find
  // the first active send stream and use that instead, reassociating receive
  // streams.

  delete it->second;
  send_streams_.erase(it);
  if (send_streams_.empty()) {
    SetSend(false);
  }
  return true;
}

bool WebRtcVoiceMediaChannel::AddRecvStream(const StreamParams& sp) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::AddRecvStream");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "AddRecvStream: " << sp.ToString();

  if (!sp.has_ssrcs()) {
    // This is a StreamParam with unsignaled SSRCs. Store it, so it can be used
    // later when we know the SSRCs on the first packet arrival.
    unsignaled_stream_params_ = sp;
    return true;
  }

  if (!ValidateStreamParams(sp)) {
    return false;
  }

  const uint32_t ssrc = sp.first_ssrc();
  if (ssrc == 0) {
    RTC_DLOG(LS_WARNING) << "AddRecvStream with ssrc==0 is not supported.";
    return false;
  }

  // If this stream was previously received unsignaled, we promote it, possibly
  // recreating the AudioReceiveStream, if stream ids have changed.
  if (MaybeDeregisterUnsignaledRecvStream(ssrc)) {
    recv_streams_[ssrc]->MaybeRecreateAudioReceiveStream(sp.stream_ids());
    return true;
  }

  if (recv_streams_.find(ssrc) != recv_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Stream already exists with ssrc " << ssrc;
    return false;
  }

  // Create a new channel for receiving audio data.
  recv_streams_.insert(std::make_pair(
      ssrc,
      new WebRtcAudioReceiveStream(
          ssrc, receiver_reports_ssrc_, recv_transport_cc_enabled_,
          recv_nack_enabled_, sp.stream_ids(), recv_rtp_extensions_, call_,
          this, media_transport(), engine()->decoder_factory_, decoder_map_,
          codec_pair_id_, engine()->audio_jitter_buffer_max_packets_,
          engine()->audio_jitter_buffer_fast_accelerate_,
          engine()->audio_jitter_buffer_min_delay_ms_,
          unsignaled_frame_decryptor_, crypto_options_)));
  recv_streams_[ssrc]->SetPlayout(playout_);

  return true;
}

bool WebRtcVoiceMediaChannel::RemoveRecvStream(uint32_t ssrc) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::RemoveRecvStream");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "RemoveRecvStream: " << ssrc;

  if (ssrc == 0) {
    // This indicates that we need to remove the unsignaled stream parameters
    // that are cached.
    unsignaled_stream_params_ = StreamParams();
    return true;
  }

  const auto it = recv_streams_.find(ssrc);
  if (it == recv_streams_.end()) {
    RTC_LOG(LS_WARNING) << "Try to remove stream with ssrc " << ssrc
                        << " which doesn't exist.";
    return false;
  }

  MaybeDeregisterUnsignaledRecvStream(ssrc);

  it->second->SetRawAudioSink(nullptr);
  delete it->second;
  recv_streams_.erase(it);
  return true;
}

bool WebRtcVoiceMediaChannel::SetLocalSource(uint32_t ssrc,
                                             AudioSource* source) {
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    if (source) {
      // Return an error if trying to set a valid source with an invalid ssrc.
      RTC_LOG(LS_ERROR) << "SetLocalSource failed with ssrc " << ssrc;
      return false;
    }

    // The channel likely has gone away, do nothing.
    return true;
  }

  if (source) {
    it->second->SetSource(source);
  } else {
    it->second->ClearSource();
  }

  return true;
}

bool WebRtcVoiceMediaChannel::SetOutputVolume(uint32_t ssrc, double volume) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  std::vector<uint32_t> ssrcs(1, ssrc);
  // SSRC of 0 represents the default receive stream.
  if (ssrc == 0) {
    default_recv_volume_ = volume;
    ssrcs = unsignaled_recv_ssrcs_;
  }
  for (uint32_t ssrc : ssrcs) {
    const auto it = recv_streams_.find(ssrc);
    if (it == recv_streams_.end()) {
      RTC_LOG(LS_WARNING) << "SetOutputVolume: no recv stream " << ssrc;
      return false;
    }
    it->second->SetOutputVolume(volume);
    RTC_LOG(LS_INFO) << "SetOutputVolume() to " << volume
                     << " for recv stream with ssrc " << ssrc;
  }
  return true;
}

bool WebRtcVoiceMediaChannel::CanInsertDtmf() {
  return dtmf_payload_type_.has_value() && send_;
}

void WebRtcVoiceMediaChannel::SetFrameDecryptor(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto matching_stream = recv_streams_.find(ssrc);
  if (matching_stream != recv_streams_.end()) {
    matching_stream->second->SetFrameDecryptor(frame_decryptor);
  }
  // Handle unsignaled frame decryptors.
  if (ssrc == 0) {
    unsignaled_frame_decryptor_ = frame_decryptor;
  }
}

void WebRtcVoiceMediaChannel::SetFrameEncryptor(
    uint32_t ssrc,
    rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto matching_stream = send_streams_.find(ssrc);
  if (matching_stream != send_streams_.end()) {
    matching_stream->second->SetFrameEncryptor(frame_encryptor);
  }
}

bool WebRtcVoiceMediaChannel::InsertDtmf(uint32_t ssrc,
                                         int event,
                                         int duration) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_INFO) << "WebRtcVoiceMediaChannel::InsertDtmf";
  if (!CanInsertDtmf()) {
    return false;
  }

  // Figure out which WebRtcAudioSendStream to send the event on.
  auto it = ssrc != 0 ? send_streams_.find(ssrc) : send_streams_.begin();
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "The specified ssrc " << ssrc << " is not in use.";
    return false;
  }
  if (event < kMinTelephoneEventCode || event > kMaxTelephoneEventCode) {
    RTC_LOG(LS_WARNING) << "DTMF event code " << event << " out of range.";
    return false;
  }
  RTC_DCHECK_NE(-1, dtmf_payload_freq_);
  return it->second->SendTelephoneEvent(*dtmf_payload_type_, dtmf_payload_freq_,
                                        event, duration);
}

void WebRtcVoiceMediaChannel::OnPacketReceived(rtc::CopyOnWriteBuffer* packet,
                                               int64_t packet_time_us) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());

  webrtc::PacketReceiver::DeliveryStatus delivery_result =
      call_->Receiver()->DeliverPacket(webrtc::MediaType::AUDIO, *packet,
                                       packet_time_us);

  if (delivery_result != webrtc::PacketReceiver::DELIVERY_UNKNOWN_SSRC) {
    return;
  }

  // Create an unsignaled receive stream for this previously not received ssrc.
  // If there already is N unsignaled receive streams, delete the oldest.
  // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=5208
  uint32_t ssrc = 0;
  if (!GetRtpSsrc(packet->cdata(), packet->size(), &ssrc)) {
    return;
  }
  RTC_DCHECK(std::find(unsignaled_recv_ssrcs_.begin(),
                       unsignaled_recv_ssrcs_.end(),
                       ssrc) == unsignaled_recv_ssrcs_.end());

  // Add new stream.
  StreamParams sp = unsignaled_stream_params_;
  sp.ssrcs.push_back(ssrc);
  RTC_LOG(LS_INFO) << "Creating unsignaled receive stream for SSRC=" << ssrc;
  if (!AddRecvStream(sp)) {
    RTC_LOG(LS_WARNING) << "Could not create unsignaled receive stream.";
    return;
  }
  unsignaled_recv_ssrcs_.push_back(ssrc);
  RTC_HISTOGRAM_COUNTS_LINEAR("WebRTC.Audio.NumOfUnsignaledStreams",
                              unsignaled_recv_ssrcs_.size(), 1, 100, 101);

  // Remove oldest unsignaled stream, if we have too many.
  if (unsignaled_recv_ssrcs_.size() > kMaxUnsignaledRecvStreams) {
    uint32_t remove_ssrc = unsignaled_recv_ssrcs_.front();
    RTC_DLOG(LS_INFO) << "Removing unsignaled receive stream with SSRC="
                      << remove_ssrc;
    RemoveRecvStream(remove_ssrc);
  }
  RTC_DCHECK_GE(kMaxUnsignaledRecvStreams, unsignaled_recv_ssrcs_.size());

  SetOutputVolume(ssrc, default_recv_volume_);

  // The default sink can only be attached to one stream at a time, so we hook
  // it up to the *latest* unsignaled stream we've seen, in order to support the
  // case where the SSRC of one unsignaled stream changes.
  if (default_sink_) {
    for (uint32_t drop_ssrc : unsignaled_recv_ssrcs_) {
      auto it = recv_streams_.find(drop_ssrc);
      it->second->SetRawAudioSink(nullptr);
    }
    std::unique_ptr<webrtc::AudioSinkInterface> proxy_sink(
        new ProxySink(default_sink_.get()));
    SetRawAudioSink(ssrc, std::move(proxy_sink));
  }

  delivery_result = call_->Receiver()->DeliverPacket(webrtc::MediaType::AUDIO,
                                                     *packet, packet_time_us);
  RTC_DCHECK_NE(webrtc::PacketReceiver::DELIVERY_UNKNOWN_SSRC, delivery_result);
}

void WebRtcVoiceMediaChannel::OnRtcpReceived(rtc::CopyOnWriteBuffer* packet,
                                             int64_t packet_time_us) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());

  // Forward packet to Call as well.
  call_->Receiver()->DeliverPacket(webrtc::MediaType::AUDIO, *packet,
                                   packet_time_us);
}

void WebRtcVoiceMediaChannel::OnNetworkRouteChanged(
    const std::string& transport_name,
    const rtc::NetworkRoute& network_route) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  call_->GetTransportControllerSend()->OnNetworkRouteChanged(transport_name,
                                                             network_route);
  call_->OnAudioTransportOverheadChanged(network_route.packet_overhead);
}

bool WebRtcVoiceMediaChannel::MuteStream(uint32_t ssrc, bool muted) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  const auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    RTC_LOG(LS_WARNING) << "The specified ssrc " << ssrc << " is not in use.";
    return false;
  }
  it->second->SetMuted(muted);

  // TODO(solenberg):
  // We set the AGC to mute state only when all the channels are muted.
  // This implementation is not ideal, instead we should signal the AGC when
  // the mic channel is muted/unmuted. We can't do it today because there
  // is no good way to know which stream is mapping to the mic channel.
  bool all_muted = muted;
  for (const auto& kv : send_streams_) {
    all_muted = all_muted && kv.second->muted();
  }
  engine()->apm()->set_output_will_be_muted(all_muted);

  return true;
}

bool WebRtcVoiceMediaChannel::SetMaxSendBitrate(int bps) {
  RTC_LOG(LS_INFO) << "WebRtcVoiceMediaChannel::SetMaxSendBitrate.";
  max_send_bitrate_bps_ = bps;
  bool success = true;
  for (const auto& kv : send_streams_) {
    if (!kv.second->SetMaxSendBitrate(max_send_bitrate_bps_)) {
      success = false;
    }
  }
  return success;
}

void WebRtcVoiceMediaChannel::OnReadyToSend(bool ready) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_VERBOSE) << "OnReadyToSend: " << (ready ? "Ready." : "Not ready.");
  call_->SignalChannelNetworkState(
      webrtc::MediaType::AUDIO,
      ready ? webrtc::kNetworkUp : webrtc::kNetworkDown);
}

bool WebRtcVoiceMediaChannel::GetStats(VoiceMediaInfo* info) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::GetStats");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(info);

  // Get SSRC and stats for each sender.
  RTC_DCHECK_EQ(info->senders.size(), 0U);
  for (const auto& stream : send_streams_) {
    webrtc::AudioSendStream::Stats stats =
        stream.second->GetStats(recv_streams_.size() > 0);
    VoiceSenderInfo sinfo;
    sinfo.add_ssrc(stats.local_ssrc);
    sinfo.bytes_sent = stats.bytes_sent;
    sinfo.packets_sent = stats.packets_sent;
    sinfo.packets_lost = stats.packets_lost;
    sinfo.fraction_lost = stats.fraction_lost;
    sinfo.codec_name = stats.codec_name;
    sinfo.codec_payload_type = stats.codec_payload_type;
    sinfo.ext_seqnum = stats.ext_seqnum;
    sinfo.jitter_ms = stats.jitter_ms;
    sinfo.rtt_ms = stats.rtt_ms;
    sinfo.audio_level = stats.audio_level;
    sinfo.total_input_energy = stats.total_input_energy;
    sinfo.total_input_duration = stats.total_input_duration;
    sinfo.typing_noise_detected = (send_ ? stats.typing_noise_detected : false);
    sinfo.ana_statistics = stats.ana_statistics;
    sinfo.apm_statistics = stats.apm_statistics;
    info->senders.push_back(sinfo);
  }

  // Get SSRC and stats for each receiver.
  RTC_DCHECK_EQ(info->receivers.size(), 0U);
  for (const auto& stream : recv_streams_) {
    uint32_t ssrc = stream.first;
    // When SSRCs are unsignaled, there's only one audio MediaStreamTrack, but
    // multiple RTP streams can be received over time (if the SSRC changes for
    // whatever reason). We only want the RTCMediaStreamTrackStats to represent
    // the stats for the most recent stream (the one whose audio is actually
    // routed to the MediaStreamTrack), so here we ignore any unsignaled SSRCs
    // except for the most recent one (last in the vector). This is somewhat of
    // a hack, and means you don't get *any* stats for these inactive streams,
    // but it's slightly better than the previous behavior, which was "highest
    // SSRC wins".
    // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=8158
    if (!unsignaled_recv_ssrcs_.empty()) {
      auto end_it = --unsignaled_recv_ssrcs_.end();
      if (std::find(unsignaled_recv_ssrcs_.begin(), end_it, ssrc) != end_it) {
        continue;
      }
    }
    webrtc::AudioReceiveStream::Stats stats = stream.second->GetStats();
    VoiceReceiverInfo rinfo;
    rinfo.add_ssrc(stats.remote_ssrc);
    rinfo.bytes_rcvd = stats.bytes_rcvd;
    rinfo.packets_rcvd = stats.packets_rcvd;
    rinfo.packets_lost = stats.packets_lost;
    rinfo.fraction_lost = stats.fraction_lost;
    rinfo.codec_name = stats.codec_name;
    rinfo.codec_payload_type = stats.codec_payload_type;
    rinfo.ext_seqnum = stats.ext_seqnum;
    rinfo.jitter_ms = stats.jitter_ms;
    rinfo.jitter_buffer_ms = stats.jitter_buffer_ms;
    rinfo.jitter_buffer_preferred_ms = stats.jitter_buffer_preferred_ms;
    rinfo.delay_estimate_ms = stats.delay_estimate_ms;
    rinfo.audio_level = stats.audio_level;
    rinfo.total_output_energy = stats.total_output_energy;
    rinfo.total_samples_received = stats.total_samples_received;
    rinfo.total_output_duration = stats.total_output_duration;
    rinfo.concealed_samples = stats.concealed_samples;
    rinfo.concealment_events = stats.concealment_events;
    rinfo.jitter_buffer_delay_seconds = stats.jitter_buffer_delay_seconds;
    rinfo.expand_rate = stats.expand_rate;
    rinfo.speech_expand_rate = stats.speech_expand_rate;
    rinfo.secondary_decoded_rate = stats.secondary_decoded_rate;
    rinfo.secondary_discarded_rate = stats.secondary_discarded_rate;
    rinfo.accelerate_rate = stats.accelerate_rate;
    rinfo.preemptive_expand_rate = stats.preemptive_expand_rate;
    rinfo.delayed_packet_outage_samples = stats.delayed_packet_outage_samples;
    rinfo.decoding_calls_to_silence_generator =
        stats.decoding_calls_to_silence_generator;
    rinfo.decoding_calls_to_neteq = stats.decoding_calls_to_neteq;
    rinfo.decoding_normal = stats.decoding_normal;
    rinfo.decoding_plc = stats.decoding_plc;
    rinfo.decoding_cng = stats.decoding_cng;
    rinfo.decoding_plc_cng = stats.decoding_plc_cng;
    rinfo.decoding_muted_output = stats.decoding_muted_output;
    rinfo.capture_start_ntp_time_ms = stats.capture_start_ntp_time_ms;
    rinfo.jitter_buffer_flushes = stats.jitter_buffer_flushes;

    info->receivers.push_back(rinfo);
  }

  // Get codec info
  for (const AudioCodec& codec : send_codecs_) {
    webrtc::RtpCodecParameters codec_params = codec.ToCodecParameters();
    info->send_codecs.insert(
        std::make_pair(codec_params.payload_type, std::move(codec_params)));
  }
  for (const AudioCodec& codec : recv_codecs_) {
    webrtc::RtpCodecParameters codec_params = codec.ToCodecParameters();
    info->receive_codecs.insert(
        std::make_pair(codec_params.payload_type, std::move(codec_params)));
  }

  return true;
}

void WebRtcVoiceMediaChannel::SetRawAudioSink(
    uint32_t ssrc,
    std::unique_ptr<webrtc::AudioSinkInterface> sink) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::SetRawAudioSink: ssrc:"
                      << ssrc << " " << (sink ? "(ptr)" : "NULL");
  if (ssrc == 0) {
    if (!unsignaled_recv_ssrcs_.empty()) {
      std::unique_ptr<webrtc::AudioSinkInterface> proxy_sink(
          sink ? new ProxySink(sink.get()) : nullptr);
      SetRawAudioSink(unsignaled_recv_ssrcs_.back(), std::move(proxy_sink));
    }
    default_sink_ = std::move(sink);
    return;
  }
  const auto it = recv_streams_.find(ssrc);
  if (it == recv_streams_.end()) {
    RTC_LOG(LS_WARNING) << "SetRawAudioSink: no recv stream " << ssrc;
    return;
  }
  it->second->SetRawAudioSink(std::move(sink));
}

std::vector<webrtc::RtpSource> WebRtcVoiceMediaChannel::GetSources(
    uint32_t ssrc) const {
  auto it = recv_streams_.find(ssrc);
  if (it == recv_streams_.end()) {
    RTC_LOG(LS_ERROR) << "Attempting to get contributing sources for SSRC:"
                      << ssrc << " which doesn't exist.";
    return std::vector<webrtc::RtpSource>();
  }
  return it->second->GetSources();
}

bool WebRtcVoiceMediaChannel::MaybeDeregisterUnsignaledRecvStream(
    uint32_t ssrc) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto it = std::find(unsignaled_recv_ssrcs_.begin(),
                      unsignaled_recv_ssrcs_.end(), ssrc);
  if (it != unsignaled_recv_ssrcs_.end()) {
    unsignaled_recv_ssrcs_.erase(it);
    return true;
  }
  return false;
}
}  // namespace cricket

#endif  // HAVE_WEBRTC_VOICE
