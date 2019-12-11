/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/transport/webrtc_key_value_config.h"
#include "api/video/video_bitrate_allocation.h"
#include "modules/include/module.h"
#include "modules/rtp_rtcp/include/flexfec_sender.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtp_packet_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/deprecation.h"

namespace webrtc {

// Forward declarations.
class FrameEncryptorInterface;
class OverheadObserver;
class RateLimiter;
class ReceiveStatisticsProvider;
class RemoteBitrateEstimator;
class RtcEventLog;
class RTPSender;
class Transport;
class VideoBitrateAllocationObserver;

namespace rtcp {
class TransportFeedback;
}

class RtpRtcp : public Module, public RtcpFeedbackSenderInterface {
 public:
  struct Configuration {
    Configuration();
    Configuration(Configuration&& rhs);

    // True for a audio version of the RTP/RTCP module object false will create
    // a video version.
    bool audio = false;
    bool receiver_only = false;

    // The clock to use to read time. If nullptr then system clock will be used.
    Clock* clock = nullptr;

    ReceiveStatisticsProvider* receive_statistics = nullptr;

    // Transport object that will be called when packets are ready to be sent
    // out on the network.
    Transport* outgoing_transport = nullptr;

    // Called when the receiver requests an intra frame.
    RtcpIntraFrameObserver* intra_frame_callback = nullptr;

    // Called when the receiver sends a loss notification.
    RtcpLossNotificationObserver* rtcp_loss_notification_observer = nullptr;

    // Called when we receive a changed estimate from the receiver of out
    // stream.
    RtcpBandwidthObserver* bandwidth_callback = nullptr;

    NetworkStateEstimateObserver* network_state_estimate_observer = nullptr;
    TransportFeedbackObserver* transport_feedback_callback = nullptr;
    VideoBitrateAllocationObserver* bitrate_allocation_observer = nullptr;
    RtcpRttStats* rtt_stats = nullptr;
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer = nullptr;

    // Estimates the bandwidth available for a set of streams from the same
    // client.
    RemoteBitrateEstimator* remote_bitrate_estimator = nullptr;

    // Spread any bursts of packets into smaller bursts to minimize packet loss.
    RtpPacketSender* paced_sender = nullptr;

    // Generate FlexFEC packets.
    // TODO(brandtr): Remove when FlexfecSender is wired up to PacedSender.
    FlexfecSender* flexfec_sender = nullptr;

    BitrateStatisticsObserver* send_bitrate_observer = nullptr;
    SendSideDelayObserver* send_side_delay_observer = nullptr;
    RtcEventLog* event_log = nullptr;
    SendPacketObserver* send_packet_observer = nullptr;
    RateLimiter* retransmission_rate_limiter = nullptr;
    OverheadObserver* overhead_observer = nullptr;
    RtcpAckObserver* ack_observer = nullptr;

    int rtcp_report_interval_ms = 0;

    // Update network2 instead of pacer_exit field of video timing extension.
    bool populate_network2_timestamp = false;

    // E2EE Custom Video Frame Encryption
    FrameEncryptorInterface* frame_encryptor = nullptr;
    // Require all outgoing frames to be encrypted with a FrameEncryptor.
    bool require_frame_encryption = false;

    // Corresponds to extmap-allow-mixed in SDP negotiation.
    bool extmap_allow_mixed = false;

    // If set, field trials are read from |field_trials|, otherwise
    // defaults to  webrtc::FieldTrialBasedConfig.
    const WebRtcKeyValueConfig* field_trials = nullptr;

    // SSRCs for media and retransmission, respectively.
    // FlexFec SSRC is fetched from |flexfec_sender|.
    absl::optional<uint32_t> local_media_ssrc;
    absl::optional<uint32_t> rtx_send_ssrc;

   private:
    RTC_DISALLOW_COPY_AND_ASSIGN(Configuration);
  };

  // Creates an RTP/RTCP module object using provided |configuration|.
  static std::unique_ptr<RtpRtcp> Create(const Configuration& configuration);

  // **************************************************************************
  // Receiver functions
  // **************************************************************************

  virtual void IncomingRtcpPacket(const uint8_t* incoming_packet,
                                  size_t incoming_packet_length) = 0;

  virtual void SetRemoteSSRC(uint32_t ssrc) = 0;

  // **************************************************************************
  // Sender
  // **************************************************************************

