/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/jsep_transport_controller.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/async_dns_resolver.h"
#include "api/candidate.h"
#include "api/dtls_transport_interface.h"
#include "api/environment/environment.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/local_network_access_permission.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/transport/data_channel_transport_interface.h"
#include "api/transport/enums.h"
#include "call/payload_type.h"
#include "call/payload_type_picker.h"
#include "media/base/codec.h"
#include "media/sctp/sctp_transport_internal.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/packet_transport_internal.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "p2p/dtls/dtls_transport.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/dtls_transport.h"
#include "pc/jsep_transport.h"
#include "pc/rtp_transport.h"
#include "pc/rtp_transport_internal.h"
#include "pc/sctp_transport.h"
#include "pc/session_description.h"
#include "pc/transport_stats.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/trace_event.h"

using webrtc::SdpType;

namespace webrtc {

JsepTransportController::JsepTransportController(
    const Environment& env,
    Thread* network_thread,
    PortAllocator* port_allocator,
    AsyncDnsResolverFactoryInterface* async_dns_resolver_factory,
    LocalNetworkAccessPermissionFactoryInterface* lna_permission_factory,
    PayloadTypePicker& payload_type_picker,
    Config config)
    : env_(env),
      network_thread_(network_thread),
      port_allocator_(port_allocator),
      async_dns_resolver_factory_(async_dns_resolver_factory),
      lna_permission_factory_(lna_permission_factory),
      transports_(
          [this](const std::string& mid, JsepTransport* transport) {
            return OnTransportChanged(mid, transport);
          },
          [this]() {
            RTC_DCHECK_RUN_ON(network_thread_);
            UpdateAggregateStates_n();
          }),
      config_(std::move(config)),
      active_reset_srtp_params_(config_.active_reset_srtp_params),
      bundles_(config_.bundle_policy),
      payload_type_picker_(payload_type_picker) {
  // The `transport_observer` is assumed to be non-null.
  RTC_DCHECK(config_.transport_observer);
  RTC_DCHECK(config_.rtcp_handler);
  RTC_DCHECK(config_.ice_transport_factory);
  RTC_DCHECK(config_.on_dtls_handshake_error_);
}

JsepTransportController::~JsepTransportController() {
  // Channel destructors may try to send packets, so this needs to happen on
  // the network thread.
  RTC_DCHECK_RUN_ON(network_thread_);
  DestroyAllJsepTransports_n();
}

RTCError JsepTransportController::SetLocalDescription(
    SdpType type,
    const SessionDescription* local_desc,
    const SessionDescription* remote_desc) {
  RTC_DCHECK(local_desc);
  TRACE_EVENT0("webrtc", "JsepTransportController::SetLocalDescription");

  if (!network_thread_->IsCurrent()) {
    return network_thread_->BlockingCall([this, type, local_desc, remote_desc] {
      return SetLocalDescription(type, local_desc, remote_desc);
    });
  }

  RTC_DCHECK_RUN_ON(network_thread_);

  if (!initial_offerer_.has_value()) {
    initial_offerer_.emplace(type == SdpType::kOffer);
    if (*initial_offerer_) {
      SetIceRole_n(ICEROLE_CONTROLLING);
    } else {
      SetIceRole_n(ICEROLE_CONTROLLED);
    }
  }
  return ApplyDescription_n(/*local=*/true, type, local_desc, remote_desc);
}

RTCError JsepTransportController::SetRemoteDescription(
    SdpType type,
    const SessionDescription* local_desc,
    const SessionDescription* remote_desc) {
  RTC_DCHECK(remote_desc);
  TRACE_EVENT0("webrtc", "JsepTransportController::SetRemoteDescription");
  if (!network_thread_->IsCurrent()) {
    return network_thread_->BlockingCall([this, type, local_desc, remote_desc] {
      return SetRemoteDescription(type, local_desc, remote_desc);
    });
  }

  RTC_DCHECK_RUN_ON(network_thread_);
  return ApplyDescription_n(/*local=*/false, type, local_desc, remote_desc);
}

RtpTransportInternal* JsepTransportController::GetRtpTransport(
    absl::string_view mid) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto jsep_transport = GetJsepTransportForMid(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->rtp_transport();
}

DataChannelTransportInterface* JsepTransportController::GetDataChannelTransport(
    const std::string& mid) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto jsep_transport = GetJsepTransportForMid(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->data_channel_transport();
}

DtlsTransportInternal* JsepTransportController::GetDtlsTransport(
    const std::string& mid) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto jsep_transport = GetJsepTransportForMid(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->rtp_dtls_transport();
}

const DtlsTransportInternal* JsepTransportController::GetRtcpDtlsTransport(
    const std::string& mid) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto jsep_transport = GetJsepTransportForMid(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->rtcp_dtls_transport();
}

scoped_refptr<DtlsTransport> JsepTransportController::LookupDtlsTransportByMid(
    const std::string& mid) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto jsep_transport = GetJsepTransportForMid(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->RtpDtlsTransport();
}

scoped_refptr<SctpTransport> JsepTransportController::GetSctpTransport(
    const std::string& mid) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto jsep_transport = GetJsepTransportForMid(mid);
  if (!jsep_transport) {
    return nullptr;
  }
  return jsep_transport->SctpTransport();
}

void JsepTransportController::SetIceConfig(const IceConfig& config) {
  RTC_DCHECK_RUN_ON(network_thread_);
  ice_config_ = config;
  for (auto& dtls : GetDtlsTransports()) {
    dtls->ice_transport()->SetIceConfig(ice_config_);
  }
}

void JsepTransportController::SetNeedsIceRestartFlag() {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (auto& transport : transports_.Transports()) {
    transport->SetNeedsIceRestartFlag();
  }
}

bool JsepTransportController::NeedsIceRestart(
    const std::string& transport_name) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  const JsepTransport* transport = GetJsepTransportByName(transport_name);
  if (!transport) {
    return false;
  }
  return transport->needs_ice_restart();
}

std::optional<SSLRole> JsepTransportController::GetDtlsRole(
    const std::string& mid) const {
  // TODO(tommi): Remove this hop. Currently it's called from the signaling
  // thread during negotiations, potentially multiple times.
  // WebRtcSessionDescriptionFactory::InternalCreateAnswer is one example.
  if (!network_thread_->IsCurrent()) {
    return network_thread_->BlockingCall([&] { return GetDtlsRole(mid); });
  }

  RTC_DCHECK_RUN_ON(network_thread_);

  const JsepTransport* t = GetJsepTransportForMid(mid);
  if (!t) {
    return std::optional<SSLRole>();
  }
  return t->GetDtlsRole();
}

