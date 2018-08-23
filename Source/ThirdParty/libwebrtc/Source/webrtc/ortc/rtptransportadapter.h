/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ORTC_RTPTRANSPORTADAPTER_H_
#define ORTC_RTPTRANSPORTADAPTER_H_

#include <memory>
#include <vector>

#include "api/rtcerror.h"
#include "media/base/streamparams.h"
#include "ortc/rtptransportcontrolleradapter.h"
#include "pc/channel.h"
#include "pc/rtptransportinternaladapter.h"
#include "pc/srtptransport.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace webrtc {
// This class is a wrapper over an RtpTransport or an SrtpTransport. The base
// class RtpTransportInternalAdapter keeps a raw pointer, |transport_|, of the
// transport object and implements both the public SrtpTransportInterface and
// RtpTransport internal interface by calling the |transport_| underneath.
//
// This adapter can be used as an unencrypted RTP transport or an SrtpTransport
// with RtpSenderAdapter, RtpReceiverAdapter, and RtpTransportControllerAdapter.
//
// TODO(deadbeef): When BaseChannel is split apart into separate
// "RtpTransport"/"RtpTransceiver"/"RtpSender"/"RtpReceiver" objects, this
// adapter object can be removed.
class RtpTransportAdapter : public RtpTransportInternalAdapter {
 public:
  // |rtp| can't be null. |rtcp| can if RTCP muxing is used immediately (meaning
  // |rtcp_parameters.mux| is also true).
  static RTCErrorOr<std::unique_ptr<RtpTransportInterface>> CreateProxied(
      const RtpTransportParameters& rtcp_parameters,
      PacketTransportInterface* rtp,
      PacketTransportInterface* rtcp,
      RtpTransportControllerAdapter* rtp_transport_controller);

  static RTCErrorOr<std::unique_ptr<SrtpTransportInterface>> CreateSrtpProxied(
      const RtpTransportParameters& rtcp_parameters,
      PacketTransportInterface* rtp,
      PacketTransportInterface* rtcp,
      RtpTransportControllerAdapter* rtp_transport_controller);

  ~RtpTransportAdapter() override;

  // RtpTransportInterface implementation.
  PacketTransportInterface* GetRtpPacketTransport() const override {
    return rtp_packet_transport_;
  }
  PacketTransportInterface* GetRtcpPacketTransport() const override {
    return rtcp_packet_transport_;
  }
  RTCError SetParameters(const RtpTransportParameters& parameters) override;
  RtpTransportParameters GetParameters() const override { return parameters_; }

  // SRTP specific implementation.
  RTCError SetSrtpSendKey(const cricket::CryptoParams& params) override;
  RTCError SetSrtpReceiveKey(const cricket::CryptoParams& params) override;

  // Methods used internally by OrtcFactory.
  RtpTransportControllerAdapter* rtp_transport_controller() {
    return rtp_transport_controller_;
  }
  void TakeOwnershipOfRtpTransportController(
      std::unique_ptr<RtpTransportControllerInterface> controller);

  // Used by RtpTransportControllerAdapter to tell when it should stop
  // returning this transport from GetTransports().
  sigslot::signal1<RtpTransportAdapter*> SignalDestroyed;

  bool IsSrtpActive() const override { return transport_->IsSrtpActive(); }

 protected:
  RtpTransportAdapter* GetInternal() override { return this; }

 private:
  RtpTransportAdapter(const RtcpParameters& rtcp_params,
                      PacketTransportInterface* rtp,
                      PacketTransportInterface* rtcp,
                      RtpTransportControllerAdapter* rtp_transport_controller,
                      bool is_srtp_transport);

  void OnReadyToSend(bool ready) { SignalReadyToSend(ready); }

  void OnRtcpPacketReceived(rtc::CopyOnWriteBuffer* packet,
                            const rtc::PacketTime& time) {
    SignalRtcpPacketReceived(packet, time);
  }

  void OnWritableState(bool writable) { SignalWritableState(writable); }

  PacketTransportInterface* rtp_packet_transport_ = nullptr;
  PacketTransportInterface* rtcp_packet_transport_ = nullptr;
  RtpTransportControllerAdapter* const rtp_transport_controller_ = nullptr;
  // Non-null if this class owns the transport controller.
  std::unique_ptr<RtpTransportControllerInterface>
      owned_rtp_transport_controller_;
  RtpTransportParameters parameters_;

  // Only one of them is non-null;
  std::unique_ptr<RtpTransport> unencrypted_rtp_transport_;
  std::unique_ptr<SrtpTransport> srtp_transport_;

  rtc::Thread* network_thread_ = nullptr;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtpTransportAdapter);
};

}  // namespace webrtc

#endif  // ORTC_RTPTRANSPORTADAPTER_H_
