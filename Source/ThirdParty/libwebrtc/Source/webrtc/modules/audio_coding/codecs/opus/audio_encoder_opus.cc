/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/opus/audio_encoder_opus.h"

#include <algorithm>
#include <iterator>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/numerics/exp_filter.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/audio_network_adaptor_impl.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller_manager.h"
#include "webrtc/modules/audio_coding/codecs/opus/opus_interface.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {

constexpr int kSampleRateHz = 48000;

// Opus API allows a min bitrate of 500bps, but Opus documentation suggests
// a minimum bitrate of 6kbps.
constexpr int kMinBitrateBps = 6000;

constexpr int kMaxBitrateBps = 512000;

#if WEBRTC_OPUS_SUPPORT_120MS_PTIME
constexpr int kSupportedFrameLengths[] = {20, 60, 120};
#else
constexpr int kSupportedFrameLengths[] = {20, 60};
#endif

// PacketLossFractionSmoother uses an exponential filter with a time constant
// of -1.0 / ln(0.9999) = 10000 ms.
constexpr float kAlphaForPacketLossFractionSmoother = 0.9999f;

AudioEncoderOpus::Config CreateConfig(const CodecInst& codec_inst) {
  AudioEncoderOpus::Config config;
  config.frame_size_ms = rtc::CheckedDivExact(codec_inst.pacsize, 48);
  config.num_channels = codec_inst.channels;
  config.bitrate_bps = rtc::Optional<int>(codec_inst.rate);
  config.payload_type = codec_inst.pltype;
  config.application = config.num_channels == 1 ? AudioEncoderOpus::kVoip
                                                : AudioEncoderOpus::kAudio;
  config.supported_frame_lengths_ms.push_back(config.frame_size_ms);
#if WEBRTC_OPUS_VARIABLE_COMPLEXITY
  config.low_rate_complexity = 9;
#endif
  return config;
}

// Optimize the loss rate to configure Opus. Basically, optimized loss rate is
// the input loss rate rounded down to various levels, because a robustly good
// audio quality is achieved by lowering the packet loss down.
// Additionally, to prevent toggling, margins are used, i.e., when jumping to
// a loss rate from below, a higher threshold is used than jumping to the same
// level from above.
float OptimizePacketLossRate(float new_loss_rate, float old_loss_rate) {
  RTC_DCHECK_GE(new_loss_rate, 0.0f);
  RTC_DCHECK_LE(new_loss_rate, 1.0f);
  RTC_DCHECK_GE(old_loss_rate, 0.0f);
  RTC_DCHECK_LE(old_loss_rate, 1.0f);
  constexpr float kPacketLossRate20 = 0.20f;
  constexpr float kPacketLossRate10 = 0.10f;
  constexpr float kPacketLossRate5 = 0.05f;
  constexpr float kPacketLossRate1 = 0.01f;
  constexpr float kLossRate20Margin = 0.02f;
  constexpr float kLossRate10Margin = 0.01f;
  constexpr float kLossRate5Margin = 0.01f;
  if (new_loss_rate >=
      kPacketLossRate20 +
          kLossRate20Margin *
              (kPacketLossRate20 - old_loss_rate > 0 ? 1 : -1)) {
    return kPacketLossRate20;
  } else if (new_loss_rate >=
             kPacketLossRate10 +
                 kLossRate10Margin *
                     (kPacketLossRate10 - old_loss_rate > 0 ? 1 : -1)) {
    return kPacketLossRate10;
  } else if (new_loss_rate >=
             kPacketLossRate5 +
                 kLossRate5Margin *
                     (kPacketLossRate5 - old_loss_rate > 0 ? 1 : -1)) {
    return kPacketLossRate5;
  } else if (new_loss_rate >= kPacketLossRate1) {
    return kPacketLossRate1;
  } else {
    return 0.0f;
  }
}

}  // namespace

