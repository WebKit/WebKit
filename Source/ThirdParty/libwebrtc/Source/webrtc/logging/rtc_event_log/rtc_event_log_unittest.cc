/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "webrtc/base/buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/fakeclock.h"
#include "webrtc/base/random.h"
#include "webrtc/call/call.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_parser.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log_unittest_helper.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

// Files generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#endif

namespace webrtc {

namespace {

const RTPExtensionType kExtensionTypes[] = {
    RTPExtensionType::kRtpExtensionTransmissionTimeOffset,
    RTPExtensionType::kRtpExtensionAudioLevel,
    RTPExtensionType::kRtpExtensionAbsoluteSendTime,
    RTPExtensionType::kRtpExtensionVideoRotation,
    RTPExtensionType::kRtpExtensionTransportSequenceNumber};
const char* kExtensionNames[] = {
    RtpExtension::kTimestampOffsetUri, RtpExtension::kAudioLevelUri,
    RtpExtension::kAbsSendTimeUri, RtpExtension::kVideoRotationUri,
    RtpExtension::kTransportSequenceNumberUri};
const size_t kNumExtensions = 5;

void PrintActualEvents(const ParsedRtcEventLog& parsed_log) {
  std::map<int, size_t> actual_event_counts;
  for (size_t i = 0; i < parsed_log.GetNumberOfEvents(); i++) {
    actual_event_counts[parsed_log.GetEventType(i)]++;
  }
  printf("Actual events: ");
  for (auto kv : actual_event_counts) {
    printf("%d_count = %zu, ", kv.first, kv.second);
  }
  printf("\n");
  for (size_t i = 0; i < parsed_log.GetNumberOfEvents(); i++) {
    printf("%4d ", parsed_log.GetEventType(i));
  }
  printf("\n");
}

void PrintExpectedEvents(size_t rtp_count,
                         size_t rtcp_count,
                         size_t playout_count,
                         size_t bwe_loss_count) {
  printf(
      "Expected events: rtp_count = %zu, rtcp_count = %zu,"
      "playout_count = %zu, bwe_loss_count = %zu\n",
      rtp_count, rtcp_count, playout_count, bwe_loss_count);
  size_t rtcp_index = 1, playout_index = 1, bwe_loss_index = 1;
  printf("strt  cfg  cfg ");
  for (size_t i = 1; i <= rtp_count; i++) {
    printf(" rtp ");
    if (i * rtcp_count >= rtcp_index * rtp_count) {
      printf("rtcp ");
      rtcp_index++;
    }
    if (i * playout_count >= playout_index * rtp_count) {
      printf("play ");
      playout_index++;
    }
    if (i * bwe_loss_count >= bwe_loss_index * rtp_count) {
      printf("loss ");
      bwe_loss_index++;
    }
  }
  printf("end \n");
}
}  // namespace

/*
 * Bit number i of extension_bitvector is set to indicate the
 * presence of extension number i from kExtensionTypes / kExtensionNames.
 * The least significant bit extension_bitvector has number 0.
 */
RtpPacketToSend GenerateRtpPacket(const RtpHeaderExtensionMap* extensions,
                                  uint32_t csrcs_count,
                                  size_t packet_size,
                                  Random* prng) {
  RTC_CHECK_GE(packet_size, 16 + 4 * csrcs_count + 4 * kNumExtensions);

  std::vector<uint32_t> csrcs;
  for (unsigned i = 0; i < csrcs_count; i++) {
    csrcs.push_back(prng->Rand<uint32_t>());
  }

  RtpPacketToSend rtp_packet(extensions, packet_size);
  rtp_packet.SetPayloadType(prng->Rand(127));
  rtp_packet.SetMarker(prng->Rand<bool>());
  rtp_packet.SetSequenceNumber(prng->Rand<uint16_t>());
  rtp_packet.SetSsrc(prng->Rand<uint32_t>());
  rtp_packet.SetTimestamp(prng->Rand<uint32_t>());
  rtp_packet.SetCsrcs(csrcs);

  rtp_packet.SetExtension<TransmissionOffset>(prng->Rand(0x00ffffff));
  rtp_packet.SetExtension<AudioLevel>(prng->Rand<bool>(), prng->Rand(127));
  rtp_packet.SetExtension<AbsoluteSendTime>(prng->Rand<int32_t>());
  rtp_packet.SetExtension<VideoOrientation>(prng->Rand(2));
  rtp_packet.SetExtension<TransportSequenceNumber>(prng->Rand<uint16_t>());

  size_t payload_size = packet_size - rtp_packet.headers_size();
  uint8_t* payload = rtp_packet.AllocatePayload(payload_size);
  for (size_t i = 0; i < payload_size; i++) {
    payload[i] = prng->Rand<uint8_t>();
  }
  return rtp_packet;
}

rtc::Buffer GenerateRtcpPacket(Random* prng) {
  rtcp::ReportBlock report_block;
  report_block.SetMediaSsrc(prng->Rand<uint32_t>());  // Remote SSRC.
  report_block.SetFractionLost(prng->Rand(50));

  rtcp::SenderReport sender_report;
  sender_report.SetSenderSsrc(prng->Rand<uint32_t>());
  sender_report.SetNtp(NtpTime(prng->Rand<uint32_t>(), prng->Rand<uint32_t>()));
  sender_report.SetPacketCount(prng->Rand<uint32_t>());
  sender_report.AddReportBlock(report_block);

  return sender_report.Build();
}

void GenerateVideoReceiveConfig(uint32_t extensions_bitvector,
                                VideoReceiveStream::Config* config,
                                Random* prng) {
  // Create a map from a payload type to an encoder name.
  VideoReceiveStream::Decoder decoder;
  decoder.payload_type = prng->Rand(0, 127);
  decoder.payload_name = (prng->Rand<bool>() ? "VP8" : "H264");
  config->decoders.push_back(decoder);
  // Add SSRCs for the stream.
  config->rtp.remote_ssrc = prng->Rand<uint32_t>();
  config->rtp.local_ssrc = prng->Rand<uint32_t>();
  // Add extensions and settings for RTCP.
  config->rtp.rtcp_mode =
      prng->Rand<bool>() ? RtcpMode::kCompound : RtcpMode::kReducedSize;
  config->rtp.remb = prng->Rand<bool>();
  config->rtp.rtx_ssrc = prng->Rand<uint32_t>();
  // Add a map from a payload type to a new payload type for RTX.
  config->rtp.rtx_payload_types.insert(
      std::make_pair(prng->Rand(0, 127), prng->Rand(0, 127)));
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    if (extensions_bitvector & (1u << i)) {
      config->rtp.extensions.push_back(
          RtpExtension(kExtensionNames[i], prng->Rand<int>()));
    }
  }
}

