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

#include "webrtc/media/engine/webrtcvoiceengine.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "webrtc/api/call/audio_sink.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/base64.h"
#include "webrtc/base/byteorder.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/race_checker.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/media/base/audiosource.h"
#include "webrtc/media/base/mediaconstants.h"
#include "webrtc/media/base/streamparams.h"
#include "webrtc/media/engine/adm_helpers.h"
#include "webrtc/media/engine/apm_helpers.h"
#include "webrtc/media/engine/payload_type_mapper.h"
#include "webrtc/media/engine/webrtcmediaengine.h"
#include "webrtc/media/engine/webrtcvoe.h"
#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"
#include "webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/system_wrappers/include/field_trial.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/transmit_mixer.h"

namespace cricket {
namespace {

constexpr size_t kMaxUnsignaledRecvStreams = 4;

const int kDefaultTraceFilter = webrtc::kTraceNone | webrtc::kTraceTerseInfo |
                                webrtc::kTraceWarning | webrtc::kTraceError |
                                webrtc::kTraceCritical;
const int kElevatedTraceFilter = kDefaultTraceFilter | webrtc::kTraceStateInfo |
                                 webrtc::kTraceInfo;

constexpr int kNackRtpHistoryMs = 5000;

// Check to verify that the define for the intelligibility enhancer is properly
// set.
#if !defined(WEBRTC_INTELLIGIBILITY_ENHANCER) || \
    (WEBRTC_INTELLIGIBILITY_ENHANCER != 0 &&     \
     WEBRTC_INTELLIGIBILITY_ENHANCER != 1)
#error "Set WEBRTC_INTELLIGIBILITY_ENHANCER to either 0 or 1"
#endif

// For SendSideBwe, Opus bitrate should be in the range between 6000 and 32000.
const int kOpusMinBitrateBps = 6000;
const int kOpusBitrateFbBps = 32000;

// Default audio dscp value.
// See http://tools.ietf.org/html/rfc2474 for details.
// See also http://tools.ietf.org/html/draft-jennings-rtcweb-qos-00
const rtc::DiffServCodePoint kAudioDscpValue = rtc::DSCP_EF;

// Constants from voice_engine_defines.h.
const int kMinTelephoneEventCode = 0;           // RFC4733 (Section 2.3.1)
const int kMaxTelephoneEventCode = 255;

const int kMinPayloadType = 0;
const int kMaxPayloadType = 127;

class ProxySink : public webrtc::AudioSinkInterface {
 public:
  ProxySink(AudioSinkInterface* sink) : sink_(sink) { RTC_DCHECK(sink); }

  void OnData(const Data& audio) override { sink_->OnData(audio); }

 private:
  webrtc::AudioSinkInterface* sink_;
};

bool ValidateStreamParams(const StreamParams& sp) {
  if (sp.ssrcs.empty()) {
    LOG(LS_ERROR) << "No SSRCs in stream parameters: " << sp.ToString();
    return false;
  }
  if (sp.ssrcs.size() > 1) {
    LOG(LS_ERROR) << "Multiple SSRCs in stream parameters: " << sp.ToString();
    return false;
  }
  return true;
}

// Dumps an AudioCodec in RFC 2327-ish format.
std::string ToString(const AudioCodec& codec) {
  std::stringstream ss;
  ss << codec.name << "/" << codec.clockrate << "/" << codec.channels;
  if (!codec.params.empty()) {
    ss << " {";
    for (const auto& param : codec.params) {
      ss << " " << param.first << "=" << param.second;
    }
    ss << " }";
  }
  ss << " (" << codec.id << ")";
  return ss.str();
}

bool IsCodec(const AudioCodec& codec, const char* ref_name) {
  return (_stricmp(codec.name.c_str(), ref_name) == 0);
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

rtc::Optional<std::string> GetAudioNetworkAdaptorConfig(
    const AudioOptions& options) {
  if (options.audio_network_adaptor && *options.audio_network_adaptor &&
      options.audio_network_adaptor_config) {
    // Turn on audio network adaptor only when |options_.audio_network_adaptor|
    // equals true and |options_.audio_network_adaptor_config| has a value.
    return options.audio_network_adaptor_config;
  }
  return rtc::Optional<std::string>();
}

webrtc::AudioState::Config MakeAudioStateConfig(
    VoEWrapper* voe_wrapper,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer) {
  webrtc::AudioState::Config config;
  config.voice_engine = voe_wrapper->engine();
  if (audio_mixer) {
    config.audio_mixer = audio_mixer;
  } else {
    config.audio_mixer = webrtc::AudioMixerImpl::Create();
  }
  return config;
}

// |max_send_bitrate_bps| is the bitrate from "b=" in SDP.
// |rtp_max_bitrate_bps| is the bitrate from RtpSender::SetParameters.
rtc::Optional<int> ComputeSendBitrate(int max_send_bitrate_bps,
                                      rtc::Optional<int> rtp_max_bitrate_bps,
                                      const webrtc::AudioCodecSpec& spec) {
  // If application-configured bitrate is set, take minimum of that and SDP
  // bitrate.
  const int bps =
      rtp_max_bitrate_bps
          ? webrtc::MinPositive(max_send_bitrate_bps, *rtp_max_bitrate_bps)
          : max_send_bitrate_bps;
  if (bps <= 0) {
    return rtc::Optional<int>(spec.info.default_bitrate_bps);
  }

  if (bps < spec.info.min_bitrate_bps) {
    // If codec is not multi-rate and |bps| is less than the fixed bitrate then
    // fail. If codec is not multi-rate and |bps| exceeds or equal the fixed
    // bitrate then ignore.
    LOG(LS_ERROR) << "Failed to set codec " << spec.format.name
                  << " to bitrate " << bps << " bps"
                  << ", requires at least " << spec.info.min_bitrate_bps
                  << " bps.";
    return rtc::Optional<int>();
  }

  if (spec.info.HasFixedBitrate()) {
    return rtc::Optional<int>(spec.info.default_bitrate_bps);
  } else {
    // If codec is multi-rate then just set the bitrate.
    return rtc::Optional<int>(std::min(bps, spec.info.max_bitrate_bps));
  }
}

}  // namespace

WebRtcVoiceEngine::WebRtcVoiceEngine(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>& encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer)
    : WebRtcVoiceEngine(adm,
                        encoder_factory,
                        decoder_factory,
                        audio_mixer,
                        nullptr) {}

WebRtcVoiceEngine::WebRtcVoiceEngine(
    webrtc::AudioDeviceModule* adm,
    const rtc::scoped_refptr<webrtc::AudioEncoderFactory>& encoder_factory,
    const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
    rtc::scoped_refptr<webrtc::AudioMixer> audio_mixer,
    VoEWrapper* voe_wrapper)
    : adm_(adm),
      encoder_factory_(encoder_factory),
      decoder_factory_(decoder_factory),
      audio_mixer_(audio_mixer),
      voe_wrapper_(voe_wrapper) {
  // This may be called from any thread, so detach thread checkers.
  worker_thread_checker_.DetachFromThread();
  signal_thread_checker_.DetachFromThread();
  LOG(LS_INFO) << "WebRtcVoiceEngine::WebRtcVoiceEngine";
  RTC_DCHECK(decoder_factory);
  RTC_DCHECK(encoder_factory);
  // The rest of our initialization will happen in Init.
}

WebRtcVoiceEngine::~WebRtcVoiceEngine() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "WebRtcVoiceEngine::~WebRtcVoiceEngine";
  if (initialized_) {
    StopAecDump();
    voe_wrapper_->base()->Terminate();
    webrtc::Trace::SetTraceCallback(nullptr);
  }
}

void WebRtcVoiceEngine::Init() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "WebRtcVoiceEngine::Init";