  // Sets the maximum size of an RTP packet, including RTP headers.
  virtual void SetMaxRtpPacketSize(size_t size) = 0;

  // Returns max RTP packet size. Takes into account RTP headers and
  // FEC/ULP/RED overhead (when FEC is enabled).
  virtual size_t MaxRtpPacketSize() const = 0;

  virtual void RegisterSendPayloadFrequency(int payload_type,
                                            int payload_frequency) = 0;

  // Unregisters a send payload.
  // |payload_type| - payload type of codec
  // Returns -1 on failure else 0.
  virtual int32_t DeRegisterSendPayload(int8_t payload_type) = 0;

  virtual void SetExtmapAllowMixed(bool extmap_allow_mixed) = 0;

  // (De)registers RTP header extension type and id.
  // Returns -1 on failure else 0.
  virtual int32_t RegisterSendRtpHeaderExtension(RTPExtensionType type,
                                                 uint8_t id) = 0;
  // Register extension by uri, returns false on failure.
  virtual bool RegisterRtpHeaderExtension(const std::string& uri, int id) = 0;

  virtual int32_t DeregisterSendRtpHeaderExtension(RTPExtensionType type) = 0;

  // Returns true if RTP module is send media, and any of the extensions
  // required for bandwidth estimation is registered.
  virtual bool SupportsPadding() const = 0;
  // Same as SupportsPadding(), but additionally requires that
  // SetRtxSendStatus() has been called with the kRtxRedundantPayloads option
  // enabled.
  virtual bool SupportsRtxPayloadPadding() const = 0;

  // Returns start timestamp.
  virtual uint32_t StartTimestamp() const = 0;

  // Sets start timestamp. Start timestamp is set to a random value if this
  // function is never called.
  virtual void SetStartTimestamp(uint32_t timestamp) = 0;

  // Returns SequenceNumber.
  virtual uint16_t SequenceNumber() const = 0;

  // Sets SequenceNumber, default is a random number.
  virtual void SetSequenceNumber(uint16_t seq) = 0;

  virtual void SetRtpState(const RtpState& rtp_state) = 0;
  virtual void SetRtxState(const RtpState& rtp_state) = 0;
  virtual RtpState GetRtpState() const = 0;
  virtual RtpState GetRtxState() const = 0;

  // Returns SSRC.
  uint32_t SSRC() const override = 0;

  // Sets SSRC, default is a random number.
  // TODO(bugs.webrtc.org/10774): Remove.
  virtual void SetSSRC(uint32_t ssrc) = 0;

  // Sets the value for sending in the RID (and Repaired) RTP header extension.
  // RIDs are used to identify an RTP stream if SSRCs are not negotiated.
  // If the RID and Repaired RID extensions are not registered, the RID will
  // not be sent.
  virtual void SetRid(const std::string& rid) = 0;

  // Sets the value for sending in the MID RTP header extension.
  // The MID RTP header extension should be registered for this to do anything.
  // Once set, this value can not be changed or removed.
  virtual void SetMid(const std::string& mid) = 0;

  // Sets CSRC.
  // |csrcs| - vector of CSRCs
  virtual void SetCsrcs(const std::vector<uint32_t>& csrcs) = 0;

  // Turns on/off sending RTX (RFC 4588). The modes can be set as a combination
  // of values of the enumerator RtxMode.
  virtual void SetRtxSendStatus(int modes) = 0;

  // Returns status of sending RTX (RFC 4588). The returned value can be
  // a combination of values of the enumerator RtxMode.
  virtual int RtxSendStatus() const = 0;

  // Sets the SSRC to use when sending RTX packets. This doesn't enable RTX,
  // only the SSRC is set.
  // TODO(bugs.webrtc.org/10774): Remove.
  virtual void SetRtxSsrc(uint32_t ssrc) = 0;

  // Sets the payload type to use when sending RTX packets. Note that this
  // doesn't enable RTX, only the payload type is set.
  virtual void SetRtxSendPayloadType(int payload_type,
                                     int associated_payload_type) = 0;

  // Returns the FlexFEC SSRC, if there is one.
  virtual absl::optional<uint32_t> FlexfecSsrc() const = 0;

  // Sets sending status. Sends kRtcpByeCode when going from true to false.
  // Returns -1 on failure else 0.
  virtual int32_t SetSendingStatus(bool sending) = 0;

