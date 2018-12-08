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

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/video_bitrate_allocation.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module.h"
#include "modules/rtp_rtcp/include/flexfec_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/deprecation.h"

namespace webrtc {

// Forward declarations.
class FrameEncryptorInterface;
class OverheadObserver;
class RateLimiter;
class ReceiveStatisticsProvider;
class RemoteBitrateEstimator;
class RtcEventLog;
class Transport;
class VideoBitrateAllocationObserver;

namespace rtcp {
class TransportFeedback;
}

class RtpRtcp : public Module, public RtcpFeedbackSenderInterface {
 public:
  struct Configuration {
    Configuration();

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

    // Called when the receiver request a intra frame.
    RtcpIntraFrameObserver* intra_frame_callback = nullptr;

    // Called when we receive a changed estimate from the receiver of out
    // stream.
    RtcpBandwidthObserver* bandwidth_callback = nullptr;

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

    TransportSequenceNumberAllocator* transport_sequence_number_allocator =
        nullptr;
    BitrateStatisticsObserver* send_bitrate_observer = nullptr;
    FrameCountObserver* send_frame_count_observer = nullptr;
    SendSideDelayObserver* send_side_delay_observer = nullptr;
    RtcEventLog* event_log = nullptr;
    SendPacketObserver* send_packet_observer = nullptr;
    RateLimiter* retransmission_rate_limiter = nullptr;
    OverheadObserver* overhead_observer = nullptr;
    RtpKeepAliveConfig keepalive_config;

    int rtcp_report_interval_ms = 0;

    // Update network2 instead of pacer_exit field of video timing extension.
    bool populate_network2_timestamp = false;

    // E2EE Custom Video Frame Encryption
    FrameEncryptorInterface* frame_encryptor = nullptr;
    // Require all outgoing frames to be encrypted with a FrameEncryptor.
    bool require_frame_encryption = false;

    // Corresponds to extmap-allow-mixed in SDP negotiation.
    bool extmap_allow_mixed = false;

   private:
    RTC_DISALLOW_COPY_AND_ASSIGN(Configuration);
  };

  // Create a RTP/RTCP module object using the system clock.
  // |configuration|  - Configuration of the RTP/RTCP module.
  static RtpRtcp* CreateRtpRtcp(const RtpRtcp::Configuration& configuration);

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

  // Sets codec name and payload type. Returns -1 on failure else 0.
  virtual int32_t RegisterSendPayload(const CodecInst& voice_codec) = 0;

  virtual void RegisterVideoSendPayload(int payload_type,
                                        const char* payload_name) = 0;

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

  virtual bool HasBweExtensions() const = 0;

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
  virtual void SetSSRC(uint32_t ssrc) = 0;

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

  // Returns current bitrate in Kbit/s.
  virtual void BitrateSent(uint32_t* total_rate,
                           uint32_t* video_rate,
                           uint32_t* fec_rate,
                           uint32_t* nack_rate) const = 0;

  // Used by the codec module to deliver a video or audio frame for
  // packetization.
  // |frame_type|    - type of frame to send
  // |payload_type|  - payload type of frame to send
  // |timestamp|     - timestamp of frame to send
  // |payload_data|  - payload buffer of frame to send
  // |payload_size|  - size of payload buffer to send
  // |fragmentation| - fragmentation offset data for fragmented frames such
  //                   as layers or RED
  // |transport_frame_id_out| - set to RTP timestamp.
  // Returns true on success.
  virtual bool SendOutgoingData(FrameType frame_type,
                                int8_t payload_type,
                                uint32_t timestamp,
                                int64_t capture_time_ms,
                                const uint8_t* payload_data,
                                size_t payload_size,
                                const RTPFragmentationHeader* fragmentation,
                                const RTPVideoHeader* rtp_video_header,
                                uint32_t* transport_frame_id_out) = 0;

  virtual bool TimeToSendPacket(uint32_t ssrc,
                                uint16_t sequence_number,
                                int64_t capture_time_ms,
                                bool retransmission,
                                const PacedPacketInfo& pacing_info) = 0;

  virtual size_t TimeToSendPadding(size_t bytes,
                                   const PacedPacketInfo& pacing_info) = 0;

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

