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
#include "api/environment/environment_factory.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/strings/json.h"
#include "system_wrappers/include/clock.h"
#include "test/call_config_utils.h"
#include "test/encoder_settings.h"
#include "test/fake_decoder.h"
#include "test/rtp_file_reader.h"
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

  RtpHeaderExtensionMap extensions(/*extmap_allow_mixed=*/true);
  // Skip i = 0 since it maps to kRtpExtensionNone.
  for (int i = 1; i < kRtpExtensionNumberOfExtensions; i++) {
    RTPExtensionType extension_type = static_cast<RTPExtensionType>(i);
    // Extensions are registered with an ID, which you signal to the
    // peer so they know what to expect. This code only cares about
    // parsing so the value of the ID isn't relevant.
    extensions.RegisterByType(i, extension_type);
  }

  // Setup the video streams based on the configuration.
  CallConfig call_config(CreateEnvironment());
  std::unique_ptr<Call> call(Call::Create(call_config));
  SetupVideoStreams(&receive_stream_configs, stream_state.get(), call.get());

  // Start replaying the provided stream now that it has been configured.
  for (const auto& receive_stream : stream_state->receive_streams) {
    receive_stream->Start();
  }

  ReplayPackets(&fake_clock, call.get(), rtp_reader.get(), extensions);

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

void RtpReplayer::ReplayPackets(
    rtc::FakeClock* clock,
    Call* call,
    test::RtpFileReader* rtp_reader,
    const RtpPacketReceived::ExtensionManager& extensions) {
  int64_t replay_start_ms = -1;

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

    RtpPacketReceived received_packet(
        &extensions, Timestamp::Micros(clock->TimeNanos() / 1000));
    if (!received_packet.Parse(packet.data, packet.length)) {
      RTC_LOG(LS_ERROR) << "Packet error, corrupt packets or incorrect setup?";
      break;
    }
#ifdef WEBRTC_WEBKIT_BUILD
    // Set the clock rate if zero - always 90K for video
    if (received_packet.payload_type_frequency() == 0) {
        received_packet.set_payload_type_frequency(kVideoPayloadTypeFrequency);
    }
#else
    // Set the clock rate - always 90K for video
    received_packet.set_payload_type_frequency(kVideoPayloadTypeFrequency);
#endif

    call->Receiver()->DeliverRtpPacket(
        MediaType::VIDEO, std::move(received_packet),
        [&](const RtpPacketReceived& parsed_packet) {
          RTC_LOG(LS_ERROR) << "Unknown SSRC: " << parsed_packet.Ssrc();
          return false;
        });
  }
}
}  // namespace test
}  // namespace webrtc