  // TaskQueue expects to be created/destroyed on the same thread.
  low_priority_worker_queue_.reset(
      new rtc::TaskQueue("rtc-low-prio", rtc::TaskQueue::Priority::LOW));

  // VoEWrapper needs to be created on the worker thread. It's expected to be
  // null here unless it's being injected for testing.
  if (!voe_wrapper_) {
    voe_wrapper_.reset(new VoEWrapper());
  }

  // Load our audio codec lists.
  LOG(LS_INFO) << "Supported send codecs in order of preference:";
  send_codecs_ = CollectCodecs(encoder_factory_->GetSupportedEncoders());
  for (const AudioCodec& codec : send_codecs_) {
    LOG(LS_INFO) << ToString(codec);
  }

  LOG(LS_INFO) << "Supported recv codecs in order of preference:";
  recv_codecs_ = CollectCodecs(decoder_factory_->GetSupportedDecoders());
  for (const AudioCodec& codec : recv_codecs_) {
    LOG(LS_INFO) << ToString(codec);
  }

  channel_config_.enable_voice_pacing = true;

  // Temporarily turn logging level up for the Init() call.
  webrtc::Trace::SetTraceCallback(this);
  webrtc::Trace::set_level_filter(kElevatedTraceFilter);
  LOG(LS_INFO) << webrtc::VoiceEngine::GetVersionString();
  RTC_CHECK_EQ(0, voe_wrapper_->base()->Init(adm_.get(), nullptr,
                                             decoder_factory_));
  webrtc::Trace::set_level_filter(kDefaultTraceFilter);

  // No ADM supplied? Get the default one from VoE.
  if (!adm_) {
    adm_ = voe_wrapper_->base()->audio_device_module();
  }
  RTC_DCHECK(adm_);

  apm_ = voe_wrapper_->base()->audio_processing();
  RTC_DCHECK(apm_);

  transmit_mixer_ = voe_wrapper_->base()->transmit_mixer();
  RTC_DCHECK(transmit_mixer_);

  // Save the default AGC configuration settings. This must happen before
  // calling ApplyOptions or the default will be overwritten.
  default_agc_config_ = webrtc::apm_helpers::GetAgcConfig(apm_);

  // Set default engine options.
  {
    AudioOptions options;
    options.echo_cancellation = rtc::Optional<bool>(true);
    options.auto_gain_control = rtc::Optional<bool>(true);
    options.noise_suppression = rtc::Optional<bool>(true);
    options.highpass_filter = rtc::Optional<bool>(true);
    options.stereo_swapping = rtc::Optional<bool>(false);
    options.audio_jitter_buffer_max_packets = rtc::Optional<int>(50);
    options.audio_jitter_buffer_fast_accelerate = rtc::Optional<bool>(false);
    options.typing_detection = rtc::Optional<bool>(true);
    options.adjust_agc_delta = rtc::Optional<int>(0);
    options.experimental_agc = rtc::Optional<bool>(false);
    options.extended_filter_aec = rtc::Optional<bool>(false);
    options.delay_agnostic_aec = rtc::Optional<bool>(false);
    options.experimental_ns = rtc::Optional<bool>(false);
    options.intelligibility_enhancer = rtc::Optional<bool>(false);
    options.level_control = rtc::Optional<bool>(false);
    options.residual_echo_detector = rtc::Optional<bool>(true);
    bool error = ApplyOptions(options);
    RTC_DCHECK(error);
  }

  // Set default audio devices.
#if !defined(WEBRTC_IOS)
  webrtc::adm_helpers::SetRecordingDevice(adm_);
  apm()->Initialize();
  webrtc::adm_helpers::SetPlayoutDevice(adm_);
#endif  // !WEBRTC_IOS

  // May be null for VoE injected for testing.
  if (voe()->engine()) {
    audio_state_ =
        webrtc::AudioState::Create(MakeAudioStateConfig(voe(), audio_mixer_));
  }

  initialized_ = true;
}

rtc::scoped_refptr<webrtc::AudioState>
    WebRtcVoiceEngine::GetAudioState() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return audio_state_;
}

VoiceMediaChannel* WebRtcVoiceEngine::CreateChannel(
    webrtc::Call* call,
    const MediaConfig& config,
    const AudioOptions& options) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return new WebRtcVoiceMediaChannel(this, config, options, call);
}

bool WebRtcVoiceEngine::ApplyOptions(const AudioOptions& options_in) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "WebRtcVoiceEngine::ApplyOptions: " << options_in.ToString();
  AudioOptions options = options_in;  // The options are modified below.

  // Set and adjust echo canceller options.
  // kEcConference is AEC with high suppression.
  webrtc::EcModes ec_mode = webrtc::kEcConference;
  if (options.aecm_generate_comfort_noise) {
    LOG(LS_VERBOSE) << "Comfort noise explicitly set to "
                    << *options.aecm_generate_comfort_noise
                    << " (default is false).";
  }

#if defined(WEBRTC_IOS)
  // On iOS, VPIO provides built-in EC.
  options.echo_cancellation = rtc::Optional<bool>(false);
  options.extended_filter_aec = rtc::Optional<bool>(false);
  LOG(LS_INFO) << "Always disable AEC on iOS. Use built-in instead.";
#elif defined(ANDROID)
  ec_mode = webrtc::kEcAecm;
  options.extended_filter_aec = rtc::Optional<bool>(false);
#endif

  // Delay Agnostic AEC automatically turns on EC if not set except on iOS
  // where the feature is not supported.
  bool use_delay_agnostic_aec = false;
#if !defined(WEBRTC_IOS)
  if (options.delay_agnostic_aec) {
    use_delay_agnostic_aec = *options.delay_agnostic_aec;
    if (use_delay_agnostic_aec) {
      options.echo_cancellation = rtc::Optional<bool>(true);
      options.extended_filter_aec = rtc::Optional<bool>(true);
      ec_mode = webrtc::kEcConference;
    }
  }
#endif

// Set and adjust noise suppressor options.
#if defined(WEBRTC_IOS)
  // On iOS, VPIO provides built-in NS.
  options.noise_suppression = rtc::Optional<bool>(false);
  options.typing_detection = rtc::Optional<bool>(false);
  options.experimental_ns = rtc::Optional<bool>(false);
  LOG(LS_INFO) << "Always disable NS on iOS. Use built-in instead.";
#elif defined(ANDROID)
  options.typing_detection = rtc::Optional<bool>(false);
  options.experimental_ns = rtc::Optional<bool>(false);
#endif

// Set and adjust gain control options.
#if defined(WEBRTC_IOS)
  // On iOS, VPIO provides built-in AGC.
  options.auto_gain_control = rtc::Optional<bool>(false);
  options.experimental_agc = rtc::Optional<bool>(false);
  LOG(LS_INFO) << "Always disable AGC on iOS. Use built-in instead.";
#elif defined(ANDROID)
  options.experimental_agc = rtc::Optional<bool>(false);
#endif

#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
  // Turn off the gain control if specified by the field trial. The purpose of the field trial is to reduce the amount of resampling performed inside the audio processing module on mobile platforms by whenever possible turning off the fixed AGC mode and the high-pass filter. (https://bugs.chromium.org/p/webrtc/issues/detail?id=6181).
  if (webrtc::field_trial::IsEnabled(
          "WebRTC-Audio-MinimizeResamplingOnMobile")) {
    options.auto_gain_control = rtc::Optional<bool>(false);
    LOG(LS_INFO) << "Disable AGC according to field trial.";
    if (!(options.noise_suppression.value_or(false) or
          options.echo_cancellation.value_or(false))) {
      // If possible, turn off the high-pass filter.
      LOG(LS_INFO) << "Disable high-pass filter in response to field trial.";
      options.highpass_filter = rtc::Optional<bool>(false);
    }
  }
