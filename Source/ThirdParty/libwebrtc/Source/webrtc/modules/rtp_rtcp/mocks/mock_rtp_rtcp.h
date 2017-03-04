/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_MOCKS_MOCK_RTP_RTCP_H_
#define WEBRTC_MODULES_RTP_RTCP_MOCKS_MOCK_RTP_RTCP_H_

#include <set>
#include <utility>
#include <vector>

#include "webrtc/base/optional.h"
#include "webrtc/modules/include/module.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockRtpData : public RtpData {
 public:
  MOCK_METHOD3(OnReceivedPayloadData,
               int32_t(const uint8_t* payload_data,
                       size_t payload_size,
                       const WebRtcRTPHeader* rtp_header));

  MOCK_METHOD2(OnRecoveredPacket,
               bool(const uint8_t* packet, size_t packet_length));
};

class MockRtpRtcp : public RtpRtcp {
 public:
  MOCK_METHOD1(RegisterDefaultModule, int32_t(RtpRtcp* module));
  MOCK_METHOD0(DeRegisterDefaultModule, int32_t());
  MOCK_METHOD0(DefaultModuleRegistered, bool());
  MOCK_METHOD0(NumberChildModules, uint32_t());
  MOCK_METHOD1(RegisterSyncModule, int32_t(RtpRtcp* module));
  MOCK_METHOD0(DeRegisterSyncModule, int32_t());
  MOCK_METHOD2(IncomingRtcpPacket,
               int32_t(const uint8_t* incoming_packet, size_t packet_length));
  MOCK_METHOD1(SetRemoteSSRC, void(uint32_t ssrc));
  MOCK_METHOD4(IncomingAudioNTP,
               int32_t(uint32_t audio_received_ntp_secs,
                       uint32_t audio_received_ntp_frac,
                       uint32_t audio_rtcp_arrival_time_secs,
                       uint32_t audio_rtcp_arrival_time_frac));
  MOCK_METHOD0(InitSender, int32_t());
  MOCK_METHOD1(RegisterSendTransport, int32_t(Transport* outgoing_transport));
  MOCK_METHOD1(SetMaxRtpPacketSize, void(size_t size));
  MOCK_METHOD1(SetTransportOverhead, void(int transport_overhead_per_packet));
  MOCK_CONST_METHOD0(MaxPayloadSize, size_t());
  MOCK_CONST_METHOD0(MaxRtpPacketSize, size_t());
  MOCK_METHOD1(RegisterSendPayload, int32_t(const CodecInst& voice_codec));
  MOCK_METHOD1(RegisterSendPayload, int32_t(const VideoCodec& video_codec));
  MOCK_METHOD2(RegisterVideoSendPayload,
               void(int payload_type, const char* payload_name));
  MOCK_METHOD1(DeRegisterSendPayload, int32_t(int8_t payload_type));
  MOCK_METHOD2(RegisterSendRtpHeaderExtension,
               int32_t(RTPExtensionType type, uint8_t id));
  MOCK_METHOD1(DeregisterSendRtpHeaderExtension,
               int32_t(RTPExtensionType type));
  MOCK_CONST_METHOD0(HasBweExtensions, bool());
  MOCK_CONST_METHOD0(StartTimestamp, uint32_t());
  MOCK_METHOD1(SetStartTimestamp, void(uint32_t timestamp));
  MOCK_CONST_METHOD0(SequenceNumber, uint16_t());
  MOCK_METHOD1(SetSequenceNumber, void(uint16_t seq));
  MOCK_METHOD1(SetRtpState, void(const RtpState& rtp_state));
  MOCK_METHOD1(SetRtxState, void(const RtpState& rtp_state));
  MOCK_CONST_METHOD0(GetRtpState, RtpState());
  MOCK_CONST_METHOD0(GetRtxState, RtpState());
  MOCK_CONST_METHOD0(SSRC, uint32_t());
  MOCK_METHOD1(SetSSRC, void(uint32_t ssrc));
  MOCK_CONST_METHOD1(CSRCs, int32_t(uint32_t csrcs[kRtpCsrcSize]));
  MOCK_METHOD1(SetCsrcs, void(const std::vector<uint32_t>& csrcs));
  MOCK_METHOD1(SetCSRCStatus, int32_t(bool include));
  MOCK_METHOD1(SetRtxSendStatus, void(int modes));
  MOCK_CONST_METHOD0(RtxSendStatus, int());
  MOCK_METHOD1(SetRtxSsrc, void(uint32_t));
  MOCK_METHOD2(SetRtxSendPayloadType, void(int, int));
  MOCK_CONST_METHOD0(FlexfecSsrc, rtc::Optional<uint32_t>());
  MOCK_CONST_METHOD0(RtxSendPayloadType, std::pair<int, int>());
  MOCK_METHOD1(SetSendingStatus, int32_t(bool sending));
  MOCK_CONST_METHOD0(Sending, bool());
  MOCK_METHOD1(SetSendingMediaStatus, void(bool sending));
  MOCK_CONST_METHOD0(SendingMedia, bool());
  MOCK_CONST_METHOD4(BitrateSent,
                     void(uint32_t* total_rate,
                          uint32_t* video_rate,
                          uint32_t* fec_rate,
                          uint32_t* nack_rate));
  MOCK_METHOD1(RegisterVideoBitrateObserver, void(BitrateStatisticsObserver*));
  MOCK_CONST_METHOD0(GetVideoBitrateObserver, BitrateStatisticsObserver*(void));
  MOCK_CONST_METHOD1(EstimatedReceiveBandwidth,
                     int(uint32_t* available_bandwidth));
  MOCK_METHOD9(SendOutgoingData,
               bool(FrameType frame_type,
                    int8_t payload_type,
                    uint32_t timestamp,
                    int64_t capture_time_ms,
                    const uint8_t* payload_data,
                    size_t payload_size,
                    const RTPFragmentationHeader* fragmentation,
                    const RTPVideoHeader* rtp_video_header,
                    uint32_t* frame_id_out));
  MOCK_METHOD5(TimeToSendPacket,
               bool(uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    bool retransmission,
                    const PacedPacketInfo& pacing_info));
  MOCK_METHOD2(TimeToSendPadding,
               size_t(size_t bytes, const PacedPacketInfo& pacing_info));
  MOCK_METHOD2(RegisterRtcpObservers,
               void(RtcpIntraFrameObserver* intra_frame_callback,
                    RtcpBandwidthObserver* bandwidth_callback));
  MOCK_CONST_METHOD0(RTCP, RtcpMode());
  MOCK_METHOD1(SetRTCPStatus, void(RtcpMode method));
  MOCK_METHOD1(SetCNAME, int32_t(const char cname[RTCP_CNAME_SIZE]));
  MOCK_CONST_METHOD2(RemoteCNAME,
                     int32_t(uint32_t remote_ssrc,
                             char cname[RTCP_CNAME_SIZE]));
  MOCK_CONST_METHOD5(RemoteNTP,
                     int32_t(uint32_t* received_ntp_secs,
                             uint32_t* received_ntp_frac,
                             uint32_t* rtcp_arrival_time_secs,
                             uint32_t* rtcp_arrival_time_frac,
                             uint32_t* rtcp_timestamp));
  MOCK_METHOD2(AddMixedCNAME,
               int32_t(uint32_t ssrc, const char cname[RTCP_CNAME_SIZE]));
  MOCK_METHOD1(RemoveMixedCNAME, int32_t(uint32_t ssrc));
  MOCK_CONST_METHOD5(RTT,
                     int32_t(uint32_t remote_ssrc,
                             int64_t* rtt,
                             int64_t* avg_rtt,
                             int64_t* min_rtt,
                             int64_t* max_rtt));
  MOCK_METHOD1(SendRTCP, int32_t(RTCPPacketType packet_type));
  MOCK_METHOD1(SendCompoundRTCP,
               int32_t(const std::set<RTCPPacketType>& packet_types));
  MOCK_METHOD1(SendRTCPReferencePictureSelection, int32_t(uint64_t picture_id));
  MOCK_METHOD1(SendRTCPSliceLossIndication, int32_t(uint8_t picture_id));
  MOCK_CONST_METHOD2(DataCountersRTP,
                     int32_t(size_t* bytes_sent, uint32_t* packets_sent));
  MOCK_CONST_METHOD2(GetSendStreamDataCounters,
                     void(StreamDataCounters*, StreamDataCounters*));
  MOCK_CONST_METHOD3(GetRtpPacketLossStats,
                     void(bool, uint32_t, struct RtpPacketLossStats*));
  MOCK_METHOD1(RemoteRTCPStat, int32_t(RTCPSenderInfo* sender_info));
  MOCK_CONST_METHOD1(RemoteRTCPStat,
                     int32_t(std::vector<RTCPReportBlock>* receive_blocks));
  MOCK_METHOD4(SetRTCPApplicationSpecificData,
               int32_t(uint8_t sub_type,
                       uint32_t name,
                       const uint8_t* data,
                       uint16_t length));
  MOCK_METHOD1(SetRTCPVoIPMetrics, int32_t(const RTCPVoIPMetric* voip_metric));
  MOCK_METHOD1(SetRtcpXrRrtrStatus, void(bool enable));
  MOCK_CONST_METHOD0(RtcpXrRrtrStatus, bool());
  MOCK_CONST_METHOD0(REMB, bool());
  MOCK_METHOD1(SetREMBStatus, void(bool enable));
  MOCK_METHOD2(SetREMBData,
               void(uint32_t bitrate, const std::vector<uint32_t>& ssrcs));
  MOCK_CONST_METHOD0(TMMBR, bool());
  MOCK_METHOD1(SetTMMBRStatus, void(bool enable));
  MOCK_METHOD1(OnBandwidthEstimateUpdate, void(uint16_t bandwidth_kbit));
  MOCK_CONST_METHOD0(SelectiveRetransmissions, int());
  MOCK_METHOD1(SetSelectiveRetransmissions, int(uint8_t settings));
  MOCK_METHOD2(SendNACK, int32_t(const uint16_t* nack_list, uint16_t size));
  MOCK_METHOD1(SendNack, void(const std::vector<uint16_t>& sequence_numbers));
  MOCK_METHOD2(SetStorePacketsStatus,
               void(bool enable, uint16_t number_to_store));
  MOCK_CONST_METHOD0(StorePackets, bool());
  MOCK_METHOD1(RegisterRtcpStatisticsCallback, void(RtcpStatisticsCallback*));
  MOCK_METHOD0(GetRtcpStatisticsCallback, RtcpStatisticsCallback*());
  MOCK_METHOD1(SendFeedbackPacket, bool(const rtcp::TransportFeedback& packet));
  MOCK_METHOD1(SetAudioPacketSize, int32_t(uint16_t packet_size_samples));
  MOCK_METHOD3(SendTelephoneEventOutband,
               int32_t(uint8_t key, uint16_t time_ms, uint8_t level));
  MOCK_METHOD1(SetSendREDPayloadType, int32_t(int8_t payload_type));
  MOCK_CONST_METHOD1(SendREDPayloadType, int32_t(int8_t* payload_type));
  MOCK_METHOD2(SetRTPAudioLevelIndicationStatus,
               int32_t(bool enable, uint8_t id));
  MOCK_METHOD1(SetAudioLevel, int32_t(uint8_t level_dbov));
  MOCK_METHOD1(SetTargetSendBitrate, void(uint32_t bitrate_bps));
  MOCK_METHOD2(SetUlpfecConfig,
               void(int red_payload_type, int fec_payload_type));
  MOCK_METHOD2(SetFecParameters,
               bool(const FecProtectionParams& delta_params,
                    const FecProtectionParams& key_params));
  MOCK_METHOD1(SetKeyFrameRequestMethod, int32_t(KeyFrameRequestMethod method));
  MOCK_METHOD0(RequestKeyFrame, int32_t());
  MOCK_METHOD0(TimeUntilNextProcess, int64_t());
  MOCK_METHOD0(Process, void());
  MOCK_METHOD1(RegisterSendFrameCountObserver, void(FrameCountObserver*));
  MOCK_CONST_METHOD0(GetSendFrameCountObserver, FrameCountObserver*(void));
  MOCK_METHOD1(RegisterSendChannelRtpStatisticsCallback,
               void(StreamDataCountersCallback*));
  MOCK_CONST_METHOD0(GetSendChannelRtpStatisticsCallback,
                     StreamDataCountersCallback*(void));
  MOCK_METHOD1(SetVideoBitrateAllocation, void(const BitrateAllocation&));
  // Members.
  unsigned int remote_ssrc_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_MOCKS_MOCK_RTP_RTCP_H_