class AudioEncoderOpus::PacketLossFractionSmoother {
 public:
  explicit PacketLossFractionSmoother(const Clock* clock)
      : clock_(clock),
        last_sample_time_ms_(clock_->TimeInMilliseconds()),
        smoother_(kAlphaForPacketLossFractionSmoother) {}

  // Gets the smoothed packet loss fraction.
  float GetAverage() const {
    float value = smoother_.filtered();
    return (value == rtc::ExpFilter::kValueUndefined) ? 0.0f : value;
  }

  // Add new observation to the packet loss fraction smoother.
  void AddSample(float packet_loss_fraction) {
    int64_t now_ms = clock_->TimeInMilliseconds();
    smoother_.Apply(static_cast<float>(now_ms - last_sample_time_ms_),
                    packet_loss_fraction);
    last_sample_time_ms_ = now_ms;
  }

 private:
  const Clock* const clock_;
  int64_t last_sample_time_ms_;

  // An exponential filter is used to smooth the packet loss fraction.
  rtc::ExpFilter smoother_;
};

AudioEncoderOpus::Config::Config() {
#if WEBRTC_OPUS_VARIABLE_COMPLEXITY
  low_rate_complexity = 9;
#endif
}
AudioEncoderOpus::Config::Config(const Config&) = default;
AudioEncoderOpus::Config::~Config() = default;
auto AudioEncoderOpus::Config::operator=(const Config&) -> Config& = default;

bool AudioEncoderOpus::Config::IsOk() const {
  if (frame_size_ms <= 0 || frame_size_ms % 10 != 0)
    return false;
  if (num_channels != 1 && num_channels != 2)
    return false;
  if (bitrate_bps &&
      (*bitrate_bps < kMinBitrateBps || *bitrate_bps > kMaxBitrateBps))
    return false;
  if (complexity < 0 || complexity > 10)
    return false;
  if (low_rate_complexity < 0 || low_rate_complexity > 10)
    return false;
  return true;
}

int AudioEncoderOpus::Config::GetBitrateBps() const {
  RTC_DCHECK(IsOk());
  if (bitrate_bps)
    return *bitrate_bps;  // Explicitly set value.
  else
    return num_channels == 1 ? 32000 : 64000;  // Default value.
}

rtc::Optional<int> AudioEncoderOpus::Config::GetNewComplexity() const {
  RTC_DCHECK(IsOk());
  const int bitrate_bps = GetBitrateBps();
  if (bitrate_bps >=
          complexity_threshold_bps - complexity_threshold_window_bps &&
      bitrate_bps <=
          complexity_threshold_bps + complexity_threshold_window_bps) {
    // Within the hysteresis window; make no change.
    return rtc::Optional<int>();
  }
  return bitrate_bps <= complexity_threshold_bps
             ? rtc::Optional<int>(low_rate_complexity)
             : rtc::Optional<int>(complexity);
}

AudioEncoderOpus::AudioEncoderOpus(
    const Config& config,
    AudioNetworkAdaptorCreator&& audio_network_adaptor_creator,
    std::unique_ptr<SmoothingFilter> bitrate_smoother)
    : send_side_bwe_with_overhead_(webrtc::field_trial::IsEnabled(
          "WebRTC-SendSideBwe-WithOverhead")),
      packet_loss_rate_(0.0),
      inst_(nullptr),
      packet_loss_fraction_smoother_(new PacketLossFractionSmoother(
          config.clock)),
      audio_network_adaptor_creator_(
          audio_network_adaptor_creator
              ? std::move(audio_network_adaptor_creator)
              : [this](const std::string& config_string,
                       RtcEventLog* event_log,
                       const Clock* clock) {
                  return DefaultAudioNetworkAdaptorCreator(config_string,
                                                           event_log, clock);
              }),
      bitrate_smoother_(bitrate_smoother
          ? std::move(bitrate_smoother) : std::unique_ptr<SmoothingFilter>(
              // We choose 5sec as initial time constant due to empirical data.
              new SmoothingFilterImpl(5000, config.clock))) {
  RTC_CHECK(RecreateEncoderInstance(config));
}

