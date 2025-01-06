/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/aec_dump_based_simulator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "api/audio/audio_processing.h"
#include "api/scoped_refptr.h"
#include "common_audio/channel_buffer.h"
#include "common_audio/wav_file.h"
#include "modules/audio_processing/debug.pb.h"
#include "modules/audio_processing/echo_control_mobile_impl.h"
#include "modules/audio_processing/test/aec_dump_based_simulator.h"
#include "modules/audio_processing/test/audio_processing_simulator.h"
#include "modules/audio_processing/test/protobuf_utils.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace test {
namespace {

// Verify output bitexactness for the fixed interface.
// TODO(peah): Check whether it would make sense to add a threshold
// to use for checking the bitexactness in a soft manner.
bool VerifyFixedBitExactness(const webrtc::audioproc::Stream& msg,
                             const Int16Frame& frame) {
  if (sizeof(frame.data[0]) * frame.data.size() != msg.output_data().size()) {
    return false;
  } else {
    const int16_t* frame_data = frame.data.data();
    for (int k = 0; k < frame.num_channels * frame.samples_per_channel; ++k) {
      if (msg.output_data().data()[k] != frame_data[k]) {
        return false;
      }
    }
  }
  return true;
}

// Verify output bitexactness for the float interface.
bool VerifyFloatBitExactness(const webrtc::audioproc::Stream& msg,
                             const StreamConfig& out_config,
                             const ChannelBuffer<float>& out_buf) {
  if (static_cast<size_t>(msg.output_channel_size()) !=
          out_config.num_channels() ||
      msg.output_channel(0).size() != out_config.num_frames()) {
    return false;
  } else {
    for (int ch = 0; ch < msg.output_channel_size(); ++ch) {
      for (size_t sample = 0; sample < out_config.num_frames(); ++sample) {
        if (msg.output_channel(ch).data()[sample] !=
            out_buf.channels()[ch][sample]) {
          return false;
        }
      }
    }
  }
  return true;
}

// Selectively reads the next proto-buf message from dump-file or string input.
// Returns a bool indicating whether a new message was available.
bool ReadNextMessage(bool use_dump_file,
                     FILE* dump_input_file,
                     std::stringstream& input,
                     webrtc::audioproc::Event& event_msg) {
  if (use_dump_file) {
    return ReadMessageFromFile(dump_input_file, &event_msg);
  }
  return ReadMessageFromString(&input, &event_msg);
}

}  // namespace

AecDumpBasedSimulator::AecDumpBasedSimulator(
    const SimulationSettings& settings,
    absl::Nonnull<scoped_refptr<AudioProcessing>> audio_processing)
    : AudioProcessingSimulator(settings, std::move(audio_processing)) {
  MaybeOpenCallOrderFile();
}

AecDumpBasedSimulator::~AecDumpBasedSimulator() = default;