void GenerateVideoSendConfig(uint32_t extensions_bitvector,
                             VideoSendStream::Config* config,
                             Random* prng) {
  // Create a map from a payload type to an encoder name.
  config->encoder_settings.payload_type = prng->Rand(0, 127);
  config->encoder_settings.payload_name = (prng->Rand<bool>() ? "VP8" : "H264");
  // Add SSRCs for the stream.
  config->rtp.ssrcs.push_back(prng->Rand<uint32_t>());
  // Add a map from a payload type to new ssrcs and a new payload type for RTX.
  config->rtp.rtx.ssrcs.push_back(prng->Rand<uint32_t>());
  config->rtp.rtx.payload_type = prng->Rand(0, 127);
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    if (extensions_bitvector & (1u << i)) {
      config->rtp.extensions.push_back(
          RtpExtension(kExtensionNames[i], prng->Rand<int>()));
    }
  }
}

void GenerateAudioReceiveConfig(uint32_t extensions_bitvector,
                                AudioReceiveStream::Config* config,
                                Random* prng) {
  // Add SSRCs for the stream.
  config->rtp.remote_ssrc = prng->Rand<uint32_t>();
  config->rtp.local_ssrc = prng->Rand<uint32_t>();
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    if (extensions_bitvector & (1u << i)) {
      config->rtp.extensions.push_back(
          RtpExtension(kExtensionNames[i], prng->Rand<int>()));
    }
  }
}

void GenerateAudioSendConfig(uint32_t extensions_bitvector,
                             AudioSendStream::Config* config,
                             Random* prng) {
  // Add SSRC to the stream.
  config->rtp.ssrc = prng->Rand<uint32_t>();
  // Add header extensions.
  for (unsigned i = 0; i < kNumExtensions; i++) {
    if (extensions_bitvector & (1u << i)) {
      config->rtp.extensions.push_back(
          RtpExtension(kExtensionNames[i], prng->Rand<int>()));
    }
  }
}