#endif

#if (WEBRTC_INTELLIGIBILITY_ENHANCER == 0)
  // Hardcode the intelligibility enhancer to be off.
  options.intelligibility_enhancer = rtc::Optional<bool>(false);
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
        options.echo_cancellation = rtc::Optional<bool>(false);
        LOG(LS_INFO) << "Disabling EC since built-in EC will be used instead";
      }
    }
    webrtc::apm_helpers::SetEcStatus(
        apm(), *options.echo_cancellation, ec_mode);
#if !defined(ANDROID)
    webrtc::apm_helpers::SetEcMetricsStatus(apm(), *options.echo_cancellation);
#endif
    if (ec_mode == webrtc::kEcAecm) {
      bool cn = options.aecm_generate_comfort_noise.value_or(false);
      webrtc::apm_helpers::SetAecmMode(apm(), cn);
    }
  }

  if (options.auto_gain_control) {
    bool built_in_agc_avaliable = adm()->BuiltInAGCIsAvailable();
    if (built_in_agc_avaliable) {
      if (adm()->EnableBuiltInAGC(*options.auto_gain_control) == 0 &&
          *options.auto_gain_control) {
        // Disable internal software AGC if built-in AGC is enabled,
        // i.e., replace the software AGC with the built-in AGC.
        options.auto_gain_control = rtc::Optional<bool>(false);
        LOG(LS_INFO) << "Disabling AGC since built-in AGC will be used instead";
      }
    }
    webrtc::apm_helpers::SetAgcStatus(apm(), adm(), *options.auto_gain_control);
  }

  if (options.tx_agc_target_dbov || options.tx_agc_digital_compression_gain ||
      options.tx_agc_limiter || options.adjust_agc_delta) {
    // Override default_agc_config_. Generally, an unset option means "leave
    // the VoE bits alone" in this function, so we want whatever is set to be
    // stored as the new "default". If we didn't, then setting e.g.
    // tx_agc_target_dbov would reset digital compression gain and limiter
    // settings.
    // Also, if we don't update default_agc_config_, then adjust_agc_delta
    // would be an offset from the original values, and not whatever was set
    // explicitly.
    default_agc_config_.targetLeveldBOv = options.tx_agc_target_dbov.value_or(
        default_agc_config_.targetLeveldBOv);
    default_agc_config_.digitalCompressionGaindB =
        options.tx_agc_digital_compression_gain.value_or(
            default_agc_config_.digitalCompressionGaindB);
    default_agc_config_.limiterEnable =
        options.tx_agc_limiter.value_or(default_agc_config_.limiterEnable);

    webrtc::AgcConfig config = default_agc_config_;
    if (options.adjust_agc_delta) {
      config.targetLeveldBOv -= *options.adjust_agc_delta;
      LOG(LS_INFO) << "Adjusting AGC level from default -"
                   << default_agc_config_.targetLeveldBOv << "dB to -"
                   << config.targetLeveldBOv << "dB";
    }
    webrtc::apm_helpers::SetAgcConfig(apm_, config);
  }

  if (options.intelligibility_enhancer) {
    intelligibility_enhancer_ = options.intelligibility_enhancer;
  }
  if (intelligibility_enhancer_ && *intelligibility_enhancer_) {
    LOG(LS_INFO) << "Enabling NS when Intelligibility Enhancer is active.";
    options.noise_suppression = intelligibility_enhancer_;
  }

  if (options.noise_suppression) {
    if (adm()->BuiltInNSIsAvailable()) {
      bool builtin_ns =
          *options.noise_suppression &&
          !(intelligibility_enhancer_ && *intelligibility_enhancer_);
      if (adm()->EnableBuiltInNS(builtin_ns) == 0 && builtin_ns) {
        // Disable internal software NS if built-in NS is enabled,
        // i.e., replace the software NS with the built-in NS.
        options.noise_suppression = rtc::Optional<bool>(false);
        LOG(LS_INFO) << "Disabling NS since built-in NS will be used instead";
      }
    }
    webrtc::apm_helpers::SetNsStatus(apm(), *options.noise_suppression);
  }

  if (options.stereo_swapping) {
    LOG(LS_INFO) << "Stereo swapping enabled? " << *options.stereo_swapping;
    transmit_mixer()->EnableStereoChannelSwapping(*options.stereo_swapping);
  }

  if (options.audio_jitter_buffer_max_packets) {
    LOG(LS_INFO) << "NetEq capacity is "
                 << *options.audio_jitter_buffer_max_packets;
    channel_config_.acm_config.neteq_config.max_packets_in_buffer =
        std::max(20, *options.audio_jitter_buffer_max_packets);
  }
  if (options.audio_jitter_buffer_fast_accelerate) {
    LOG(LS_INFO) << "NetEq fast mode? "
                 << *options.audio_jitter_buffer_fast_accelerate;
    channel_config_.acm_config.neteq_config.enable_fast_accelerate =
        *options.audio_jitter_buffer_fast_accelerate;
  }

  if (options.typing_detection) {
    LOG(LS_INFO) << "Typing detection is enabled? "
                 << *options.typing_detection;
    webrtc::apm_helpers::SetTypingDetectionStatus(
        apm(), *options.typing_detection);
  }

  webrtc::Config config;

  if (options.delay_agnostic_aec)
    delay_agnostic_aec_ = options.delay_agnostic_aec;
  if (delay_agnostic_aec_) {
    LOG(LS_INFO) << "Delay agnostic aec is enabled? " << *delay_agnostic_aec_;
    config.Set<webrtc::DelayAgnostic>(
        new webrtc::DelayAgnostic(*delay_agnostic_aec_));
  }

  if (options.extended_filter_aec) {
    extended_filter_aec_ = options.extended_filter_aec;
  }
  if (extended_filter_aec_) {
    LOG(LS_INFO) << "Extended filter aec is enabled? " << *extended_filter_aec_;
    config.Set<webrtc::ExtendedFilter>(
        new webrtc::ExtendedFilter(*extended_filter_aec_));
  }

  if (options.experimental_ns) {
    experimental_ns_ = options.experimental_ns;
  }
  if (experimental_ns_) {
    LOG(LS_INFO) << "Experimental ns is enabled? " << *experimental_ns_;
    config.Set<webrtc::ExperimentalNs>(
        new webrtc::ExperimentalNs(*experimental_ns_));
  }

  if (intelligibility_enhancer_) {
    LOG(LS_INFO) << "Intelligibility Enhancer is enabled? "
                 << *intelligibility_enhancer_;
    config.Set<webrtc::Intelligibility>(
        new webrtc::Intelligibility(*intelligibility_enhancer_));
  }

  if (options.level_control) {
    level_control_ = options.level_control;
  }

  LOG(LS_INFO) << "Level control: "
               << (!!level_control_ ? *level_control_ : -1);
  if (level_control_) {
    apm_config_.level_controller.enabled = *level_control_;
    if (options.level_control_initial_peak_level_dbfs) {
      apm_config_.level_controller.initial_peak_level_dbfs =
          *options.level_control_initial_peak_level_dbfs;
    }
  }

  if (options.highpass_filter) {
    apm_config_.high_pass_filter.enabled = *options.highpass_filter;
  }

  if (options.residual_echo_detector) {
    apm_config_.residual_echo_detector.enabled =
        *options.residual_echo_detector;
  }

  apm()->SetExtraOptions(config);
  apm()->ApplyConfig(apm_config_);

  if (options.recording_sample_rate) {
    LOG(LS_INFO) << "Recording sample rate is "
                 << *options.recording_sample_rate;
    if (adm()->SetRecordingSampleRate(*options.recording_sample_rate)) {
      LOG_RTCERR1(SetRecordingSampleRate, *options.recording_sample_rate);
    }
  }

  if (options.playout_sample_rate) {
    LOG(LS_INFO) << "Playout sample rate is " << *options.playout_sample_rate;
    if (adm()->SetPlayoutSampleRate(*options.playout_sample_rate)) {
      LOG_RTCERR1(SetPlayoutSampleRate, *options.playout_sample_rate);
    }
  }
  return true;
}