  // Forces a send of a RTCP packet. Periodic SR and RR are triggered via the
  // process function.
  // Returns -1 on failure else 0.
  virtual int32_t SendRTCP(RTCPPacketType rtcp_packet_type) = 0;

  // Forces a send of a RTCP packet with more than one packet type.
  // periodic SR and RR are triggered via the process function
  // Returns -1 on failure else 0.
  virtual int32_t SendCompoundRTCP(
      const std::set<RTCPPacketType>& rtcp_packet_types) = 0;

  // Returns statistics of the amount of data sent.
  // Returns -1 on failure else 0.
  virtual int32_t DataCountersRTP(size_t* bytes_sent,
                                  uint32_t* packets_sent) const = 0;

  // Returns send statistics for the RTP and RTX stream.
  virtual void GetSendStreamDataCounters(
      StreamDataCounters* rtp_counters,
      StreamDataCounters* rtx_counters) const = 0;

  // Returns packet loss statistics for the RTP stream.
  virtual void GetRtpPacketLossStats(
      bool outgoing,
      uint32_t ssrc,
      struct RtpPacketLossStats* loss_stats) const = 0;

  // Returns received RTCP report block.
  // Returns -1 on failure else 0.
  virtual int32_t RemoteRTCPStat(
      std::vector<RTCPReportBlock>* receive_blocks) const = 0;

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

  // TODO(holmer): Propagate this API to VideoEngine.
  // Returns the currently configured selective retransmission settings.
  virtual int SelectiveRetransmissions() const = 0;

  // TODO(holmer): Propagate this API to VideoEngine.
  // Sets the selective retransmission settings, which will decide which
  // packets will be retransmitted if NACKed. Settings are constructed by
  // combining the constants in enum RetransmissionMode with bitwise OR.
  // All packets are retransmitted if kRetransmitAllPackets is set, while no
  // packets are retransmitted if kRetransmitOff is set.
  // By default all packets except FEC packets are retransmitted. For VP8
  // with temporal scalability only base layer packets are retransmitted.
  // Returns -1 on failure, otherwise 0.
  virtual int SetSelectiveRetransmissions(uint8_t settings) = 0;

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
  virtual void RegisterRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback) = 0;
  virtual RtcpStatisticsCallback* GetRtcpStatisticsCallback() = 0;
  // BWE feedback packets.
  bool SendFeedbackPacket(const rtcp::TransportFeedback& packet) override = 0;

  virtual void SetVideoBitrateAllocation(
      const VideoBitrateAllocation& bitrate) = 0;

  // **************************************************************************
  // Audio
  // **************************************************************************

  // Sends a TelephoneEvent tone using RFC 2833 (4733).
  // Returns -1 on failure else 0.
  virtual int32_t SendTelephoneEventOutband(uint8_t key,
                                            uint16_t time_ms,
                                            uint8_t level) = 0;

  // Store the audio level in dBov for header-extension-for-audio-level-
  // indication.
  // This API shall be called before transmision of an RTP packet to ensure
  // that the |level| part of the extended RTP header is updated.
  // return -1 on failure else 0.
  virtual int32_t SetAudioLevel(uint8_t level_dbov) = 0;

  // **************************************************************************
  // Video
  // **************************************************************************

  // Set RED and ULPFEC payload types. A payload type of -1 means that the
  // corresponding feature is turned off. Note that we DO NOT support enabling
  // ULPFEC without enabling RED, and RED is only ever used when ULPFEC is
  // enabled.
  virtual void SetUlpfecConfig(int red_payload_type,
                               int ulpfec_payload_type) = 0;

  // Set FEC rates, max frames before FEC is sent, and type of FEC masks.
  // Returns false on failure.
  virtual bool SetFecParameters(const FecProtectionParams& delta_params,
                                const FecProtectionParams& key_params) = 0;

  // Deprecated version of member function above.
  RTC_DEPRECATED
  int32_t SetFecParameters(const FecProtectionParams* delta_params,
                           const FecProtectionParams* key_params);

  // Set method for requestion a new key frame.
  // Returns -1 on failure else 0.
  virtual int32_t SetKeyFrameRequestMethod(KeyFrameRequestMethod method) = 0;

  // Sends a request for a keyframe.
  // Returns -1 on failure else 0.
  virtual int32_t RequestKeyFrame() = 0;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_H_