void GenerateAudioNetworkAdaptation(
    uint32_t extensions_bitvector,
    AudioNetworkAdaptor::EncoderRuntimeConfig* config,
    Random* prng) {
  config->bitrate_bps = rtc::Optional<int>(prng->Rand(0, 3000000));
  config->enable_fec = rtc::Optional<bool>(prng->Rand<bool>());
  config->enable_dtx = rtc::Optional<bool>(prng->Rand<bool>());
  config->frame_length_ms = rtc::Optional<int>(prng->Rand(10, 120));
  config->num_channels = rtc::Optional<size_t>(prng->Rand(1, 2));
  config->uplink_packet_loss_fraction =
      rtc::Optional<float>(prng->Rand<float>());
}

// Test for the RtcEventLog class. Dumps some RTP packets and other events
// to disk, then reads them back to see if they match.
void LogSessionAndReadBack(size_t rtp_count,
                           size_t rtcp_count,
                           size_t playout_count,
                           size_t bwe_loss_count,
                           uint32_t extensions_bitvector,
                           uint32_t csrcs_count,
                           unsigned int random_seed) {
  ASSERT_LE(rtcp_count, rtp_count);
  ASSERT_LE(playout_count, rtp_count);
  ASSERT_LE(bwe_loss_count, rtp_count);
  std::vector<RtpPacketToSend> rtp_packets;
  std::vector<rtc::Buffer> rtcp_packets;
  std::vector<uint32_t> playout_ssrcs;
  std::vector<std::pair<int32_t, uint8_t> > bwe_loss_updates;

  VideoReceiveStream::Config receiver_config(nullptr);
  VideoSendStream::Config sender_config(nullptr);

  Random prng(random_seed);

  // Initialize rtp header extensions to be used in generated rtp packets.
  RtpHeaderExtensionMap extensions;
  for (unsigned i = 0; i < kNumExtensions; i++) {
    if (extensions_bitvector & (1u << i)) {
      extensions.Register(kExtensionTypes[i], i + 1);
    }
  }
  // Create rtp_count RTP packets containing random data.
  for (size_t i = 0; i < rtp_count; i++) {
    size_t packet_size = prng.Rand(1000, 1100);
    rtp_packets.push_back(
        GenerateRtpPacket(&extensions, csrcs_count, packet_size, &prng));
  }
  // Create rtcp_count RTCP packets containing random data.
  for (size_t i = 0; i < rtcp_count; i++) {
    rtcp_packets.push_back(GenerateRtcpPacket(&prng));
  }
  // Create playout_count random SSRCs to use when logging AudioPlayout events.
  for (size_t i = 0; i < playout_count; i++) {
    playout_ssrcs.push_back(prng.Rand<uint32_t>());
  }
  // Create bwe_loss_count random bitrate updates for LossBasedBwe.
  for (size_t i = 0; i < bwe_loss_count; i++) {
    bwe_loss_updates.push_back(
        std::make_pair(prng.Rand<int32_t>(), prng.Rand<uint8_t>()));
  }
  // Create configurations for the video streams.
  GenerateVideoReceiveConfig(extensions_bitvector, &receiver_config, &prng);
  GenerateVideoSendConfig(extensions_bitvector, &sender_config, &prng);
  const int config_count = 2;

  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  // When log_dumper goes out of scope, it causes the log file to be flushed
  // to disk.
  {
    rtc::ScopedFakeClock fake_clock;
    fake_clock.SetTimeMicros(prng.Rand<uint32_t>());
    std::unique_ptr<RtcEventLog> log_dumper(RtcEventLog::Create());
    log_dumper->LogVideoReceiveStreamConfig(receiver_config);
    fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
    log_dumper->LogVideoSendStreamConfig(sender_config);
    fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
    size_t rtcp_index = 1;
    size_t playout_index = 1;
    size_t bwe_loss_index = 1;
    for (size_t i = 1; i <= rtp_count; i++) {
      log_dumper->LogRtpHeader(
          (i % 2 == 0) ? kIncomingPacket : kOutgoingPacket,
          (i % 3 == 0) ? MediaType::AUDIO : MediaType::VIDEO,
          rtp_packets[i - 1].data(), rtp_packets[i - 1].size());
      fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
      if (i * rtcp_count >= rtcp_index * rtp_count) {
        log_dumper->LogRtcpPacket(
            (rtcp_index % 2 == 0) ? kIncomingPacket : kOutgoingPacket,
            rtcp_index % 3 == 0 ? MediaType::AUDIO : MediaType::VIDEO,
            rtcp_packets[rtcp_index - 1].data(),
            rtcp_packets[rtcp_index - 1].size());
        rtcp_index++;
        fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
      }
      if (i * playout_count >= playout_index * rtp_count) {
        log_dumper->LogAudioPlayout(playout_ssrcs[playout_index - 1]);
        playout_index++;
        fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
      }
      if (i * bwe_loss_count >= bwe_loss_index * rtp_count) {
        log_dumper->LogLossBasedBweUpdate(
            bwe_loss_updates[bwe_loss_index - 1].first,
            bwe_loss_updates[bwe_loss_index - 1].second, i);
        bwe_loss_index++;
        fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
      }
      if (i == rtp_count / 2) {
        log_dumper->StartLogging(temp_filename, 10000000);
        fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
      }
    }
    log_dumper->StopLogging();
  }

  // Read the generated file from disk.
  ParsedRtcEventLog parsed_log;

  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

  // Verify that what we read back from the event log is the same as
  // what we wrote down. For RTCP we log the full packets, but for
  // RTP we should only log the header.
  const size_t event_count = config_count + playout_count + bwe_loss_count +
                             rtcp_count + rtp_count + 2;
  EXPECT_GE(1000u, event_count);  // The events must fit in the message queue.
  EXPECT_EQ(event_count, parsed_log.GetNumberOfEvents());
  if (event_count != parsed_log.GetNumberOfEvents()) {
    // Print the expected and actual event types for easier debugging.
    PrintActualEvents(parsed_log);
    PrintExpectedEvents(rtp_count, rtcp_count, playout_count, bwe_loss_count);
  }
  RtcEventLogTestHelper::VerifyLogStartEvent(parsed_log, 0);
  RtcEventLogTestHelper::VerifyVideoReceiveStreamConfig(parsed_log, 1,
                                                        receiver_config);
  RtcEventLogTestHelper::VerifyVideoSendStreamConfig(parsed_log, 2,
                                                     sender_config);
  size_t event_index = config_count + 1;
  size_t rtcp_index = 1;
  size_t playout_index = 1;
  size_t bwe_loss_index = 1;
  for (size_t i = 1; i <= rtp_count; i++) {
    RtcEventLogTestHelper::VerifyRtpEvent(
        parsed_log, event_index,
        (i % 2 == 0) ? kIncomingPacket : kOutgoingPacket,
        (i % 3 == 0) ? MediaType::AUDIO : MediaType::VIDEO,
        rtp_packets[i - 1].data(), rtp_packets[i - 1].headers_size(),
        rtp_packets[i - 1].size());
    event_index++;
    if (i * rtcp_count >= rtcp_index * rtp_count) {
      RtcEventLogTestHelper::VerifyRtcpEvent(
          parsed_log, event_index,
          rtcp_index % 2 == 0 ? kIncomingPacket : kOutgoingPacket,
          rtcp_index % 3 == 0 ? MediaType::AUDIO : MediaType::VIDEO,
          rtcp_packets[rtcp_index - 1].data(),
          rtcp_packets[rtcp_index - 1].size());
      event_index++;
      rtcp_index++;
    }
    if (i * playout_count >= playout_index * rtp_count) {
      RtcEventLogTestHelper::VerifyPlayoutEvent(
          parsed_log, event_index, playout_ssrcs[playout_index - 1]);
      event_index++;
      playout_index++;
    }
    if (i * bwe_loss_count >= bwe_loss_index * rtp_count) {
      RtcEventLogTestHelper::VerifyBweLossEvent(
          parsed_log, event_index, bwe_loss_updates[bwe_loss_index - 1].first,
          bwe_loss_updates[bwe_loss_index - 1].second, i);
      event_index++;
      bwe_loss_index++;
    }
  }

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

TEST(RtcEventLogTest, LogSessionAndReadBack) {
  // Log 5 RTP, 2 RTCP, 0 playout events and 0 BWE events
  // with no header extensions or CSRCS.
  LogSessionAndReadBack(5, 2, 0, 0, 0, 0, 321);

  // Enable AbsSendTime and TransportSequenceNumbers.
  uint32_t extensions = 0;
  for (uint32_t i = 0; i < kNumExtensions; i++) {
    if (kExtensionTypes[i] == RTPExtensionType::kRtpExtensionAbsoluteSendTime ||
        kExtensionTypes[i] ==
            RTPExtensionType::kRtpExtensionTransportSequenceNumber) {
      extensions |= 1u << i;
    }
  }
  LogSessionAndReadBack(8, 2, 0, 0, extensions, 0, 3141592653u);

  extensions = (1u << kNumExtensions) - 1;  // Enable all header extensions.
  LogSessionAndReadBack(9, 2, 3, 2, extensions, 2, 2718281828u);

  // Try all combinations of header extensions and up to 2 CSRCS.
  for (extensions = 0; extensions < (1u << kNumExtensions); extensions++) {
    for (uint32_t csrcs_count = 0; csrcs_count < 3; csrcs_count++) {
      LogSessionAndReadBack(5 + extensions,   // Number of RTP packets.
                            2 + csrcs_count,  // Number of RTCP packets.
                            3 + csrcs_count,  // Number of playout events.
                            1 + csrcs_count,  // Number of BWE loss events.
                            extensions,       // Bit vector choosing extensions.
                            csrcs_count,      // Number of contributing sources.
                            extensions * 3 + csrcs_count + 1);  // Random seed.
    }
  }
}

TEST(RtcEventLogTest, LogEventAndReadBack) {
  Random prng(987654321);

  // Create one RTP and one RTCP packet containing random data.
  size_t packet_size = prng.Rand(1000, 1100);
  RtpPacketToSend rtp_packet =
      GenerateRtpPacket(nullptr, 0, packet_size, &prng);
  rtc::Buffer rtcp_packet = GenerateRtcpPacket(&prng);

  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  // Add RTP, start logging, add RTCP and then stop logging
  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTimeMicros(prng.Rand<uint32_t>());
  std::unique_ptr<RtcEventLog> log_dumper(RtcEventLog::Create());

  log_dumper->LogRtpHeader(kIncomingPacket, MediaType::VIDEO, rtp_packet.data(),
                           rtp_packet.size());
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));

  log_dumper->StartLogging(temp_filename, 10000000);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));

  log_dumper->LogRtcpPacket(kOutgoingPacket, MediaType::VIDEO,
                            rtcp_packet.data(), rtcp_packet.size());
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));

  log_dumper->StopLogging();

  // Read the generated file from disk.
  ParsedRtcEventLog parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

  // Verify that what we read back from the event log is the same as
  // what we wrote down.
  EXPECT_EQ(4u, parsed_log.GetNumberOfEvents());

  RtcEventLogTestHelper::VerifyLogStartEvent(parsed_log, 0);

  RtcEventLogTestHelper::VerifyRtpEvent(
      parsed_log, 1, kIncomingPacket, MediaType::VIDEO, rtp_packet.data(),
      rtp_packet.headers_size(), rtp_packet.size());

  RtcEventLogTestHelper::VerifyRtcpEvent(parsed_log, 2, kOutgoingPacket,
                                         MediaType::VIDEO, rtcp_packet.data(),
                                         rtcp_packet.size());

  RtcEventLogTestHelper::VerifyLogEndEvent(parsed_log, 3);

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

