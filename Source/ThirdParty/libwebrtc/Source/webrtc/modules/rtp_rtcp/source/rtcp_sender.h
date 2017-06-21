/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_

#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "webrtc/api/call/transport.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/random.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/include/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_nack_stats.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/dlrr.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/tmmb_item.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ModuleRtpRtcpImpl;
class RtcEventLog;

class NACKStringBuilder {
 public:
  NACKStringBuilder();
  ~NACKStringBuilder();

  void PushNACK(uint16_t nack);
  std::string GetResult();

 private:
  std::ostringstream stream_;
  int count_;
  uint16_t prevNack_;
  bool consecutive_;
};

class RTCPSender {
 public:
  struct FeedbackState {
    FeedbackState();

    uint32_t packets_sent;
    size_t media_bytes_sent;
    uint32_t send_bitrate;

    uint32_t last_rr_ntp_secs;
    uint32_t last_rr_ntp_frac;
    uint32_t remote_sr;

    bool has_last_xr_rr;
    rtcp::ReceiveTimeInfo last_xr_rr;

    // Used when generating TMMBR.
    ModuleRtpRtcpImpl* module;
  };

  RTCPSender(bool audio,
             Clock* clock,
             ReceiveStatistics* receive_statistics,
             RtcpPacketTypeCounterObserver* packet_type_counter_observer,
             RtcEventLog* event_log,
             Transport* outgoing_transport);
  virtual ~RTCPSender();

  RtcpMode Status() const;
  void SetRTCPStatus(RtcpMode method);

  bool Sending() const;
  int32_t SetSendingStatus(const FeedbackState& feedback_state,
                           bool enabled);  // combine the functions

  int32_t SetNackStatus(bool enable);

  void SetTimestampOffset(uint32_t timestamp_offset);

  void SetLastRtpTime(uint32_t rtp_timestamp, int64_t capture_time_ms);

  uint32_t SSRC() const;

  void SetSSRC(uint32_t ssrc);

  void SetRemoteSSRC(uint32_t ssrc);

  int32_t SetCNAME(const char* cName);

  int32_t AddMixedCNAME(uint32_t SSRC, const char* c_name);

  int32_t RemoveMixedCNAME(uint32_t SSRC);

  bool TimeToSendRTCPReport(bool sendKeyframeBeforeRTP = false) const;

  int32_t SendRTCP(const FeedbackState& feedback_state,
                   RTCPPacketType packetType,
                   int32_t nackSize = 0,
                   const uint16_t* nackList = 0);

  int32_t SendCompoundRTCP(const FeedbackState& feedback_state,
                           const std::set<RTCPPacketType>& packetTypes,
                           int32_t nackSize = 0,
                           const uint16_t* nackList = 0);

  bool REMB() const;

  void SetREMBStatus(bool enable);

  void SetREMBData(uint32_t bitrate, const std::vector<uint32_t>& ssrcs);

  bool TMMBR() const;

  void SetTMMBRStatus(bool enable);

  void SetMaxRtpPacketSize(size_t max_packet_size);

