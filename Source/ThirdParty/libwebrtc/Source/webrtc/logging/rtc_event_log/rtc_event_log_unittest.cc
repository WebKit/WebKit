/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/output/rtc_event_log_output_file.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_event_log_parser_new.h"
#include "logging/rtc_event_log/rtc_event_log_unittest_helper.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/checks.h"
#include "rtc_base/fakeclock.h"
#include "rtc_base/random.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

namespace {

RtpHeaderExtensionMap ExtensionMapWithAllSupportedExtensions() {
  RtpHeaderExtensionMap all_extensions;
  all_extensions.Register<AudioLevel>(RtpExtension::kAudioLevelDefaultId);
  all_extensions.Register<TransmissionOffset>(
      RtpExtension::kTimestampOffsetDefaultId);
  all_extensions.Register<AbsoluteSendTime>(
      RtpExtension::kAbsSendTimeDefaultId);
  all_extensions.Register<VideoOrientation>(
      RtpExtension::kVideoRotationDefaultId);
  all_extensions.Register<TransportSequenceNumber>(
      RtpExtension::kTransportSequenceNumberDefaultId);
  return all_extensions;
}

struct EventCounts {
  size_t audio_send_streams = 0;
  size_t audio_recv_streams = 0;
  size_t video_send_streams = 0;
  size_t video_recv_streams = 0;
  size_t alr_states = 0;
  size_t audio_playouts = 0;
  size_t ana_configs = 0;
  size_t bwe_loss_events = 0;
  size_t bwe_delay_events = 0;
  size_t probe_creations = 0;
  size_t probe_successes = 0;
  size_t probe_failures = 0;
  size_t ice_configs = 0;
  size_t ice_events = 0;
  size_t incoming_rtp_packets = 0;
  size_t outgoing_rtp_packets = 0;
  size_t incoming_rtcp_packets = 0;
  size_t outgoing_rtcp_packets = 0;

  size_t total_nonconfig_events() const {
    return alr_states + audio_playouts + ana_configs + bwe_loss_events +
           bwe_delay_events + probe_creations + probe_successes +
           probe_failures + ice_configs + ice_events + incoming_rtp_packets +
           outgoing_rtp_packets + incoming_rtcp_packets + outgoing_rtcp_packets;
  }

  size_t total_config_events() const {
    return audio_send_streams + audio_recv_streams + video_send_streams +
           video_recv_streams;
  }