RTCErrorOr<PayloadType> JsepTransportController::SuggestPayloadType(
    const std::string& mid,
    Codec codec) {
  // Because SDP processing runs on the signal thread and Call processing
  // runs on the worker thread, we allow cross thread invocation until we
  // can clean up the thread work.
  if (!network_thread_->IsCurrent()) {
    return network_thread_->BlockingCall([&] {
      RTC_DCHECK_RUN_ON(network_thread_);
      return SuggestPayloadType(mid, codec);
    });
  }
  RTC_DCHECK_RUN_ON(network_thread_);
  const JsepTransport* transport = GetJsepTransportForMid(mid);
  if (transport) {
    RTCErrorOr<PayloadType> local_result =
        transport->local_payload_types().LookupPayloadType(codec);
    if (local_result.ok()) {
      return local_result;
    }
    RTCErrorOr<PayloadType> remote_result =
        transport->remote_payload_types().LookupPayloadType(codec);
    if (remote_result.ok()) {
      RTCErrorOr<Codec> local_codec =
          transport->local_payload_types().LookupCodec(remote_result.value());
      if (local_result.ok()) {
        // Already in use, possibly for something else.
        // Fall through to SuggestMapping.
        RTC_LOG(LS_WARNING) << "Ignoring remote suggestion of PT "
                            << static_cast<int>(remote_result.value())
                            << " for " << codec << "; already in use";
      } else {
        // Tell the local payload type registry that we've taken this
        RTC_DCHECK(local_result.error().type() ==
                   RTCErrorType::INVALID_PARAMETER);
        AddLocalMapping(mid, remote_result.value(), codec);
        return remote_result;
      }
    }
    return payload_type_picker_.SuggestMapping(
        codec, &transport->local_payload_types());
  }
  // If there is no transport, there are no exclusions.
  return payload_type_picker_.SuggestMapping(codec, nullptr);
}

RTCError JsepTransportController::AddLocalMapping(const std::string& mid,
                                                  PayloadType payload_type,
                                                  const Codec& codec) {
  // Because SDP processing runs on the signal thread and Call processing
  // runs on the worker thread, we allow cross thread invocation until we
  // can clean up the thread work.
  if (!network_thread_->IsCurrent()) {
    return network_thread_->BlockingCall([&] {
      RTC_DCHECK_RUN_ON(network_thread_);
      return AddLocalMapping(mid, payload_type, codec);
    });
  }
  RTC_DCHECK_RUN_ON(network_thread_);
  JsepTransport* transport = GetJsepTransportForMid(mid);
  if (!transport) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "AddLocalMapping: no transport for mid");
  }
  return transport->local_payload_types().AddMapping(payload_type, codec);
}

bool JsepTransportController::SetLocalCertificate(
    const scoped_refptr<RTCCertificate>& certificate) {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->BlockingCall(
        [&] { return SetLocalCertificate(certificate); });
  }

  RTC_DCHECK_RUN_ON(network_thread_);

  // Can't change a certificate, or set a null certificate.
  if (certificate_ || !certificate) {
    return false;
  }
  certificate_ = certificate;

  // Set certificate for JsepTransport, which verifies it matches the
  // fingerprint in SDP, and DTLS transport.
  // Fallback from DTLS to SDES is not supported.
  for (auto& transport : transports_.Transports()) {
    transport->SetLocalCertificate(certificate_);
  }
  for (auto& dtls : GetDtlsTransports()) {
    bool set_cert_success = dtls->SetLocalCertificate(certificate_);
    RTC_DCHECK(set_cert_success);
  }
  return true;
}

scoped_refptr<RTCCertificate> JsepTransportController::GetLocalCertificate(
    const std::string& transport_name) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  const JsepTransport* t = GetJsepTransportByName(transport_name);
  if (!t) {
    return nullptr;
  }
  return t->GetLocalCertificate();
}

std::unique_ptr<SSLCertChain> JsepTransportController::GetRemoteSSLCertChain(
    const std::string& transport_name) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  // Get the certificate from the RTP transport's DTLS handshake. Should be
  // identical to the RTCP transport's, since they were given the same remote
  // fingerprint.
  auto jsep_transport = GetJsepTransportByName(transport_name);
  if (!jsep_transport) {
    return nullptr;
  }
  auto dtls = jsep_transport->rtp_dtls_transport();
  if (!dtls) {
    return nullptr;
  }

  return dtls->GetRemoteSSLCertChain();
}

void JsepTransportController::MaybeStartGathering() {
  if (!network_thread_->IsCurrent()) {
    network_thread_->BlockingCall([&] { MaybeStartGathering(); });
    return;
  }

  for (auto& dtls : GetDtlsTransports()) {
    dtls->ice_transport()->MaybeStartGathering();
  }
}

RTCError JsepTransportController::AddRemoteCandidates(
    const std::string& transport_name,
    const Candidates& candidates) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(VerifyCandidates(candidates).ok());
  auto jsep_transport = GetJsepTransportByName(transport_name);
  if (!jsep_transport) {
    RTC_LOG(LS_WARNING) << "Not adding candidate because the JsepTransport "
                           "doesn't exist. Ignore it.";
    return RTCError::OK();
  }
  return jsep_transport->AddRemoteCandidates(candidates);
}

bool JsepTransportController::RemoveRemoteCandidate(const IceCandidate* c) {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->BlockingCall(
        [&] { return RemoveRemoteCandidate(c); });
  }

  RTC_DCHECK_RUN_ON(network_thread_);
  std::string mid = c->sdp_mid();
  if (!VerifyCandidate(c->candidate()).ok() || mid.empty()) {
    RTC_LOG(LS_ERROR) << "Candidate invalid or missing sdp_mid: "
                      << c->candidate().ToSensitiveString();
    return false;
  }
  JsepTransport* jsep_transport = GetJsepTransportForMid(mid);
  if (!jsep_transport) {
    RTC_LOG(LS_WARNING) << "No Transport for mid=" << mid;
    return false;
  }
  DtlsTransportInternal* dtls =
      c->candidate().component() == ICE_CANDIDATE_COMPONENT_RTP
          ? jsep_transport->rtp_dtls_transport()
          : jsep_transport->rtcp_dtls_transport();
  if (dtls) {
    dtls->ice_transport()->RemoveRemoteCandidate(c->candidate());
  }
  return true;
}

bool JsepTransportController::GetStats(const std::string& transport_name,
                                       TransportStats* stats) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  const JsepTransport* transport = GetJsepTransportByName(transport_name);
  if (!transport) {
    return false;
  }
  return transport->GetStats(stats);
}

void JsepTransportController::SetActiveResetSrtpParams(
    bool active_reset_srtp_params) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO)
      << "Updating the active_reset_srtp_params for JsepTransportController: "
      << active_reset_srtp_params;
  active_reset_srtp_params_ = active_reset_srtp_params;
  for (auto& transport : transports_.Transports()) {
    transport->SetActiveResetSrtpParams(active_reset_srtp_params);
  }
}

RTCError JsepTransportController::RollbackTransports() {
  if (!network_thread_->IsCurrent()) {
    return network_thread_->BlockingCall(
        [this] { return RollbackTransports(); });
  }
  RTC_DCHECK_RUN_ON(network_thread_);
  bundles_.Rollback();
  if (!transports_.RollbackTransports()) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INTERNAL_ERROR,
                         "Failed to roll back transport state.");
  }
  return RTCError::OK();
}

