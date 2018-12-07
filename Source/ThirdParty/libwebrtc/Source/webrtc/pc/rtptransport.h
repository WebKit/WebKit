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

#include "call/rtp_demuxer.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "pc/rtptransportinternal.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace rtc {

class CopyOnWriteBuffer;
struct PacketOptions;
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

  PacketTransportInterface* GetRtpPacketTransport() const override {
    return rtp_packet_transport_;
  }
  PacketTransportInterface* GetRtcpPacketTransport() const override {
    return rtcp_packet_transport_;
  }

  // TODO(zstein): Use these RtcpParameters for configuration elsewhere.
  RTCError SetParameters(const RtpTransportParameters& parameters) override;
  RtpTransportParameters GetParameters() const override;

  bool IsReadyToSend() const override { return ready_to_send_; }

  bool IsWritable(bool rtcp) const override;

  bool SendRtpPacket(rtc::CopyOnWriteBuffer* packet,
                     const rtc::PacketOptions& options,
                     int flags) override;

  bool SendRtcpPacket(rtc::CopyOnWriteBuffer* packet,
                      const rtc::PacketOptions& options,
                      int flags) override;

  bool IsSrtpActive() const override { return false; }

  void UpdateRtpHeaderExtensionMap(
      const cricket::RtpHeaderExtensions& header_extensions) override;

  bool RegisterRtpDemuxerSink(const RtpDemuxerCriteria& criteria,
                              RtpPacketSinkInterface* sink) override;

  bool UnregisterRtpDemuxerSink(RtpPacketSinkInterface* sink) override;

 protected:
  // TODO(zstein): Remove this when we remove RtpTransportAdapter.
  RtpTransportAdapter* GetInternal() override;

  // These methods will be used in the subclasses.
  void DemuxPacket(rtc::CopyOnWriteBuffer* packet, int64_t packet_time_us);

  bool SendPacket(bool rtcp,
                  rtc::CopyOnWriteBuffer* packet,
                  const rtc::PacketOptions& options,
                  int flags);

  // Overridden by SrtpTransport.
  virtual void OnNetworkRouteChanged(
      absl::optional<rtc::NetworkRoute> network_route);
  virtual void OnRtpPacketReceived(rtc::CopyOnWriteBuffer* packet,
                                   int64_t packet_time_us);
  virtual void OnRtcpPacketReceived(rtc::CopyOnWriteBuffer* packet,
                                    int64_t packet_time_us);
  // Overridden by SrtpTransport and DtlsSrtpTransport.
  virtual void OnWritableState(rtc::PacketTransportInternal* packet_transport);

 private:
  void OnReadyToSend(rtc::PacketTransportInternal* transport);
  void OnSentPacket(rtc::PacketTransportInternal* packet_transport,
                    const rtc::SentPacket& sent_packet);
  void OnReadPacket(rtc::PacketTransportInternal* transport,
                    const char* data,
                    size_t len,
                    const int64_t& packet_time_us,
                    int flags);

  // Updates "ready to send" for an individual channel and fires
  // SignalReadyToSend.
  void SetReadyToSend(bool rtcp, bool ready);

  void MaybeSignalReadyToSend();

  bool IsTransportWritable();

  // SRTP specific methods.
  // TODO(zhihuang): Improve the inheritance model so that the RtpTransport
  // doesn't need to implement SRTP specfic methods.
  RTCError SetSrtpSendKey(const cricket::CryptoParams& params) override {
    RTC_NOTREACHED();
    return RTCError::OK();
  }
  RTCError SetSrtpReceiveKey(const cricket::CryptoParams& params) override {
    RTC_NOTREACHED();
    return RTCError::OK();
  }

  bool rtcp_mux_enabled_;

  rtc::PacketTransportInternal* rtp_packet_transport_ = nullptr;
  rtc::PacketTransportInternal* rtcp_packet_transport_ = nullptr;

  bool ready_to_send_ = false;
  bool rtp_ready_to_send_ = false;
  bool rtcp_ready_to_send_ = false;

  RtpTransportParameters parameters_;
  RtpDemuxer rtp_demuxer_;

  // Used for identifying the MID for RtpDemuxer.
  RtpHeaderExtensionMap header_extension_map_;
};

}  // namespace webrtc

#endif  // PC_RTPTRANSPORT_H_