void AecDumpBasedSimulator::PrepareProcessStreamCall(
    const webrtc::audioproc::Stream& msg) {
  if (msg.has_input_data()) {
    // Fixed interface processing.
    // Verify interface invariance.
    RTC_CHECK(interface_used_ == InterfaceType::kFixedInterface ||
              interface_used_ == InterfaceType::kNotSpecified);
    interface_used_ = InterfaceType::kFixedInterface;

    // Populate input buffer.
    RTC_CHECK_EQ(sizeof(fwd_frame_.data[0]) * fwd_frame_.data.size(),
                 msg.input_data().size());
    memcpy(fwd_frame_.data.data(), msg.input_data().data(),
           msg.input_data().size());
  } else {
    // Float interface processing.
    // Verify interface invariance.
    RTC_CHECK(interface_used_ == InterfaceType::kFloatInterface ||
              interface_used_ == InterfaceType::kNotSpecified);
    interface_used_ = InterfaceType::kFloatInterface;

    RTC_CHECK_EQ(in_buf_->num_channels(),
                 static_cast<size_t>(msg.input_channel_size()));

    // Populate input buffer.
    for (size_t i = 0; i < in_buf_->num_channels(); ++i) {
      RTC_CHECK_EQ(in_buf_->num_frames() * sizeof(*in_buf_->channels()[i]),
                   msg.input_channel(i).size());
      std::memcpy(in_buf_->channels()[i], msg.input_channel(i).data(),
                  msg.input_channel(i).size());
    }
  }

  if (artificial_nearend_buffer_reader_) {
    if (artificial_nearend_buffer_reader_->Read(
            artificial_nearend_buf_.get())) {
      if (msg.has_input_data()) {
        int16_t* fwd_frame_data = fwd_frame_.data.data();
        for (size_t k = 0; k < in_buf_->num_frames(); ++k) {
          fwd_frame_data[k] = rtc::saturated_cast<int16_t>(
              fwd_frame_data[k] +
              static_cast<int16_t>(32767 *
                                   artificial_nearend_buf_->channels()[0][k]));
        }
      } else {
        for (int i = 0; i < msg.input_channel_size(); ++i) {
          for (size_t k = 0; k < in_buf_->num_frames(); ++k) {
            in_buf_->channels()[i][k] +=
                artificial_nearend_buf_->channels()[0][k];
            in_buf_->channels()[i][k] = std::min(
                32767.f, std::max(-32768.f, in_buf_->channels()[i][k]));
          }
        }
      }
    } else {
      if (!artificial_nearend_eof_reported_) {
        std::cout << "The artificial nearend file ended before the recording.";
        artificial_nearend_eof_reported_ = true;
      }
    }
  }

  if (!settings_.use_stream_delay || *settings_.use_stream_delay) {
    if (!settings_.stream_delay) {
      if (msg.has_delay()) {
        RTC_CHECK_EQ(AudioProcessing::kNoError,
                     ap_->set_stream_delay_ms(msg.delay()));
      }
    } else {
      RTC_CHECK_EQ(AudioProcessing::kNoError,
                   ap_->set_stream_delay_ms(*settings_.stream_delay));
    }
  }

  if (settings_.override_key_pressed.has_value()) {
    // Key pressed state overridden.
    ap_->set_stream_key_pressed(*settings_.override_key_pressed);
  } else {
    // Set the recorded key pressed state.
    if (msg.has_keypress()) {
      ap_->set_stream_key_pressed(msg.keypress());
    }
  }

  // Set the applied input level if available.
  aec_dump_applied_input_level_ =
      msg.has_applied_input_volume()
          ? std::optional<int>(msg.applied_input_volume())
          : std::nullopt;
}

void AecDumpBasedSimulator::VerifyProcessStreamBitExactness(
    const webrtc::audioproc::Stream& msg) {
  if (bitexact_output_) {
    if (interface_used_ == InterfaceType::kFixedInterface) {
      bitexact_output_ = VerifyFixedBitExactness(msg, fwd_frame_);
    } else {
      bitexact_output_ = VerifyFloatBitExactness(msg, out_config_, *out_buf_);
    }
  }
}

void AecDumpBasedSimulator::PrepareReverseProcessStreamCall(
    const webrtc::audioproc::ReverseStream& msg) {
  if (msg.has_data()) {
    // Fixed interface processing.
    // Verify interface invariance.
    RTC_CHECK(interface_used_ == InterfaceType::kFixedInterface ||
              interface_used_ == InterfaceType::kNotSpecified);
    interface_used_ = InterfaceType::kFixedInterface;

    // Populate input buffer.
    RTC_CHECK_EQ(sizeof(rev_frame_.data[0]) * rev_frame_.data.size(),
                 msg.data().size());
    memcpy(rev_frame_.data.data(), msg.data().data(), msg.data().size());
  } else {
    // Float interface processing.
    // Verify interface invariance.
    RTC_CHECK(interface_used_ == InterfaceType::kFloatInterface ||
              interface_used_ == InterfaceType::kNotSpecified);
    interface_used_ = InterfaceType::kFloatInterface;

    RTC_CHECK_EQ(reverse_in_buf_->num_channels(),
                 static_cast<size_t>(msg.channel_size()));

    // Populate input buffer.
    for (int i = 0; i < msg.channel_size(); ++i) {
      RTC_CHECK_EQ(reverse_in_buf_->num_frames() *
                       sizeof(*reverse_in_buf_->channels()[i]),
                   msg.channel(i).size());
      std::memcpy(reverse_in_buf_->channels()[i], msg.channel(i).data(),
                  msg.channel(i).size());
    }
  }
}