scoped_refptr<IceTransportInterface>
JsepTransportController::CreateIceTransport(const std::string& transport_name,
                                            bool rtcp) {
  int component =
      rtcp ? ICE_CANDIDATE_COMPONENT_RTCP : ICE_CANDIDATE_COMPONENT_RTP;

  IceTransportInit init(env_);
  init.set_port_allocator(port_allocator_);
  init.set_async_dns_resolver_factory(async_dns_resolver_factory_);
  init.set_lna_permission_factory(lna_permission_factory_);
  auto transport = config_.ice_transport_factory->CreateIceTransport(
      transport_name, component, std::move(init));
  RTC_DCHECK(transport);
  transport->internal()->SetIceRole(ice_role_);
  transport->internal()->SetIceConfig(ice_config_);
  return transport;
}

std::unique_ptr<DtlsTransportInternal>
JsepTransportController::CreateDtlsTransport(const ContentInfo& content_info,
                                             IceTransportInternal* ice) {
  RTC_DCHECK_RUN_ON(network_thread_);

  std::unique_ptr<DtlsTransportInternal> dtls;

  if (config_.dtls_transport_factory) {
    dtls = config_.dtls_transport_factory->CreateDtlsTransport(
        ice, config_.crypto_options, config_.ssl_max_version);
  } else {
    dtls = std::make_unique<DtlsTransportInternalImpl>(
        env_, ice, config_.crypto_options, config_.ssl_max_version);
  }

  RTC_DCHECK(dtls);
  RTC_DCHECK_EQ(ice, dtls->ice_transport());

  if (certificate_) {
    bool set_cert_success = dtls->SetLocalCertificate(certificate_);
    RTC_DCHECK(set_cert_success);
  }

  // Connect to signals offered by the DTLS and ICE transport.
  dtls->SubscribeWritableState(this,
                               [this](PacketTransportInternal* transport) {
                                 RTC_DCHECK_RUN_ON(network_thread_);
                                 OnTransportWritableState_n(transport);
                               });
  dtls->SubscribeReceivingState([this](PacketTransportInternal* transport) {
    RTC_DCHECK_RUN_ON(network_thread_);
    OnTransportReceivingState_n(transport);
  });
  dtls->ice_transport()->AddGatheringStateCallback(
      this, [this](IceTransportInternal* transport) {
        RTC_DCHECK_RUN_ON(network_thread_);
        OnTransportGatheringState_n(transport);
      });
  dtls->ice_transport()->SubscribeCandidateGathered(
      [this](IceTransportInternal* transport, const Candidate& candidate) {
        RTC_DCHECK_RUN_ON(network_thread_);
        OnTransportCandidateGathered_n(transport, candidate);
      });
  dtls->ice_transport()->SetCandidateErrorCallback(
      [this](IceTransportInternal* transport,
             const IceCandidateErrorEvent& error) {
        RTC_DCHECK_RUN_ON(network_thread_);
        OnTransportCandidateError_n(transport, error);
      });
  dtls->ice_transport()->SetCandidatesRemovedCallback(
      [this](IceTransportInternal* transport, const Candidates& candidates) {
        RTC_DCHECK_RUN_ON(network_thread_);
        OnTransportCandidatesRemoved_n(transport, candidates);
      });
  dtls->ice_transport()->SubscribeRoleConflict(
      [this](IceTransportInternal* transport) {
        RTC_DCHECK_RUN_ON(network_thread_);
        OnTransportRoleConflict_n(transport);
      });
  dtls->ice_transport()->SubscribeIceTransportStateChanged(
      [this](IceTransportInternal* transport) {
        RTC_DCHECK_RUN_ON(network_thread_);
        OnTransportStateChanged_n(transport);
      });
  dtls->ice_transport()->SetCandidatePairChangeCallback(
      [this](const CandidatePairChangeEvent& event) {
        RTC_DCHECK_RUN_ON(network_thread_);
        OnTransportCandidatePairChanged_n(event);
      });

  dtls->SubscribeDtlsHandshakeError(
      [this](SSLHandshakeError error) { OnDtlsHandshakeError(error); });
  return dtls;
}

std::unique_ptr<RtpTransport>
JsepTransportController::CreateUnencryptedRtpTransport(
    const std::string& transport_name,
    PacketTransportInternal* rtp_packet_transport,
    PacketTransportInternal* rtcp_packet_transport) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto unencrypted_rtp_transport = std::make_unique<RtpTransport>(
      rtcp_packet_transport == nullptr, env_.field_trials());
  unencrypted_rtp_transport->SetRtpPacketTransport(rtp_packet_transport);
  if (rtcp_packet_transport) {
    unencrypted_rtp_transport->SetRtcpPacketTransport(rtcp_packet_transport);
  }
  return unencrypted_rtp_transport;
}

std::unique_ptr<DtlsSrtpTransport>
JsepTransportController::CreateDtlsSrtpTransport(
    const std::string& transport_name,
    DtlsTransportInternal* rtp_dtls_transport,
    DtlsTransportInternal* rtcp_dtls_transport) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto dtls_srtp_transport = std::make_unique<DtlsSrtpTransport>(
      rtcp_dtls_transport == nullptr, env_.field_trials());
  if (config_.enable_external_auth) {
    dtls_srtp_transport->EnableExternalAuth();
  }

  dtls_srtp_transport->SetDtlsTransports(rtp_dtls_transport,
                                         rtcp_dtls_transport);
  dtls_srtp_transport->SetActiveResetSrtpParams(active_reset_srtp_params_);
  // Capturing this in the callback because JsepTransportController will always
  // outlive the DtlsSrtpTransport.
  dtls_srtp_transport->SetOnDtlsStateChange([this]() {
    RTC_DCHECK_RUN_ON(this->network_thread_);
    this->UpdateAggregateStates_n();
  });
  return dtls_srtp_transport;
}

std::vector<DtlsTransportInternal*>
JsepTransportController::GetDtlsTransports() {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<DtlsTransportInternal*> dtls_transports;
  for (auto jsep_transport : transports_.Transports()) {
    RTC_DCHECK(jsep_transport);
    if (jsep_transport->rtp_dtls_transport()) {
      dtls_transports.push_back(jsep_transport->rtp_dtls_transport());
    }

    if (jsep_transport->rtcp_dtls_transport()) {
      dtls_transports.push_back(jsep_transport->rtcp_dtls_transport());
    }
  }
  return dtls_transports;
}

std::vector<DtlsTransportInternal*>
JsepTransportController::GetActiveDtlsTransports() {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<DtlsTransportInternal*> dtls_transports;
  for (auto jsep_transport : transports_.ActiveTransports()) {
    RTC_DCHECK(jsep_transport);
    if (jsep_transport->rtp_dtls_transport()) {
      dtls_transports.push_back(jsep_transport->rtp_dtls_transport());
    }

    if (jsep_transport->rtcp_dtls_transport()) {
      dtls_transports.push_back(jsep_transport->rtcp_dtls_transport());
    }
  }
  return dtls_transports;
}