// TODO(solenberg): Remove, once AudioMonitor is gone.
int WebRtcVoiceEngine::GetInputLevel() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int8_t level = transmit_mixer()->AudioLevel();
  RTC_DCHECK_LE(0, level);
  return level;
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
  if (webrtc::field_trial::IsEnabled("WebRTC-Audio-SendSideBwe")) {
    capabilities.header_extensions.push_back(webrtc::RtpExtension(
        webrtc::RtpExtension::kTransportSequenceNumberUri,
        webrtc::RtpExtension::kTransportSequenceNumberDefaultId));
  }
  return capabilities;
}

int WebRtcVoiceEngine::GetLastEngineError() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return voe_wrapper_->error();
}

void WebRtcVoiceEngine::Print(webrtc::TraceLevel level, const char* trace,
                              int length) {
  // Note: This callback can happen on any thread!
  rtc::LoggingSeverity sev = rtc::LS_VERBOSE;
  if (level == webrtc::kTraceError || level == webrtc::kTraceCritical)
    sev = rtc::LS_ERROR;
  else if (level == webrtc::kTraceWarning)
    sev = rtc::LS_WARNING;
  else if (level == webrtc::kTraceStateInfo || level == webrtc::kTraceInfo)
    sev = rtc::LS_INFO;
  else if (level == webrtc::kTraceTerseInfo)
    sev = rtc::LS_INFO;

  // Skip past boilerplate prefix text.
  if (length < 72) {
    std::string msg(trace, length);
    LOG(LS_ERROR) << "Malformed webrtc log message: ";
    LOG_V(sev) << msg;
  } else {
    std::string msg(trace + 71, length - 72);
    LOG_V(sev) << "webrtc: " << msg;
  }
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

int WebRtcVoiceEngine::CreateVoEChannel() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return voe_wrapper_->base()->CreateChannel(channel_config_);
}

webrtc::AudioDeviceModule* WebRtcVoiceEngine::adm() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(adm_);
  return adm_;
}

webrtc::AudioProcessing* WebRtcVoiceEngine::apm() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(apm_);
  return apm_;
}

webrtc::voe::TransmitMixer* WebRtcVoiceEngine::transmit_mixer() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(transmit_mixer_);
  return transmit_mixer_;
}

