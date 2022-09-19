/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fuzzers/utils/rtp_replayer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "rtc_base/strings/json.h"
#include "system_wrappers/include/clock.h"
#include "test/call_config_utils.h"
#include "test/encoder_settings.h"
#include "test/fake_decoder.h"
#include "test/rtp_file_reader.h"
#include "test/rtp_header_parser.h"
#include "test/run_loop.h"

namespace webrtc {
namespace test {

void RtpReplayer::Replay(const std::string& replay_config_filepath,
                         const uint8_t* rtp_dump_data,
                         size_t rtp_dump_size) {
  auto stream_state = std::make_unique<StreamState>();
  std::vector<VideoReceiveStreamInterface::Config> receive_stream_configs =
      ReadConfigFromFile(replay_config_filepath, &(stream_state->transport));
  return Replay(std::move(stream_state), std::move(receive_stream_configs),
                rtp_dump_data, rtp_dump_size);
}

void RtpReplayer::Replay(
    std::unique_ptr<StreamState> stream_state,
    std::vector<VideoReceiveStreamInterface::Config> receive_stream_configs,
    const uint8_t* rtp_dump_data,
    size_t rtp_dump_size) {
  RunLoop loop;
  rtc::ScopedBaseFakeClock fake_clock;

  // Work around: webrtc calls webrtc::Random(clock.TimeInMicroseconds())
  // everywhere and Random expects non-zero seed. Let's set the clock non-zero
  // to make them happy.
  fake_clock.SetTime(webrtc::Timestamp::Millis(1));

  // Attempt to create an RtpReader from the input file.
  auto rtp_reader = CreateRtpReader(rtp_dump_data, rtp_dump_size);
  if (rtp_reader == nullptr) {
    RTC_LOG(LS_ERROR) << "Failed to create the rtp_reader";
    return;
  }

  // Setup the video streams based on the configuration.
  webrtc::RtcEventLogNull event_log;
  std::unique_ptr<TaskQueueFactory> task_queue_factory =
      CreateDefaultTaskQueueFactory();
  Call::Config call_config(&event_log);
  call_config.task_queue_factory = task_queue_factory.get();
  FieldTrialBasedConfig field_trials;
  call_config.trials = &field_trials;
  std::unique_ptr<Call> call(Call::Create(call_config));
  SetupVideoStreams(&receive_stream_configs, stream_state.get(), call.get());

  // Start replaying the provided stream now that it has been configured.
  for (const auto& receive_stream : stream_state->receive_streams) {
    receive_stream->Start();
  }

  ReplayPackets(&fake_clock, call.get(), rtp_reader.get());

  for (const auto& receive_stream : stream_state->receive_streams) {
    call->DestroyVideoReceiveStream(receive_stream);
  }
}

std::vector<VideoReceiveStreamInterface::Config>
RtpReplayer::ReadConfigFromFile(const std::string& replay_config,
                                Transport* transport) {
  Json::CharReaderBuilder factory;
  std::unique_ptr<Json::CharReader> json_reader =
      absl::WrapUnique(factory.newCharReader());
  Json::Value json_configs;
  Json::String errors;
  if (!json_reader->parse(replay_config.data(),
                          replay_config.data() + replay_config.length(),
                          &json_configs, &errors)) {
    RTC_LOG(LS_ERROR)
        << "Error parsing JSON replay configuration for the fuzzer: " << errors;
    return {};
  }

  std::vector<VideoReceiveStreamInterface::Config> receive_stream_configs;
  receive_stream_configs.reserve(json_configs.size());
  for (const auto& json : json_configs) {
    receive_stream_configs.push_back(
        ParseVideoReceiveStreamJsonConfig(transport, json));
  }
  return receive_stream_configs;
}

void RtpReplayer::SetupVideoStreams(
    std::vector<VideoReceiveStreamInterface::Config>* receive_stream_configs,
    StreamState* stream_state,
    Call* call) {
  stream_state->decoder_factory = std::make_unique<InternalDecoderFactory>();
  for (auto& receive_config : *receive_stream_configs) {
    // Attach the decoder for the corresponding payload type in the config.
    for (auto& decoder : receive_config.decoders) {
      decoder = test::CreateMatchingDecoder(decoder.payload_type,
                                            decoder.video_format.name);
    }

    // Create the window to display the rendered video.
    stream_state->sinks.emplace_back(
        test::VideoRenderer::Create("Fuzzing WebRTC Video Config", 640, 480));
    // Create a receive stream for this config.
    receive_config.renderer = stream_state->sinks.back().get();
    receive_config.decoder_factory = stream_state->decoder_factory.get();
    stream_state->receive_streams.emplace_back(
        call->CreateVideoReceiveStream(std::move(receive_config)));
  }
}

std::unique_ptr<test::RtpFileReader> RtpReplayer::CreateRtpReader(
    const uint8_t* rtp_dump_data,
    size_t rtp_dump_size) {
  std::unique_ptr<test::RtpFileReader> rtp_reader(test::RtpFileReader::Create(
      test::RtpFileReader::kRtpDump, rtp_dump_data, rtp_dump_size, {}));
  if (!rtp_reader) {
    RTC_LOG(LS_ERROR) << "Unable to open input file with any supported format";
    return nullptr;
  }
  return rtp_reader;
}

void RtpReplayer::ReplayPackets(rtc::FakeClock* clock,
                                Call* call,
                                test::RtpFileReader* rtp_reader) {
  int64_t replay_start_ms = -1;
  int num_packets = 0;
  std::map<uint32_t, int> unknown_packets;

  while (true) {
    int64_t now_ms = rtc::TimeMillis();
    if (replay_start_ms == -1) {
      replay_start_ms = now_ms;
    }

    test::RtpPacket packet;
    if (!rtp_reader->NextPacket(&packet)) {
      break;
    }

    int64_t deliver_in_ms = replay_start_ms + packet.time_ms - now_ms;
    if (deliver_in_ms > 0) {
      // StatsCounter::ReportMetricToAggregatedCounter is O(elapsed time).
      // Set an upper limit to prevent waste time.
      clock->AdvanceTime(webrtc::TimeDelta::Millis(
          std::min(deliver_in_ms, static_cast<int64_t>(100))));
    }

    ++num_packets;
    switch (call->Receiver()->DeliverPacket(
        webrtc::MediaType::VIDEO,
        rtc::CopyOnWriteBuffer(packet.data, packet.length),
        /* packet_time_us */ -1)) {
      case PacketReceiver::DELIVERY_OK:
        break;
      case PacketReceiver::DELIVERY_UNKNOWN_SSRC: {
        RTPHeader header;
        std::unique_ptr<RtpHeaderParser> parser(
            RtpHeaderParser::CreateForTest());

        parser->Parse(packet.data, packet.length, &header);
        if (unknown_packets[header.ssrc] == 0) {
          RTC_LOG(LS_ERROR) << "Unknown SSRC: " << header.ssrc;
        }
        ++unknown_packets[header.ssrc];
        break;
      }
      case PacketReceiver::DELIVERY_PACKET_ERROR: {
        RTC_LOG(LS_ERROR)
            << "Packet error, corrupt packets or incorrect setup?";
        RTPHeader header;
        std::unique_ptr<RtpHeaderParser> parser(
            RtpHeaderParser::CreateForTest());
        parser->Parse(packet.data, packet.length, &header);
        RTC_LOG(LS_ERROR) << "Packet packet_length=" << packet.length
                          << " payload_type=" << header.payloadType
                          << " sequence_number=" << header.sequenceNumber
                          << " time_stamp=" << header.timestamp
                          << " ssrc=" << header.ssrc;
        break;
      }
    }
  }
  RTC_LOG(LS_INFO) << "num_packets: " << num_packets;

  for (const auto& unknown_packet : unknown_packets) {
    RTC_LOG(LS_ERROR) << "Packets for unknown ssrc " << unknown_packet.first
                      << ":" << unknown_packet.second;
  }
}

}  // namespace test
}  // namespace webrtc