RTCError JsepTransportController::ApplyDescription_n(
    bool local,
    SdpType type,
    const SessionDescription* local_desc,
    const SessionDescription* remote_desc) {
  TRACE_EVENT0("webrtc", "JsepTransportController::ApplyDescription_n");

  // Stash away the description object that we'll be applying (since this
  // function is used for both local and remote).
  const SessionDescription* description = local ? local_desc : remote_desc;

  RTC_DCHECK(description);

  RTCError error;
  error =
      ValidateAndMaybeUpdateBundleGroups(local, type, local_desc, remote_desc);
  if (!error.ok()) {
    return error;
  }

  std::map<const ContentGroup*, std::vector<int>>
      merged_encrypted_extension_ids_by_bundle;
  if (!bundles_.bundle_groups().empty()) {
    merged_encrypted_extension_ids_by_bundle =
        MergeEncryptedHeaderExtensionIdsForBundles(description);
  }

  for (const ContentInfo& content_info : description->contents()) {
    // Don't create transports for rejected m-lines and bundled m-lines.
    if (content_info.rejected ||
        !bundles_.IsFirstMidInGroup(content_info.mid())) {
      continue;
    }
    error = MaybeCreateJsepTransport(local, content_info, *description);
    if (!error.ok()) {
      return error;
    }
  }

  RTC_DCHECK(description->contents().size() ==
             description->transport_infos().size());
  for (size_t i = 0; i < description->contents().size(); ++i) {
    const ContentInfo& content_info = description->contents()[i];
    const TransportInfo& transport_info = description->transport_infos()[i];

    if (content_info.rejected) {
      // This may cause groups to be removed from |bundles_.bundle_groups()|.
      HandleRejectedContent(content_info);
      continue;
    }

    const ContentGroup* established_bundle_group =
        bundles_.LookupGroupByMid(content_info.mid());

    // For bundle members that are not BUNDLE-tagged (not first in the group),
    // configure their transport to be the same as the BUNDLE-tagged transport.
    if (established_bundle_group &&
        content_info.mid() != *established_bundle_group->FirstContentName()) {
      if (!HandleBundledContent(content_info, *established_bundle_group)) {
        return RTCError(RTCErrorType::INVALID_PARAMETER,
                        "Failed to process the bundled m= section with "
                        "mid='" +
                            content_info.mid() + "'.");
      }
      continue;
    }

    error = ValidateContent(content_info);
    if (!error.ok()) {
      return error;
    }

    std::vector<int> extension_ids;
    // Is BUNDLE-tagged (first in the group)?
    if (established_bundle_group &&
        content_info.mid() == *established_bundle_group->FirstContentName()) {
      auto it = merged_encrypted_extension_ids_by_bundle.find(
          established_bundle_group);
      RTC_DCHECK(it != merged_encrypted_extension_ids_by_bundle.end());
      extension_ids = it->second;
    } else {
      extension_ids = GetEncryptedHeaderExtensionIds(content_info);
    }

    int rtp_abs_sendtime_extn_id =
        GetRtpAbsSendTimeHeaderExtensionId(content_info);

    JsepTransport* transport = GetJsepTransportForMid(content_info.mid());
    if (!transport) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::INVALID_PARAMETER,
          "Could not find transport for m= section with mid='" +
              content_info.mid() + "'");
    }

    SetIceRole_n(DetermineIceRole(transport, transport_info, type, local));

    JsepTransportDescription jsep_description = CreateJsepTransportDescription(
        content_info, transport_info, extension_ids, rtp_abs_sendtime_extn_id);
    if (local) {
      error =
          transport->SetLocalJsepTransportDescription(jsep_description, type);
    } else {
      error =
          transport->SetRemoteJsepTransportDescription(jsep_description, type);
    }

    if (!error.ok()) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::INVALID_PARAMETER,
          "Failed to apply the description for m= section with mid='" +
              content_info.mid() + "': " + error.message());
    }
    error = transport->RecordPayloadTypes(local, type, content_info);
    if (!error.ok()) {
      RTC_LOG(LS_ERROR) << "RecordPayloadTypes failed: "
                        << ToString(error.type()) << " - " << error.message();
      return error;
    }
  }
  if (type == SdpType::kAnswer) {
    transports_.CommitTransports();
    bundles_.Commit();
  }
  return RTCError::OK();
}

