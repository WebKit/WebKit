/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_test.h"

#include <iostream>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"

namespace webrtc {
namespace test {
namespace {

absl::optional<Operations> ActionToOperations(
    absl::optional<NetEqSimulator::Action> a) {
  if (!a) {
    return absl::nullopt;
  }
  switch (*a) {
    case NetEqSimulator::Action::kAccelerate:
      return absl::make_optional(kAccelerate);
    case NetEqSimulator::Action::kExpand:
      return absl::make_optional(kExpand);
    case NetEqSimulator::Action::kNormal:
      return absl::make_optional(kNormal);
    case NetEqSimulator::Action::kPreemptiveExpand:
      return absl::make_optional(kPreemptiveExpand);
  }
}

}  // namespace

void DefaultNetEqTestErrorCallback::OnInsertPacketError(
    const NetEqInput::PacketData& packet) {
  std::cerr << "InsertPacket returned an error." << std::endl;
  std::cerr << "Packet data: " << packet.ToString() << std::endl;
  FATAL();
}

void DefaultNetEqTestErrorCallback::OnGetAudioError() {
  std::cerr << "GetAudio returned an error." << std::endl;
  FATAL();
}

NetEqTest::NetEqTest(const NetEq::Config& config,
                     const DecoderMap& codecs,
                     const ExtDecoderMap& ext_codecs,
                     std::unique_ptr<NetEqInput> input,
                     std::unique_ptr<AudioSink> output,
                     Callbacks callbacks)
    : neteq_(NetEq::Create(config, CreateBuiltinAudioDecoderFactory())),
      input_(std::move(input)),
      output_(std::move(output)),
      callbacks_(callbacks),
      sample_rate_hz_(config.sample_rate_hz) {
  RTC_CHECK(!config.enable_muted_state)
      << "The code does not handle enable_muted_state";
  RegisterDecoders(codecs);
  RegisterExternalDecoders(ext_codecs);
}

NetEqTest::~NetEqTest() = default;

int64_t NetEqTest::Run() {
  int64_t simulation_time = 0;
  SimulationStepResult step_result;
  do {
    step_result = RunToNextGetAudio();
    simulation_time += step_result.simulation_step_ms;
  } while (!step_result.is_simulation_finished);
  if (callbacks_.simulation_ended_callback) {
    callbacks_.simulation_ended_callback->SimulationEnded(simulation_time);
  }
  return simulation_time;
}

NetEqTest::SimulationStepResult NetEqTest::RunToNextGetAudio() {
  SimulationStepResult result;
  const int64_t start_time_ms = *input_->NextEventTime();
  int64_t time_now_ms = start_time_ms;
  current_state_.packet_iat_ms.clear();

  while (!input_->ended()) {
    // Advance time to next event.
    RTC_DCHECK(input_->NextEventTime());
    time_now_ms = *input_->NextEventTime();
    // Check if it is time to insert packet.
    if (input_->NextPacketTime() && time_now_ms >= *input_->NextPacketTime()) {
      std::unique_ptr<NetEqInput::PacketData> packet_data = input_->PopPacket();
      RTC_CHECK(packet_data);
      const size_t payload_data_length =
          packet_data->payload.size() - packet_data->header.paddingLength;
      if (payload_data_length != 0) {
        int error = neteq_->InsertPacket(
            packet_data->header,
            rtc::ArrayView<const uint8_t>(packet_data->payload),
            static_cast<uint32_t>(packet_data->time_ms * sample_rate_hz_ /
                                  1000));
        if (error != NetEq::kOK && callbacks_.error_callback) {
          callbacks_.error_callback->OnInsertPacketError(*packet_data);
        }
        if (callbacks_.post_insert_packet) {
          callbacks_.post_insert_packet->AfterInsertPacket(*packet_data,
                                                           neteq_.get());
        }
      } else {
        neteq_->InsertEmptyPacket(packet_data->header);
      }
      if (last_packet_time_ms_) {
        current_state_.packet_iat_ms.push_back(time_now_ms -
                                               *last_packet_time_ms_);
      }
      last_packet_time_ms_ = absl::make_optional<int>(time_now_ms);
    }

    // Check if it is time to get output audio.
    if (input_->NextOutputEventTime() &&
        time_now_ms >= *input_->NextOutputEventTime()) {
      if (callbacks_.get_audio_callback) {
        callbacks_.get_audio_callback->BeforeGetAudio(neteq_.get());
      }
      AudioFrame out_frame;
      bool muted;
      int error = neteq_->GetAudio(&out_frame, &muted,
                                   ActionToOperations(next_action_));
      next_action_ = absl::nullopt;
      RTC_CHECK(!muted) << "The code does not handle enable_muted_state";
      if (error != NetEq::kOK) {
        if (callbacks_.error_callback) {
          callbacks_.error_callback->OnGetAudioError();
        }
      } else {
        sample_rate_hz_ = out_frame.sample_rate_hz_;
      }
      if (callbacks_.get_audio_callback) {
        callbacks_.get_audio_callback->AfterGetAudio(time_now_ms, out_frame,
                                                     muted, neteq_.get());
      }

      if (output_) {
        RTC_CHECK(output_->WriteArray(
            out_frame.data(),
            out_frame.samples_per_channel_ * out_frame.num_channels_));
      }

      input_->AdvanceOutputEvent();
      result.simulation_step_ms =
          input_->NextEventTime().value_or(time_now_ms) - start_time_ms;
      const auto operations_state = neteq_->GetOperationsAndState();
      current_state_.current_delay_ms = operations_state.current_buffer_size_ms;
      current_state_.packet_size_ms = operations_state.current_frame_size_ms;
      current_state_.next_packet_available =
          operations_state.next_packet_available;
      current_state_.packet_buffer_flushed =
          operations_state.packet_buffer_flushes >
          prev_ops_state_.packet_buffer_flushes;
      // TODO(ivoc): Add more accurate reporting by tracking the origin of
      // samples in the sync buffer.
      result.action_times_ms[Action::kExpand] = 0;
      result.action_times_ms[Action::kAccelerate] = 0;
      result.action_times_ms[Action::kPreemptiveExpand] = 0;
      result.action_times_ms[Action::kNormal] = 0;

      if (out_frame.speech_type_ == AudioFrame::SpeechType::kPLC ||
          out_frame.speech_type_ == AudioFrame::SpeechType::kPLCCNG) {
        // Consider the whole frame to be the result of expansion.
        result.action_times_ms[Action::kExpand] = 10;
      } else if (operations_state.accelerate_samples -
                     prev_ops_state_.accelerate_samples >
                 0) {
        // Consider the whole frame to be the result of acceleration.
        result.action_times_ms[Action::kAccelerate] = 10;
      } else if (operations_state.preemptive_samples -
                     prev_ops_state_.preemptive_samples >
                 0) {
        // Consider the whole frame to be the result of preemptive expansion.
        result.action_times_ms[Action::kPreemptiveExpand] = 10;
      } else {
        // Consider the whole frame to be the result of normal playout.
        result.action_times_ms[Action::kNormal] = 10;
      }
      result.is_simulation_finished = input_->ended();
      prev_ops_state_ = operations_state;
      return result;
    }
  }
  result.simulation_step_ms =
      input_->NextEventTime().value_or(time_now_ms) - start_time_ms;
  result.is_simulation_finished = true;
  return result;
}

void NetEqTest::SetNextAction(NetEqTest::Action next_operation) {
  next_action_ = absl::optional<Action>(next_operation);
}

NetEqTest::NetEqState NetEqTest::GetNetEqState() {
  return current_state_;
}

NetEqNetworkStatistics NetEqTest::SimulationStats() {
  NetEqNetworkStatistics stats;
  RTC_CHECK_EQ(neteq_->NetworkStatistics(&stats), 0);
  return stats;
}

NetEqLifetimeStatistics NetEqTest::LifetimeStats() const {
  return neteq_->GetLifetimeStatistics();
}

NetEqTest::DecoderMap NetEqTest::StandardDecoderMap() {
  DecoderMap codecs = {
    {0, std::make_pair(NetEqDecoder::kDecoderPCMu, "pcmu")},
    {8, std::make_pair(NetEqDecoder::kDecoderPCMa, "pcma")},
#ifdef WEBRTC_CODEC_ILBC
    {102, std::make_pair(NetEqDecoder::kDecoderILBC, "ilbc")},
#endif
    {103, std::make_pair(NetEqDecoder::kDecoderISAC, "isac")},
#if !defined(WEBRTC_ANDROID)
    {104, std::make_pair(NetEqDecoder::kDecoderISACswb, "isac-swb")},
#endif
#ifdef WEBRTC_CODEC_OPUS
    {111, std::make_pair(NetEqDecoder::kDecoderOpus, "opus")},
#endif
    {93, std::make_pair(NetEqDecoder::kDecoderPCM16B, "pcm16-nb")},
    {94, std::make_pair(NetEqDecoder::kDecoderPCM16Bwb, "pcm16-wb")},
    {95, std::make_pair(NetEqDecoder::kDecoderPCM16Bswb32kHz, "pcm16-swb32")},
    {96, std::make_pair(NetEqDecoder::kDecoderPCM16Bswb48kHz, "pcm16-swb48")},
    {9, std::make_pair(NetEqDecoder::kDecoderG722, "g722")},
    {106, std::make_pair(NetEqDecoder::kDecoderAVT, "avt")},
    {114, std::make_pair(NetEqDecoder::kDecoderAVT16kHz, "avt-16")},
    {115, std::make_pair(NetEqDecoder::kDecoderAVT32kHz, "avt-32")},
    {116, std::make_pair(NetEqDecoder::kDecoderAVT48kHz, "avt-48")},
    {117, std::make_pair(NetEqDecoder::kDecoderRED, "red")},
    {13, std::make_pair(NetEqDecoder::kDecoderCNGnb, "cng-nb")},
    {98, std::make_pair(NetEqDecoder::kDecoderCNGwb, "cng-wb")},
    {99, std::make_pair(NetEqDecoder::kDecoderCNGswb32kHz, "cng-swb32")},
    {100, std::make_pair(NetEqDecoder::kDecoderCNGswb48kHz, "cng-swb48")}
  };
  return codecs;
}

void NetEqTest::RegisterDecoders(const DecoderMap& codecs) {
  for (const auto& c : codecs) {
    RTC_CHECK_EQ(
        neteq_->RegisterPayloadType(c.second.first, c.second.second, c.first),
        NetEq::kOK)
        << "Cannot register " << c.second.second << " to payload type "
        << c.first;
  }
}

void NetEqTest::RegisterExternalDecoders(const ExtDecoderMap& codecs) {
  for (const auto& c : codecs) {
    RTC_CHECK_EQ(
        neteq_->RegisterExternalDecoder(c.second.decoder, c.second.codec,
                                        c.second.codec_name, c.first),
        NetEq::kOK)
        << "Cannot register " << c.second.codec_name << " to payload type "
        << c.first;
  }
}

}  // namespace test
}  // namespace webrtc