AudioCodecs WebRtcVoiceEngine::CollectCodecs(
    const std::vector<webrtc::AudioCodecSpec>& specs) const {
  PayloadTypeMapper mapper;
  AudioCodecs out;

  // Only generate CN payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_cn = {{ 8000,  false },
                                                        { 16000, false },
                                                        { 32000, false }};
  // Only generate telephone-event payload types for these clockrates:
  std::map<int, bool, std::greater<int>> generate_dtmf = {{ 8000,  false },
                                                          { 16000, false },
                                                          { 32000, false },
                                                          { 48000, false }};

  auto map_format = [&mapper](const webrtc::SdpAudioFormat& format,
                              AudioCodecs* out) {
    rtc::Optional<AudioCodec> opt_codec = mapper.ToAudioCodec(format);
    if (opt_codec) {
      if (out) {
        out->push_back(*opt_codec);
      }
    } else {
      LOG(LS_ERROR) << "Unable to assign payload type to format: " << format;
    }

    return opt_codec;
  };

  for (const auto& spec : specs) {
    // We need to do some extra stuff before adding the main codecs to out.
    rtc::Optional<AudioCodec> opt_codec = map_format(spec.format, nullptr);
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
      int ch,
      webrtc::AudioTransport* voe_audio_transport,
      uint32_t ssrc,
      const std::string& c_name,
      const rtc::Optional<webrtc::AudioSendStream::Config::SendCodecSpec>&
          send_codec_spec,
      const std::vector<webrtc::RtpExtension>& extensions,
      int max_send_bitrate_bps,
      const rtc::Optional<std::string>& audio_network_adaptor_config,
      webrtc::Call* call,
      webrtc::Transport* send_transport,
      const rtc::scoped_refptr<webrtc::AudioEncoderFactory>& encoder_factory)
      : voe_audio_transport_(voe_audio_transport),
        call_(call),
        config_(send_transport),
        send_side_bwe_with_overhead_(
            webrtc::field_trial::IsEnabled("WebRTC-SendSideBwe-WithOverhead")),
        max_send_bitrate_bps_(max_send_bitrate_bps),
        rtp_parameters_(CreateRtpParametersWithOneEncoding()) {
    RTC_DCHECK_GE(ch, 0);
    // TODO(solenberg): Once we're not using FakeWebRtcVoiceEngine anymore:
    // RTC_DCHECK(voe_audio_transport);
    RTC_DCHECK(call);
    RTC_DCHECK(encoder_factory);
    config_.rtp.ssrc = ssrc;
    config_.rtp.c_name = c_name;
    config_.voe_channel_id = ch;
    config_.rtp.extensions = extensions;
    config_.audio_network_adaptor_config = audio_network_adaptor_config;
    config_.encoder_factory = encoder_factory;
    rtp_parameters_.encodings[0].ssrc = rtc::Optional<uint32_t>(ssrc);

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
    ReconfigureAudioSendStream();
  }

  void SetAudioNetworkAdaptorConfig(
      const rtc::Optional<std::string>& audio_network_adaptor_config) {
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

  bool SendTelephoneEvent(int payload_type, int payload_freq, int event,
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

  webrtc::AudioSendStream::Stats GetStats() const {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    return stream_->GetStats();
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
    RTC_CHECK_RUNS_SERIALIZED(&audio_capture_race_checker_);
    RTC_DCHECK(voe_audio_transport_);
    voe_audio_transport_->PushCaptureData(config_.voe_channel_id, audio_data,
                                          bits_per_sample, sample_rate,
                                          number_of_channels, number_of_frames);
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

  // Accessor to the VoE channel ID.
  int channel() const {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    return config_.voe_channel_id;
  }

  const webrtc::RtpParameters& rtp_parameters() const {
    return rtp_parameters_;
  }

  bool ValidateRtpParameters(const webrtc::RtpParameters& rtp_parameters) {
    if (rtp_parameters.encodings.size() != 1) {
      LOG(LS_ERROR)
          << "Attempted to set RtpParameters without exactly one encoding";
      return false;
    }
    if (rtp_parameters.encodings[0].ssrc != rtp_parameters_.encodings[0].ssrc) {
      LOG(LS_ERROR) << "Attempted to set RtpParameters with modified SSRC";
      return false;
    }
    return true;
  }

  bool SetRtpParameters(const webrtc::RtpParameters& parameters) {
    if (!ValidateRtpParameters(parameters)) {
      return false;
    }

    rtc::Optional<int> send_rate;
    if (audio_codec_spec_) {
      send_rate = ComputeSendBitrate(max_send_bitrate_bps_,
                                     parameters.encodings[0].max_bitrate_bps,
                                     *audio_codec_spec_);
      if (!send_rate) {
        return false;
      }
    }

    const rtc::Optional<int> old_rtp_max_bitrate =
        rtp_parameters_.encodings[0].max_bitrate_bps;

    rtp_parameters_ = parameters;

    if (rtp_parameters_.encodings[0].max_bitrate_bps != old_rtp_max_bitrate) {
      // Reconfigure AudioSendStream with new bit rate.
      if (send_rate) {
        config_.send_codec_spec->target_bitrate_bps = send_rate;
      }
      UpdateAllowedBitrateRange();
      ReconfigureAudioSendStream();
    } else {
      // parameters.encodings[0].active could have changed.
      UpdateSendState();
    }
    return true;
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
        !STR_CASE_CMP(config_.send_codec_spec->format.name.c_str(),
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
    config_.rtp.nack.rtp_history_ms =
        send_codec_spec.nack_enabled ? kNackRtpHistoryMs : 0;
    config_.send_codec_spec =
        rtc::Optional<webrtc::AudioSendStream::Config::SendCodecSpec>(
            send_codec_spec);
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
  webrtc::AudioTransport* const voe_audio_transport_ = nullptr;
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
  rtc::Optional<webrtc::AudioCodecSpec> audio_codec_spec_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcAudioSendStream);
};

class WebRtcVoiceMediaChannel::WebRtcAudioReceiveStream {
 public:
  WebRtcAudioReceiveStream(
      int ch,
      uint32_t remote_ssrc,
      uint32_t local_ssrc,
      bool use_transport_cc,
      bool use_nack,
      const std::string& sync_group,
      const std::vector<webrtc::RtpExtension>& extensions,
      webrtc::Call* call,
      webrtc::Transport* rtcp_send_transport,
      const rtc::scoped_refptr<webrtc::AudioDecoderFactory>& decoder_factory,
      const std::map<int, webrtc::SdpAudioFormat>& decoder_map)
      : call_(call), config_() {
    RTC_DCHECK_GE(ch, 0);
    RTC_DCHECK(call);
    config_.rtp.remote_ssrc = remote_ssrc;
    config_.rtp.local_ssrc = local_ssrc;
    config_.rtp.transport_cc = use_transport_cc;
    config_.rtp.nack.rtp_history_ms = use_nack ? kNackRtpHistoryMs : 0;
    config_.rtp.extensions = extensions;
    config_.rtcp_send_transport = rtcp_send_transport;
    config_.voe_channel_id = ch;
    config_.sync_group = sync_group;
    config_.decoder_factory = decoder_factory;
    config_.decoder_map = decoder_map;
    RecreateAudioReceiveStream();
  }

  ~WebRtcAudioReceiveStream() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    call_->DestroyAudioReceiveStream(stream_);
  }

  void RecreateAudioReceiveStream(uint32_t local_ssrc) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.rtp.local_ssrc = local_ssrc;
    RecreateAudioReceiveStream();
  }

  void RecreateAudioReceiveStream(bool use_transport_cc, bool use_nack) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.rtp.transport_cc = use_transport_cc;
    config_.rtp.nack.rtp_history_ms = use_nack ? kNackRtpHistoryMs : 0;
    RecreateAudioReceiveStream();
  }

  void RecreateAudioReceiveStream(
      const std::vector<webrtc::RtpExtension>& extensions) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.rtp.extensions = extensions;
    RecreateAudioReceiveStream();
  }

  // Set a new payload type -> decoder map.
  void RecreateAudioReceiveStream(
      const std::map<int, webrtc::SdpAudioFormat>& decoder_map) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    config_.decoder_map = decoder_map;
    RecreateAudioReceiveStream();
  }

  void MaybeRecreateAudioReceiveStream(const std::string& sync_group) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    if (config_.sync_group != sync_group) {
      config_.sync_group = sync_group;
      RecreateAudioReceiveStream();
    }
  }

  webrtc::AudioReceiveStream::Stats GetStats() const {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    return stream_->GetStats();
  }

  int GetOutputLevel() const {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    return stream_->GetOutputLevel();
  }

  int channel() const {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    return config_.voe_channel_id;
  }

  void SetRawAudioSink(std::unique_ptr<webrtc::AudioSinkInterface> sink) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    stream_->SetSink(std::move(sink));
  }

  void SetOutputVolume(double volume) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    stream_->SetGain(volume);
  }

  void SetPlayout(bool playout) {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    if (playout) {
      LOG(LS_INFO) << "Starting playout for channel #" << channel();
      stream_->Start();
    } else {
      LOG(LS_INFO) << "Stopping playout for channel #" << channel();
      stream_->Stop();
    }
    playout_ = playout;
  }

  std::vector<webrtc::RtpSource> GetSources() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    RTC_DCHECK(stream_);
    return stream_->GetSources();
  }

 private:
  void RecreateAudioReceiveStream() {
    RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
    if (stream_) {
      call_->DestroyAudioReceiveStream(stream_);
    }
    stream_ = call_->CreateAudioReceiveStream(config_);
    RTC_CHECK(stream_);
    SetPlayout(playout_);
  }

  rtc::ThreadChecker worker_thread_checker_;
  webrtc::Call* call_ = nullptr;
  webrtc::AudioReceiveStream::Config config_;
  // The stream is owned by WebRtcAudioReceiveStream and may be reallocated if
  // configuration changes.
  webrtc::AudioReceiveStream* stream_ = nullptr;
  bool playout_ = false;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WebRtcAudioReceiveStream);
};

WebRtcVoiceMediaChannel::WebRtcVoiceMediaChannel(WebRtcVoiceEngine* engine,
                                                 const MediaConfig& config,
                                                 const AudioOptions& options,
                                                 webrtc::Call* call)
    : VoiceMediaChannel(config), engine_(engine), call_(call) {
  LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::WebRtcVoiceMediaChannel";
  RTC_DCHECK(call);
  engine->RegisterChannel(this);
  SetOptions(options);
}

WebRtcVoiceMediaChannel::~WebRtcVoiceMediaChannel() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::~WebRtcVoiceMediaChannel";
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
  return kAudioDscpValue;
}