RTCError JsepTransportController::ValidateAndMaybeUpdateBundleGroups(
    bool local,
    SdpType type,
    const SessionDescription* local_desc,
    const SessionDescription* remote_desc) {
  const SessionDescription* description = local ? local_desc : remote_desc;

  RTC_DCHECK(description);

  std::vector<const ContentGroup*> new_bundle_groups =
      description->GetGroupsByName(GROUP_TYPE_BUNDLE);
  // Verify `new_bundle_groups`.
  std::map<std::string, const ContentGroup*> new_bundle_groups_by_mid;
  for (const ContentGroup* new_bundle_group : new_bundle_groups) {
    for (const std::string& content_name : new_bundle_group->content_names()) {
      // The BUNDLE group must not contain a MID that is a member of a different
      // BUNDLE group, or that contains the same MID multiple times.
      if (new_bundle_groups_by_mid.find(content_name) !=
          new_bundle_groups_by_mid.end()) {
        return RTCError(RTCErrorType::INVALID_PARAMETER,
                        "A BUNDLE group contains a MID='" + content_name +
                            "' that is already in a BUNDLE group.");
      }
      new_bundle_groups_by_mid.insert(
          std::make_pair(content_name, new_bundle_group));
      // The BUNDLE group must not contain a MID that no m= section has.
      if (!description->GetContentByName(content_name)) {
        return RTCError(RTCErrorType::INVALID_PARAMETER,
                        "A BUNDLE group contains a MID='" + content_name +
                            "' matching no m= section.");
      }
    }
  }

  if (type == SdpType::kOffer) {
    // For an offer, we need to verify that there is not a conflicting mapping
    // between existing and new bundle groups. For example, if the existing
    // groups are [[1,2],[3,4]] and new are [[1,3],[2,4]] or [[1,2,3,4]], or
    // vice versa. Switching things around like this requires a separate offer
    // that removes the relevant sections from their group, as per RFC 8843,
    // section 7.5.2.
    std::map<const ContentGroup*, const ContentGroup*>
        new_bundle_groups_by_existing_bundle_groups;
    std::map<const ContentGroup*, const ContentGroup*>
        existing_bundle_groups_by_new_bundle_groups;
    for (const ContentGroup* new_bundle_group : new_bundle_groups) {
      for (const std::string& mid : new_bundle_group->content_names()) {
        ContentGroup* existing_bundle_group = bundles_.LookupGroupByMid(mid);
        if (!existing_bundle_group) {
          continue;
        }
        auto it = new_bundle_groups_by_existing_bundle_groups.find(
            existing_bundle_group);
        if (it != new_bundle_groups_by_existing_bundle_groups.end() &&
            it->second != new_bundle_group) {
          return RTCError(RTCErrorType::INVALID_PARAMETER,
                          "MID " + mid + " in the offer has changed group.");
        }
        new_bundle_groups_by_existing_bundle_groups.insert(
            std::make_pair(existing_bundle_group, new_bundle_group));
        it = existing_bundle_groups_by_new_bundle_groups.find(new_bundle_group);
        if (it != existing_bundle_groups_by_new_bundle_groups.end() &&
            it->second != existing_bundle_group) {
          return RTCError(RTCErrorType::INVALID_PARAMETER,
                          "MID " + mid + " in the offer has changed group.");
        }
        existing_bundle_groups_by_new_bundle_groups.insert(
            std::make_pair(new_bundle_group, existing_bundle_group));
      }
    }
  } else if (type == SdpType::kAnswer) {
    if ((local && remote_desc) || (!local && local_desc)) {
      std::vector<const ContentGroup*> offered_bundle_groups =
          local ? remote_desc->GetGroupsByName(GROUP_TYPE_BUNDLE)
                : local_desc->GetGroupsByName(GROUP_TYPE_BUNDLE);

      std::map<std::string, const ContentGroup*> offered_bundle_groups_by_mid;
      for (const ContentGroup* offered_bundle_group : offered_bundle_groups) {
        for (const std::string& content_name :
             offered_bundle_group->content_names()) {
          offered_bundle_groups_by_mid[content_name] = offered_bundle_group;
        }
      }

      std::map<const ContentGroup*, const ContentGroup*>
          new_bundle_groups_by_offered_bundle_groups;
      for (const ContentGroup* new_bundle_group : new_bundle_groups) {
        if (!new_bundle_group->FirstContentName()) {
          // Empty groups could be a subset of any group.
          continue;
        }
        // The group in the answer (new_bundle_group) must have a corresponding
        // group in the offer (original_group), because the answer groups may
        // only be subsets of the offer groups.
        auto it = offered_bundle_groups_by_mid.find(
            *new_bundle_group->FirstContentName());
        if (it == offered_bundle_groups_by_mid.end()) {
          return RTCError(RTCErrorType::INVALID_PARAMETER,
                          "A BUNDLE group was added in the answer that did not "
                          "exist in the offer.");
        }
        const ContentGroup* offered_bundle_group = it->second;
        if (new_bundle_groups_by_offered_bundle_groups.find(
                offered_bundle_group) !=
            new_bundle_groups_by_offered_bundle_groups.end()) {
          return RTCError(RTCErrorType::INVALID_PARAMETER,
                          "A MID in the answer has changed group.");
        }
        new_bundle_groups_by_offered_bundle_groups.insert(
            std::make_pair(offered_bundle_group, new_bundle_group));
        for (const std::string& content_name :
             new_bundle_group->content_names()) {
          it = offered_bundle_groups_by_mid.find(content_name);
          // The BUNDLE group in answer should be a subset of offered group.
          if (it == offered_bundle_groups_by_mid.end() ||
              it->second != offered_bundle_group) {
            return RTCError(RTCErrorType::INVALID_PARAMETER,
                            "A BUNDLE group in answer contains a MID='" +
                                content_name +
                                "' that was not in the offered group.");
          }
        }
      }

      for (const auto& bundle_group : bundles_.bundle_groups()) {
        for (const std::string& content_name : bundle_group->content_names()) {
          // An answer that removes m= sections from pre-negotiated BUNDLE group
          // without rejecting it, is invalid.
          auto it = new_bundle_groups_by_mid.find(content_name);
          if (it == new_bundle_groups_by_mid.end()) {
            auto* content_info = description->GetContentByName(content_name);
            if (!content_info || !content_info->rejected) {
              return RTCError(RTCErrorType::INVALID_PARAMETER,
                              "Answer cannot remove m= section with mid='" +
                                  content_name +
                                  "' from already-established BUNDLE group.");
            }
          }
        }
      }
    }
  }

  if (config_.bundle_policy ==
          PeerConnectionInterface::kBundlePolicyMaxBundle &&
      !description->HasGroup(GROUP_TYPE_BUNDLE) &&
      description->contents().size() > 1) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "max-bundle is used but no bundle group found.");
  }

  bundles_.Update(description, type);

  for (const auto& bundle_group : bundles_.bundle_groups()) {
    if (!bundle_group->FirstContentName())
      continue;

    // The first MID in a BUNDLE group is BUNDLE-tagged.
    auto bundled_content =
        description->GetContentByName(*bundle_group->FirstContentName());
    if (!bundled_content) {
      return RTCError(
          RTCErrorType::INVALID_PARAMETER,
          "An m= section associated with the BUNDLE-tag doesn't exist.");
    }

    // If the `bundled_content` is rejected, other contents in the bundle group
    // must also be rejected.
    if (bundled_content->rejected) {
      for (const auto& content_name : bundle_group->content_names()) {
        auto other_content = description->GetContentByName(content_name);
        if (!other_content->rejected) {
          return RTCError(RTCErrorType::INVALID_PARAMETER,
                          "The m= section with mid='" + content_name +
                              "' should be rejected.");
        }
      }
    }
  }
  return RTCError::OK();
}

RTCError JsepTransportController::ValidateContent(
    const ContentInfo& content_info) {
  if (config_.rtcp_mux_policy ==
          PeerConnectionInterface::kRtcpMuxPolicyRequire &&
      content_info.type == MediaProtocolType::kRtp &&
      !content_info.bundle_only &&
      !content_info.media_description()->rtcp_mux()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "The m= section with mid='" + content_info.mid() +
                        "' is invalid. RTCP-MUX is not "
                        "enabled when it is required.");
  }
  return RTCError::OK();
}

void JsepTransportController::HandleRejectedContent(
    const ContentInfo& content_info) {
  // If the content is rejected, let the
  // BaseChannel/SctpTransport change the RtpTransport/DtlsTransport first,
  // then destroy the webrtc::JsepTransport.
  ContentGroup* bundle_group = bundles_.LookupGroupByMid(content_info.mid());
  if (bundle_group && !bundle_group->content_names().empty() &&
      content_info.mid() == *bundle_group->FirstContentName()) {
    // Rejecting a BUNDLE group's first mid means we are rejecting the entire
    // group.
    for (const auto& content_name : bundle_group->content_names()) {
      transports_.RemoveTransportForMid(content_name);
    }
    // Delete the BUNDLE group.
    bundles_.DeleteGroup(bundle_group);
  } else {
    transports_.RemoveTransportForMid(content_info.mid());
    if (bundle_group) {
      // Remove the rejected content from the `bundle_group`.
      bundles_.DeleteMid(bundle_group, content_info.mid());
    }
  }
}

bool JsepTransportController::HandleBundledContent(
    const ContentInfo& content_info,
    const ContentGroup& bundle_group) {
  TRACE_EVENT0("webrtc", "JsepTransportController::HandleBundledContent");
  RTC_DCHECK(bundle_group.FirstContentName());
  auto jsep_transport =
      GetJsepTransportByName(*bundle_group.FirstContentName());
  RTC_DCHECK(jsep_transport);
  // If the content is bundled, let the
  // BaseChannel/SctpTransport change the RtpTransport/DtlsTransport first,
  // then destroy the webrtc::JsepTransport.
  // TODO(bugs.webrtc.org/9719) For media transport this is far from ideal,
  // because it means that we first create media transport and start
  // connecting it, and then we destroy it. We will need to address it before
  // video path is enabled.
  return transports_.SetTransportForMid(content_info.mid(), jsep_transport);
}

JsepTransportDescription
JsepTransportController::CreateJsepTransportDescription(
    const ContentInfo& content_info,
    const TransportInfo& transport_info,
    const std::vector<int>& encrypted_extension_ids,
    int rtp_abs_sendtime_extn_id) {
  TRACE_EVENT0("webrtc",
               "JsepTransportController::CreateJsepTransportDescription");
  const MediaContentDescription* content_desc =
      content_info.media_description();
  RTC_DCHECK(content_desc);
  bool rtcp_mux_enabled = content_info.type == MediaProtocolType::kSctp
                              ? true
                              : content_desc->rtcp_mux();

  return JsepTransportDescription(rtcp_mux_enabled, encrypted_extension_ids,
                                  rtp_abs_sendtime_extn_id,
                                  transport_info.description);
}

