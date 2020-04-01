/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_DATAGRAM_RTP_TRANSPORT_H_
#define PC_DATAGRAM_RTP_TRANSPORT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/crypto/crypto_options.h"
#include "api/transport/datagram_transport_interface.h"
#include "api/transport/media/media_transport_interface.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/packet_transport_internal.h"
#include "pc/rtp_transport_internal.h"
#include "rtc_base/buffer.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

constexpr int kDatagramDtlsAdaptorComponent = -1;

// RTP transport which uses the DatagramTransportInterface to send and receive
// packets.
class DatagramRtpTransport : public RtpTransportInternal,
                             public webrtc::DatagramSinkInterface,
                             public webrtc::MediaTransportStateCallback {
 public:
  DatagramRtpTransport(
      const std::vector<webrtc::RtpExtension>& rtp_header_extensions,
      cricket::IceTransportInternal* ice_transport,
      DatagramTransportInterface* datagram_transport);

  ~DatagramRtpTransport() override;

  // =====================================================
  // Overrides for webrtc::DatagramTransportSinkInterface
  // and MediaTransportStateCallback
  // =====================================================
  void OnDatagramReceived(rtc::ArrayView<const uint8_t> data) override;

  void OnDatagramSent(webrtc::DatagramId datagram_id) override;

  void OnDatagramAcked(const webrtc::DatagramAck& ack) override;

  void OnDatagramLost(webrtc::DatagramId datagram_id) override;

  void OnStateChanged(webrtc::MediaTransportState state) override;

  // =====================================================
  // RtpTransportInternal overrides
  // =====================================================
  bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                     const rtc::PacketOptions& options,
                     int flags) override;

  bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketOptions& options,
                      int flags) override;

  const std::string& transport_name() const override;

  // Datagram transport always muxes RTCP.
  bool rtcp_mux_enabled() const override { return true; }
  void SetRtcpMuxEnabled(bool enable) override {}

  int SetRtpOption(rtc::Socket::Option opt, int value) override;
  int SetRtcpOption(rtc::Socket::Option opt, int value) override;

  bool IsReadyToSend() const override;

  bool IsWritable(bool rtcp) const override;

  bool IsSrtpActive() const override { return false; }

  void UpdateRtpHeaderExtensionMap(
      const cricket::RtpHeaderExtensions& header_extensions) override;

  bool RegisterRtpDemuxerSink(const RtpDemuxerCriteria& criteria,
                              RtpPacketSinkInterface* sink) override;

  bool UnregisterRtpDemuxerSink(RtpPacketSinkInterface* sink) override;

 private:
  // RTP/RTCP packet info stored for each sent packet.
  struct SentPacketInfo {
    // RTP packet info with ssrc and transport sequence number.
    SentPacketInfo(int64_t packet_id,
                   uint32_t ssrc,
                   uint16_t transport_sequence_number)
        : ssrc(ssrc),
          transport_sequence_number(transport_sequence_number),
          packet_id(packet_id) {}

    // Packet info without SSRC and transport sequence number used for RTCP
    // packets, RTP packets when transport sequence number is not provided or
    // when feedback translation is disabled.
    explicit SentPacketInfo(int64_t packet_id) : packet_id(packet_id) {}

    SentPacketInfo() = default;

    absl::optional<uint32_t> ssrc;

    // Transport sequence number (if it was provided in outgoing RTP packet).
    // It is used to re-create RTCP feedback packets from datagram ACKs.
    absl::optional<uint16_t> transport_sequence_number;

    // Packet id from rtc::PacketOptions. It is required to propagage sent
    // notification up the stack (SignalSentPacket).
    int64_t packet_id = 0;
  };

  // Finds SentPacketInfo for given |datagram_id| and removes map entry.
  // Returns false if entry was not found.
  bool GetAndRemoveSentPacketInfo(webrtc::DatagramId datagram_id,
                                  SentPacketInfo* sent_packet_info);

  // Sends datagram to datagram_transport.
  bool SendDatagram(rtc::ArrayView<const uint8_t> data,
                    webrtc::DatagramId datagram_id);

  // Propagates network route changes from ICE.
  void OnNetworkRouteChanged(absl::optional<rtc::NetworkRoute> network_route);

  rtc::ThreadChecker thread_checker_;
  cricket::IceTransportInternal* ice_transport_;
  webrtc::DatagramTransportInterface* datagram_transport_;

  RtpDemuxer rtp_demuxer_;

  MediaTransportState state_ = MediaTransportState::kPending;

  // Extension map for parsing transport sequence numbers.
  webrtc::RtpHeaderExtensionMap rtp_header_extension_map_;

  // Keeps information about sent RTP packet until they are Acked or Lost.
  std::map<webrtc::DatagramId, SentPacketInfo> sent_rtp_packet_map_;

  // Current datagram_id, incremented after each sent RTP packets.
  // Datagram id is passed to datagram transport when we send datagram and we
  // get it back in notifications about Sent, Acked and Lost datagrams.
  int64_t current_datagram_id_ = 0;

  // TODO(sukhanov): Previous nonzero timestamp is required for workaround for
  // zero timestamps received, which sometimes are received from datagram
  // transport. Investigate if we can eliminate zero timestamps.
  int64_t previous_nonzero_timestamp_us_ = 0;

  // Disable datagram to RTCP feedback translation and enable RTCP feedback
  // loop (note that having both RTCP and datagram feedback loops is
  // inefficient, but can be useful in tests and experiments).
  const bool disable_datagram_to_rtcp_feeback_translation_;
};

}  // namespace webrtc

#endif  // PC_DATAGRAM_RTP_TRANSPORT_H_
