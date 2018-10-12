/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "ortc/rtptransportadapter.h"

#include <algorithm>  // For std::find.
#include <set>
#include <utility>  // For std::move.

#include "absl/memory/memory.h"
#include "api/proxy.h"
#include "rtc_base/logging.h"

namespace webrtc {

BEGIN_OWNED_PROXY_MAP(RtpTransport)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_CONSTMETHOD0(PacketTransportInterface*, GetRtpPacketTransport)
PROXY_CONSTMETHOD0(PacketTransportInterface*, GetRtcpPacketTransport)
PROXY_METHOD1(RTCError, SetParameters, const RtpTransportParameters&)
PROXY_CONSTMETHOD0(RtpTransportParameters, GetParameters)
protected:
RtpTransportAdapter* GetInternal() override {
  return internal();
}
END_PROXY_MAP()

BEGIN_OWNED_PROXY_MAP(SrtpTransport)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_CONSTMETHOD0(PacketTransportInterface*, GetRtpPacketTransport)
PROXY_CONSTMETHOD0(PacketTransportInterface*, GetRtcpPacketTransport)
PROXY_METHOD1(RTCError, SetParameters, const RtpTransportParameters&)
PROXY_CONSTMETHOD0(RtpTransportParameters, GetParameters)
PROXY_METHOD1(RTCError, SetSrtpSendKey, const cricket::CryptoParams&)
PROXY_METHOD1(RTCError, SetSrtpReceiveKey, const cricket::CryptoParams&)
protected:
RtpTransportAdapter* GetInternal() override {
  return internal();
}
END_PROXY_MAP()

// static
RTCErrorOr<std::unique_ptr<RtpTransportInterface>>
RtpTransportAdapter::CreateProxied(
    const RtpTransportParameters& parameters,
    PacketTransportInterface* rtp,
    PacketTransportInterface* rtcp,
    RtpTransportControllerAdapter* rtp_transport_controller) {
  if (!rtp) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Must provide an RTP packet transport.");
  }
  if (!parameters.rtcp.mux && !rtcp) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Must provide an RTCP packet transport when RTCP muxing is not used.");
  }
  if (parameters.rtcp.mux && rtcp) {
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
  std::unique_ptr<RtpTransportAdapter> transport_adapter(
      new RtpTransportAdapter(parameters.rtcp, rtp, rtcp,
                              rtp_transport_controller,
                              false /*is_srtp_transport*/));
  RTCError params_result = transport_adapter->SetParameters(parameters);
  if (!params_result.ok()) {
    return std::move(params_result);
  }

  return RtpTransportProxyWithInternal<RtpTransportAdapter>::Create(
      rtp_transport_controller->signaling_thread(),
      rtp_transport_controller->worker_thread(), std::move(transport_adapter));
}

RTCErrorOr<std::unique_ptr<SrtpTransportInterface>>
RtpTransportAdapter::CreateSrtpProxied(
    const RtpTransportParameters& parameters,
    PacketTransportInterface* rtp,
    PacketTransportInterface* rtcp,
    RtpTransportControllerAdapter* rtp_transport_controller) {
  if (!rtp) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_PARAMETER,
                         "Must provide an RTP packet transport.");
  }
  if (!parameters.rtcp.mux && !rtcp) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_PARAMETER,
        "Must provide an RTCP packet transport when RTCP muxing is not used.");
  }
  if (parameters.rtcp.mux && rtcp) {
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

  std::unique_ptr<RtpTransportAdapter> transport_adapter;
  transport_adapter.reset(new RtpTransportAdapter(parameters.rtcp, rtp, rtcp,
                                                  rtp_transport_controller,
                                                  true /*is_srtp_transport*/));
  RTCError params_result = transport_adapter->SetParameters(parameters);
  if (!params_result.ok()) {
    return std::move(params_result);
  }

  return SrtpTransportProxyWithInternal<RtpTransportAdapter>::Create(
      rtp_transport_controller->signaling_thread(),
      rtp_transport_controller->worker_thread(), std::move(transport_adapter));
}