AudioEncoderOpus::AudioEncoderOpus(const CodecInst& codec_inst)
    : AudioEncoderOpus(CreateConfig(codec_inst), nullptr) {}

AudioEncoderOpus::~AudioEncoderOpus() {
  RTC_CHECK_EQ(0, WebRtcOpus_EncoderFree(inst_));
}

int AudioEncoderOpus::SampleRateHz() const {
  return kSampleRateHz;
}

size_t AudioEncoderOpus::NumChannels() const {
  return config_.num_channels;
}

size_t AudioEncoderOpus::Num10MsFramesInNextPacket() const {
  return Num10msFramesPerPacket();
}

size_t AudioEncoderOpus::Max10MsFramesInAPacket() const {
  return Num10msFramesPerPacket();
}

int AudioEncoderOpus::GetTargetBitrate() const {
  return config_.GetBitrateBps();
}

void AudioEncoderOpus::Reset() {
  RTC_CHECK(RecreateEncoderInstance(config_));
}

bool AudioEncoderOpus::SetFec(bool enable) {
  if (enable) {
    RTC_CHECK_EQ(0, WebRtcOpus_EnableFec(inst_));
  } else {
    RTC_CHECK_EQ(0, WebRtcOpus_DisableFec(inst_));
  }
  config_.fec_enabled = enable;
  return true;
}

bool AudioEncoderOpus::SetDtx(bool enable) {
  if (enable) {
    RTC_CHECK_EQ(0, WebRtcOpus_EnableDtx(inst_));
  } else {
    RTC_CHECK_EQ(0, WebRtcOpus_DisableDtx(inst_));
  }
  config_.dtx_enabled = enable;
  return true;
}

bool AudioEncoderOpus::GetDtx() const {
  return config_.dtx_enabled;
}

bool AudioEncoderOpus::SetApplication(Application application) {
  auto conf = config_;
  switch (application) {
    case Application::kSpeech:
      conf.application = AudioEncoderOpus::kVoip;
      break;
    case Application::kAudio:
      conf.application = AudioEncoderOpus::kAudio;
      break;
  }
  return RecreateEncoderInstance(conf);
}

void AudioEncoderOpus::SetMaxPlaybackRate(int frequency_hz) {
  auto conf = config_;
  conf.max_playback_rate_hz = frequency_hz;
  RTC_CHECK(RecreateEncoderInstance(conf));
}

bool AudioEncoderOpus::EnableAudioNetworkAdaptor(
    const std::string& config_string,
    RtcEventLog* event_log,
    const Clock* clock) {
  audio_network_adaptor_ =
      audio_network_adaptor_creator_(config_string, event_log, clock);
  return audio_network_adaptor_.get() != nullptr;
}

void AudioEncoderOpus::DisableAudioNetworkAdaptor() {
  audio_network_adaptor_.reset(nullptr);
}

void AudioEncoderOpus::OnReceivedUplinkPacketLossFraction(
    float uplink_packet_loss_fraction) {
  if (!audio_network_adaptor_) {
    packet_loss_fraction_smoother_->AddSample(uplink_packet_loss_fraction);
    float average_fraction_loss = packet_loss_fraction_smoother_->GetAverage();
    return SetProjectedPacketLossRate(average_fraction_loss);
  }
  audio_network_adaptor_->SetUplinkPacketLossFraction(
      uplink_packet_loss_fraction);
  ApplyAudioNetworkAdaptor();
}