bool WebRtcVoiceMediaChannel::SetSendParameters(
    const AudioSendParameters& params) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::SetSendParameters");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "WebRtcVoiceMediaChannel::SetSendParameters: "
               << params.ToString();
  // TODO(pthatcher): Refactor this to be more clean now that we have
  // all the information at once.

  if (!SetSendCodecs(params.codecs)) {
    return false;
  }

  if (!ValidateRtpExtensions(params.extensions)) {
    return false;
  }
  std::vector<webrtc::RtpExtension> filtered_extensions =
      FilterRtpExtensions(params.extensions,
                          webrtc::RtpExtension::IsSupportedForAudio, true);
  if (send_rtp_extensions_ != filtered_extensions) {
    send_rtp_extensions_.swap(filtered_extensions);
    for (auto& it : send_streams_) {
      it.second->SetRtpExtensions(send_rtp_extensions_);
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
  LOG(LS_INFO) << "WebRtcVoiceMediaChannel::SetRecvParameters: "
               << params.ToString();
  // TODO(pthatcher): Refactor this to be more clean now that we have
  // all the information at once.

  if (!SetRecvCodecs(params.codecs)) {
    return false;
  }

  if (!ValidateRtpExtensions(params.extensions)) {
    return false;
  }
  std::vector<webrtc::RtpExtension> filtered_extensions =
      FilterRtpExtensions(params.extensions,
                          webrtc::RtpExtension::IsSupportedForAudio, false);
  if (recv_rtp_extensions_ != filtered_extensions) {
    recv_rtp_extensions_.swap(filtered_extensions);
    for (auto& it : recv_streams_) {
      it.second->RecreateAudioReceiveStream(recv_rtp_extensions_);
    }
  }
  return true;
}

webrtc::RtpParameters WebRtcVoiceMediaChannel::GetRtpSendParameters(
    uint32_t ssrc) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    LOG(LS_WARNING) << "Attempting to get RTP send parameters for stream "
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

bool WebRtcVoiceMediaChannel::SetRtpSendParameters(
    uint32_t ssrc,
    const webrtc::RtpParameters& parameters) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    LOG(LS_WARNING) << "Attempting to set RTP send parameters for stream "
                    << "with ssrc " << ssrc << " which doesn't exist.";
    return false;
  }

  // TODO(deadbeef): Handle setting parameters with a list of codecs in a
  // different order (which should change the send codec).
  webrtc::RtpParameters current_parameters = GetRtpSendParameters(ssrc);
  if (current_parameters.codecs != parameters.codecs) {
    LOG(LS_ERROR) << "Using SetParameters to change the set of codecs "
                  << "is not currently supported.";
    return false;
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
      LOG(LS_WARNING) << "Attempting to get RTP parameters for the default, "
                         "unsignaled audio receive stream, but not yet "
                         "configured to receive such a stream.";
      return rtp_params;
    }
    rtp_params.encodings.emplace_back();
  } else {
    auto it = recv_streams_.find(ssrc);
    if (it == recv_streams_.end()) {
      LOG(LS_WARNING) << "Attempting to get RTP receive parameters for stream "
                      << "with ssrc " << ssrc << " which doesn't exist.";
      return webrtc::RtpParameters();
    }
    rtp_params.encodings.emplace_back();
    // TODO(deadbeef): Return stream-specific parameters.
    rtp_params.encodings[0].ssrc = rtc::Optional<uint32_t>(ssrc);
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
      LOG(LS_WARNING) << "Attempting to set RTP parameters for the default, "
                         "unsignaled audio receive stream, but not yet "
                         "configured to receive such a stream.";
      return false;
    }
  } else {
    auto it = recv_streams_.find(ssrc);
    if (it == recv_streams_.end()) {
      LOG(LS_WARNING) << "Attempting to set RTP receive parameters for stream "
                      << "with ssrc " << ssrc << " which doesn't exist.";
      return false;
    }
  }

  webrtc::RtpParameters current_parameters = GetRtpReceiveParameters(ssrc);
  if (current_parameters != parameters) {
    LOG(LS_ERROR) << "Changing the RTP receive parameters is currently "
                  << "unsupported.";
    return false;
  }
  return true;
}

bool WebRtcVoiceMediaChannel::SetOptions(const AudioOptions& options) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "Setting voice channel options: "
               << options.ToString();

  // We retain all of the existing options, and apply the given ones
  // on top.  This means there is no way to "clear" options such that
  // they go back to the engine default.
  options_.SetAll(options);
  if (!engine()->ApplyOptions(options_)) {
    LOG(LS_WARNING) <<
        "Failed to apply engine options during channel SetOptions.";
    return false;
  }

  rtc::Optional<std::string> audio_network_adaptor_config =
      GetAudioNetworkAdaptorConfig(options_);
  for (auto& it : send_streams_) {
    it.second->SetAudioNetworkAdaptorConfig(audio_network_adaptor_config);
  }

  LOG(LS_INFO) << "Set voice channel options. Current options: "
               << options_.ToString();
  return true;
}