void AecDumpBasedSimulator::Process() {
  ConfigureAudioProcessor();

  if (settings_.artificial_nearend_filename) {
    std::unique_ptr<WavReader> artificial_nearend_file(
        new WavReader(settings_.artificial_nearend_filename->c_str()));

    RTC_CHECK_EQ(1, artificial_nearend_file->num_channels())
        << "Only mono files for the artificial nearend are supported, "
           "reverted to not using the artificial nearend file";

    const int sample_rate_hz = artificial_nearend_file->sample_rate();
    artificial_nearend_buffer_reader_.reset(
        new ChannelBufferWavReader(std::move(artificial_nearend_file)));
    artificial_nearend_buf_.reset(new ChannelBuffer<float>(
        rtc::CheckedDivExact(sample_rate_hz, kChunksPerSecond), 1));
  }

  const bool use_dump_file = !settings_.aec_dump_input_string.has_value();
  std::stringstream input;
  if (use_dump_file) {
    dump_input_file_ =
        OpenFile(settings_.aec_dump_input_filename->c_str(), "rb");
  } else {
    input << settings_.aec_dump_input_string.value();
  }

  webrtc::audioproc::Event event_msg;
  int capture_frames_since_init = 0;
  int init_index = 0;
  while (ReadNextMessage(use_dump_file, dump_input_file_, input, event_msg)) {
    SelectivelyToggleDataDumping(init_index, capture_frames_since_init);
    HandleEvent(event_msg, capture_frames_since_init, init_index);

    // Perfom an early exit if the init block to process has been fully
    // processed
    if (finished_processing_specified_init_block_) {
      break;
    }
    RTC_CHECK(!settings_.init_to_process ||
              *settings_.init_to_process >= init_index);
  }

  if (use_dump_file) {
    fclose(dump_input_file_);
  }

  DetachAecDump();
}

void AecDumpBasedSimulator::Analyze() {
  const bool use_dump_file = !settings_.aec_dump_input_string.has_value();
  std::stringstream input;
  if (use_dump_file) {
    dump_input_file_ =
        OpenFile(settings_.aec_dump_input_filename->c_str(), "rb");
  } else {
    input << settings_.aec_dump_input_string.value();
  }

  webrtc::audioproc::Event event_msg;
  int num_capture_frames = 0;
  int num_render_frames = 0;
  int init_index = 0;
  while (ReadNextMessage(use_dump_file, dump_input_file_, input, event_msg)) {
    if (event_msg.type() == webrtc::audioproc::Event::INIT) {
      ++init_index;
      constexpr float kNumFramesPerSecond = 100.f;
      float capture_time_seconds = num_capture_frames / kNumFramesPerSecond;
      float render_time_seconds = num_render_frames / kNumFramesPerSecond;

      std::cout << "Inits:" << std::endl;
      std::cout << init_index << ": -->" << std::endl;
      std::cout << " Time:" << std::endl;
      std::cout << "  Capture: " << capture_time_seconds << " s ("
                << num_capture_frames << " frames) " << std::endl;
      std::cout << "  Render: " << render_time_seconds << " s ("
                << num_render_frames << " frames) " << std::endl;
    } else if (event_msg.type() == webrtc::audioproc::Event::STREAM) {
      ++num_capture_frames;
    } else if (event_msg.type() == webrtc::audioproc::Event::REVERSE_STREAM) {
      ++num_render_frames;
    }
  }

  if (use_dump_file) {
    fclose(dump_input_file_);
  }
}

void AecDumpBasedSimulator::HandleEvent(
    const webrtc::audioproc::Event& event_msg,
    int& capture_frames_since_init,
    int& init_index) {
  switch (event_msg.type()) {
    case webrtc::audioproc::Event::INIT:
      RTC_CHECK(event_msg.has_init());
      ++init_index;
      capture_frames_since_init = 0;
      HandleMessage(event_msg.init(), init_index);
      break;
    case webrtc::audioproc::Event::STREAM:
      RTC_CHECK(event_msg.has_stream());
      ++capture_frames_since_init;
      HandleMessage(event_msg.stream());
      break;
    case webrtc::audioproc::Event::REVERSE_STREAM:
      RTC_CHECK(event_msg.has_reverse_stream());
      HandleMessage(event_msg.reverse_stream());
      break;
    case webrtc::audioproc::Event::CONFIG:
      RTC_CHECK(event_msg.has_config());
      HandleMessage(event_msg.config());
      break;
    case webrtc::audioproc::Event::RUNTIME_SETTING:
      HandleMessage(event_msg.runtime_setting());
      break;
    case webrtc::audioproc::Event::UNKNOWN_EVENT:
      RTC_CHECK_NOTREACHED();
  }
}