void AudioEncoderOpus::OnReceivedUplinkBandwidth(
    int target_audio_bitrate_bps,
    rtc::Optional<int64_t> probing_interval_ms) {
  if (audio_network_adaptor_) {
    audio_network_adaptor_->SetTargetAudioBitrate(target_audio_bitrate_bps);
    // We give smoothed bitrate allocation to audio network adaptor as
    // the uplink bandwidth.
    // The probing spikes should not affect the bitrate smoother more than 25%.
    // To simplify the calculations we use a step response as input signal.
    // The step response of an exponential filter is
    // u(t) = 1 - e^(-t / time_constant).
    // In order to limit the affect of a BWE spike within 25% of its value
    // before
    // the next probing, we would choose a time constant that fulfills
    // 1 - e^(-probing_interval_ms / time_constant) < 0.25
    // Then 4 * probing_interval_ms is a good choice.
    if (probing_interval_ms)
      bitrate_smoother_->SetTimeConstantMs(*probing_interval_ms * 4);
    bitrate_smoother_->AddSample(target_audio_bitrate_bps);

    ApplyAudioNetworkAdaptor();
  } else if (send_side_bwe_with_overhead_) {
    if (!overhead_bytes_per_packet_) {
      LOG(LS_INFO)
          << "AudioEncoderOpus: Overhead unknown, target audio bitrate "
          << target_audio_bitrate_bps << " bps is ignored.";
      return;
    }
    const int overhead_bps = static_cast<int>(
        *overhead_bytes_per_packet_ * 8 * 100 / Num10MsFramesInNextPacket());
    SetTargetBitrate(std::min(
        kMaxBitrateBps,
        std::max(kMinBitrateBps, target_audio_bitrate_bps - overhead_bps)));
  } else {
    SetTargetBitrate(target_audio_bitrate_bps);
  }
}

void AudioEncoderOpus::OnReceivedRtt(int rtt_ms) {
  if (!audio_network_adaptor_)
    return;
  audio_network_adaptor_->SetRtt(rtt_ms);
  ApplyAudioNetworkAdaptor();
}

void AudioEncoderOpus::OnReceivedOverhead(size_t overhead_bytes_per_packet) {
  if (audio_network_adaptor_) {
    audio_network_adaptor_->SetOverhead(overhead_bytes_per_packet);
    ApplyAudioNetworkAdaptor();
  } else {
    overhead_bytes_per_packet_ =
        rtc::Optional<size_t>(overhead_bytes_per_packet);
  }
}

void AudioEncoderOpus::SetReceiverFrameLengthRange(int min_frame_length_ms,
                                                   int max_frame_length_ms) {
  // Ensure that |SetReceiverFrameLengthRange| is called before
  // |EnableAudioNetworkAdaptor|, otherwise we need to recreate
  // |audio_network_adaptor_|, which is not a needed use case.
  RTC_DCHECK(!audio_network_adaptor_);

  config_.supported_frame_lengths_ms.clear();
  std::copy_if(std::begin(kSupportedFrameLengths),
               std::end(kSupportedFrameLengths),
               std::back_inserter(config_.supported_frame_lengths_ms),
               [&](int frame_length_ms) {
                 return frame_length_ms >= min_frame_length_ms &&
                        frame_length_ms <= max_frame_length_ms;
               });
  RTC_DCHECK(std::is_sorted(config_.supported_frame_lengths_ms.begin(),
                            config_.supported_frame_lengths_ms.end()));
}

AudioEncoder::EncodedInfo AudioEncoderOpus::EncodeImpl(
    uint32_t rtp_timestamp,
    rtc::ArrayView<const int16_t> audio,
    rtc::Buffer* encoded) {
  MaybeUpdateUplinkBandwidth();

  if (input_buffer_.empty())
    first_timestamp_in_buffer_ = rtp_timestamp;

  input_buffer_.insert(input_buffer_.end(), audio.cbegin(), audio.cend());
  if (input_buffer_.size() <
      (Num10msFramesPerPacket() * SamplesPer10msFrame())) {
    return EncodedInfo();
  }
  RTC_CHECK_EQ(input_buffer_.size(),
               Num10msFramesPerPacket() * SamplesPer10msFrame());

  const size_t max_encoded_bytes = SufficientOutputBufferSize();
  EncodedInfo info;
  info.encoded_bytes =
      encoded->AppendData(
          max_encoded_bytes, [&] (rtc::ArrayView<uint8_t> encoded) {
            int status = WebRtcOpus_Encode(
                inst_, &input_buffer_[0],
                rtc::CheckedDivExact(input_buffer_.size(),
                                     config_.num_channels),
                rtc::saturated_cast<int16_t>(max_encoded_bytes),
                encoded.data());

            RTC_CHECK_GE(status, 0);  // Fails only if fed invalid data.

            return static_cast<size_t>(status);
          });
  input_buffer_.clear();

  // Will use new packet size for next encoding.
  config_.frame_size_ms = next_frame_length_ms_;

  info.encoded_timestamp = first_timestamp_in_buffer_;
  info.payload_type = config_.payload_type;
  info.send_even_if_empty = true;  // Allows Opus to send empty packets.
  info.speech = (info.encoded_bytes > 0);
  info.encoder_type = CodecType::kOpus;
  return info;
}