TEST(RtcEventLogTest, LogLossBasedBweUpdateAndReadBack) {
  Random prng(1234);

  // Generate a random packet loss event.
  int32_t bitrate = prng.Rand(0, 10000000);
  uint8_t fraction_lost = prng.Rand<uint8_t>();
  int32_t total_packets = prng.Rand(1, 1000);

  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  // Start logging, add the packet loss event and then stop logging.
  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTimeMicros(prng.Rand<uint32_t>());
  std::unique_ptr<RtcEventLog> log_dumper(RtcEventLog::Create());
  log_dumper->StartLogging(temp_filename, 10000000);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogLossBasedBweUpdate(bitrate, fraction_lost, total_packets);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->StopLogging();

  // Read the generated file from disk.
  ParsedRtcEventLog parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

  // Verify that what we read back from the event log is the same as
  // what we wrote down.
  EXPECT_EQ(3u, parsed_log.GetNumberOfEvents());
  RtcEventLogTestHelper::VerifyLogStartEvent(parsed_log, 0);
  RtcEventLogTestHelper::VerifyBweLossEvent(parsed_log, 1, bitrate,
                                            fraction_lost, total_packets);
  RtcEventLogTestHelper::VerifyLogEndEvent(parsed_log, 2);

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

TEST(RtcEventLogTest, LogDelayBasedBweUpdateAndReadBack) {
  Random prng(1234);

  // Generate 3 random packet delay event.
  int32_t bitrate1 = prng.Rand(0, 10000000);
  int32_t bitrate2 = prng.Rand(0, 10000000);
  int32_t bitrate3 = prng.Rand(0, 10000000);

  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  // Start logging, add the packet delay events and then stop logging.
  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTimeMicros(prng.Rand<uint32_t>());
  std::unique_ptr<RtcEventLog> log_dumper(RtcEventLog::Create());
  log_dumper->StartLogging(temp_filename, 10000000);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogDelayBasedBweUpdate(bitrate1, kBwNormal);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogDelayBasedBweUpdate(bitrate2, kBwOverusing);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogDelayBasedBweUpdate(bitrate3, kBwUnderusing);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->StopLogging();

  // Read the generated file from disk.
  ParsedRtcEventLog parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

  // Verify that what we read back from the event log is the same as
  // what we wrote down.
  EXPECT_EQ(5u, parsed_log.GetNumberOfEvents());
  RtcEventLogTestHelper::VerifyLogStartEvent(parsed_log, 0);
  RtcEventLogTestHelper::VerifyBweDelayEvent(parsed_log, 1, bitrate1,
                                             kBwNormal);
  RtcEventLogTestHelper::VerifyBweDelayEvent(parsed_log, 2, bitrate2,
                                             kBwOverusing);
  RtcEventLogTestHelper::VerifyBweDelayEvent(parsed_log, 3, bitrate3,
                                             kBwUnderusing);
  RtcEventLogTestHelper::VerifyLogEndEvent(parsed_log, 4);

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

TEST(RtcEventLogTest, LogProbeClusterCreatedAndReadBack) {
  Random prng(794613);

  int bitrate_bps0 = prng.Rand(0, 10000000);
  int bitrate_bps1 = prng.Rand(0, 10000000);
  int bitrate_bps2 = prng.Rand(0, 10000000);
  int min_probes0 = prng.Rand(0, 100);
  int min_probes1 = prng.Rand(0, 100);
  int min_probes2 = prng.Rand(0, 100);
  int min_bytes0 = prng.Rand(0, 10000);
  int min_bytes1 = prng.Rand(0, 10000);
  int min_bytes2 = prng.Rand(0, 10000);

  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTimeMicros(prng.Rand<uint32_t>());
  std::unique_ptr<RtcEventLog> log_dumper(RtcEventLog::Create());

  log_dumper->StartLogging(temp_filename, 10000000);
  log_dumper->LogProbeClusterCreated(0, bitrate_bps0, min_probes0, min_bytes0);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogProbeClusterCreated(1, bitrate_bps1, min_probes1, min_bytes1);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogProbeClusterCreated(2, bitrate_bps2, min_probes2, min_bytes2);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->StopLogging();

  // Read the generated file from disk.
  ParsedRtcEventLog parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

  // Verify that what we read back from the event log is the same as
  // what we wrote down.
  EXPECT_EQ(5u, parsed_log.GetNumberOfEvents());
  RtcEventLogTestHelper::VerifyLogStartEvent(parsed_log, 0);
  RtcEventLogTestHelper::VerifyBweProbeCluster(parsed_log, 1, 0, bitrate_bps0,
                                               min_probes0, min_bytes0);
  RtcEventLogTestHelper::VerifyBweProbeCluster(parsed_log, 2, 1, bitrate_bps1,
                                               min_probes1, min_bytes1);
  RtcEventLogTestHelper::VerifyBweProbeCluster(parsed_log, 3, 2, bitrate_bps2,
                                               min_probes2, min_bytes2);
  RtcEventLogTestHelper::VerifyLogEndEvent(parsed_log, 4);

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

TEST(RtcEventLogTest, LogProbeResultSuccessAndReadBack) {
  Random prng(192837);

  int bitrate_bps0 = prng.Rand(0, 10000000);
  int bitrate_bps1 = prng.Rand(0, 10000000);
  int bitrate_bps2 = prng.Rand(0, 10000000);

  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTimeMicros(prng.Rand<uint32_t>());
  std::unique_ptr<RtcEventLog> log_dumper(RtcEventLog::Create());

  log_dumper->StartLogging(temp_filename, 10000000);
  log_dumper->LogProbeResultSuccess(0, bitrate_bps0);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogProbeResultSuccess(1, bitrate_bps1);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogProbeResultSuccess(2, bitrate_bps2);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->StopLogging();

  // Read the generated file from disk.
  ParsedRtcEventLog parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

  // Verify that what we read back from the event log is the same as
  // what we wrote down.
  EXPECT_EQ(5u, parsed_log.GetNumberOfEvents());
  RtcEventLogTestHelper::VerifyLogStartEvent(parsed_log, 0);
  RtcEventLogTestHelper::VerifyProbeResultSuccess(parsed_log, 1, 0,
                                                  bitrate_bps0);
  RtcEventLogTestHelper::VerifyProbeResultSuccess(parsed_log, 2, 1,
                                                  bitrate_bps1);
  RtcEventLogTestHelper::VerifyProbeResultSuccess(parsed_log, 3, 2,
                                                  bitrate_bps2);
  RtcEventLogTestHelper::VerifyLogEndEvent(parsed_log, 4);

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

TEST(RtcEventLogTest, LogProbeResultFailureAndReadBack) {
  Random prng(192837);

  // Find the name of the current test, in order to use it as a temporary
  // filename.
  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::string temp_filename =
      test::OutputPath() + test_info->test_case_name() + test_info->name();

  rtc::ScopedFakeClock fake_clock;
  fake_clock.SetTimeMicros(prng.Rand<uint32_t>());
  std::unique_ptr<RtcEventLog> log_dumper(RtcEventLog::Create());

  log_dumper->StartLogging(temp_filename, 10000000);
  log_dumper->LogProbeResultFailure(
      0, ProbeFailureReason::kInvalidSendReceiveInterval);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogProbeResultFailure(
      1, ProbeFailureReason::kInvalidSendReceiveRatio);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->LogProbeResultFailure(2, ProbeFailureReason::kTimeout);
  fake_clock.AdvanceTimeMicros(prng.Rand(1, 1000));
  log_dumper->StopLogging();

  // Read the generated file from disk.
  ParsedRtcEventLog parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

  // Verify that what we read back from the event log is the same as
  // what we wrote down.
  EXPECT_EQ(5u, parsed_log.GetNumberOfEvents());
  RtcEventLogTestHelper::VerifyLogStartEvent(parsed_log, 0);
  RtcEventLogTestHelper::VerifyProbeResultFailure(
      parsed_log, 1, 0, ProbeFailureReason::kInvalidSendReceiveInterval);
  RtcEventLogTestHelper::VerifyProbeResultFailure(
      parsed_log, 2, 1, ProbeFailureReason::kInvalidSendReceiveRatio);
  RtcEventLogTestHelper::VerifyProbeResultFailure(parsed_log, 3, 2,
                                                  ProbeFailureReason::kTimeout);
  RtcEventLogTestHelper::VerifyLogEndEvent(parsed_log, 4);

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename.c_str());
}

class ConfigReadWriteTest {
 public:
  ConfigReadWriteTest() : prng(987654321) {}
  virtual ~ConfigReadWriteTest() {}
  virtual void GenerateConfig(uint32_t extensions_bitvector) = 0;
  virtual void VerifyConfig(const ParsedRtcEventLog& parsed_log,
                            size_t index) = 0;
  virtual void LogConfig(RtcEventLog* event_log) = 0;

  void DoTest() {
    // Find the name of the current test, in order to use it as a temporary
    // filename.
    auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string temp_filename =
        test::OutputPath() + test_info->test_case_name() + test_info->name();

    // Use all extensions.
    uint32_t extensions_bitvector = (1u << kNumExtensions) - 1;
    GenerateConfig(extensions_bitvector);

    // Log a single config event and stop logging.
    rtc::ScopedFakeClock fake_clock;
    fake_clock.SetTimeMicros(prng.Rand<uint32_t>());
    std::unique_ptr<RtcEventLog> log_dumper(RtcEventLog::Create());

    log_dumper->StartLogging(temp_filename, 10000000);
    LogConfig(log_dumper.get());
    log_dumper->StopLogging();

    // Read the generated file from disk.
    ParsedRtcEventLog parsed_log;
    ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

    // Check the generated number of events.
    EXPECT_EQ(3u, parsed_log.GetNumberOfEvents());

    RtcEventLogTestHelper::VerifyLogStartEvent(parsed_log, 0);

    // Verify that the parsed config struct matches the one that was logged.
    VerifyConfig(parsed_log, 1);

    RtcEventLogTestHelper::VerifyLogEndEvent(parsed_log, 2);

    // Clean up temporary file - can be pretty slow.
    remove(temp_filename.c_str());
  }
  Random prng;
};

class AudioReceiveConfigReadWriteTest : public ConfigReadWriteTest {
 public:
  void GenerateConfig(uint32_t extensions_bitvector) override {
    GenerateAudioReceiveConfig(extensions_bitvector, &config, &prng);
  }
  void LogConfig(RtcEventLog* event_log) override {
    event_log->LogAudioReceiveStreamConfig(config);
  }
  void VerifyConfig(const ParsedRtcEventLog& parsed_log,
                    size_t index) override {
    RtcEventLogTestHelper::VerifyAudioReceiveStreamConfig(parsed_log, index,
                                                          config);
  }
  AudioReceiveStream::Config config;
};

class AudioSendConfigReadWriteTest : public ConfigReadWriteTest {
 public:
  AudioSendConfigReadWriteTest() : config(nullptr) {}
  void GenerateConfig(uint32_t extensions_bitvector) override {
    GenerateAudioSendConfig(extensions_bitvector, &config, &prng);
  }
  void LogConfig(RtcEventLog* event_log) override {
    event_log->LogAudioSendStreamConfig(config);
  }
  void VerifyConfig(const ParsedRtcEventLog& parsed_log,
                    size_t index) override {
    RtcEventLogTestHelper::VerifyAudioSendStreamConfig(parsed_log, index,
                                                       config);
  }
  AudioSendStream::Config config;
};

class VideoReceiveConfigReadWriteTest : public ConfigReadWriteTest {
 public:
  VideoReceiveConfigReadWriteTest() : config(nullptr) {}
  void GenerateConfig(uint32_t extensions_bitvector) override {
    GenerateVideoReceiveConfig(extensions_bitvector, &config, &prng);
  }
  void LogConfig(RtcEventLog* event_log) override {
    event_log->LogVideoReceiveStreamConfig(config);
  }
  void VerifyConfig(const ParsedRtcEventLog& parsed_log,
                    size_t index) override {
    RtcEventLogTestHelper::VerifyVideoReceiveStreamConfig(parsed_log, index,
                                                          config);
  }
  VideoReceiveStream::Config config;
};

class VideoSendConfigReadWriteTest : public ConfigReadWriteTest {
 public:
  VideoSendConfigReadWriteTest() : config(nullptr) {}
  void GenerateConfig(uint32_t extensions_bitvector) override {
    GenerateVideoSendConfig(extensions_bitvector, &config, &prng);
  }
  void LogConfig(RtcEventLog* event_log) override {
    event_log->LogVideoSendStreamConfig(config);
  }
  void VerifyConfig(const ParsedRtcEventLog& parsed_log,
                    size_t index) override {
    RtcEventLogTestHelper::VerifyVideoSendStreamConfig(parsed_log, index,
                                                       config);
  }
  VideoSendStream::Config config;
};

class AudioNetworkAdaptationReadWriteTest : public ConfigReadWriteTest {
 public:
  void GenerateConfig(uint32_t extensions_bitvector) override {
    GenerateAudioNetworkAdaptation(extensions_bitvector, &config, &prng);
  }
  void LogConfig(RtcEventLog* event_log) override {
    event_log->LogAudioNetworkAdaptation(config);
  }
  void VerifyConfig(const ParsedRtcEventLog& parsed_log,
                    size_t index) override {
    RtcEventLogTestHelper::VerifyAudioNetworkAdaptation(parsed_log, index,
                                                        config);
  }
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
};

TEST(RtcEventLogTest, LogAudioReceiveConfig) {
  AudioReceiveConfigReadWriteTest test;
  test.DoTest();
}

TEST(RtcEventLogTest, LogAudioSendConfig) {
  AudioSendConfigReadWriteTest test;
  test.DoTest();
}

TEST(RtcEventLogTest, LogVideoReceiveConfig) {
  VideoReceiveConfigReadWriteTest test;
  test.DoTest();
}

TEST(RtcEventLogTest, LogVideoSendConfig) {
  VideoSendConfigReadWriteTest test;
  test.DoTest();
}

TEST(RtcEventLogTest, LogAudioNetworkAdaptation) {
  AudioNetworkAdaptationReadWriteTest test;
  test.DoTest();
}

}  // namespace webrtc