std::vector<int> JsepTransportController::GetEncryptedHeaderExtensionIds(
    const ContentInfo& content_info) {
  const MediaContentDescription* content_desc =
      content_info.media_description();

  if (!config_.crypto_options.srtp.enable_encrypted_rtp_header_extensions) {
    return std::vector<int>();
  }

  std::vector<int> encrypted_header_extension_ids;
  for (const auto& extension : content_desc->rtp_header_extensions()) {
    if (!extension.encrypt) {
      continue;
    }
    if (!absl::c_linear_search(encrypted_header_extension_ids, extension.id)) {
      encrypted_header_extension_ids.push_back(extension.id);
    }
  }
  return encrypted_header_extension_ids;
}

std::map<const ContentGroup*, std::vector<int>>
JsepTransportController::MergeEncryptedHeaderExtensionIdsForBundles(
    const SessionDescription* description) {
  RTC_DCHECK(description);
  RTC_DCHECK(!bundles_.bundle_groups().empty());
  std::map<const ContentGroup*, std::vector<int>>
      merged_encrypted_extension_ids_by_bundle;
  // Union the encrypted header IDs in the group when bundle is enabled.
  for (const ContentInfo& content_info : description->contents()) {
    auto group = bundles_.LookupGroupByMid(content_info.mid());
    if (!group)
      continue;
    // Get or create list of IDs for the BUNDLE group.
    std::vector<int>& merged_ids =
        merged_encrypted_extension_ids_by_bundle[group];
    // Add IDs not already in the list.
    std::vector<int> extension_ids =
        GetEncryptedHeaderExtensionIds(content_info);
    for (int id : extension_ids) {
      if (!absl::c_linear_search(merged_ids, id)) {
        merged_ids.push_back(id);
      }
    }
  }
  return merged_encrypted_extension_ids_by_bundle;
}

int JsepTransportController::GetRtpAbsSendTimeHeaderExtensionId(
    const ContentInfo& content_info) {
  if (!config_.enable_external_auth) {
    return -1;
  }

  const MediaContentDescription* content_desc =
      content_info.media_description();

  const RtpExtension* send_time_extension =
      RtpExtension::FindHeaderExtensionByUri(
          content_desc->rtp_header_extensions(), RtpExtension::kAbsSendTimeUri,
          config_.crypto_options.srtp.enable_encrypted_rtp_header_extensions
              ? RtpExtension::kPreferEncryptedExtension
              : RtpExtension::kDiscardEncryptedExtension);
  return send_time_extension ? send_time_extension->id : -1;
}

const JsepTransport* JsepTransportController::GetJsepTransportForMid(
    const std::string& mid) const {
  return transports_.GetTransportForMid(mid);
}

JsepTransport* JsepTransportController::GetJsepTransportForMid(
    const std::string& mid) {
  return transports_.GetTransportForMid(mid);
}
const JsepTransport* JsepTransportController::GetJsepTransportForMid(
    absl::string_view mid) const {
  return transports_.GetTransportForMid(mid);
}

JsepTransport* JsepTransportController::GetJsepTransportForMid(
    absl::string_view mid) {
  return transports_.GetTransportForMid(mid);
}

const JsepTransport* JsepTransportController::GetJsepTransportByName(
    const std::string& transport_name) const {
  return transports_.GetTransportByName(transport_name);
}

JsepTransport* JsepTransportController::GetJsepTransportByName(
    const std::string& transport_name) {
  return transports_.GetTransportByName(transport_name);
}

RTCError JsepTransportController::MaybeCreateJsepTransport(
    bool local,
    const ContentInfo& content_info,
    const SessionDescription& description) {
  JsepTransport* transport = GetJsepTransportByName(content_info.mid());
  if (transport) {
    return RTCError::OK();
  }

  scoped_refptr<IceTransportInterface> ice =
      CreateIceTransport(content_info.mid(), /*rtcp=*/false);

  std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport =
      CreateDtlsTransport(content_info, ice->internal());

  std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport;
  std::unique_ptr<RtpTransport> unencrypted_rtp_transport;
  std::unique_ptr<DtlsSrtpTransport> dtls_srtp_transport;

  scoped_refptr<IceTransportInterface> rtcp_ice;
  if (config_.rtcp_mux_policy !=
          PeerConnectionInterface::kRtcpMuxPolicyRequire &&
      content_info.type == MediaProtocolType::kRtp) {
    rtcp_ice = CreateIceTransport(content_info.mid(), /*rtcp=*/true);
    rtcp_dtls_transport =
        CreateDtlsTransport(content_info, rtcp_ice->internal());
  }

  if (config_.disable_encryption) {
    RTC_LOG(LS_INFO)
        << "Creating UnencryptedRtpTransport, becayse encryption is disabled.";
    unencrypted_rtp_transport = CreateUnencryptedRtpTransport(
        content_info.mid(), rtp_dtls_transport.get(),
        rtcp_dtls_transport.get());
  } else {
    RTC_LOG(LS_INFO) << "Creating DtlsSrtpTransport.";
    dtls_srtp_transport =
        CreateDtlsSrtpTransport(content_info.mid(), rtp_dtls_transport.get(),
                                rtcp_dtls_transport.get());
  }

  std::unique_ptr<SctpTransportInternal> sctp_transport;
  if (config_.sctp_factory) {
    sctp_transport = config_.sctp_factory->CreateSctpTransport(
        env_, rtp_dtls_transport.get());
  }

  std::unique_ptr<JsepTransport> jsep_transport =
      std::make_unique<JsepTransport>(
          content_info.mid(), certificate_, std::move(ice), std::move(rtcp_ice),
          std::move(unencrypted_rtp_transport), std::move(dtls_srtp_transport),
          std::move(rtp_dtls_transport), std::move(rtcp_dtls_transport),
          std::move(sctp_transport),
          [&]() {
            RTC_DCHECK_RUN_ON(network_thread_);
            UpdateAggregateStates_n();
          },
          payload_type_picker_);

  jsep_transport->rtp_transport()->SubscribeRtcpPacketReceived(
      this, [this](CopyOnWriteBuffer* buffer, int64_t packet_time_ms) {
        RTC_DCHECK_RUN_ON(network_thread_);
        OnRtcpPacketReceived_n(buffer, packet_time_ms);
      });
  jsep_transport->rtp_transport()->SetUnDemuxableRtpPacketReceivedHandler(
      [this](RtpPacketReceived& packet) {
        RTC_DCHECK_RUN_ON(network_thread_);
        OnUnDemuxableRtpPacketReceived_n(packet);
      });

  transports_.RegisterTransport(content_info.mid(), std::move(jsep_transport));
  UpdateAggregateStates_n();
  return RTCError::OK();
}

void JsepTransportController::DestroyAllJsepTransports_n() {
  transports_.DestroyAllTransports();
}

