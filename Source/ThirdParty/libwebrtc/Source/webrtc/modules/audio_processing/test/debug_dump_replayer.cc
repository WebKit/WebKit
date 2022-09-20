/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/debug_dump_replayer.h"

#include <string>

#include "absl/strings/string_view.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "modules/audio_processing/test/protobuf_utils.h"
#include "modules/audio_processing/test/runtime_setting_util.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

namespace {

void MaybeResetBuffer(std::unique_ptr<ChannelBuffer<float>>* buffer,
                      const StreamConfig& config) {
  auto& buffer_ref = *buffer;
  if (!buffer_ref.get() || buffer_ref->num_frames() != config.num_frames() ||
      buffer_ref->num_channels() != config.num_channels()) {
    buffer_ref.reset(
        new ChannelBuffer<float>(config.num_frames(), config.num_channels()));
  }
}

}  // namespace

DebugDumpReplayer::DebugDumpReplayer()
    : input_(nullptr),  // will be created upon usage.
      reverse_(nullptr),
      output_(nullptr),
      apm_(nullptr),
      debug_file_(nullptr) {}

DebugDumpReplayer::~DebugDumpReplayer() {
  if (debug_file_)
    fclose(debug_file_);
}

bool DebugDumpReplayer::SetDumpFile(absl::string_view filename) {
  debug_file_ = fopen(std::string(filename).c_str(), "rb");
  LoadNextMessage();
  return debug_file_;
}

// Get next event that has not run.
absl::optional<audioproc::Event> DebugDumpReplayer::GetNextEvent() const {
  if (!has_next_event_)
    return absl::nullopt;
  else
    return next_event_;
}

// Run the next event. Returns the event type.
bool DebugDumpReplayer::RunNextEvent() {
  if (!has_next_event_)
    return false;
  switch (next_event_.type()) {
    case audioproc::Event::INIT:
      OnInitEvent(next_event_.init());
      break;
    case audioproc::Event::STREAM:
      OnStreamEvent(next_event_.stream());
      break;
    case audioproc::Event::REVERSE_STREAM:
      OnReverseStreamEvent(next_event_.reverse_stream());
      break;
    case audioproc::Event::CONFIG:
      OnConfigEvent(next_event_.config());
      break;
    case audioproc::Event::RUNTIME_SETTING:
      OnRuntimeSettingEvent(next_event_.runtime_setting());
      break;
    case audioproc::Event::UNKNOWN_EVENT:
      // We do not expect to receive UNKNOWN event.
      RTC_CHECK_NOTREACHED();
  }
  LoadNextMessage();
  return true;
}

const ChannelBuffer<float>* DebugDumpReplayer::GetOutput() const {
  return output_.get();
}

StreamConfig DebugDumpReplayer::GetOutputConfig() const {
  return output_config_;
}

// OnInitEvent reset the input/output/reserve channel format.
void DebugDumpReplayer::OnInitEvent(const audioproc::Init& msg) {
  RTC_CHECK(msg.has_num_input_channels());
  RTC_CHECK(msg.has_output_sample_rate());
  RTC_CHECK(msg.has_num_output_channels());
  RTC_CHECK(msg.has_reverse_sample_rate());
  RTC_CHECK(msg.has_num_reverse_channels());

  input_config_ = StreamConfig(msg.sample_rate(), msg.num_input_channels());
  output_config_ =
      StreamConfig(msg.output_sample_rate(), msg.num_output_channels());
  reverse_config_ =
      StreamConfig(msg.reverse_sample_rate(), msg.num_reverse_channels());

  MaybeResetBuffer(&input_, input_config_);
  MaybeResetBuffer(&output_, output_config_);
  MaybeResetBuffer(&reverse_, reverse_config_);
}

// OnStreamEvent replays an input signal and verifies the output.
void DebugDumpReplayer::OnStreamEvent(const audioproc::Stream& msg) {
  // APM should have been created.
  RTC_CHECK(apm_.get());

  apm_->set_stream_analog_level(msg.level());
  RTC_CHECK_EQ(AudioProcessing::kNoError,
               apm_->set_stream_delay_ms(msg.delay()));

  if (msg.has_keypress()) {
    apm_->set_stream_key_pressed(msg.keypress());
  } else {
    apm_->set_stream_key_pressed(true);
  }

  RTC_CHECK_EQ(input_config_.num_channels(),
               static_cast<size_t>(msg.input_channel_size()));
  RTC_CHECK_EQ(input_config_.num_frames() * sizeof(float),
               msg.input_channel(0).size());

  for (int i = 0; i < msg.input_channel_size(); ++i) {
    memcpy(input_->channels()[i], msg.input_channel(i).data(),
           msg.input_channel(i).size());
  }

  RTC_CHECK_EQ(AudioProcessing::kNoError,
               apm_->ProcessStream(input_->channels(), input_config_,
                                   output_config_, output_->channels()));
}