void RtpTransportAdapter::TakeOwnershipOfRtpTransportController(
    std::unique_ptr<RtpTransportControllerInterface> controller) {
  RTC_DCHECK_EQ(rtp_transport_controller_, controller->GetInternal());
  RTC_DCHECK(owned_rtp_transport_controller_.get() == nullptr);
  owned_rtp_transport_controller_ = std::move(controller);
}

RtpTransportAdapter::RtpTransportAdapter(
    const RtcpParameters& rtcp_params,
    PacketTransportInterface* rtp,
    PacketTransportInterface* rtcp,
    RtpTransportControllerAdapter* rtp_transport_controller,
    bool is_srtp_transport)
    : rtp_packet_transport_(rtp),
      rtcp_packet_transport_(rtcp),
      rtp_transport_controller_(rtp_transport_controller),
      network_thread_(rtp_transport_controller_->network_thread()) {
  parameters_.rtcp = rtcp_params;
  // CNAME should have been filled by OrtcFactory if empty.
  RTC_DCHECK(!parameters_.rtcp.cname.empty());
  RTC_DCHECK(rtp_transport_controller);

  if (is_srtp_transport) {
    srtp_transport_ = absl::make_unique<SrtpTransport>(rtcp == nullptr);
    transport_ = srtp_transport_.get();
  } else {
    unencrypted_rtp_transport_ =
        absl::make_unique<RtpTransport>(rtcp == nullptr);
    transport_ = unencrypted_rtp_transport_.get();
  }
  RTC_DCHECK(transport_);

  network_thread_->Invoke<void>(RTC_FROM_HERE, [=] {
    SetRtpPacketTransport(rtp->GetInternal());
    if (rtcp) {
      SetRtcpPacketTransport(rtcp->GetInternal());
    }
  });

  transport_->SignalReadyToSend.connect(this,
                                        &RtpTransportAdapter::OnReadyToSend);
  transport_->SignalRtcpPacketReceived.connect(
      this, &RtpTransportAdapter::OnRtcpPacketReceived);
  transport_->SignalWritableState.connect(
      this, &RtpTransportAdapter::OnWritableState);
}

RtpTransportAdapter::~RtpTransportAdapter() {
  SignalDestroyed(this);
}

RTCError RtpTransportAdapter::SetParameters(
    const RtpTransportParameters& parameters) {
  if (!parameters.rtcp.mux && parameters_.rtcp.mux) {
    LOG_AND_RETURN_ERROR(webrtc::RTCErrorType::INVALID_STATE,
                         "Can't disable RTCP muxing after enabling.");
  }
  if (!parameters.rtcp.cname.empty() &&
      parameters.rtcp.cname != parameters_.rtcp.cname) {
    LOG_AND_RETURN_ERROR(webrtc::RTCErrorType::UNSUPPORTED_OPERATION,
                         "Changing the RTCP CNAME is currently unsupported.");
  }
  // If the CNAME is empty, use the existing one.
  RtpTransportParameters copy = parameters;
  if (copy.rtcp.cname.empty()) {
    copy.rtcp.cname = parameters_.rtcp.cname;
  }
  RTCError err =
      rtp_transport_controller_->SetRtpTransportParameters(copy, this);
  if (!err.ok()) {
    return err;
  }
  parameters_ = copy;
  if (parameters_.rtcp.mux) {
    rtcp_packet_transport_ = nullptr;
  }
  return RTCError::OK();
}

RTCError RtpTransportAdapter::SetSrtpSendKey(
    const cricket::CryptoParams& params) {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->Invoke<RTCError>(
        RTC_FROM_HERE, [&] { return SetSrtpSendKey(params); });
  }
  return transport_->SetSrtpSendKey(params);
}

RTCError RtpTransportAdapter::SetSrtpReceiveKey(
    const cricket::CryptoParams& params) {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->Invoke<RTCError>(
        RTC_FROM_HERE, [&] { return SetSrtpReceiveKey(params); });
  }
  return transport_->SetSrtpReceiveKey(params);
}

}  // namespace webrtc
