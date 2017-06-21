/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_RTPTRANSPORT_H_
#define WEBRTC_PC_RTPTRANSPORT_H_

#include "webrtc/api/ortc/rtptransportinterface.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/pc/bundlefilter.h"

namespace rtc {

class CopyOnWriteBuffer;
struct PacketOptions;
struct PacketTime;
class PacketTransportInternal;

}  // namespace rtc

namespace webrtc {

class RtpTransport : public RtpTransportInterface, public sigslot::has_slots<> {
 public:
  RtpTransport(const RtpTransport&) = delete;
  RtpTransport& operator=(const RtpTransport&) = delete;

  explicit RtpTransport(bool rtcp_mux_enabled)
      : rtcp_mux_enabled_(rtcp_mux_enabled) {}

  bool rtcp_mux_enabled() const { return rtcp_mux_enabled_; }
  void SetRtcpMuxEnabled(bool enable);

  rtc::PacketTransportInternal* rtp_packet_transport() const {
    return rtp_packet_transport_;
  }
  void SetRtpPacketTransport(rtc::PacketTransportInternal* rtp);

  rtc::PacketTransportInternal* rtcp_packet_transport() const {
    return rtcp_packet_transport_;
  }
  void SetRtcpPacketTransport(rtc::PacketTransportInternal* rtcp);

  PacketTransportInterface* GetRtpPacketTransport() const override;
  PacketTransportInterface* GetRtcpPacketTransport() const override;

  // TODO(zstein): Use these RtcpParameters for configuration elsewhere.
  RTCError SetRtcpParameters(const RtcpParameters& parameters) override;
  RtcpParameters GetRtcpParameters() const override;

  // Called whenever a transport's ready-to-send state changes. The argument
  // is true if all used transports are ready to send. This is more specific
  // than just "writable"; it means the last send didn't return ENOTCONN.
  sigslot::signal1<bool> SignalReadyToSend;

  bool IsWritable(bool rtcp) const;

  bool SendPacket(bool rtcp,
                  const rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options,
                  int flags);

  bool HandlesPayloadType(int payload_type) const;

  void AddHandledPayloadType(int payload_type);

  // TODO(zstein): Consider having two signals - RtcPacketReceived and
  // RtcpPacketReceived.
  // The first argument is true for RTCP packets and false for RTP packets.
  sigslot::signal3<bool, rtc::CopyOnWriteBuffer&, const rtc::PacketTime&>
      SignalPacketReceived;

 protected:
  // TODO(zstein): Remove this when we remove RtpTransportAdapter.
  RtpTransportAdapter* GetInternal() override;

 private:
  bool HandlesPacket(const uint8_t* data, size_t len);

  void OnReadyToSend(rtc::PacketTransportInternal* transport);

  // Updates "ready to send" for an individual channel and fires
  // SignalReadyToSend.
  void SetReadyToSend(bool rtcp, bool ready);

  void MaybeSignalReadyToSend();

  void OnReadPacket(rtc::PacketTransportInternal* transport,
                    const char* data,
                    size_t len,
                    const rtc::PacketTime& packet_time,
                    int flags);

  bool WantsPacket(bool rtcp, const rtc::CopyOnWriteBuffer* packet);

  bool rtcp_mux_enabled_;

  rtc::PacketTransportInternal* rtp_packet_transport_ = nullptr;
  rtc::PacketTransportInternal* rtcp_packet_transport_ = nullptr;

  bool ready_to_send_ = false;
  bool rtp_ready_to_send_ = false;
  bool rtcp_ready_to_send_ = false;

  RtcpParameters rtcp_parameters_;

  cricket::BundleFilter bundle_filter_;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_RTPTRANSPORT_H_