void AecDumpBasedSimulator::HandleMessage(
    const webrtc::audioproc::Config& msg) {
  if (settings_.use_verbose_logging) {
    std::cout << "Config at frame:" << std::endl;
    std::cout << " Forward: " << get_num_process_stream_calls() << std::endl;
    std::cout << " Reverse: " << get_num_reverse_process_stream_calls()
              << std::endl;
  }

  if (!settings_.discard_all_settings_in_aecdump) {
    if (settings_.use_verbose_logging) {
      std::cout << "Setting used in config:" << std::endl;
    }
    AudioProcessing::Config apm_config = ap_->GetConfig();

    if (msg.has_aec_enabled() || settings_.use_aec) {
      bool enable = settings_.use_aec ? *settings_.use_aec : msg.aec_enabled();
      apm_config.echo_canceller.enabled = enable;
      if (settings_.use_verbose_logging) {
        std::cout << " aec_enabled: " << (enable ? "true" : "false")
                  << std::endl;
      }
    }

    if (msg.has_aecm_enabled() || settings_.use_aecm) {
      bool enable =
          settings_.use_aecm ? *settings_.use_aecm : msg.aecm_enabled();
      apm_config.echo_canceller.enabled |= enable;
      apm_config.echo_canceller.mobile_mode = enable;
      if (settings_.use_verbose_logging) {
        std::cout << " aecm_enabled: " << (enable ? "true" : "false")
                  << std::endl;
      }
    }

    if (msg.has_aecm_comfort_noise_enabled() &&
        msg.aecm_comfort_noise_enabled()) {
      RTC_LOG(LS_ERROR) << "Ignoring deprecated setting: AECM comfort noise";
    }

    if (msg.has_aecm_routing_mode() &&
        static_cast<webrtc::EchoControlMobileImpl::RoutingMode>(
            msg.aecm_routing_mode()) != EchoControlMobileImpl::kSpeakerphone) {
      RTC_LOG(LS_ERROR) << "Ignoring deprecated setting: AECM routing mode: "
                        << msg.aecm_routing_mode();
    }

    if (msg.has_agc_enabled() || settings_.use_agc) {
      bool enable = settings_.use_agc ? *settings_.use_agc : msg.agc_enabled();
      apm_config.gain_controller1.enabled = enable;
      if (settings_.use_verbose_logging) {
        std::cout << " agc_enabled: " << (enable ? "true" : "false")
                  << std::endl;
      }
    }

    if (msg.has_agc_mode() || settings_.agc_mode) {
      int mode = settings_.agc_mode ? *settings_.agc_mode : msg.agc_mode();
      apm_config.gain_controller1.mode =
          static_cast<webrtc::AudioProcessing::Config::GainController1::Mode>(
              mode);
      if (settings_.use_verbose_logging) {
        std::cout << " agc_mode: " << mode << std::endl;
      }
    }

    if (msg.has_agc_limiter_enabled() || settings_.use_agc_limiter) {
      bool enable = settings_.use_agc_limiter ? *settings_.use_agc_limiter
                                              : msg.agc_limiter_enabled();
      apm_config.gain_controller1.enable_limiter = enable;
      if (settings_.use_verbose_logging) {
        std::cout << " agc_limiter_enabled: " << (enable ? "true" : "false")
                  << std::endl;
      }
    }

    if (settings_.use_agc2) {
      bool enable = *settings_.use_agc2;
      apm_config.gain_controller2.enabled = enable;
      if (settings_.agc2_fixed_gain_db) {
        apm_config.gain_controller2.fixed_digital.gain_db =
            *settings_.agc2_fixed_gain_db;
      }
      if (settings_.use_verbose_logging) {
        std::cout << " agc2_enabled: " << (enable ? "true" : "false")
                  << std::endl;
      }
    }

    if (msg.has_noise_robust_agc_enabled()) {
      apm_config.gain_controller1.analog_gain_controller.enabled =
          settings_.use_analog_agc ? *settings_.use_analog_agc
                                   : msg.noise_robust_agc_enabled();
      if (settings_.use_verbose_logging) {
        std::cout << " noise_robust_agc_enabled: "
                  << (msg.noise_robust_agc_enabled() ? "true" : "false")
                  << std::endl;
      }
    }

    if (msg.has_transient_suppression_enabled() || settings_.use_ts) {
      bool enable = settings_.use_ts ? *settings_.use_ts
                                     : msg.transient_suppression_enabled();
      apm_config.transient_suppression.enabled = enable;
      if (settings_.use_verbose_logging) {
        std::cout << " transient_suppression_enabled: "
                  << (enable ? "true" : "false") << std::endl;
      }
    }

    if (msg.has_hpf_enabled() || settings_.use_hpf) {
      bool enable = settings_.use_hpf ? *settings_.use_hpf : msg.hpf_enabled();
      apm_config.high_pass_filter.enabled = enable;
      if (settings_.use_verbose_logging) {
        std::cout << " hpf_enabled: " << (enable ? "true" : "false")
                  << std::endl;
      }
    }

    if (msg.has_ns_enabled() || settings_.use_ns) {
      bool enable = settings_.use_ns ? *settings_.use_ns : msg.ns_enabled();
      apm_config.noise_suppression.enabled = enable;
      if (settings_.use_verbose_logging) {
        std::cout << " ns_enabled: " << (enable ? "true" : "false")
                  << std::endl;
      }
    }

    if (msg.has_ns_level() || settings_.ns_level) {
      int level = settings_.ns_level ? *settings_.ns_level : msg.ns_level();
      apm_config.noise_suppression.level =
          static_cast<AudioProcessing::Config::NoiseSuppression::Level>(level);
      if (settings_.use_verbose_logging) {
        std::cout << " ns_level: " << level << std::endl;
      }
    }

    if (msg.has_pre_amplifier_enabled() || settings_.use_pre_amplifier) {
      const bool enable = settings_.use_pre_amplifier
                              ? *settings_.use_pre_amplifier
                              : msg.pre_amplifier_enabled();
      apm_config.pre_amplifier.enabled = enable;
    }

    if (msg.has_pre_amplifier_fixed_gain_factor() ||
        settings_.pre_amplifier_gain_factor) {
      const float gain = settings_.pre_amplifier_gain_factor
                             ? *settings_.pre_amplifier_gain_factor
                             : msg.pre_amplifier_fixed_gain_factor();
      apm_config.pre_amplifier.fixed_gain_factor = gain;
    }

    if (settings_.use_verbose_logging && msg.has_experiments_description() &&
        !msg.experiments_description().empty()) {
      std::cout << " experiments not included by default in the simulation: "
                << msg.experiments_description() << std::endl;
    }

    ap_->ApplyConfig(apm_config);
  }
}

