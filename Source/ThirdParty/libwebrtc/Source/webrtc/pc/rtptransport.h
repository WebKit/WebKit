/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTPTRANSPORT_H_
#define PC_RTPTRANSPORT_H_

#include <string>

#include "pc/bundlefilter.h"
#include "pc/rtptransportinternal.h"
#include "rtc_base/sigslot.h"

namespace rtc {

class CopyOnWriteBuffer;
struct PacketOptions;
struct PacketTime;
class PacketTransportInternal;

}  // namespace rtc

namespace webrtc {

class RtpTransport : public RtpTransportInternal {
 public:
  RtpTransport(const RtpTransport&) = delete;
  RtpTransport& operator=(const RtpTransport&) = delete;

  explicit RtpTransport(bool rtcp_mux_enabled)
      : rtcp_mux_enabled_(rtcp_mux_enabled) {}

  bool rtcp_mux_enabled() const override { return rtcp_mux_enabled_; }
  void SetRtcpMuxEnabled(bool enable) override;

  rtc::PacketTransportInternal* rtp_packet_transport() const override {
    return rtp_packet_transport_;
  }
  void SetRtpPacketTransport(rtc::PacketTransportInternal* rtp) override;

  rtc::PacketTransportInternal* rtcp_packet_transport() const override {
    return rtcp_packet_transport_;
  }
  void SetRtcpPacketTransport(rtc::PacketTransportInternal* rtcp) override;

  PacketTransportInterface* GetRtpPacketTransport() const override;
  PacketTransportInterface* GetRtcpPacketTransport() const override;

  // TODO(zstein): Use these RtcpParameters for configuration elsewhere.
  RTCError SetParameters(const RtpTransportParameters& parameters) override;
  RtpTransportParameters GetParameters() const override;

  bool IsWritable(bool rtcp) const override;

  bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                     const rtc::PacketOptions& options,
                     int flags) override;

  bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketOptions& options,
                      int flags) override;

  bool HandlesPayloadType(int payload_type) const override;

  void AddHandledPayloadType(int payload_type) override;

 protected:
  // TODO(zstein): Remove this when we remove RtpTransportAdapter.
  RtpTransportAdapter* GetInternal() override;

 private:
  bool IsRtpTransportWritable();
  bool HandlesPacket(const uint8_t* data, size_t len);

  void OnReadyToSend(rtc::PacketTransportInternal* transport);
  void OnNetworkRouteChange(rtc::Optional<rtc::NetworkRoute> network_route);
  void OnWritableState(rtc::PacketTransportInternal* packet_transport);
  void OnSentPacket(rtc::PacketTransportInternal* packet_transport,
                    const rtc::SentPacket& sent_packet);

  // Updates "ready to send" for an individual channel and fires
  // SignalReadyToSend.
  void SetReadyToSend(bool rtcp, bool ready);

  void MaybeSignalReadyToSend();

  bool SendPacket(bool rtcp,
                  rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options,
                  int flags);

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

  RtpTransportParameters parameters_;

  cricket::BundleFilter bundle_filter_;
};

}  // namespace webrtc

#endif  // PC_RTPTRANSPORT_H_