void JsepTransportController::SetIceRole_n(IceRole ice_role) {
  ice_role_ = ice_role;
  auto dtls_transports = GetDtlsTransports();
  for (auto& dtls : dtls_transports) {
    dtls->ice_transport()->SetIceRole(ice_role_);
  }
}

IceRole JsepTransportController::DetermineIceRole(
    JsepTransport* jsep_transport,
    const TransportInfo& transport_info,
    SdpType type,
    bool local) {
  IceRole ice_role = ice_role_;
  auto tdesc = transport_info.description;
  if (local) {
    // The initial offer side may use ICE Lite, in which case, per RFC5245
    // Section 5.1.1, the answer side should take the controlling role if it is
    // in the full ICE mode.
    //
    // When both sides use ICE Lite, the initial offer side must take the
    // controlling role, and this is the default logic implemented in
    // SetLocalDescription in JsepTransportController.
    if (jsep_transport->remote_description() &&
        jsep_transport->remote_description()->transport_desc.ice_mode ==
            ICEMODE_LITE &&
        ice_role_ == ICEROLE_CONTROLLED && tdesc.ice_mode == ICEMODE_FULL) {
      ice_role = ICEROLE_CONTROLLING;
    }
  } else {
    // If our role is webrtc::ICEROLE_CONTROLLED and the remote endpoint
    // supports only ice_lite, this local endpoint should take the CONTROLLING
    // role.
    // TODO(deadbeef): This is a session-level attribute, so it really shouldn't
    // be in a TransportDescription in the first place...
    if (ice_role_ == ICEROLE_CONTROLLED && tdesc.ice_mode == ICEMODE_LITE) {
      ice_role = ICEROLE_CONTROLLING;
    }

    // If we use ICE Lite and the remote endpoint uses the full implementation
    // of ICE, the local endpoint must take the controlled role, and the other
    // side must be the controlling role.
    if (jsep_transport->local_description() &&
        jsep_transport->local_description()->transport_desc.ice_mode ==
            ICEMODE_LITE &&
        ice_role_ == ICEROLE_CONTROLLING && tdesc.ice_mode == ICEMODE_FULL) {
      ice_role = ICEROLE_CONTROLLED;
    }
  }

  return ice_role;
}

void JsepTransportController::OnTransportWritableState_n(
    PacketTransportInternal* transport) {
  RTC_LOG(LS_INFO) << " Transport " << transport->transport_name()
                   << " writability changed to " << transport->writable()
                   << ".";
  UpdateAggregateStates_n();
}

void JsepTransportController::OnTransportReceivingState_n(
    PacketTransportInternal* transport) {
  UpdateAggregateStates_n();
}

void JsepTransportController::OnTransportGatheringState_n(
    IceTransportInternal* transport) {
  UpdateAggregateStates_n();
}

void JsepTransportController::OnTransportCandidateGathered_n(
    IceTransportInternal* transport,
    const Candidate& candidate) {
  // We should never signal peer-reflexive candidates.
  if (candidate.is_prflx()) {
    RTC_DCHECK_NOTREACHED();
    return;
  }

  signal_ice_candidates_gathered_.Send(transport->transport_name(),
                                       std::vector<Candidate>{candidate});
}

void JsepTransportController::OnTransportCandidateError_n(
    IceTransportInternal* transport,
    const IceCandidateErrorEvent& event) {
  signal_ice_candidate_error_.Send(event);
}
void JsepTransportController::OnTransportCandidatesRemoved_n(
    IceTransportInternal* transport,
    const Candidates& candidates) {
  signal_ice_candidates_removed_.Send(transport, candidates);
}
void JsepTransportController::OnTransportCandidatePairChanged_n(
    const CandidatePairChangeEvent& event) {
  RTC_DCHECK(!event.transport_name.empty());
  signal_ice_candidate_pair_changed_.Send(event);
}

void JsepTransportController::OnTransportRoleConflict_n(
    IceTransportInternal* transport) {
  // Note: since the role conflict is handled entirely on the network thread,
  // we don't need to worry about role conflicts occurring on two ports at
  // once. The first one encountered should immediately reverse the role.
  IceRole reversed_role = (ice_role_ == ICEROLE_CONTROLLING)
                              ? ICEROLE_CONTROLLED
                              : ICEROLE_CONTROLLING;
  RTC_LOG(LS_INFO) << "Got role conflict; switching to "
                   << (reversed_role == ICEROLE_CONTROLLING ? "controlling"
                                                            : "controlled")
                   << " role.";
  SetIceRole_n(reversed_role);
}

void JsepTransportController::OnTransportStateChanged_n(
    IceTransportInternal* transport) {
  RTC_LOG(LS_INFO) << transport->transport_name() << " Transport "
                   << transport->component()
                   << " state changed. Check if state is complete.";
  UpdateAggregateStates_n();
}

