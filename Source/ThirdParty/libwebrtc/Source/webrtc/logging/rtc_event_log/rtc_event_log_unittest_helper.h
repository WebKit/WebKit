/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_UNITTEST_HELPER_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_UNITTEST_HELPER_H_

#include <memory>

#include "logging/rtc_event_log/events/rtc_event.h"
#include "logging/rtc_event_log/events/rtc_event_alr_state.h"
#include "logging/rtc_event_log/events/rtc_event_audio_network_adaptation.h"
#include "logging/rtc_event_log/events/rtc_event_audio_playout.h"
#include "logging/rtc_event_log/events/rtc_event_audio_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_audio_send_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_delay_based.h"
#include "logging/rtc_event_log/events/rtc_event_bwe_update_loss_based.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/events/rtc_event_probe_cluster_created.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_success.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"
#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"
#include "logging/rtc_event_log/events/rtc_event_video_receive_stream_config.h"
#include "logging/rtc_event_log/events/rtc_event_video_send_stream_config.h"
#include "logging/rtc_event_log/rtc_event_log_parser_new.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "rtc_base/random.h"

namespace webrtc {

namespace test {

class EventGenerator {
 public:
  explicit EventGenerator(uint64_t seed) : prng_(seed) {}

  std::unique_ptr<RtcEventAlrState> NewAlrState();

  std::unique_ptr<RtcEventAudioPlayout> NewAudioPlayout(uint32_t ssrc);

  std::unique_ptr<RtcEventAudioNetworkAdaptation> NewAudioNetworkAdaptation();

  std::unique_ptr<RtcEventBweUpdateDelayBased> NewBweUpdateDelayBased();

  std::unique_ptr<RtcEventBweUpdateLossBased> NewBweUpdateLossBased();

  std::unique_ptr<RtcEventProbeClusterCreated> NewProbeClusterCreated();

  std::unique_ptr<RtcEventProbeResultFailure> NewProbeResultFailure();

  std::unique_ptr<RtcEventProbeResultSuccess> NewProbeResultSuccess();

  std::unique_ptr<RtcEventIceCandidatePairConfig> NewIceCandidatePairConfig();

  std::unique_ptr<RtcEventIceCandidatePair> NewIceCandidatePair();

  std::unique_ptr<RtcEventRtcpPacketIncoming> NewRtcpPacketIncoming();

  std::unique_ptr<RtcEventRtcpPacketOutgoing> NewRtcpPacketOutgoing();

  // |all_configured_exts| determines whether the RTP packet exhibits all
  // configured extensions, or a random subset thereof.
  void RandomizeRtpPacket(size_t payload_size,
                          size_t padding_size,
                          uint32_t ssrc,
                          const RtpHeaderExtensionMap& extension_map,
                          RtpPacket* rtp_packet,
                          bool all_configured_exts);

  // |all_configured_exts| determines whether the RTP packet exhibits all
  // configured extensions, or a random subset thereof.
  std::unique_ptr<RtcEventRtpPacketIncoming> NewRtpPacketIncoming(
      uint32_t ssrc,
      const RtpHeaderExtensionMap& extension_map,
      bool all_configured_exts = true);

  // |all_configured_exts| determines whether the RTP packet exhibits all
  // configured extensions, or a random subset thereof.
  std::unique_ptr<RtcEventRtpPacketOutgoing> NewRtpPacketOutgoing(
      uint32_t ssrc,
      const RtpHeaderExtensionMap& extension_map,
      bool all_configured_exts = true);

  // |configure_all| determines whether all supported extensions are configured,
  // or a random subset.
  RtpHeaderExtensionMap NewRtpHeaderExtensionMap(bool configure_all = false);

  std::unique_ptr<RtcEventAudioReceiveStreamConfig> NewAudioReceiveStreamConfig(
      uint32_t ssrc,
      const RtpHeaderExtensionMap& extensions);

  std::unique_ptr<RtcEventAudioSendStreamConfig> NewAudioSendStreamConfig(
      uint32_t ssrc,
      const RtpHeaderExtensionMap& extensions);

  std::unique_ptr<RtcEventVideoReceiveStreamConfig> NewVideoReceiveStreamConfig(
      uint32_t ssrc,
      const RtpHeaderExtensionMap& extensions);

  std::unique_ptr<RtcEventVideoSendStreamConfig> NewVideoSendStreamConfig(
      uint32_t ssrc,
      const RtpHeaderExtensionMap& extensions);

 private:
  rtcp::ReportBlock NewReportBlock();
  rtcp::SenderReport NewSenderReport();
  rtcp::ReceiverReport NewReceiverReport();

