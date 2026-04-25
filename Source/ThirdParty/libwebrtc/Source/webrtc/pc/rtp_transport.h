/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTP_TRANSPORT_H_
#define PC_RTP_TRANSPORT_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "api/field_trials_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/ecn_marking.h"
#include "api/units/timestamp.h"
#include "call/rtp_demuxer.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "p2p/base/packet_transport_internal.h"
#include "pc/rtp_transport_internal.h"
#include "pc/session_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"

namespace webrtc {

class CopyOnWriteBuffer;
class DtlsSrtpTransport;

class RtpTransport : public RtpTransportInternal {
 public:
  RtpTransport(const RtpTransport&) = delete;
  RtpTransport& operator=(const RtpTransport&) = delete;

  RtpTransport(bool rtcp_mux_enabled, const FieldTrialsView& field_trials)
      : rtcp_mux_enabled_(rtcp_mux_enabled) {}

  virtual DtlsSrtpTransport* AsDtlsSrtpTransport() { return nullptr; }

  bool rtcp_mux_enabled() const override { return rtcp_mux_enabled_; }
  void SetRtcpMuxEnabled(bool enable) override;

  const std::string& transport_name() const override;

  int SetRtpOption(Socket::Option opt, int value) override;
  int SetRtcpOption(Socket::Option opt, int value) override;

  PacketTransportInternal* rtp_packet_transport() const {
    return rtp_packet_transport_;
  }
  void SetRtpPacketTransport(PacketTransportInternal* rtp);
  void SetRtpPacketTransportOwned(std::unique_ptr<PacketTransportInternal> rtp);

  PacketTransportInternal* rtcp_packet_transport() const {
    return rtcp_packet_transport_;
  }
  void SetRtcpPacketTransport(PacketTransportInternal* rtcp);
  void SetRtcpPacketTransportOwned(
      std::unique_ptr<PacketTransportInternal> rtcp);

  bool IsReadyToSend() const override { return ready_to_send_; }

  bool IsWritable(bool rtcp) const override;

  bool SendRtpPacket(CopyOnWriteBuffer* packet,
                     const AsyncSocketPacketOptions& options,
                     int flags) override;

  bool SendRtcpPacket(CopyOnWriteBuffer* packet,
                      const AsyncSocketPacketOptions& options,
                      int flags) override;

  bool IsSrtpActive() const override { return false; }

  void UpdateRtpHeaderExtensionMap(
      const RtpHeaderExtensions& header_extensions) override;

  bool RegisterRtpDemuxerSink(const RtpDemuxerCriteria& criteria,
                              RtpPacketSinkInterface* sink) override;

  bool UnregisterRtpDemuxerSink(RtpPacketSinkInterface* sink) override;

 protected:
  // These methods will be used in the subclasses.
  void DemuxPacket(CopyOnWriteBuffer packet,
                   Timestamp arrival_time,
                   EcnMarking ecn);

  flat_set<uint32_t> GetSsrcsForSink(RtpPacketSinkInterface* sink);

  // Overridden by SrtpTransport.
  virtual void OnNetworkRouteChanged(std::optional<NetworkRoute> network_route);
  virtual void OnRtpPacketReceived(const ReceivedIpPacket& packet);
  virtual void OnRtcpPacketReceived(const ReceivedIpPacket& packet);
  // Overridden by SrtpTransport and DtlsSrtpTransport.
  virtual void OnWritableState(PacketTransportInternal* packet_transport);

 private:
  bool SendPacket(bool rtcp,
                  CopyOnWriteBuffer* packet,
                  const AsyncSocketPacketOptions& options,
                  int flags);
  // Helper function for SetRt(c)pPacketTransport
  void ChangePacketTransport(PacketTransportInternal* new_transport,
                             PacketTransportInternal*& transport_to_change);
  void OnReadyToSend(PacketTransportInternal* transport);
  void OnSentPacket(PacketTransportInternal* packet_transport,
                    const SentPacketInfo& sent_packet);
  void OnReadPacket(PacketTransportInternal* transport,
                    const ReceivedIpPacket& received_packet);

  // Updates "ready to send" for an individual channel and fires
  // SignalReadyToSend.
  void SetReadyToSend(bool rtcp, bool ready);

  void MaybeSignalReadyToSend();

  bool IsTransportWritable();

  bool rtcp_mux_enabled_;

  PacketTransportInternal* rtp_packet_transport_ = nullptr;
  PacketTransportInternal* rtcp_packet_transport_ = nullptr;
  std::unique_ptr<PacketTransportInternal> owned_rtp_packet_transport_;
  std::unique_ptr<PacketTransportInternal> owned_rtcp_packet_transport_;

  bool ready_to_send_ = false;
  bool received_rtp_with_ecn_ = false;
  bool rtp_ready_to_send_ = false;
  bool rtcp_ready_to_send_ = false;

  RtpDemuxer rtp_demuxer_;

  // Used for identifying the MID for RtpDemuxer.
  RtpHeaderExtensionMap header_extension_map_;
  // Guard against recursive "ready to send" signals
  bool processing_ready_to_send_ = false;
  ScopedTaskSafety safety_;
};

}  // namespace webrtc

#endif  // PC_RTP_TRANSPORT_H_
