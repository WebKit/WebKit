/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/ortc/rtptransportadapter.h"

#include <algorithm>  // For std::find.
#include <set>
#include <sstream>
#include <utility>  // For std::move.

#include "webrtc/api/proxy.h"
#include "webrtc/base/logging.h"

namespace webrtc {

BEGIN_OWNED_PROXY_MAP(RtpTransport)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_CONSTMETHOD0(PacketTransportInterface*, GetRtpPacketTransport)
PROXY_CONSTMETHOD0(PacketTransportInterface*, GetRtcpPacketTransport)
PROXY_METHOD1(RTCError, SetRtcpParameters, const RtcpParameters&)
PROXY_CONSTMETHOD0(RtcpParameters, GetRtcpParameters)
protected:
RtpTransportAdapter* GetInternal() override {
  return internal();
}
END_PROXY_MAP()

// static
RTCErrorOr<std::unique_ptr<RtpTransportInterface>>
RtpTransportAdapter::CreateProxied(
    const RtcpParameters& rtcp_parameters,
    PacketTransportInterface* rtp,
    PacketTransportInterface* rtcp,
    RtpTransportControllerAdapter* rtp_transport_controller) {
  if (!rtp) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Must provide an RTP packet transport.");
  }
  if (!rtcp_parameters.mux && !rtcp) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Must provide an RTCP packet transport when RTCP muxing is not used.");
  }
  if (rtcp_parameters.mux && rtcp) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Creating an RtpTransport with RTCP muxing enabled, "
                         "with a separate RTCP packet transport?");
  }
  if (!rtp_transport_controller) {
    // Since OrtcFactory::CreateRtpTransport creates an RtpTransportController
    // automatically when one isn't passed in, this should never be reached.
    RTC_NOTREACHED();
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Must provide an RTP transport controller.");
  }
  return RtpTransportProxyWithInternal<RtpTransportAdapter>::Create(
      rtp_transport_controller->signaling_thread(),
      rtp_transport_controller->worker_thread(),
      std::unique_ptr<RtpTransportAdapter>(new RtpTransportAdapter(
          rtcp_parameters, rtp, rtcp, rtp_transport_controller)));
}

void RtpTransportAdapter::TakeOwnershipOfRtpTransportController(
    std::unique_ptr<RtpTransportControllerInterface> controller) {
  RTC_DCHECK_EQ(rtp_transport_controller_, controller->GetInternal());
  RTC_DCHECK(owned_rtp_transport_controller_.get() == nullptr);
  owned_rtp_transport_controller_ = std::move(controller);
}

RtpTransportAdapter::RtpTransportAdapter(
    const RtcpParameters& rtcp_parameters,
    PacketTransportInterface* rtp,
    PacketTransportInterface* rtcp,
    RtpTransportControllerAdapter* rtp_transport_controller)
    : rtp_packet_transport_(rtp),
      rtcp_packet_transport_(rtcp),
      rtp_transport_controller_(rtp_transport_controller),
      rtcp_parameters_(rtcp_parameters) {
  RTC_DCHECK(rtp_transport_controller);
  // CNAME should have been filled by OrtcFactory if empty.
  RTC_DCHECK(!rtcp_parameters_.cname.empty());
}

RtpTransportAdapter::~RtpTransportAdapter() {
  SignalDestroyed(this);
}

PacketTransportInterface* RtpTransportAdapter::GetRtpPacketTransport() const {
  return rtp_packet_transport_;
}

PacketTransportInterface* RtpTransportAdapter::GetRtcpPacketTransport() const {
  return rtcp_packet_transport_;
}

RTCError RtpTransportAdapter::SetRtcpParameters(
    const RtcpParameters& parameters) {
  if (!parameters.mux && rtcp_parameters_.mux) {
    LOG_AND_RETURN_ERROR(webrtc::RTCErrorType::INVALID_STATE,
                         "Can't disable RTCP muxing after enabling.");
  }
  if (!parameters.cname.empty() && parameters.cname != rtcp_parameters_.cname) {
    LOG_AND_RETURN_ERROR(webrtc::RTCErrorType::UNSUPPORTED_OPERATION,
                         "Changing the RTCP CNAME is currently unsupported.");
  }
  // If the CNAME is empty, use the existing one.
  RtcpParameters copy = parameters;
  if (copy.cname.empty()) {
    copy.cname = rtcp_parameters_.cname;
  }
  RTCError err = rtp_transport_controller_->SetRtcpParameters(copy, this);
  if (!err.ok()) {
    return err;
  }
  rtcp_parameters_ = copy;
  if (rtcp_parameters_.mux) {
    rtcp_packet_transport_ = nullptr;
  }
  return RTCError::OK();
}

}  // namespace webrtc
