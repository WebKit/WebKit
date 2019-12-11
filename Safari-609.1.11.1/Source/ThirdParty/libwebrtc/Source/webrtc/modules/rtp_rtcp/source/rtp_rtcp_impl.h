/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "api/video/video_bitrate_allocation.h"
#include "modules/include/module_common_types.h"
#include "modules/include/module_fec_types.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"  // RTCPPacketType
#include "modules/rtp_rtcp/source/rtcp_packet/tmmb_item.h"
#include "modules/rtp_rtcp/source/rtcp_receiver.h"
#include "modules/rtp_rtcp/source/rtcp_sender.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/gtest_prod_util.h"

namespace webrtc {

class Clock;
struct PacedPacketInfo;
struct RTPVideoHeader;

class ModuleRtpRtcpImpl : public RtpRtcp, public RTCPReceiver::ModuleRtpRtcp {
 public:
  explicit ModuleRtpRtcpImpl(const RtpRtcp::Configuration& configuration);
  ~ModuleRtpRtcpImpl() override;

  // Returns the number of milliseconds until the module want a worker thread to
  // call Process.
  int64_t TimeUntilNextProcess() override;

  // Process any pending tasks such as timeouts.
  void Process() override;

  // Receiver part.

  // Called when we receive an RTCP packet.
  void IncomingRtcpPacket(const uint8_t* incoming_packet,
                          size_t incoming_packet_length) override;

  void SetRemoteSSRC(uint32_t ssrc) override;

  // Sender part.
  void RegisterSendPayloadFrequency(int payload_type,
                                    int payload_frequency) override;

  int32_t DeRegisterSendPayload(int8_t payload_type) override;

  void SetExtmapAllowMixed(bool extmap_allow_mixed) override;

  // Register RTP header extension.
  int32_t RegisterSendRtpHeaderExtension(RTPExtensionType type,
                                         uint8_t id) override;
  bool RegisterRtpHeaderExtension(const std::string& uri, int id) override;

  int32_t DeregisterSendRtpHeaderExtension(RTPExtensionType type) override;

  bool SupportsPadding() const override;
  bool SupportsRtxPayloadPadding() const override;

  // Get start timestamp.
  uint32_t StartTimestamp() const override;

  // Configure start timestamp, default is a random number.
  void SetStartTimestamp(uint32_t timestamp) override;

  uint16_t SequenceNumber() const override;

  // Set SequenceNumber, default is a random number.
  void SetSequenceNumber(uint16_t seq) override;

  void SetRtpState(const RtpState& rtp_state) override;
  void SetRtxState(const RtpState& rtp_state) override;
  RtpState GetRtpState() const override;
  RtpState GetRtxState() const override;

  uint32_t SSRC() const override;

  // Configure SSRC, default is a random number.
  void SetSSRC(uint32_t ssrc) override;

  void SetRid(const std::string& rid) override;

  void SetMid(const std::string& mid) override;

  void SetCsrcs(const std::vector<uint32_t>& csrcs) override;

  RTCPSender::FeedbackState GetFeedbackState();

  void SetRtxSendStatus(int mode) override;
  int RtxSendStatus() const override;

  void SetRtxSsrc(uint32_t ssrc) override;

  void SetRtxSendPayloadType(int payload_type,
                             int associated_payload_type) override;

  absl::optional<uint32_t> FlexfecSsrc() const override;

  // Sends kRtcpByeCode when going from true to false.
  int32_t SetSendingStatus(bool sending) override;

  bool Sending() const override;

  // Drops or relays media packets.
  void SetSendingMediaStatus(bool sending) override;

  bool SendingMedia() const override;

  void SetAsPartOfAllocation(bool part_of_allocation) override;

  bool OnSendingRtpFrame(uint32_t timestamp,
                         int64_t capture_time_ms,
                         int payload_type,
                         bool force_sender_report) override;

  bool TrySendPacket(RtpPacketToSend* packet,
                     const PacedPacketInfo& pacing_info) override;


  std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes) override;

  // RTCP part.

  // Get RTCP status.
  RtcpMode RTCP() const override;

  // Configure RTCP status i.e on/off.
  void SetRTCPStatus(RtcpMode method) override;

  // Set RTCP CName.
  int32_t SetCNAME(const char* c_name) override;

  // Get remote CName.
  int32_t RemoteCNAME(uint32_t remote_ssrc,
                      char c_name[RTCP_CNAME_SIZE]) const override;

  // Get remote NTP.
  int32_t RemoteNTP(uint32_t* received_ntp_secs,
                    uint32_t* received_ntp_frac,
                    uint32_t* rtcp_arrival_time_secs,
                    uint32_t* rtcp_arrival_time_frac,
                    uint32_t* rtcp_timestamp) const override;

  int32_t AddMixedCNAME(uint32_t ssrc, const char* c_name) override;

  int32_t RemoveMixedCNAME(uint32_t ssrc) override;

  // Get RoundTripTime.
  int32_t RTT(uint32_t remote_ssrc,
              int64_t* rtt,
              int64_t* avg_rtt,
              int64_t* min_rtt,
              int64_t* max_rtt) const override;

  int64_t ExpectedRetransmissionTimeMs() const override;

  // Force a send of an RTCP packet.
  // Normal SR and RR are triggered via the process function.
  int32_t SendRTCP(RTCPPacketType rtcpPacketType) override;

  // Statistics of the amount of data sent and received.
  int32_t DataCountersRTP(size_t* bytes_sent,
                          uint32_t* packets_sent) const override;