  size_t total_events() const {
    return total_nonconfig_events() + total_config_events();
  }
};

class RtcEventLogSession
    : public ::testing::TestWithParam<
          std::tuple<uint64_t, int64_t, RtcEventLog::EncodingType>> {
 public:
  RtcEventLogSession()
      : seed_(std::get<0>(GetParam())),
        prng_(seed_),
        output_period_ms_(std::get<1>(GetParam())),
        encoding_type_(std::get<2>(GetParam())),
        gen_(seed_ * 880001UL),
        verifier_(encoding_type_) {
    clock_.SetTimeMicros(prng_.Rand<uint32_t>());
    // Find the name of the current test, in order to use it as a temporary
    // filename.
    // TODO(terelius): Use a general utility function to generate a temp file.
    auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name =
        std::string(test_info->test_case_name()) + "_" + test_info->name();
    std::replace(test_name.begin(), test_name.end(), '/', '_');
    temp_filename_ = test::OutputPath() + test_name;
  }

  // Create and buffer the config events and |num_events_before_log_start|
  // randomized non-config events. Then call StartLogging and finally create and
  // write the remaining non-config events.
  void WriteLog(EventCounts count, size_t num_events_before_log_start);
  void ReadAndVerifyLog();

 private:
  void WriteAudioRecvConfigs(size_t audio_recv_streams, RtcEventLog* event_log);
  void WriteAudioSendConfigs(size_t audio_send_streams, RtcEventLog* event_log);
  void WriteVideoRecvConfigs(size_t video_recv_streams, RtcEventLog* event_log);
  void WriteVideoSendConfigs(size_t video_send_streams, RtcEventLog* event_log);

  std::vector<std::pair<uint32_t, RtpHeaderExtensionMap>> incoming_extensions_;
  std::vector<std::pair<uint32_t, RtpHeaderExtensionMap>> outgoing_extensions_;

  // Config events.
  std::vector<std::unique_ptr<RtcEventAudioSendStreamConfig>>
      audio_send_config_list_;
  std::vector<std::unique_ptr<RtcEventAudioReceiveStreamConfig>>
      audio_recv_config_list_;
  std::vector<std::unique_ptr<RtcEventVideoSendStreamConfig>>
      video_send_config_list_;
  std::vector<std::unique_ptr<RtcEventVideoReceiveStreamConfig>>
      video_recv_config_list_;

  // Regular events.
  std::vector<std::unique_ptr<RtcEventAlrState>> alr_state_list_;
  std::map<uint32_t, std::vector<std::unique_ptr<RtcEventAudioPlayout>>>
      audio_playout_map_;  // Groups audio by SSRC.
  std::vector<std::unique_ptr<RtcEventAudioNetworkAdaptation>>
      ana_configs_list_;
  std::vector<std::unique_ptr<RtcEventBweUpdateLossBased>> bwe_loss_list_;
  std::vector<std::unique_ptr<RtcEventBweUpdateDelayBased>> bwe_delay_list_;
  std::vector<std::unique_ptr<RtcEventProbeClusterCreated>>
      probe_creation_list_;
  std::vector<std::unique_ptr<RtcEventProbeResultSuccess>> probe_success_list_;
  std::vector<std::unique_ptr<RtcEventProbeResultFailure>> probe_failure_list_;
  std::vector<std::unique_ptr<RtcEventIceCandidatePairConfig>> ice_config_list_;
  std::vector<std::unique_ptr<RtcEventIceCandidatePair>> ice_event_list_;
  std::map<uint32_t, std::vector<std::unique_ptr<RtcEventRtpPacketIncoming>>>
      incoming_rtp_map_;  // Groups incoming RTP by SSRC.
  std::map<uint32_t, std::vector<std::unique_ptr<RtcEventRtpPacketOutgoing>>>
      outgoing_rtp_map_;  // Groups outgoing RTP by SSRC.
  std::vector<std::unique_ptr<RtcEventRtcpPacketIncoming>> incoming_rtcp_list_;
  std::vector<std::unique_ptr<RtcEventRtcpPacketOutgoing>> outgoing_rtcp_list_;

  int64_t start_time_us_;
  int64_t utc_start_time_us_;
  int64_t stop_time_us_;

  const uint64_t seed_;
  Random prng_;
  const int64_t output_period_ms_;
  const RtcEventLog::EncodingType encoding_type_;
  test::EventGenerator gen_;
  test::EventVerifier verifier_;
  rtc::ScopedFakeClock clock_;
  std::string temp_filename_;
};

bool SsrcUsed(
    uint32_t ssrc,
    const std::vector<std::pair<uint32_t, RtpHeaderExtensionMap>>& streams) {
  for (const auto& kv : streams) {
    if (kv.first == ssrc)
      return true;
  }
  return false;
}

void RtcEventLogSession::WriteAudioRecvConfigs(size_t audio_recv_streams,
                                               RtcEventLog* event_log) {
  RTC_CHECK(event_log != nullptr);
  uint32_t ssrc;
  for (size_t i = 0; i < audio_recv_streams; i++) {
    clock_.AdvanceTimeMicros(prng_.Rand(20) * 1000);
    do {
      ssrc = prng_.Rand<uint32_t>();
    } while (SsrcUsed(ssrc, incoming_extensions_));
    RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
    incoming_extensions_.emplace_back(ssrc, extensions);
    auto event = gen_.NewAudioReceiveStreamConfig(ssrc, extensions);
    event_log->Log(event->Copy());
    audio_recv_config_list_.push_back(std::move(event));
  }
}

void RtcEventLogSession::WriteAudioSendConfigs(size_t audio_send_streams,
                                               RtcEventLog* event_log) {
  RTC_CHECK(event_log != nullptr);
  uint32_t ssrc;
  for (size_t i = 0; i < audio_send_streams; i++) {
    clock_.AdvanceTimeMicros(prng_.Rand(20) * 1000);
    do {
      ssrc = prng_.Rand<uint32_t>();
    } while (SsrcUsed(ssrc, outgoing_extensions_));
    RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
    outgoing_extensions_.emplace_back(ssrc, extensions);
    auto event = gen_.NewAudioSendStreamConfig(ssrc, extensions);
    event_log->Log(event->Copy());
    audio_send_config_list_.push_back(std::move(event));
  }
}

void RtcEventLogSession::WriteVideoRecvConfigs(size_t video_recv_streams,
                                               RtcEventLog* event_log) {
  RTC_CHECK(event_log != nullptr);
  RTC_CHECK_GE(video_recv_streams, 1);

  // Force least one stream to use all header extensions, to ensure
  // (statistically) that every extension is tested in packet creation.
  RtpHeaderExtensionMap all_extensions =
      ExtensionMapWithAllSupportedExtensions();

  clock_.AdvanceTimeMicros(prng_.Rand(20) * 1000);
  uint32_t ssrc = prng_.Rand<uint32_t>();
  incoming_extensions_.emplace_back(prng_.Rand<uint32_t>(), all_extensions);
  auto event = gen_.NewVideoReceiveStreamConfig(ssrc, all_extensions);
  event_log->Log(event->Copy());
  video_recv_config_list_.push_back(std::move(event));
  for (size_t i = 1; i < video_recv_streams; i++) {
    clock_.AdvanceTimeMicros(prng_.Rand(20) * 1000);
    do {
      ssrc = prng_.Rand<uint32_t>();
    } while (SsrcUsed(ssrc, incoming_extensions_));
    RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
    incoming_extensions_.emplace_back(ssrc, extensions);
    auto event = gen_.NewVideoReceiveStreamConfig(ssrc, extensions);
    event_log->Log(event->Copy());
    video_recv_config_list_.push_back(std::move(event));
  }
}

void RtcEventLogSession::WriteVideoSendConfigs(size_t video_send_streams,
                                               RtcEventLog* event_log) {
  RTC_CHECK(event_log != nullptr);
  RTC_CHECK_GE(video_send_streams, 1);

  // Force least one stream to use all header extensions, to ensure
  // (statistically) that every extension is tested in packet creation.
  RtpHeaderExtensionMap all_extensions =
      ExtensionMapWithAllSupportedExtensions();

  clock_.AdvanceTimeMicros(prng_.Rand(20) * 1000);
  uint32_t ssrc = prng_.Rand<uint32_t>();
  outgoing_extensions_.emplace_back(prng_.Rand<uint32_t>(), all_extensions);
  auto event = gen_.NewVideoSendStreamConfig(ssrc, all_extensions);
  event_log->Log(event->Copy());
  video_send_config_list_.push_back(std::move(event));
  for (size_t i = 1; i < video_send_streams; i++) {
    clock_.AdvanceTimeMicros(prng_.Rand(20) * 1000);
    do {
      ssrc = prng_.Rand<uint32_t>();
    } while (SsrcUsed(ssrc, outgoing_extensions_));
    RtpHeaderExtensionMap extensions = gen_.NewRtpHeaderExtensionMap();
    outgoing_extensions_.emplace_back(ssrc, extensions);
    auto event = gen_.NewVideoSendStreamConfig(ssrc, extensions);
    event_log->Log(event->Copy());
    video_send_config_list_.push_back(std::move(event));
  }
}

void RtcEventLogSession::WriteLog(EventCounts count,
                                  size_t num_events_before_start) {
  // TODO(terelius): Allow test to run with either a real or a fake clock_.
  // Maybe always use the ScopedFakeClock, but conditionally SleepMs()?

  // The log file will be flushed to disk when the event_log goes out of scope.
  std::unique_ptr<RtcEventLog> event_log(RtcEventLog::Create(encoding_type_));

  // We can't send or receive packets without configured streams.
  RTC_CHECK_GE(count.video_recv_streams, 1);
  RTC_CHECK_GE(count.video_send_streams, 1);

  WriteAudioRecvConfigs(count.audio_recv_streams, event_log.get());
  WriteAudioSendConfigs(count.audio_send_streams, event_log.get());
  WriteVideoRecvConfigs(count.video_recv_streams, event_log.get());
  WriteVideoSendConfigs(count.video_send_streams, event_log.get());

  size_t remaining_events = count.total_nonconfig_events();
  ASSERT_LE(num_events_before_start, remaining_events);
  size_t remaining_events_at_start = remaining_events - num_events_before_start;
  for (; remaining_events > 0; remaining_events--) {
    if (remaining_events == remaining_events_at_start) {
      clock_.AdvanceTimeMicros(prng_.Rand(20) * 1000);
      event_log->StartLogging(
          absl::make_unique<RtcEventLogOutputFile>(temp_filename_, 10000000),
          output_period_ms_);
      start_time_us_ = rtc::TimeMicros();
      utc_start_time_us_ = rtc::TimeUTCMicros();
    }

    clock_.AdvanceTimeMicros(prng_.Rand(20) * 1000);
    size_t selection = prng_.Rand(remaining_events - 1);

    if (selection < count.alr_states) {
      auto event = gen_.NewAlrState();
      event_log->Log(event->Copy());
      alr_state_list_.push_back(std::move(event));
      count.alr_states--;
      continue;
    }
    selection -= count.alr_states;

    if (selection < count.audio_playouts) {
      size_t stream = prng_.Rand(incoming_extensions_.size() - 1);
      // This might be a video SSRC, but the parser does not use the config.
      uint32_t ssrc = incoming_extensions_[stream].first;
      auto event = gen_.NewAudioPlayout(ssrc);
      event_log->Log(event->Copy());
      audio_playout_map_[ssrc].push_back(std::move(event));
      count.audio_playouts--;
      continue;
    }
    selection -= count.audio_playouts;

    if (selection < count.ana_configs) {
      auto event = gen_.NewAudioNetworkAdaptation();
      event_log->Log(event->Copy());
      ana_configs_list_.push_back(std::move(event));
      count.ana_configs--;
      continue;
    }
    selection -= count.ana_configs;

    if (selection < count.bwe_loss_events) {
      auto event = gen_.NewBweUpdateLossBased();
      event_log->Log(event->Copy());
      bwe_loss_list_.push_back(std::move(event));
      count.bwe_loss_events--;
      continue;
    }
    selection -= count.bwe_loss_events;

    if (selection < count.bwe_delay_events) {
      auto event = gen_.NewBweUpdateDelayBased();
      event_log->Log(event->Copy());
      bwe_delay_list_.push_back(std::move(event));
      count.bwe_delay_events--;
      continue;
    }
    selection -= count.bwe_delay_events;

    if (selection < count.probe_creations) {
      auto event = gen_.NewProbeClusterCreated();
      event_log->Log(event->Copy());
      probe_creation_list_.push_back(std::move(event));
      count.probe_creations--;
      continue;
    }
    selection -= count.probe_creations;

    if (selection < count.probe_successes) {
      auto event = gen_.NewProbeResultSuccess();
      event_log->Log(event->Copy());
      probe_success_list_.push_back(std::move(event));
      count.probe_successes--;
      continue;
    }
    selection -= count.probe_successes;

    if (selection < count.probe_failures) {
      auto event = gen_.NewProbeResultFailure();
      event_log->Log(event->Copy());
      probe_failure_list_.push_back(std::move(event));
      count.probe_failures--;
      continue;
    }
    selection -= count.probe_failures;

    if (selection < count.ice_configs) {
      auto event = gen_.NewIceCandidatePairConfig();
      event_log->Log(event->Copy());
      ice_config_list_.push_back(std::move(event));
      count.ice_configs--;
      continue;
    }
    selection -= count.ice_configs;

    if (selection < count.ice_events) {
      auto event = gen_.NewIceCandidatePair();
      event_log->Log(event->Copy());
      ice_event_list_.push_back(std::move(event));
      count.ice_events--;
      continue;
    }
    selection -= count.ice_events;

    if (selection < count.incoming_rtp_packets) {
      size_t stream = prng_.Rand(incoming_extensions_.size() - 1);
      uint32_t ssrc = incoming_extensions_[stream].first;
      auto event =
          gen_.NewRtpPacketIncoming(ssrc, incoming_extensions_[stream].second);
      event_log->Log(event->Copy());
      incoming_rtp_map_[ssrc].push_back(std::move(event));
      count.incoming_rtp_packets--;
      continue;
    }
    selection -= count.incoming_rtp_packets;

    if (selection < count.outgoing_rtp_packets) {
      size_t stream = prng_.Rand(outgoing_extensions_.size() - 1);
      uint32_t ssrc = outgoing_extensions_[stream].first;
      auto event =
          gen_.NewRtpPacketOutgoing(ssrc, outgoing_extensions_[stream].second);
      event_log->Log(event->Copy());
      outgoing_rtp_map_[ssrc].push_back(std::move(event));
      count.outgoing_rtp_packets--;
      continue;
    }
    selection -= count.outgoing_rtp_packets;

    if (selection < count.incoming_rtcp_packets) {
      auto event = gen_.NewRtcpPacketIncoming();
      event_log->Log(event->Copy());
      incoming_rtcp_list_.push_back(std::move(event));
      count.incoming_rtcp_packets--;
      continue;
    }
    selection -= count.incoming_rtcp_packets;

    if (selection < count.outgoing_rtcp_packets) {
      auto event = gen_.NewRtcpPacketOutgoing();
      event_log->Log(event->Copy());
      outgoing_rtcp_list_.push_back(std::move(event));
      count.outgoing_rtcp_packets--;
      continue;
    }
    selection -= count.outgoing_rtcp_packets;

    RTC_NOTREACHED();
  }

  event_log->StopLogging();
  stop_time_us_ = rtc::TimeMicros();

  ASSERT_EQ(count.total_nonconfig_events(), static_cast<size_t>(0));
}

// Read the file and verify that what we read back from the event log is the
// same as what we wrote down.
void RtcEventLogSession::ReadAndVerifyLog() {
  // Read the generated file from disk.
  ParsedRtcEventLogNew parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename_));

  // Start and stop events.
  auto& parsed_start_log_events = parsed_log.start_log_events();
  ASSERT_EQ(parsed_start_log_events.size(), static_cast<size_t>(1));
  verifier_.VerifyLoggedStartEvent(start_time_us_, utc_start_time_us_,
                                   parsed_start_log_events[0]);

  auto& parsed_stop_log_events = parsed_log.stop_log_events();
  ASSERT_EQ(parsed_stop_log_events.size(), static_cast<size_t>(1));
  verifier_.VerifyLoggedStopEvent(stop_time_us_, parsed_stop_log_events[0]);

  auto& parsed_alr_state_events = parsed_log.alr_state_events();
  ASSERT_EQ(parsed_alr_state_events.size(), alr_state_list_.size());
  for (size_t i = 0; i < parsed_alr_state_events.size(); i++) {
    verifier_.VerifyLoggedAlrStateEvent(*alr_state_list_[i],
                                        parsed_alr_state_events[i]);
  }

  const auto& parsed_audio_playout_map = parsed_log.audio_playout_events();
  ASSERT_EQ(parsed_audio_playout_map.size(), audio_playout_map_.size());
  for (const auto& kv : parsed_audio_playout_map) {
    uint32_t ssrc = kv.first;
    const auto& parsed_audio_playout_stream = kv.second;
    const auto& audio_playout_stream = audio_playout_map_[ssrc];
    ASSERT_EQ(parsed_audio_playout_stream.size(), audio_playout_stream.size());
    for (size_t i = 0; i < parsed_audio_playout_map.size(); i++) {
      verifier_.VerifyLoggedAudioPlayoutEvent(*audio_playout_stream[i],
                                              parsed_audio_playout_stream[i]);
    }
  }

  auto& parsed_audio_network_adaptation_events =
      parsed_log.audio_network_adaptation_events();
  ASSERT_EQ(parsed_audio_network_adaptation_events.size(),
            ana_configs_list_.size());
  for (size_t i = 0; i < parsed_audio_network_adaptation_events.size(); i++) {
    verifier_.VerifyLoggedAudioNetworkAdaptationEvent(
        *ana_configs_list_[i], parsed_audio_network_adaptation_events[i]);
  }

  auto& parsed_bwe_delay_updates = parsed_log.bwe_delay_updates();
  ASSERT_EQ(parsed_bwe_delay_updates.size(), bwe_delay_list_.size());
  for (size_t i = 0; i < parsed_bwe_delay_updates.size(); i++) {
    verifier_.VerifyLoggedBweDelayBasedUpdate(*bwe_delay_list_[i],
                                              parsed_bwe_delay_updates[i]);
  }

  auto& parsed_bwe_loss_updates = parsed_log.bwe_loss_updates();
  ASSERT_EQ(parsed_bwe_loss_updates.size(), bwe_loss_list_.size());
  for (size_t i = 0; i < parsed_bwe_loss_updates.size(); i++) {
    verifier_.VerifyLoggedBweLossBasedUpdate(*bwe_loss_list_[i],
                                             parsed_bwe_loss_updates[i]);
  }

  auto& parsed_bwe_probe_cluster_created_events =
      parsed_log.bwe_probe_cluster_created_events();
  ASSERT_EQ(parsed_bwe_probe_cluster_created_events.size(),
            probe_creation_list_.size());
  for (size_t i = 0; i < parsed_bwe_probe_cluster_created_events.size(); i++) {
    verifier_.VerifyLoggedBweProbeClusterCreatedEvent(
        *probe_creation_list_[i], parsed_bwe_probe_cluster_created_events[i]);
  }

  auto& parsed_bwe_probe_failure_events = parsed_log.bwe_probe_failure_events();
  ASSERT_EQ(parsed_bwe_probe_failure_events.size(), probe_failure_list_.size());
  for (size_t i = 0; i < parsed_bwe_probe_failure_events.size(); i++) {
    verifier_.VerifyLoggedBweProbeFailureEvent(
        *probe_failure_list_[i], parsed_bwe_probe_failure_events[i]);
  }

  auto& parsed_bwe_probe_success_events = parsed_log.bwe_probe_success_events();
  ASSERT_EQ(parsed_bwe_probe_success_events.size(), probe_success_list_.size());
  for (size_t i = 0; i < parsed_bwe_probe_success_events.size(); i++) {
    verifier_.VerifyLoggedBweProbeSuccessEvent(
        *probe_success_list_[i], parsed_bwe_probe_success_events[i]);
  }

  auto& parsed_ice_candidate_pair_configs =
      parsed_log.ice_candidate_pair_configs();
  ASSERT_EQ(parsed_ice_candidate_pair_configs.size(), ice_config_list_.size());
  for (size_t i = 0; i < parsed_ice_candidate_pair_configs.size(); i++) {
    verifier_.VerifyLoggedIceCandidatePairConfig(
        *ice_config_list_[i], parsed_ice_candidate_pair_configs[i]);
  }

  auto& parsed_ice_candidate_pair_events =
      parsed_log.ice_candidate_pair_events();
  ASSERT_EQ(parsed_ice_candidate_pair_events.size(),
            parsed_ice_candidate_pair_events.size());
  for (size_t i = 0; i < parsed_ice_candidate_pair_events.size(); i++) {
    verifier_.VerifyLoggedIceCandidatePairEvent(
        *ice_event_list_[i], parsed_ice_candidate_pair_events[i]);
  }

  auto& parsed_incoming_rtp_packets_by_ssrc =
      parsed_log.incoming_rtp_packets_by_ssrc();
  ASSERT_EQ(parsed_incoming_rtp_packets_by_ssrc.size(),
            incoming_rtp_map_.size());
  for (const auto& kv : parsed_incoming_rtp_packets_by_ssrc) {
    uint32_t ssrc = kv.ssrc;
    const auto& parsed_rtp_stream = kv.incoming_packets;
    const auto& rtp_stream = incoming_rtp_map_[ssrc];
    ASSERT_EQ(parsed_rtp_stream.size(), rtp_stream.size());
    for (size_t i = 0; i < parsed_rtp_stream.size(); i++) {
      verifier_.VerifyLoggedRtpPacketIncoming(*rtp_stream[i],
                                              parsed_rtp_stream[i]);
    }
  }

  auto& parsed_outgoing_rtp_packets_by_ssrc =
      parsed_log.outgoing_rtp_packets_by_ssrc();
  ASSERT_EQ(parsed_outgoing_rtp_packets_by_ssrc.size(),
            outgoing_rtp_map_.size());
  for (const auto& kv : parsed_outgoing_rtp_packets_by_ssrc) {
    uint32_t ssrc = kv.ssrc;
    const auto& parsed_rtp_stream = kv.outgoing_packets;
    const auto& rtp_stream = outgoing_rtp_map_[ssrc];
    ASSERT_EQ(parsed_rtp_stream.size(), rtp_stream.size());
    for (size_t i = 0; i < parsed_rtp_stream.size(); i++) {
      verifier_.VerifyLoggedRtpPacketOutgoing(*rtp_stream[i],
                                              parsed_rtp_stream[i]);
    }
  }

  auto& parsed_incoming_rtcp_packets = parsed_log.incoming_rtcp_packets();
  ASSERT_EQ(parsed_incoming_rtcp_packets.size(), incoming_rtcp_list_.size());
  for (size_t i = 0; i < parsed_incoming_rtcp_packets.size(); i++) {
    verifier_.VerifyLoggedRtcpPacketIncoming(*incoming_rtcp_list_[i],
                                             parsed_incoming_rtcp_packets[i]);
  }

  auto& parsed_outgoing_rtcp_packets = parsed_log.outgoing_rtcp_packets();
  ASSERT_EQ(parsed_outgoing_rtcp_packets.size(), outgoing_rtcp_list_.size());
  for (size_t i = 0; i < parsed_outgoing_rtcp_packets.size(); i++) {
    verifier_.VerifyLoggedRtcpPacketOutgoing(*outgoing_rtcp_list_[i],
                                             parsed_outgoing_rtcp_packets[i]);
  }
  auto& parsed_audio_recv_configs = parsed_log.audio_recv_configs();
  ASSERT_EQ(parsed_audio_recv_configs.size(), audio_recv_config_list_.size());
  for (size_t i = 0; i < parsed_audio_recv_configs.size(); i++) {
    verifier_.VerifyLoggedAudioRecvConfig(*audio_recv_config_list_[i],
                                          parsed_audio_recv_configs[i]);
  }
  auto& parsed_audio_send_configs = parsed_log.audio_send_configs();
  ASSERT_EQ(parsed_audio_send_configs.size(), audio_send_config_list_.size());
  for (size_t i = 0; i < parsed_audio_send_configs.size(); i++) {
    verifier_.VerifyLoggedAudioSendConfig(*audio_send_config_list_[i],
                                          parsed_audio_send_configs[i]);
  }
  auto& parsed_video_recv_configs = parsed_log.video_recv_configs();
  ASSERT_EQ(parsed_video_recv_configs.size(), video_recv_config_list_.size());
  for (size_t i = 0; i < parsed_video_recv_configs.size(); i++) {
    verifier_.VerifyLoggedVideoRecvConfig(*video_recv_config_list_[i],
                                          parsed_video_recv_configs[i]);
  }
  auto& parsed_video_send_configs = parsed_log.video_send_configs();
  ASSERT_EQ(parsed_video_send_configs.size(), video_send_config_list_.size());
  for (size_t i = 0; i < parsed_video_send_configs.size(); i++) {
    verifier_.VerifyLoggedVideoSendConfig(*video_send_config_list_[i],
                                          parsed_video_send_configs[i]);
  }

  // Clean up temporary file - can be pretty slow.
  remove(temp_filename_.c_str());
}

}  // namespace