  // Returns current sending status.
  virtual bool Sending() const = 0;

  // Starts/Stops media packets. On by default.
  virtual void SetSendingMediaStatus(bool sending) = 0;

  // Returns current media sending status.
  virtual bool SendingMedia() const = 0;

  // Indicate that the packets sent by this module should be counted towards the
  // bitrate estimate since the stream participates in the bitrate allocation.
  virtual void SetAsPartOfAllocation(bool part_of_allocation) = 0;

  // Fetches the current send bitrates in bits/s.
  virtual void BitrateSent(uint32_t* total_rate,
                           uint32_t* video_rate,
                           uint32_t* fec_rate,
                           uint32_t* nack_rate) const = 0;

  virtual RTPSender* RtpSender() = 0;
  virtual const RTPSender* RtpSender() const = 0;

  // Record that a frame is about to be sent. Returns true on success, and false
  // if the module isn't ready to send.
  virtual bool OnSendingRtpFrame(uint32_t timestamp,
                                 int64_t capture_time_ms,
                                 int payload_type,
                                 bool force_sender_report) = 0;

  // Try to send the provided packet. Returns true iff packet matches any of
  // the SSRCs for this module (media/rtx/fec etc) and was forwarded to the
  // transport.
  virtual bool TrySendPacket(RtpPacketToSend* packet,
                             const PacedPacketInfo& pacing_info) = 0;

  virtual std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes) = 0;

  // Called on generation of new statistics after an RTP send.
  virtual void RegisterSendChannelRtpStatisticsCallback(
      StreamDataCountersCallback* callback) = 0;
  virtual StreamDataCountersCallback* GetSendChannelRtpStatisticsCallback()
      const = 0;

  // **************************************************************************
  // RTCP
  // **************************************************************************

  // Returns RTCP status.
  virtual RtcpMode RTCP() const = 0;

  // Sets RTCP status i.e on(compound or non-compound)/off.
  // |method| - RTCP method to use.
  virtual void SetRTCPStatus(RtcpMode method) = 0;

  // Sets RTCP CName (i.e unique identifier).
  // Returns -1 on failure else 0.
  virtual int32_t SetCNAME(const char* cname) = 0;

  // Returns remote CName.
  // Returns -1 on failure else 0.
  virtual int32_t RemoteCNAME(uint32_t remote_ssrc,
                              char cname[RTCP_CNAME_SIZE]) const = 0;

  // Returns remote NTP.
  // Returns -1 on failure else 0.
  virtual int32_t RemoteNTP(uint32_t* received_ntp_secs,
                            uint32_t* received_ntp_frac,
                            uint32_t* rtcp_arrival_time_secs,
                            uint32_t* rtcp_arrival_time_frac,
                            uint32_t* rtcp_timestamp) const = 0;

  // Returns -1 on failure else 0.
  virtual int32_t AddMixedCNAME(uint32_t ssrc, const char* cname) = 0;

  // Returns -1 on failure else 0.
  virtual int32_t RemoveMixedCNAME(uint32_t ssrc) = 0;

  // Returns current RTT (round-trip time) estimate.
  // Returns -1 on failure else 0.
  virtual int32_t RTT(uint32_t remote_ssrc,
                      int64_t* rtt,
                      int64_t* avg_rtt,
                      int64_t* min_rtt,
                      int64_t* max_rtt) const = 0;

  // Returns the estimated RTT, with fallback to a default value.
  virtual int64_t ExpectedRetransmissionTimeMs() const = 0;

  // Forces a send of a RTCP packet. Periodic SR and RR are triggered via the
  // process function.
  // Returns -1 on failure else 0.
  virtual int32_t SendRTCP(RTCPPacketType rtcp_packet_type) = 0;

  // Returns statistics of the amount of data sent.
  // Returns -1 on failure else 0.
  virtual int32_t DataCountersRTP(size_t* bytes_sent,
                                  uint32_t* packets_sent) const = 0;

  // Returns send statistics for the RTP and RTX stream.
  virtual void GetSendStreamDataCounters(
      StreamDataCounters* rtp_counters,
      StreamDataCounters* rtx_counters) const = 0;