void DebugDumpReplayer::OnReverseStreamEvent(
    const audioproc::ReverseStream& msg) {
  // APM should have been created.
  RTC_CHECK(apm_.get());

  RTC_CHECK_GT(msg.channel_size(), 0);
  RTC_CHECK_EQ(reverse_config_.num_channels(),
               static_cast<size_t>(msg.channel_size()));
  RTC_CHECK_EQ(reverse_config_.num_frames() * sizeof(float),
               msg.channel(0).size());

  for (int i = 0; i < msg.channel_size(); ++i) {
    memcpy(reverse_->channels()[i], msg.channel(i).data(),
           msg.channel(i).size());
  }

  RTC_CHECK_EQ(
      AudioProcessing::kNoError,
      apm_->ProcessReverseStream(reverse_->channels(), reverse_config_,
                                 reverse_config_, reverse_->channels()));
}

void DebugDumpReplayer::OnConfigEvent(const audioproc::Config& msg) {
  MaybeRecreateApm(msg);
  ConfigureApm(msg);
}

void DebugDumpReplayer::OnRuntimeSettingEvent(
    const audioproc::RuntimeSetting& msg) {
  RTC_CHECK(apm_.get());
  ReplayRuntimeSetting(apm_.get(), msg);
}

void DebugDumpReplayer::MaybeRecreateApm(const audioproc::Config& msg) {
  // These configurations cannot be changed on the fly.
  RTC_CHECK(msg.has_aec_delay_agnostic_enabled());
  RTC_CHECK(msg.has_aec_extended_filter_enabled());

  // We only create APM once, since changes on these fields should not
  // happen in current implementation.
  if (!apm_.get()) {
    apm_ = AudioProcessingBuilderForTesting().Create();
  }
}

void DebugDumpReplayer::ConfigureApm(const audioproc::Config& msg) {
  AudioProcessing::Config apm_config;

  // AEC2/AECM configs.
  RTC_CHECK(msg.has_aec_enabled());
  RTC_CHECK(msg.has_aecm_enabled());
  apm_config.echo_canceller.enabled = msg.aec_enabled() || msg.aecm_enabled();
  apm_config.echo_canceller.mobile_mode = msg.aecm_enabled();

  // HPF configs.
  RTC_CHECK(msg.has_hpf_enabled());
  apm_config.high_pass_filter.enabled = msg.hpf_enabled();

  // Preamp configs.
  RTC_CHECK(msg.has_pre_amplifier_enabled());
  apm_config.pre_amplifier.enabled = msg.pre_amplifier_enabled();
  apm_config.pre_amplifier.fixed_gain_factor =
      msg.pre_amplifier_fixed_gain_factor();

  // NS configs.
  RTC_CHECK(msg.has_ns_enabled());
  RTC_CHECK(msg.has_ns_level());
  apm_config.noise_suppression.enabled = msg.ns_enabled();
  apm_config.noise_suppression.level =
      static_cast<AudioProcessing::Config::NoiseSuppression::Level>(
          msg.ns_level());

  // TS configs.
  RTC_CHECK(msg.has_transient_suppression_enabled());
  apm_config.transient_suppression.enabled =
      msg.transient_suppression_enabled();

  // AGC configs.
  RTC_CHECK(msg.has_agc_enabled());
  RTC_CHECK(msg.has_agc_mode());
  RTC_CHECK(msg.has_agc_limiter_enabled());
  apm_config.gain_controller1.enabled = msg.agc_enabled();
  apm_config.gain_controller1.mode =
      static_cast<AudioProcessing::Config::GainController1::Mode>(
          msg.agc_mode());
  apm_config.gain_controller1.enable_limiter = msg.agc_limiter_enabled();
  RTC_CHECK(msg.has_noise_robust_agc_enabled());
  apm_config.gain_controller1.analog_gain_controller.enabled =
      msg.noise_robust_agc_enabled();

  apm_->ApplyConfig(apm_config);
}

void DebugDumpReplayer::LoadNextMessage() {
  has_next_event_ =
      debug_file_ && ReadMessageFromFile(debug_file_, &next_event_);
}

}  // namespace test
}  // namespace webrtc