TEST_P(RtcEventLogSession, StartLoggingFromBeginning) {
  EventCounts count;
  count.audio_send_streams = 2;
  count.audio_recv_streams = 2;
  count.video_send_streams = 3;
  count.video_recv_streams = 4;
  count.alr_states = 4;
  count.audio_playouts = 100;
  count.ana_configs = 3;
  count.bwe_loss_events = 20;
  count.bwe_delay_events = 20;
  count.probe_creations = 4;
  count.probe_successes = 2;
  count.probe_failures = 2;
  count.ice_configs = 3;
  count.ice_events = 10;
  count.incoming_rtp_packets = 100;
  count.outgoing_rtp_packets = 100;
  count.incoming_rtcp_packets = 20;
  count.outgoing_rtcp_packets = 20;

  WriteLog(count, 0);
  ReadAndVerifyLog();
}

TEST_P(RtcEventLogSession, StartLoggingInTheMiddle) {
  EventCounts count;
  count.audio_send_streams = 3;
  count.audio_recv_streams = 4;
  count.video_send_streams = 5;
  count.video_recv_streams = 6;
  count.alr_states = 10;
  count.audio_playouts = 500;
  count.ana_configs = 10;
  count.bwe_loss_events = 50;
  count.bwe_delay_events = 50;
  count.probe_creations = 10;
  count.probe_successes = 5;
  count.probe_failures = 5;
  count.ice_configs = 10;
  count.ice_events = 20;
  count.incoming_rtp_packets = 500;
  count.outgoing_rtp_packets = 500;
  count.incoming_rtcp_packets = 50;
  count.outgoing_rtcp_packets = 50;

  WriteLog(count, 500);
  ReadAndVerifyLog();
}

