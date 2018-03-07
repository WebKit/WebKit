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

#include "api/ortc/srtptransportinterface.h"
#include "api/rtcerror.h"
#include "media/base/streamparams.h"
#include "ortc/rtptransportcontrolleradapter.h"
#include "pc/channel.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/sigslot.h"

namespace webrtc {

// Implementation of SrtpTransportInterface to be used with RtpSenderAdapter,
// RtpReceiverAdapter, and RtpTransportControllerAdapter classes. This class
// is used to implement both a secure and insecure RTP transport.
//
// TODO(deadbeef): When BaseChannel is split apart into separate
// "RtpTransport"/"RtpTransceiver"/"RtpSender"/"RtpReceiver" objects, this
// adapter object can be removed.
class RtpTransportAdapter : public SrtpTransportInterface {
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
  PacketTransportInterface* GetRtpPacketTransport() const override;
  PacketTransportInterface* GetRtcpPacketTransport() const override;
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

  // Used by the RtpTransportControllerAdapter to tell if an rtp sender or
  // receiver can be created.
  bool is_srtp_transport() { return is_srtp_transport_; }
  // Used by the RtpTransportControllerAdapter to set keys for senders and
  // receivers.
  rtc::Optional<cricket::CryptoParams> send_key() { return send_key_; }
  rtc::Optional<cricket::CryptoParams> receive_key() { return receive_key_; }

 protected:
  RtpTransportAdapter* GetInternal() override { return this; }

 private:
  RtpTransportAdapter(const RtcpParameters& rtcp_params,
                      PacketTransportInterface* rtp,
                      PacketTransportInterface* rtcp,
                      RtpTransportControllerAdapter* rtp_transport_controller,
                      bool is_srtp_transport);

  PacketTransportInterface* rtp_packet_transport_;
  PacketTransportInterface* rtcp_packet_transport_;
  RtpTransportControllerAdapter* const rtp_transport_controller_;
  // Non-null if this class owns the transport controller.
  std::unique_ptr<RtpTransportControllerInterface>
      owned_rtp_transport_controller_;
  RtpTransportParameters parameters_;

  // SRTP specific members.
  rtc::Optional<cricket::CryptoParams> send_key_;
  rtc::Optional<cricket::CryptoParams> receive_key_;
  bool is_srtp_transport_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RtpTransportAdapter);
};

}  // namespace webrtc

#endif  // ORTC_RTPTRANSPORTADAPTER_H_