void JsepTransportController::UpdateAggregateStates_n() {
  TRACE_EVENT0("webrtc", "JsepTransportController::UpdateAggregateStates_n");
  auto dtls_transports = GetActiveDtlsTransports();
  IceConnectionState new_connection_state = kIceConnectionConnecting;
  PeerConnectionInterface::IceConnectionState new_ice_connection_state =
      PeerConnectionInterface::IceConnectionState::kIceConnectionNew;
  PeerConnectionInterface::PeerConnectionState new_combined_state =
      PeerConnectionInterface::PeerConnectionState::kNew;
  IceGatheringState new_gathering_state = kIceGatheringNew;
  bool any_failed = false;
  bool all_connected = !dtls_transports.empty();
  bool all_completed = !dtls_transports.empty();
  bool any_gathering = false;
  bool all_done_gathering = !dtls_transports.empty();

  std::map<IceTransportState, int> ice_state_counts;
  std::map<DtlsTransportState, int> dtls_state_counts;

  for (const auto& dtls : dtls_transports) {
    any_failed = any_failed || dtls->ice_transport()->GetState() ==
                                   IceTransportStateInternal::STATE_FAILED;
    all_connected = all_connected && dtls->writable();
    all_completed =
        all_completed && dtls->writable() &&
        dtls->ice_transport()->GetState() ==
            IceTransportStateInternal::STATE_COMPLETED &&
        dtls->ice_transport()->GetIceRole() == ICEROLE_CONTROLLING &&
        dtls->ice_transport()->gathering_state() == kIceGatheringComplete;
    any_gathering = any_gathering || dtls->ice_transport()->gathering_state() !=
                                         kIceGatheringNew;
    all_done_gathering =
        all_done_gathering &&
        dtls->ice_transport()->gathering_state() == kIceGatheringComplete;

    dtls_state_counts[dtls->dtls_state()]++;
    ice_state_counts[dtls->ice_transport()->GetIceTransportState()]++;
  }

  if (any_failed) {
    new_connection_state = kIceConnectionFailed;
  } else if (all_completed) {
    new_connection_state = kIceConnectionCompleted;
  } else if (all_connected) {
    new_connection_state = kIceConnectionConnected;
  }
  if (ice_connection_state_ != new_connection_state) {
    ice_connection_state_ = new_connection_state;

    signal_ice_connection_state_.Send(new_connection_state);
  }

  // Compute the current RTCIceConnectionState as described in
  // https://www.w3.org/TR/webrtc/#dom-rtciceconnectionstate.
  // The PeerConnection is responsible for handling the "closed" state.
  int total_ice_checking = ice_state_counts[IceTransportState::kChecking];
  int total_ice_connected = ice_state_counts[IceTransportState::kConnected];
  int total_ice_completed = ice_state_counts[IceTransportState::kCompleted];
  int total_ice_failed = ice_state_counts[IceTransportState::kFailed];
  int total_ice_disconnected =
      ice_state_counts[IceTransportState::kDisconnected];
  int total_ice_closed = ice_state_counts[IceTransportState::kClosed];
  int total_ice_new = ice_state_counts[IceTransportState::kNew];
  int total_ice = dtls_transports.size();

  if (total_ice_failed > 0) {
    // Any RTCIceTransports are in the "failed" state.
    new_ice_connection_state = PeerConnectionInterface::kIceConnectionFailed;
  } else if (total_ice_disconnected > 0) {
    // None of the previous states apply and any RTCIceTransports are in the
    // "disconnected" state.
    new_ice_connection_state =
        PeerConnectionInterface::kIceConnectionDisconnected;
  } else if (total_ice_new + total_ice_closed == total_ice) {
    // None of the previous states apply and all RTCIceTransports are in the
    // "new" or "closed" state, or there are no transports.
    new_ice_connection_state = PeerConnectionInterface::kIceConnectionNew;
  } else if (total_ice_new + total_ice_checking > 0) {
    // None of the previous states apply and any RTCIceTransports are in the
    // "new" or "checking" state.
    new_ice_connection_state = PeerConnectionInterface::kIceConnectionChecking;
  } else if (total_ice_completed + total_ice_closed == total_ice ||
             all_completed) {
    // None of the previous states apply and all RTCIceTransports are in the
    // "completed" or "closed" state.
    //
    // TODO(https://bugs.webrtc.org/10356): The all_completed condition is added
    // to mimic the behavior of the old ICE connection state, and should be
    // removed once we get end-of-candidates signaling in place.
    new_ice_connection_state = PeerConnectionInterface::kIceConnectionCompleted;
  } else if (total_ice_connected + total_ice_completed + total_ice_closed ==
             total_ice) {
    // None of the previous states apply and all RTCIceTransports are in the
    // "connected", "completed" or "closed" state.
    new_ice_connection_state = PeerConnectionInterface::kIceConnectionConnected;
  } else {
    RTC_DCHECK_NOTREACHED();
  }

  if (standardized_ice_connection_state_ != new_ice_connection_state) {
    if (standardized_ice_connection_state_ ==
            PeerConnectionInterface::kIceConnectionChecking &&
        new_ice_connection_state ==
            PeerConnectionInterface::kIceConnectionCompleted) {
      // Ensure that we never skip over the "connected" state.
      signal_standardized_ice_connection_state_.Send(
          PeerConnectionInterface::kIceConnectionConnected);
    }
    standardized_ice_connection_state_ = new_ice_connection_state;
    signal_standardized_ice_connection_state_.Send(new_ice_connection_state);
  }

  // Compute the current RTCPeerConnectionState as described in
  // https://www.w3.org/TR/webrtc/#dom-rtcpeerconnectionstate.
  // The PeerConnection is responsible for handling the "closed" state.
  // Note that "connecting" is only a valid state for DTLS transports while
  // "checking", "completed" and "disconnected" are only valid for ICE
  // transports.
  int total_connected =
      total_ice_connected + dtls_state_counts[DtlsTransportState::kConnected];
  int total_dtls_connecting =
      dtls_state_counts[DtlsTransportState::kConnecting];
  int total_failed =
      total_ice_failed + dtls_state_counts[DtlsTransportState::kFailed];
  int total_closed =
      total_ice_closed + dtls_state_counts[DtlsTransportState::kClosed];
  int total_new = total_ice_new + dtls_state_counts[DtlsTransportState::kNew];
  int total_transports = total_ice * 2;

  if (total_failed > 0) {
    // Any of the RTCIceTransports or RTCDtlsTransports are in a "failed" state.
    new_combined_state = PeerConnectionInterface::PeerConnectionState::kFailed;
  } else if (total_ice_disconnected > 0) {
    // None of the previous states apply and any RTCIceTransports or
    // RTCDtlsTransports are in the "disconnected" state.
    new_combined_state =
        PeerConnectionInterface::PeerConnectionState::kDisconnected;
  } else if (total_new + total_closed == total_transports) {
    // None of the previous states apply and all RTCIceTransports and
    // RTCDtlsTransports are in the "new" or "closed" state, or there are no
    // transports.
    new_combined_state = PeerConnectionInterface::PeerConnectionState::kNew;
  } else if (total_new + total_dtls_connecting + total_ice_checking > 0) {
    // None of the previous states apply and all RTCIceTransports or
    // RTCDtlsTransports are in the "new", "connecting" or "checking" state.
    new_combined_state =
        PeerConnectionInterface::PeerConnectionState::kConnecting;
  } else if (total_connected + total_ice_completed + total_closed ==
             total_transports) {
    // None of the previous states apply and all RTCIceTransports and
    // RTCDtlsTransports are in the "connected", "completed" or "closed" state.
    new_combined_state =
        PeerConnectionInterface::PeerConnectionState::kConnected;
  } else {
    RTC_DCHECK_NOTREACHED();
  }

  if (combined_connection_state_ != new_combined_state) {
    combined_connection_state_ = new_combined_state;
    signal_connection_state_.Send(new_combined_state);
  }

  // Compute the gathering state.
  if (dtls_transports.empty()) {
    new_gathering_state = kIceGatheringNew;
  } else if (all_done_gathering) {
    new_gathering_state = kIceGatheringComplete;
  } else if (any_gathering) {
    new_gathering_state = kIceGatheringGathering;
  }
  if (ice_gathering_state_ != new_gathering_state) {
    ice_gathering_state_ = new_gathering_state;
    signal_ice_gathering_state_.Send(new_gathering_state);
  }
}

void JsepTransportController::OnRtcpPacketReceived_n(CopyOnWriteBuffer* packet,
                                                     int64_t packet_time_us) {
  RTC_DCHECK(config_.rtcp_handler);
  config_.rtcp_handler(*packet, packet_time_us);
}

void JsepTransportController::OnUnDemuxableRtpPacketReceived_n(
    const RtpPacketReceived& packet) {
  RTC_DCHECK(config_.un_demuxable_packet_handler);
  config_.un_demuxable_packet_handler(packet);
}

void JsepTransportController::OnDtlsHandshakeError(SSLHandshakeError error) {
  config_.on_dtls_handshake_error_(error);
}

bool JsepTransportController::OnTransportChanged(
    const std::string& mid,
    JsepTransport* jsep_transport) {
  if (config_.transport_observer) {
    if (jsep_transport) {
      return config_.transport_observer->OnTransportChanged(
          mid, jsep_transport->rtp_transport(),
          jsep_transport->RtpDtlsTransport(),
          jsep_transport->data_channel_transport());
    } else {
      return config_.transport_observer->OnTransportChanged(mid, nullptr,
                                                            nullptr, nullptr);
    }
  }
  return false;
}

}  // namespace webrtc