size_t AudioEncoderOpus::Num10msFramesPerPacket() const {
  return static_cast<size_t>(rtc::CheckedDivExact(config_.frame_size_ms, 10));
}

size_t AudioEncoderOpus::SamplesPer10msFrame() const {
  return rtc::CheckedDivExact(kSampleRateHz, 100) * config_.num_channels;
}

size_t AudioEncoderOpus::SufficientOutputBufferSize() const {
  // Calculate the number of bytes we expect the encoder to produce,
  // then multiply by two to give a wide margin for error.
  const size_t bytes_per_millisecond =
      static_cast<size_t>(config_.GetBitrateBps() / (1000 * 8) + 1);
  const size_t approx_encoded_bytes =
      Num10msFramesPerPacket() * 10 * bytes_per_millisecond;
  return 2 * approx_encoded_bytes;
}

// If the given config is OK, recreate the Opus encoder instance with those
// settings, save the config, and return true. Otherwise, do nothing and return
// false.
bool AudioEncoderOpus::RecreateEncoderInstance(const Config& config) {
  if (!config.IsOk())
    return false;
  if (inst_)
    RTC_CHECK_EQ(0, WebRtcOpus_EncoderFree(inst_));
  input_buffer_.clear();
  input_buffer_.reserve(Num10msFramesPerPacket() * SamplesPer10msFrame());
  RTC_CHECK_EQ(0, WebRtcOpus_EncoderCreate(&inst_, config.num_channels,
                                           config.application));
  RTC_CHECK_EQ(0, WebRtcOpus_SetBitRate(inst_, config.GetBitrateBps()));
  if (config.fec_enabled) {
    RTC_CHECK_EQ(0, WebRtcOpus_EnableFec(inst_));
  } else {
    RTC_CHECK_EQ(0, WebRtcOpus_DisableFec(inst_));
  }
  RTC_CHECK_EQ(
      0, WebRtcOpus_SetMaxPlaybackRate(inst_, config.max_playback_rate_hz));
  // Use the default complexity if the start bitrate is within the hysteresis
  // window.
  complexity_ = config.GetNewComplexity().value_or(config.complexity);
  RTC_CHECK_EQ(0, WebRtcOpus_SetComplexity(inst_, complexity_));
  if (config.dtx_enabled) {
    RTC_CHECK_EQ(0, WebRtcOpus_EnableDtx(inst_));
  } else {
    RTC_CHECK_EQ(0, WebRtcOpus_DisableDtx(inst_));
  }
  RTC_CHECK_EQ(0,
               WebRtcOpus_SetPacketLossRate(
                   inst_, static_cast<int32_t>(packet_loss_rate_ * 100 + .5)));
  config_ = config;

  num_channels_to_encode_ = NumChannels();
  next_frame_length_ms_ = config_.frame_size_ms;
  return true;
}

void AudioEncoderOpus::SetFrameLength(int frame_length_ms) {
  next_frame_length_ms_ = frame_length_ms;
}