  void GetSendStreamDataCounters(
      StreamDataCounters* rtp_counters,
      StreamDataCounters* rtx_counters) const override;

  // Get received RTCP report, report block.
  int32_t RemoteRTCPStat(
      std::vector<RTCPReportBlock>* receive_blocks) const override;
  // A snapshot of the most recent Report Block with additional data of
  // interest to statistics. Used to implement RTCRemoteInboundRtpStreamStats.
  // Within this list, the ReportBlockData::RTCPReportBlock::source_ssrc(),
  // which is the SSRC of the corresponding outbound RTP stream, is unique.
  std::vector<ReportBlockData> GetLatestReportBlockData() const override;

  // (REMB) Receiver Estimated Max Bitrate.
  void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs) override;
  void UnsetRemb() override;

  // (TMMBR) Temporary Max Media Bit Rate.
  bool TMMBR() const override;

  void SetTMMBRStatus(bool enable) override;

  void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) override;

  size_t MaxRtpPacketSize() const override;

  void SetMaxRtpPacketSize(size_t max_packet_size) override;

  // (NACK) Negative acknowledgment part.

  // Send a Negative acknowledgment packet.
  // TODO(philipel): Deprecate SendNACK and use SendNack instead.
  int32_t SendNACK(const uint16_t* nack_list, uint16_t size) override;

  void SendNack(const std::vector<uint16_t>& sequence_numbers) override;

  // Store the sent packets, needed to answer to a negative acknowledgment
  // requests.
  void SetStorePacketsStatus(bool enable, uint16_t number_to_store) override;

  bool StorePackets() const override;

  // Called on receipt of RTCP report block from remote side.
  void RegisterRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback) override;
  RtcpStatisticsCallback* GetRtcpStatisticsCallback() override;
  void RegisterRtcpCnameCallback(RtcpCnameCallback* callback) override;

  void SetReportBlockDataObserver(ReportBlockDataObserver* observer) override;

  bool SendFeedbackPacket(const rtcp::TransportFeedback& packet) override;
  bool SendNetworkStateEstimatePacket(
      const rtcp::RemoteEstimate& packet) override;
  // (APP) Application specific data.
  int32_t SetRTCPApplicationSpecificData(uint8_t sub_type,
                                         uint32_t name,
                                         const uint8_t* data,
                                         uint16_t length) override;

  // (XR) Receiver reference time report.
  void SetRtcpXrRrtrStatus(bool enable) override;

  bool RtcpXrRrtrStatus() const override;

  // Video part.
  int32_t SendLossNotification(uint16_t last_decoded_seq_num,
                               uint16_t last_received_seq_num,
                               bool decodability_flag,
                               bool buffering_allowed) override;

  bool LastReceivedNTP(uint32_t* NTPsecs,
                       uint32_t* NTPfrac,
                       uint32_t* remote_sr) const;

  std::vector<rtcp::TmmbItem> BoundingSet(bool* tmmbr_owner);

  void BitrateSent(uint32_t* total_rate,
                   uint32_t* video_rate,
                   uint32_t* fec_rate,
                   uint32_t* nackRate) const override;

  void RegisterSendChannelRtpStatisticsCallback(
      StreamDataCountersCallback* callback) override;
  StreamDataCountersCallback* GetSendChannelRtpStatisticsCallback()
      const override;

  void OnReceivedNack(
      const std::vector<uint16_t>& nack_sequence_numbers) override;
  void OnReceivedRtcpReportBlocks(
      const ReportBlockList& report_blocks) override;
  void OnRequestSendReport() override;

  void SetVideoBitrateAllocation(
      const VideoBitrateAllocation& bitrate) override;

  RTPSender* RtpSender() override;
  const RTPSender* RtpSender() const override;

 protected:
  bool UpdateRTCPReceiveInformationTimers();

  RTPSender* rtp_sender() { return rtp_sender_.get(); }
  const RTPSender* rtp_sender() const { return rtp_sender_.get(); }

  RTCPSender* rtcp_sender() { return &rtcp_sender_; }
  const RTCPSender* rtcp_sender() const { return &rtcp_sender_; }

  RTCPReceiver* rtcp_receiver() { return &rtcp_receiver_; }
  const RTCPReceiver* rtcp_receiver() const { return &rtcp_receiver_; }

  Clock* clock() const { return clock_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(RtpRtcpImplTest, Rtt);
  FRIEND_TEST_ALL_PREFIXES(RtpRtcpImplTest, RttForReceiverOnly);
  void SetRtcpReceiverSsrcs(uint32_t main_ssrc);

  void set_rtt_ms(int64_t rtt_ms);
  int64_t rtt_ms() const;

  bool TimeToSendFullNackList(int64_t now) const;

  std::unique_ptr<RTPSender> rtp_sender_;
  RTCPSender rtcp_sender_;
  RTCPReceiver rtcp_receiver_;

  Clock* const clock_;

  int64_t last_bitrate_process_time_;
  int64_t last_rtt_process_time_;
  int64_t next_process_time_;
  uint16_t packet_overhead_;

  // Send side
  int64_t nack_last_time_sent_full_ms_;
  uint16_t nack_last_seq_number_sent_;

  RemoteBitrateEstimator* const remote_bitrate_;

  RtcpAckObserver* const ack_observer_;

  RtcpRttStats* const rtt_stats_;

  // The processed RTT from RtcpRttStats.
  rtc::CriticalSection critical_section_rtt_;
  int64_t rtt_ms_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