bool WebRtcVoiceMediaChannel::SetRecvCodecs(
    const std::vector<AudioCodec>& codecs) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());

  // Set the payload types to be used for incoming media.
  LOG(LS_INFO) << "Setting receive voice codecs.";

  if (!VerifyUniquePayloadTypes(codecs)) {
    LOG(LS_ERROR) << "Codec payload types overlap.";
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
      LOG(LS_WARNING) << codec.name << " mapped to a second payload type ("
                      << codec.id << ", was already mapped to " << old_codec.id
                      << ")";
    }
    auto format = AudioCodecToSdpAudioFormat(codec);
    if (!IsCodec(codec, "cn") && !IsCodec(codec, "telephone-event") &&
        !engine()->decoder_factory_->IsSupportedDecoder(format)) {
      LOG(LS_ERROR) << "Unsupported codec: " << format;
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
      LOG(LS_ERROR) << "Attempting to use payload type " << codec.id << " for "
                    << codec.name << ", but it is already used for "
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
    kv.second->RecreateAudioReceiveStream(decoder_map_);
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
  dtmf_payload_type_ = rtc::Optional<int>();
  dtmf_payload_freq_ = -1;

  // Validate supplied codecs list.
  for (const AudioCodec& codec : codecs) {
    // TODO(solenberg): Validate more aspects of input - that payload types
    //                  don't overlap, remove redundant/unsupported codecs etc -
    //                  the same way it is done for RtpHeaderExtensions.
    if (codec.id < kMinPayloadType || codec.id > kMaxPayloadType) {
      LOG(LS_WARNING) << "Codec payload type out of range: " << ToString(codec);
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
        dtmf_payload_type_ = rtc::Optional<int>(codec.id);
        dtmf_payload_freq_ = codec.clockrate;
      }
    }
  }

  // Scan through the list to figure out the codec to use for sending.
  rtc::Optional<webrtc::AudioSendStream::Config::SendCodecSpec> send_codec_spec;
  webrtc::Call::Config::BitrateConfig bitrate_config;
  rtc::Optional<webrtc::AudioCodecInfo> voice_codec_info;
  for (const AudioCodec& voice_codec : codecs) {
    if (!(IsCodec(voice_codec, kCnCodecName) ||
          IsCodec(voice_codec, kDtmfCodecName) ||
          IsCodec(voice_codec, kRedCodecName))) {
      webrtc::SdpAudioFormat format(voice_codec.name, voice_codec.clockrate,
                                    voice_codec.channels, voice_codec.params);

      voice_codec_info = engine()->encoder_factory_->QueryAudioEncoder(format);
      if (!voice_codec_info) {
        LOG(LS_WARNING) << "Unknown codec " << ToString(voice_codec);
        continue;
      }

      send_codec_spec =
          rtc::Optional<webrtc::AudioSendStream::Config::SendCodecSpec>(
              {voice_codec.id, format});
      if (voice_codec.bitrate > 0) {
        send_codec_spec->target_bitrate_bps =
            rtc::Optional<int>(voice_codec.bitrate);
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
          cn_codec.clockrate == send_codec_spec->format.clockrate_hz) {
        switch (cn_codec.clockrate) {
          case 8000:
          case 16000:
          case 32000:
            send_codec_spec->cng_payload_type = rtc::Optional<int>(cn_codec.id);
            break;
          default:
            LOG(LS_WARNING) << "CN frequency " << cn_codec.clockrate
                            << " not supported.";
            break;
        }
        break;
      }
    }

    // Find the telephone-event PT exactly matching the preferred send codec.
    for (const AudioCodec& dtmf_codec : dtmf_codecs) {
      if (dtmf_codec.clockrate == send_codec_spec->format.clockrate_hz) {
        dtmf_payload_type_ = rtc::Optional<int>(dtmf_codec.id);
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
  call_->SetBitrateConfig(bitrate_config);

  // Check if the transport cc feedback or NACK status has changed on the
  // preferred send codec, and in that case reconfigure all receive streams.
  if (recv_transport_cc_enabled_ != send_codec_spec_->transport_cc_enabled ||
      recv_nack_enabled_ != send_codec_spec_->nack_enabled) {
    LOG(LS_INFO) << "Recreate all the receive streams because the send "
                    "codec has changed.";
    recv_transport_cc_enabled_ = send_codec_spec_->transport_cc_enabled;
    recv_nack_enabled_ = send_codec_spec_->nack_enabled;
    for (auto& kv : recv_streams_) {
      kv.second->RecreateAudioReceiveStream(recv_transport_cc_enabled_,
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
        LOG(LS_WARNING) << "Failed to initialize recording";
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

int WebRtcVoiceMediaChannel::CreateVoEChannel() {
  int id = engine()->CreateVoEChannel();
  if (id == -1) {
    LOG_RTCERR0(CreateVoEChannel);
    return -1;
  }

  return id;
}

bool WebRtcVoiceMediaChannel::DeleteVoEChannel(int channel) {
  if (engine()->voe()->base()->DeleteChannel(channel) == -1) {
    LOG_RTCERR1(DeleteChannel, channel);
    return false;
  }
  return true;
}

bool WebRtcVoiceMediaChannel::AddSendStream(const StreamParams& sp) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::AddSendStream");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "AddSendStream: " << sp.ToString();

  uint32_t ssrc = sp.first_ssrc();
  RTC_DCHECK(0 != ssrc);

  if (GetSendChannelId(ssrc) != -1) {
    LOG(LS_ERROR) << "Stream already exists with ssrc " << ssrc;
    return false;
  }

  // Create a new channel for sending audio data.
  int channel = CreateVoEChannel();
  if (channel == -1) {
    return false;
  }

  // Save the channel to send_streams_, so that RemoveSendStream() can still
  // delete the channel in case failure happens below.
  webrtc::AudioTransport* audio_transport =
      engine()->voe()->base()->audio_transport();

  rtc::Optional<std::string> audio_network_adaptor_config =
      GetAudioNetworkAdaptorConfig(options_);
  WebRtcAudioSendStream* stream = new WebRtcAudioSendStream(
      channel, audio_transport, ssrc, sp.cname, send_codec_spec_,
      send_rtp_extensions_, max_send_bitrate_bps_, audio_network_adaptor_config,
      call_, this, engine()->encoder_factory_);
  send_streams_.insert(std::make_pair(ssrc, stream));

  // At this point the stream's local SSRC has been updated. If it is the first
  // send stream, make sure that all the receive streams are updated with the
  // same SSRC in order to send receiver reports.
  if (send_streams_.size() == 1) {
    receiver_reports_ssrc_ = ssrc;
    for (const auto& kv : recv_streams_) {
      // TODO(solenberg): Allow applications to set the RTCP SSRC of receive
      // streams instead, so we can avoid recreating the streams here.
      kv.second->RecreateAudioReceiveStream(ssrc);
    }
  }

  send_streams_[ssrc]->SetSend(send_);
  return true;
}

bool WebRtcVoiceMediaChannel::RemoveSendStream(uint32_t ssrc) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::RemoveSendStream");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "RemoveSendStream: " << ssrc;

  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    LOG(LS_WARNING) << "Try to remove stream with ssrc " << ssrc
                    << " which doesn't exist.";
    return false;
  }

  it->second->SetSend(false);

  // TODO(solenberg): If we're removing the receiver_reports_ssrc_ stream, find
  // the first active send stream and use that instead, reassociating receive
  // streams.

  // Clean up and delete the send stream+channel.
  int channel = it->second->channel();
  LOG(LS_INFO) << "Removing audio send stream " << ssrc
               << " with VoiceEngine channel #" << channel << ".";
  delete it->second;
  send_streams_.erase(it);
  if (!DeleteVoEChannel(channel)) {
    return false;
  }
  if (send_streams_.empty()) {
    SetSend(false);
  }
  return true;
}

bool WebRtcVoiceMediaChannel::AddRecvStream(const StreamParams& sp) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::AddRecvStream");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "AddRecvStream: " << sp.ToString();

  if (!ValidateStreamParams(sp)) {
    return false;
  }

  const uint32_t ssrc = sp.first_ssrc();
  if (ssrc == 0) {
    LOG(LS_WARNING) << "AddRecvStream with ssrc==0 is not supported.";
    return false;
  }

  // If this stream was previously received unsignaled, we promote it, possibly
  // recreating the AudioReceiveStream, if sync_label has changed.
  if (MaybeDeregisterUnsignaledRecvStream(ssrc)) {
    recv_streams_[ssrc]->MaybeRecreateAudioReceiveStream(sp.sync_label);
    return true;
  }

  if (GetReceiveChannelId(ssrc) != -1) {
    LOG(LS_ERROR) << "Stream already exists with ssrc " << ssrc;
    return false;
  }

  // Create a new channel for receiving audio data.
  const int channel = CreateVoEChannel();
  if (channel == -1) {
    return false;
  }

  recv_streams_.insert(std::make_pair(
      ssrc,
      new WebRtcAudioReceiveStream(
          channel, ssrc, receiver_reports_ssrc_, recv_transport_cc_enabled_,
          recv_nack_enabled_, sp.sync_label, recv_rtp_extensions_, call_, this,
          engine()->decoder_factory_, decoder_map_)));
  recv_streams_[ssrc]->SetPlayout(playout_);

  return true;
}

bool WebRtcVoiceMediaChannel::RemoveRecvStream(uint32_t ssrc) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::RemoveRecvStream");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "RemoveRecvStream: " << ssrc;

  const auto it = recv_streams_.find(ssrc);
  if (it == recv_streams_.end()) {
    LOG(LS_WARNING) << "Try to remove stream with ssrc " << ssrc
                    << " which doesn't exist.";
    return false;
  }

  MaybeDeregisterUnsignaledRecvStream(ssrc);

  const int channel = it->second->channel();

  // Clean up and delete the receive stream+channel.
  LOG(LS_INFO) << "Removing audio receive stream " << ssrc
               << " with VoiceEngine channel #" << channel << ".";
  it->second->SetRawAudioSink(nullptr);
  delete it->second;
  recv_streams_.erase(it);
  return DeleteVoEChannel(channel);
}

bool WebRtcVoiceMediaChannel::SetLocalSource(uint32_t ssrc,
                                             AudioSource* source) {
  auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    if (source) {
      // Return an error if trying to set a valid source with an invalid ssrc.
      LOG(LS_ERROR) << "SetLocalSource failed with ssrc " << ssrc;
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

// TODO(solenberg): Remove, once AudioMonitor is gone.
bool WebRtcVoiceMediaChannel::GetActiveStreams(
    AudioInfo::StreamList* actives) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  actives->clear();
  for (const auto& ch : recv_streams_) {
    int level = ch.second->GetOutputLevel();
    if (level > 0) {
      actives->push_back(std::make_pair(ch.first, level));
    }
  }
  return true;
}

// TODO(solenberg): Remove, once AudioMonitor is gone.
int WebRtcVoiceMediaChannel::GetOutputLevel() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int highest = 0;
  for (const auto& ch : recv_streams_) {
    highest = std::max(ch.second->GetOutputLevel(), highest);
  }
  return highest;
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
      LOG(LS_WARNING) << "SetOutputVolume: no recv stream " << ssrc;
      return false;
    }
    it->second->SetOutputVolume(volume);
    LOG(LS_INFO) << "SetOutputVolume() to " << volume
                 << " for recv stream with ssrc " << ssrc;
  }
  return true;
}

bool WebRtcVoiceMediaChannel::CanInsertDtmf() {
  return dtmf_payload_type_ ? true : false;
}

bool WebRtcVoiceMediaChannel::InsertDtmf(uint32_t ssrc, int event,
                                         int duration) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  LOG(LS_INFO) << "WebRtcVoiceMediaChannel::InsertDtmf";
  if (!dtmf_payload_type_) {
    return false;
  }

  // Figure out which WebRtcAudioSendStream to send the event on.
  auto it = ssrc != 0 ? send_streams_.find(ssrc) : send_streams_.begin();
  if (it == send_streams_.end()) {
    LOG(LS_WARNING) << "The specified ssrc " << ssrc << " is not in use.";
    return false;
  }
  if (event < kMinTelephoneEventCode ||
      event > kMaxTelephoneEventCode) {
    LOG(LS_WARNING) << "DTMF event code " << event << " out of range.";
    return false;
  }
  RTC_DCHECK_NE(-1, dtmf_payload_freq_);
  return it->second->SendTelephoneEvent(*dtmf_payload_type_, dtmf_payload_freq_,
                                        event, duration);
}