void AudioEncoderOpus::SetNumChannelsToEncode(size_t num_channels_to_encode) {
  RTC_DCHECK_GT(num_channels_to_encode, 0);
  RTC_DCHECK_LE(num_channels_to_encode, config_.num_channels);

  if (num_channels_to_encode_ == num_channels_to_encode)
    return;

  RTC_CHECK_EQ(0, WebRtcOpus_SetForceChannels(inst_, num_channels_to_encode));
  num_channels_to_encode_ = num_channels_to_encode;
}

void AudioEncoderOpus::SetProjectedPacketLossRate(float fraction) {
  float opt_loss_rate = OptimizePacketLossRate(fraction, packet_loss_rate_);
  if (packet_loss_rate_ != opt_loss_rate) {
    packet_loss_rate_ = opt_loss_rate;
    RTC_CHECK_EQ(
        0, WebRtcOpus_SetPacketLossRate(
               inst_, static_cast<int32_t>(packet_loss_rate_ * 100 + .5)));
  }
}

void AudioEncoderOpus::SetTargetBitrate(int bits_per_second) {
  config_.bitrate_bps = rtc::Optional<int>(
      std::max(std::min(bits_per_second, kMaxBitrateBps), kMinBitrateBps));
  RTC_DCHECK(config_.IsOk());
  RTC_CHECK_EQ(0, WebRtcOpus_SetBitRate(inst_, config_.GetBitrateBps()));
  const auto new_complexity = config_.GetNewComplexity();
  if (new_complexity && complexity_ != *new_complexity) {
    complexity_ = *new_complexity;
    RTC_CHECK_EQ(0, WebRtcOpus_SetComplexity(inst_, complexity_));
  }
}

void AudioEncoderOpus::ApplyAudioNetworkAdaptor() {
  auto config = audio_network_adaptor_->GetEncoderRuntimeConfig();
  RTC_DCHECK(!config.frame_length_ms || *config.frame_length_ms == 20 ||
             *config.frame_length_ms == 60);

  if (config.bitrate_bps)
    SetTargetBitrate(*config.bitrate_bps);
  if (config.frame_length_ms)
    SetFrameLength(*config.frame_length_ms);
  if (config.enable_fec)
    SetFec(*config.enable_fec);
  if (config.uplink_packet_loss_fraction)
    SetProjectedPacketLossRate(*config.uplink_packet_loss_fraction);
  if (config.enable_dtx)
    SetDtx(*config.enable_dtx);
  if (config.num_channels)
    SetNumChannelsToEncode(*config.num_channels);
}

std::unique_ptr<AudioNetworkAdaptor>
AudioEncoderOpus::DefaultAudioNetworkAdaptorCreator(
    const std::string& config_string,
    RtcEventLog* event_log,
    const Clock* clock) const {
  AudioNetworkAdaptorImpl::Config config;
  config.clock = clock;
  config.event_log = event_log;
  return std::unique_ptr<AudioNetworkAdaptor>(new AudioNetworkAdaptorImpl(
      config,
      ControllerManagerImpl::Create(
          config_string, NumChannels(), supported_frame_lengths_ms(),
          kMinBitrateBps, num_channels_to_encode_, next_frame_length_ms_,
          GetTargetBitrate(), config_.fec_enabled, GetDtx(), clock)));
}

void AudioEncoderOpus::MaybeUpdateUplinkBandwidth() {
  if (audio_network_adaptor_) {
    int64_t now_ms = rtc::TimeMillis();
    if (!bitrate_smoother_last_update_time_ ||
        now_ms - *bitrate_smoother_last_update_time_ >=
            config_.uplink_bandwidth_update_interval_ms) {
      rtc::Optional<float> smoothed_bitrate = bitrate_smoother_->GetAverage();
      if (smoothed_bitrate)
        audio_network_adaptor_->SetUplinkBandwidth(*smoothed_bitrate);
      bitrate_smoother_last_update_time_ = rtc::Optional<int64_t>(now_ms);
    }
  }
}

}  // namespace webrtc