void AecDumpBasedSimulator::HandleMessage(const webrtc::audioproc::Init& msg,
                                          int init_index) {
  RTC_CHECK(msg.has_sample_rate());
  RTC_CHECK(msg.has_num_input_channels());
  RTC_CHECK(msg.has_num_reverse_channels());
  RTC_CHECK(msg.has_reverse_sample_rate());

  // Do not perform the init if the init block to process is fully processed
  if (settings_.init_to_process && *settings_.init_to_process < init_index) {
    finished_processing_specified_init_block_ = true;
  }

  MaybeOpenCallOrderFile();

  if (settings_.use_verbose_logging) {
    std::cout << "Init at frame:" << std::endl;
    std::cout << " Forward: " << get_num_process_stream_calls() << std::endl;
    std::cout << " Reverse: " << get_num_reverse_process_stream_calls()
              << std::endl;
  }

  int num_output_channels;
  if (settings_.output_num_channels) {
    num_output_channels = *settings_.output_num_channels;
  } else {
    num_output_channels = msg.has_num_output_channels()
                              ? msg.num_output_channels()
                              : msg.num_input_channels();
  }

  int output_sample_rate;
  if (settings_.output_sample_rate_hz) {
    output_sample_rate = *settings_.output_sample_rate_hz;
  } else {
    output_sample_rate = msg.has_output_sample_rate() ? msg.output_sample_rate()
                                                      : msg.sample_rate();
  }

  int num_reverse_output_channels;
  if (settings_.reverse_output_num_channels) {
    num_reverse_output_channels = *settings_.reverse_output_num_channels;
  } else {
    num_reverse_output_channels = msg.has_num_reverse_output_channels()
                                      ? msg.num_reverse_output_channels()
                                      : msg.num_reverse_channels();
  }

  int reverse_output_sample_rate;
  if (settings_.reverse_output_sample_rate_hz) {
    reverse_output_sample_rate = *settings_.reverse_output_sample_rate_hz;
  } else {
    reverse_output_sample_rate = msg.has_reverse_output_sample_rate()
                                     ? msg.reverse_output_sample_rate()
                                     : msg.reverse_sample_rate();
  }

  SetupBuffersConfigsOutputs(
      msg.sample_rate(), output_sample_rate, msg.reverse_sample_rate(),
      reverse_output_sample_rate, msg.num_input_channels(), num_output_channels,
      msg.num_reverse_channels(), num_reverse_output_channels);
}