  Random prng_;
};

class EventVerifier {
 public:
  explicit EventVerifier(RtcEventLog::EncodingType encoding_type)
      : encoding_type_(encoding_type) {}

  void VerifyLoggedAlrStateEvent(const RtcEventAlrState& original_event,
                                 const LoggedAlrStateEvent& logged_event) const;

  void VerifyLoggedAudioPlayoutEvent(
      const RtcEventAudioPlayout& original_event,
      const LoggedAudioPlayoutEvent& logged_event) const;

  void VerifyLoggedAudioNetworkAdaptationEvent(
      const RtcEventAudioNetworkAdaptation& original_event,
      const LoggedAudioNetworkAdaptationEvent& logged_event) const;

  void VerifyLoggedBweDelayBasedUpdate(
      const RtcEventBweUpdateDelayBased& original_event,
      const LoggedBweDelayBasedUpdate& logged_event) const;

  void VerifyLoggedBweLossBasedUpdate(
      const RtcEventBweUpdateLossBased& original_event,
      const LoggedBweLossBasedUpdate& logged_event) const;

  void VerifyLoggedBweProbeClusterCreatedEvent(
      const RtcEventProbeClusterCreated& original_event,
      const LoggedBweProbeClusterCreatedEvent& logged_event) const;

  void VerifyLoggedBweProbeFailureEvent(
      const RtcEventProbeResultFailure& original_event,
      const LoggedBweProbeFailureEvent& logged_event) const;

  void VerifyLoggedBweProbeSuccessEvent(
      const RtcEventProbeResultSuccess& original_event,
      const LoggedBweProbeSuccessEvent& logged_event) const;

  void VerifyLoggedIceCandidatePairConfig(
      const RtcEventIceCandidatePairConfig& original_event,
      const LoggedIceCandidatePairConfig& logged_event) const;

  void VerifyLoggedIceCandidatePairEvent(
      const RtcEventIceCandidatePair& original_event,
      const LoggedIceCandidatePairEvent& logged_event) const;

  void VerifyLoggedRtpPacketIncoming(
      const RtcEventRtpPacketIncoming& original_event,
      const LoggedRtpPacketIncoming& logged_event) const;

  void VerifyLoggedRtpPacketOutgoing(
      const RtcEventRtpPacketOutgoing& original_event,
      const LoggedRtpPacketOutgoing& logged_event) const;

  template <typename EventType, typename ParsedType>
  void VerifyLoggedRtpPacket(const EventType& original_event,
                             const ParsedType& logged_event) {
    static_assert(sizeof(ParsedType) == 0,
                  "You have to use one of the two defined template "
                  "specializations of VerifyLoggedRtpPacket");
  }

  template <>
  void VerifyLoggedRtpPacket(const RtcEventRtpPacketIncoming& original_event,
                             const LoggedRtpPacketIncoming& logged_event) {
    VerifyLoggedRtpPacketIncoming(original_event, logged_event);
  }

  template <>
  void VerifyLoggedRtpPacket(const RtcEventRtpPacketOutgoing& original_event,
                             const LoggedRtpPacketOutgoing& logged_event) {
    VerifyLoggedRtpPacketOutgoing(original_event, logged_event);
  }

  void VerifyLoggedRtcpPacketIncoming(
      const RtcEventRtcpPacketIncoming& original_event,
      const LoggedRtcpPacketIncoming& logged_event) const;

  void VerifyLoggedRtcpPacketOutgoing(
      const RtcEventRtcpPacketOutgoing& original_event,
      const LoggedRtcpPacketOutgoing& logged_event) const;

  void VerifyLoggedStartEvent(int64_t start_time_us,
                              int64_t utc_start_time_us,
                              const LoggedStartEvent& logged_event) const;
  void VerifyLoggedStopEvent(int64_t stop_time_us,
                             const LoggedStopEvent& logged_event) const;

  void VerifyLoggedAudioRecvConfig(
      const RtcEventAudioReceiveStreamConfig& original_event,
      const LoggedAudioRecvConfig& logged_event) const;

  void VerifyLoggedAudioSendConfig(
      const RtcEventAudioSendStreamConfig& original_event,
      const LoggedAudioSendConfig& logged_event) const;

  void VerifyLoggedVideoRecvConfig(
      const RtcEventVideoReceiveStreamConfig& original_event,
      const LoggedVideoRecvConfig& logged_event) const;

  void VerifyLoggedVideoSendConfig(
      const RtcEventVideoSendStreamConfig& original_event,
      const LoggedVideoSendConfig& logged_event) const;

 private:
  RtcEventLog::EncodingType encoding_type_;
};

}  // namespace test
}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_UNITTEST_HELPER_H_