INSTANTIATE_TEST_CASE_P(
    RtcEventLogTest,
    RtcEventLogSession,
    ::testing::Combine(
        ::testing::Values(1234567, 7654321),
        ::testing::Values(RtcEventLog::kImmediateOutput, 1, 5),
        ::testing::Values(RtcEventLog::EncodingType::Legacy,
                          RtcEventLog::EncodingType::NewFormat)));

class RtcEventLogCircularBufferTest
    : public ::testing::TestWithParam<RtcEventLog::EncodingType> {
 public:
  RtcEventLogCircularBufferTest()
      : encoding_type_(GetParam()), verifier_(encoding_type_) {}
  const RtcEventLog::EncodingType encoding_type_;
  const test::EventVerifier verifier_;
};

TEST_P(RtcEventLogCircularBufferTest, KeepsMostRecentEvents) {
  // TODO(terelius): Maybe make a separate RtcEventLogImplTest that can access
  // the size of the cyclic buffer?
  constexpr size_t kNumEvents = 20000;
  constexpr int64_t kStartTime = 1000000;
  constexpr int32_t kStartBitrate = 1000000;

  auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  std::string test_name =
      std::string(test_info->test_case_name()) + "_" + test_info->name();
  std::replace(test_name.begin(), test_name.end(), '/', '_');
  const std::string temp_filename = test::OutputPath() + test_name;

  std::unique_ptr<rtc::ScopedFakeClock> fake_clock =
      absl::make_unique<rtc::ScopedFakeClock>();
  fake_clock->SetTimeMicros(kStartTime);

  // When log_dumper goes out of scope, it causes the log file to be flushed
  // to disk.
  std::unique_ptr<RtcEventLog> log_dumper(RtcEventLog::Create(encoding_type_));

  for (size_t i = 0; i < kNumEvents; i++) {
    // The purpose of the test is to verify that the log can handle
    // more events than what fits in the internal circular buffer. The exact
    // type of events does not matter so we chose ProbeSuccess events for
    // simplicity.
    // We base the various values on the index. We use this for some basic
    // consistency checks when we read back.
    log_dumper->Log(absl::make_unique<RtcEventProbeResultSuccess>(
        i, kStartBitrate + i * 1000));
    fake_clock->AdvanceTimeMicros(10000);
  }
  int64_t start_time_us = rtc::TimeMicros();
  int64_t utc_start_time_us = rtc::TimeUTCMicros();
  log_dumper->StartLogging(
      absl::make_unique<RtcEventLogOutputFile>(temp_filename, 10000000),
      RtcEventLog::kImmediateOutput);
  fake_clock->AdvanceTimeMicros(10000);
  int64_t stop_time_us = rtc::TimeMicros();
  log_dumper->StopLogging();

  // Read the generated file from disk.
  ParsedRtcEventLogNew parsed_log;
  ASSERT_TRUE(parsed_log.ParseFile(temp_filename));

  const auto& start_log_events = parsed_log.start_log_events();
  ASSERT_EQ(start_log_events.size(), 1u);
  verifier_.VerifyLoggedStartEvent(start_time_us, utc_start_time_us,
                                   start_log_events[0]);

  const auto& stop_log_events = parsed_log.stop_log_events();
  ASSERT_EQ(stop_log_events.size(), 1u);
  verifier_.VerifyLoggedStopEvent(stop_time_us, stop_log_events[0]);

  const auto& probe_success_events = parsed_log.bwe_probe_success_events();
  // If the following fails, it probably means that kNumEvents isn't larger
  // than the size of the cyclic buffer in the event log. Try increasing
  // kNumEvents.
  EXPECT_LT(probe_success_events.size(), kNumEvents);

  ASSERT_GT(probe_success_events.size(), 1u);
  int64_t first_timestamp_us = probe_success_events[0].timestamp_us;
  uint32_t first_id = probe_success_events[0].id;
  int32_t first_bitrate_bps = probe_success_events[0].bitrate_bps;
  // We want to reset the time to what we used when generating the events, but
  // the fake clock implementation DCHECKS if time moves backwards. We therefore
  // recreate the clock. However we must ensure that the old fake_clock is
  // destroyed before the new one is created, so we have to reset() first.
  fake_clock.reset();
  fake_clock = absl::make_unique<rtc::ScopedFakeClock>();
  fake_clock->SetTimeMicros(first_timestamp_us);
  for (size_t i = 1; i < probe_success_events.size(); i++) {
    fake_clock->AdvanceTimeMicros(10000);
    verifier_.VerifyLoggedBweProbeSuccessEvent(
        RtcEventProbeResultSuccess(first_id + i, first_bitrate_bps + i * 1000),
        probe_success_events[i]);
  }
}

INSTANTIATE_TEST_CASE_P(
    RtcEventLogTest,
    RtcEventLogCircularBufferTest,
    ::testing::Values(RtcEventLog::EncodingType::Legacy,
                      RtcEventLog::EncodingType::NewFormat));

// TODO(terelius): Verify parser behavior if the timestamps are not
// monotonically increasing in the log.


}  // namespace webrtc