void AecDumpBasedSimulator::HandleMessage(
    const webrtc::audioproc::Stream& msg) {
  if (call_order_output_file_) {
    *call_order_output_file_ << "c";
  }
  PrepareProcessStreamCall(msg);
  ProcessStream(interface_used_ == InterfaceType::kFixedInterface);
  VerifyProcessStreamBitExactness(msg);
}

void AecDumpBasedSimulator::HandleMessage(
    const webrtc::audioproc::ReverseStream& msg) {
  if (call_order_output_file_) {
    *call_order_output_file_ << "r";
  }
  PrepareReverseProcessStreamCall(msg);
  ProcessReverseStream(interface_used_ == InterfaceType::kFixedInterface);
}

void AecDumpBasedSimulator::HandleMessage(
    const webrtc::audioproc::RuntimeSetting& msg) {
  RTC_CHECK(ap_.get());
  if (msg.has_capture_pre_gain()) {
    // Handle capture pre-gain runtime setting only if not overridden.
    const bool pre_amplifier_overridden =
        (!settings_.use_pre_amplifier || *settings_.use_pre_amplifier) &&
        !settings_.pre_amplifier_gain_factor;
    const bool capture_level_adjustment_overridden =
        (!settings_.use_capture_level_adjustment ||
         *settings_.use_capture_level_adjustment) &&
        !settings_.pre_gain_factor;
    if (pre_amplifier_overridden || capture_level_adjustment_overridden) {
      ap_->SetRuntimeSetting(
          AudioProcessing::RuntimeSetting::CreateCapturePreGain(
              msg.capture_pre_gain()));
    }
  } else if (msg.has_capture_post_gain()) {
    // Handle capture post-gain runtime setting only if not overridden.
    if ((!settings_.use_capture_level_adjustment ||
         *settings_.use_capture_level_adjustment) &&
        !settings_.post_gain_factor) {
      ap_->SetRuntimeSetting(
          AudioProcessing::RuntimeSetting::CreateCapturePreGain(
              msg.capture_pre_gain()));
    }
  } else if (msg.has_capture_fixed_post_gain()) {
    // Handle capture fixed-post-gain runtime setting only if not overridden.
    if ((!settings_.use_agc2 || *settings_.use_agc2) &&
        !settings_.agc2_fixed_gain_db) {
      ap_->SetRuntimeSetting(
          AudioProcessing::RuntimeSetting::CreateCaptureFixedPostGain(
              msg.capture_fixed_post_gain()));
    }
  } else if (msg.has_playout_volume_change()) {
    ap_->SetRuntimeSetting(
        AudioProcessing::RuntimeSetting::CreatePlayoutVolumeChange(
            msg.playout_volume_change()));
  } else if (msg.has_playout_audio_device_change()) {
    ap_->SetRuntimeSetting(
        AudioProcessing::RuntimeSetting::CreatePlayoutAudioDeviceChange(
            {msg.playout_audio_device_change().id(),
             msg.playout_audio_device_change().max_volume()}));
  } else if (msg.has_capture_output_used()) {
    ap_->SetRuntimeSetting(
        AudioProcessing::RuntimeSetting::CreateCaptureOutputUsedSetting(
            msg.capture_output_used()));
  }
}

void AecDumpBasedSimulator::MaybeOpenCallOrderFile() {
  if (settings_.call_order_output_filename.has_value()) {
    const std::string filename = settings_.store_intermediate_output
                                     ? *settings_.call_order_output_filename +
                                           "_" +
                                           std::to_string(output_reset_counter_)
                                     : *settings_.call_order_output_filename;
    call_order_output_file_ = std::make_unique<std::ofstream>(filename);
  }
}

}  // namespace test
}  // namespace webrtc