void WebRtcVoiceMediaChannel::OnPacketReceived(
    rtc::CopyOnWriteBuffer* packet, const rtc::PacketTime& packet_time) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());

  const webrtc::PacketTime webrtc_packet_time(packet_time.timestamp,
                                              packet_time.not_before);
  webrtc::PacketReceiver::DeliveryStatus delivery_result =
      call_->Receiver()->DeliverPacket(webrtc::MediaType::AUDIO,
                                       packet->cdata(), packet->size(),
                                       webrtc_packet_time);
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
      unsignaled_recv_ssrcs_.end(), ssrc) == unsignaled_recv_ssrcs_.end());

  // Add new stream.
  StreamParams sp;
  sp.ssrcs.push_back(ssrc);
  LOG(LS_INFO) << "Creating unsignaled receive stream for SSRC=" << ssrc;
  if (!AddRecvStream(sp)) {
    LOG(LS_WARNING) << "Could not create unsignaled receive stream.";
    return;
  }
  unsignaled_recv_ssrcs_.push_back(ssrc);
  RTC_HISTOGRAM_COUNTS_LINEAR(
      "WebRTC.Audio.NumOfUnsignaledStreams", unsignaled_recv_ssrcs_.size(), 1,
      100, 101);

  // Remove oldest unsignaled stream, if we have too many.
  if (unsignaled_recv_ssrcs_.size() > kMaxUnsignaledRecvStreams) {
    uint32_t remove_ssrc = unsignaled_recv_ssrcs_.front();
    LOG(LS_INFO) << "Removing unsignaled receive stream with SSRC="
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
                                                     packet->cdata(),
                                                     packet->size(),
                                                     webrtc_packet_time);
  RTC_DCHECK_NE(webrtc::PacketReceiver::DELIVERY_UNKNOWN_SSRC, delivery_result);
}

void WebRtcVoiceMediaChannel::OnRtcpReceived(
    rtc::CopyOnWriteBuffer* packet, const rtc::PacketTime& packet_time) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());

  // Forward packet to Call as well.
  const webrtc::PacketTime webrtc_packet_time(packet_time.timestamp,
                                              packet_time.not_before);
  call_->Receiver()->DeliverPacket(webrtc::MediaType::AUDIO,
      packet->cdata(), packet->size(), webrtc_packet_time);
}

void WebRtcVoiceMediaChannel::OnNetworkRouteChanged(
    const std::string& transport_name,
    const rtc::NetworkRoute& network_route) {
  call_->OnNetworkRouteChanged(transport_name, network_route);
}

bool WebRtcVoiceMediaChannel::MuteStream(uint32_t ssrc, bool muted) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  const auto it = send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    LOG(LS_WARNING) << "The specified ssrc " << ssrc << " is not in use.";
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
  LOG(LS_INFO) << "WebRtcVoiceMediaChannel::SetMaxSendBitrate.";
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
  LOG(LS_VERBOSE) << "OnReadyToSend: " << (ready ? "Ready." : "Not ready.");
  call_->SignalChannelNetworkState(
      webrtc::MediaType::AUDIO,
      ready ? webrtc::kNetworkUp : webrtc::kNetworkDown);
}

void WebRtcVoiceMediaChannel::OnTransportOverheadChanged(
    int transport_overhead_per_packet) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  call_->OnTransportOverheadChanged(webrtc::MediaType::AUDIO,
                                    transport_overhead_per_packet);
}

bool WebRtcVoiceMediaChannel::GetStats(VoiceMediaInfo* info) {
  TRACE_EVENT0("webrtc", "WebRtcVoiceMediaChannel::GetStats");
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(info);

  // Get SSRC and stats for each sender.
  RTC_DCHECK_EQ(info->senders.size(), 0U);
  for (const auto& stream : send_streams_) {
    webrtc::AudioSendStream::Stats stats = stream.second->GetStats();
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
    sinfo.aec_quality_min = stats.aec_quality_min;
    sinfo.echo_delay_median_ms = stats.echo_delay_median_ms;
    sinfo.echo_delay_std_ms = stats.echo_delay_std_ms;
    sinfo.echo_return_loss = stats.echo_return_loss;
    sinfo.echo_return_loss_enhancement = stats.echo_return_loss_enhancement;
    sinfo.residual_echo_likelihood = stats.residual_echo_likelihood;
    sinfo.residual_echo_likelihood_recent_max =
        stats.residual_echo_likelihood_recent_max;
    sinfo.typing_noise_detected = (send_ ? stats.typing_noise_detected : false);
    info->senders.push_back(sinfo);
  }

  // Get SSRC and stats for each receiver.
  RTC_DCHECK_EQ(info->receivers.size(), 0U);
  for (const auto& stream : recv_streams_) {
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
    rinfo.expand_rate = stats.expand_rate;
    rinfo.speech_expand_rate = stats.speech_expand_rate;
    rinfo.secondary_decoded_rate = stats.secondary_decoded_rate;
    rinfo.accelerate_rate = stats.accelerate_rate;
    rinfo.preemptive_expand_rate = stats.preemptive_expand_rate;
    rinfo.decoding_calls_to_silence_generator =
        stats.decoding_calls_to_silence_generator;
    rinfo.decoding_calls_to_neteq = stats.decoding_calls_to_neteq;
    rinfo.decoding_normal = stats.decoding_normal;
    rinfo.decoding_plc = stats.decoding_plc;
    rinfo.decoding_cng = stats.decoding_cng;
    rinfo.decoding_plc_cng = stats.decoding_plc_cng;
    rinfo.decoding_muted_output = stats.decoding_muted_output;
    rinfo.capture_start_ntp_time_ms = stats.capture_start_ntp_time_ms;
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
  LOG(LS_VERBOSE) << "WebRtcVoiceMediaChannel::SetRawAudioSink: ssrc:" << ssrc
                  << " " << (sink ? "(ptr)" : "NULL");
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
    LOG(LS_WARNING) << "SetRawAudioSink: no recv stream " << ssrc;
    return;
  }
  it->second->SetRawAudioSink(std::move(sink));
}

std::vector<webrtc::RtpSource> WebRtcVoiceMediaChannel::GetSources(
    uint32_t ssrc) const {
  auto it = recv_streams_.find(ssrc);
  RTC_DCHECK(it != recv_streams_.end())
      << "Attempting to get contributing sources for SSRC:" << ssrc
      << " which doesn't exist.";
  return it->second->GetSources();
}

int WebRtcVoiceMediaChannel::GetReceiveChannelId(uint32_t ssrc) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  const auto it = recv_streams_.find(ssrc);
  if (it != recv_streams_.end()) {
    return it->second->channel();
  }
  return -1;
}

int WebRtcVoiceMediaChannel::GetSendChannelId(uint32_t ssrc) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  const auto it = send_streams_.find(ssrc);
  if (it != send_streams_.end()) {
    return it->second->channel();
  }
  return -1;
}

bool WebRtcVoiceMediaChannel::
    MaybeDeregisterUnsignaledRecvStream(uint32_t ssrc) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  auto it = std::find(unsignaled_recv_ssrcs_.begin(),
                      unsignaled_recv_ssrcs_.end(),
                      ssrc);
  if (it != unsignaled_recv_ssrcs_.end()) {
    unsignaled_recv_ssrcs_.erase(it);
    return true;
  }
  return false;
}
}  // namespace cricket

#endif  // HAVE_WEBRTC_VOICE