  // Returns received RTCP report block.
  // Returns -1 on failure else 0.
  // TODO(https://crbug.com/webrtc/10678): Remove this in favor of
  // GetLatestReportBlockData().
  virtual int32_t RemoteRTCPStat(
      std::vector<RTCPReportBlock>* receive_blocks) const = 0;
  // A snapshot of Report Blocks with additional data of interest to statistics.
  // Within this list, the sender-source SSRC pair is unique and per-pair the
  // ReportBlockData represents the latest Report Block that was received for
  // that pair.
  virtual std::vector<ReportBlockData> GetLatestReportBlockData() const = 0;

  // (APP) Sets application specific data.
  // Returns -1 on failure else 0.
  virtual int32_t SetRTCPApplicationSpecificData(uint8_t sub_type,
                                                 uint32_t name,
                                                 const uint8_t* data,
                                                 uint16_t length) = 0;
  // (XR) Sets Receiver Reference Time Report (RTTR) status.
  virtual void SetRtcpXrRrtrStatus(bool enable) = 0;

  // Returns current Receiver Reference Time Report (RTTR) status.
  virtual bool RtcpXrRrtrStatus() const = 0;

  // (REMB) Receiver Estimated Max Bitrate.
  // Schedules sending REMB on next and following sender/receiver reports.
  void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs) override = 0;
  // Stops sending REMB on next and following sender/receiver reports.
  void UnsetRemb() override = 0;

  // (TMMBR) Temporary Max Media Bit Rate
  virtual bool TMMBR() const = 0;

  virtual void SetTMMBRStatus(bool enable) = 0;

  // (NACK)

  // Sends a Negative acknowledgement packet.
  // Returns -1 on failure else 0.
  // TODO(philipel): Deprecate this and start using SendNack instead, mostly
  // because we want a function that actually send NACK for the specified
  // packets.
  virtual int32_t SendNACK(const uint16_t* nack_list, uint16_t size) = 0;

  // Sends NACK for the packets specified.
  // Note: This assumes the caller keeps track of timing and doesn't rely on
  // the RTP module to do this.
  virtual void SendNack(const std::vector<uint16_t>& sequence_numbers) = 0;

  // Store the sent packets, needed to answer to a Negative acknowledgment
  // requests.
  virtual void SetStorePacketsStatus(bool enable, uint16_t numberToStore) = 0;

  // Returns true if the module is configured to store packets.
  virtual bool StorePackets() const = 0;

  // Called on receipt of RTCP report block from remote side.
  // TODO(https://crbug.com/webrtc/10678): Remove RtcpStatisticsCallback in
  // favor of ReportBlockDataObserver.
  // TODO(https://crbug.com/webrtc/10679): Consider whether we want to use only
  // getters or only callbacks. If we decide on getters, the
  // ReportBlockDataObserver should also be removed in favor of
  // GetLatestReportBlockData().
  // TODO(nisse): Replace RegisterRtcpStatisticsCallback and
  // RegisterRtcpCnameCallback with construction-time settings in
  // RtpRtcp::Configuration.
  virtual void RegisterRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback) = 0;
  virtual RtcpStatisticsCallback* GetRtcpStatisticsCallback() = 0;
  virtual void RegisterRtcpCnameCallback(RtcpCnameCallback* callback) = 0;
  // TODO(https://crbug.com/webrtc/10680): When callbacks are registered at
  // construction, remove this setter.
  virtual void SetReportBlockDataObserver(
      ReportBlockDataObserver* observer) = 0;
  // BWE feedback packets.
  bool SendFeedbackPacket(const rtcp::TransportFeedback& packet) override = 0;

  virtual void SetVideoBitrateAllocation(
      const VideoBitrateAllocation& bitrate) = 0;

  // **************************************************************************
  // Video
  // **************************************************************************

  // Requests new key frame.
  // using PLI, https://tools.ietf.org/html/rfc4585#section-6.3.1.1
  void SendPictureLossIndication() { SendRTCP(kRtcpPli); }
  // using FIR, https://tools.ietf.org/html/rfc5104#section-4.3.1.2
  void SendFullIntraRequest() { SendRTCP(kRtcpFir); }

  // Sends a LossNotification RTCP message.
  // Returns -1 on failure else 0.
  virtual int32_t SendLossNotification(uint16_t last_decoded_seq_num,
                                       uint16_t last_received_seq_num,
                                       bool decodability_flag,
                                       bool buffering_allowed) = 0;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_H_