  void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set);

  int32_t SetApplicationSpecificData(uint8_t subType,
                                     uint32_t name,
                                     const uint8_t* data,
                                     uint16_t length);
  int32_t SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric);

  void SendRtcpXrReceiverReferenceTime(bool enable);

  bool RtcpXrReceiverReferenceTime() const;

  void SetCsrcs(const std::vector<uint32_t>& csrcs);

  void SetTargetBitrate(unsigned int target_bitrate);
  void SetVideoBitrateAllocation(const BitrateAllocation& bitrate);
  bool SendFeedbackPacket(const rtcp::TransportFeedback& packet);

 private:
  class RtcpContext;

  // Determine which RTCP messages should be sent and setup flags.
  void PrepareReport(const FeedbackState& feedback_state)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);

  bool AddReportBlock(const FeedbackState& feedback_state,
                      uint32_t ssrc,
                      StreamStatistician* statistician)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);

  std::unique_ptr<rtcp::RtcpPacket> BuildSR(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildRR(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildSDES(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildPLI(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildREMB(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildTMMBR(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildTMMBN(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildAPP(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildExtendedReports(
      const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildBYE(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildFIR(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  std::unique_ptr<rtcp::RtcpPacket> BuildNACK(const RtcpContext& context)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);

 private:
  const bool audio_;
  Clock* const clock_;
  Random random_ GUARDED_BY(critical_section_rtcp_sender_);
  RtcpMode method_ GUARDED_BY(critical_section_rtcp_sender_);

  RtcEventLog* const event_log_;
  Transport* const transport_;

  rtc::CriticalSection critical_section_rtcp_sender_;
  bool using_nack_ GUARDED_BY(critical_section_rtcp_sender_);
  bool sending_ GUARDED_BY(critical_section_rtcp_sender_);
  bool remb_enabled_ GUARDED_BY(critical_section_rtcp_sender_);

  int64_t next_time_to_send_rtcp_ GUARDED_BY(critical_section_rtcp_sender_);

  uint32_t timestamp_offset_ GUARDED_BY(critical_section_rtcp_sender_);
  uint32_t last_rtp_timestamp_ GUARDED_BY(critical_section_rtcp_sender_);
  int64_t last_frame_capture_time_ms_ GUARDED_BY(critical_section_rtcp_sender_);
  uint32_t ssrc_ GUARDED_BY(critical_section_rtcp_sender_);
  // SSRC that we receive on our RTP channel
  uint32_t remote_ssrc_ GUARDED_BY(critical_section_rtcp_sender_);
  std::string cname_ GUARDED_BY(critical_section_rtcp_sender_);

  ReceiveStatistics* receive_statistics_
      GUARDED_BY(critical_section_rtcp_sender_);
  std::map<uint32_t, rtcp::ReportBlock> report_blocks_
      GUARDED_BY(critical_section_rtcp_sender_);
  std::map<uint32_t, std::string> csrc_cnames_
      GUARDED_BY(critical_section_rtcp_sender_);

  // send CSRCs
  std::vector<uint32_t> csrcs_ GUARDED_BY(critical_section_rtcp_sender_);

  // Full intra request
  uint8_t sequence_number_fir_ GUARDED_BY(critical_section_rtcp_sender_);

  // REMB
  uint32_t remb_bitrate_ GUARDED_BY(critical_section_rtcp_sender_);
  std::vector<uint32_t> remb_ssrcs_ GUARDED_BY(critical_section_rtcp_sender_);

  std::vector<rtcp::TmmbItem> tmmbn_to_send_
      GUARDED_BY(critical_section_rtcp_sender_);
  uint32_t tmmbr_send_bps_ GUARDED_BY(critical_section_rtcp_sender_);
  uint32_t packet_oh_send_ GUARDED_BY(critical_section_rtcp_sender_);
  size_t max_packet_size_ GUARDED_BY(critical_section_rtcp_sender_);

  // APP
  uint8_t app_sub_type_ GUARDED_BY(critical_section_rtcp_sender_);
  uint32_t app_name_ GUARDED_BY(critical_section_rtcp_sender_);
  std::unique_ptr<uint8_t[]> app_data_
      GUARDED_BY(critical_section_rtcp_sender_);
  uint16_t app_length_ GUARDED_BY(critical_section_rtcp_sender_);

  // True if sending of XR Receiver reference time report is enabled.
  bool xr_send_receiver_reference_time_enabled_
      GUARDED_BY(critical_section_rtcp_sender_);

  // XR VoIP metric
  rtc::Optional<RTCPVoIPMetric> xr_voip_metric_
      GUARDED_BY(critical_section_rtcp_sender_);

  RtcpPacketTypeCounterObserver* const packet_type_counter_observer_;
  RtcpPacketTypeCounter packet_type_counter_
      GUARDED_BY(critical_section_rtcp_sender_);

  RtcpNackStats nack_stats_ GUARDED_BY(critical_section_rtcp_sender_);

  rtc::Optional<BitrateAllocation> video_bitrate_allocation_
      GUARDED_BY(critical_section_rtcp_sender_);

  void SetFlag(uint32_t type, bool is_volatile)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  void SetFlags(const std::set<RTCPPacketType>& types, bool is_volatile)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  bool IsFlagPresent(uint32_t type) const
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  bool ConsumeFlag(uint32_t type, bool forced = false)
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  bool AllVolatileFlagsConsumed() const
      EXCLUSIVE_LOCKS_REQUIRED(critical_section_rtcp_sender_);
  struct ReportFlag {
    ReportFlag(uint32_t type, bool is_volatile)
        : type(type), is_volatile(is_volatile) {}
    bool operator<(const ReportFlag& flag) const { return type < flag.type; }
    bool operator==(const ReportFlag& flag) const { return type == flag.type; }
    const uint32_t type;
    const bool is_volatile;
  };

  std::set<ReportFlag> report_flags_ GUARDED_BY(critical_section_rtcp_sender_);

  typedef std::unique_ptr<rtcp::RtcpPacket> (RTCPSender::*BuilderFunc)(
      const RtcpContext&);
  // Map from RTCPPacketType to builder.
  std::map<uint32_t, BuilderFunc> builders_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RTCPSender);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
