/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/jsep_transport.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/candidate.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "call/payload_type_picker.h"
#include "media/sctp/sctp_transport_internal.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/transport_description.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/dtls_transport.h"
#include "pc/rtp_transport.h"
#include "pc/sctp_transport.h"
#include "pc/session_description.h"
#include "pc/transport_stats.h"
#include "rtc_base/checks.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/thread.h"
#include "rtc_base/trace_event.h"

using webrtc::SdpType;

namespace webrtc {

JsepTransportDescription::JsepTransportDescription() {}

JsepTransportDescription::JsepTransportDescription(
    bool rtcp_mux_enabled,
    const std::vector<int>& encrypted_header_extension_ids,
    int rtp_abs_sendtime_extn_id,
    const TransportDescription& transport_desc)
    : rtcp_mux_enabled(rtcp_mux_enabled),
      encrypted_header_extension_ids(encrypted_header_extension_ids),
      rtp_abs_sendtime_extn_id(rtp_abs_sendtime_extn_id),
      transport_desc(transport_desc) {}

JsepTransportDescription::JsepTransportDescription(
    const JsepTransportDescription& from)
    : rtcp_mux_enabled(from.rtcp_mux_enabled),
      encrypted_header_extension_ids(from.encrypted_header_extension_ids),
      rtp_abs_sendtime_extn_id(from.rtp_abs_sendtime_extn_id),
      transport_desc(from.transport_desc) {}

JsepTransportDescription::~JsepTransportDescription() = default;

JsepTransportDescription& JsepTransportDescription::operator=(
    const JsepTransportDescription& from) {
  if (this == &from) {
    return *this;
  }
  rtcp_mux_enabled = from.rtcp_mux_enabled;
  encrypted_header_extension_ids = from.encrypted_header_extension_ids;
  rtp_abs_sendtime_extn_id = from.rtp_abs_sendtime_extn_id;
  transport_desc = from.transport_desc;

  return *this;
}

JsepTransport::JsepTransport(
    const std::string& mid,
    const scoped_refptr<RTCCertificate>& local_certificate,
    scoped_refptr<IceTransportInterface> ice_transport,
    scoped_refptr<IceTransportInterface> rtcp_ice_transport,
    std::unique_ptr<RtpTransport> unencrypted_rtp_transport,
    std::unique_ptr<DtlsSrtpTransport> dtls_srtp_transport,
    std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport,
    std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport,
    std::unique_ptr<SctpTransportInternal> sctp_transport,
    std::function<void()> rtcp_mux_active_callback,
    PayloadTypePicker& suggester)
    : network_thread_(Thread::Current()),
      mid_(mid),
      local_certificate_(local_certificate),
      ice_transport_(std::move(ice_transport)),
      rtcp_ice_transport_(std::move(rtcp_ice_transport)),
      unencrypted_rtp_transport_(std::move(unencrypted_rtp_transport)),
      dtls_srtp_transport_(std::move(dtls_srtp_transport)),
      rtp_dtls_transport_(
          rtp_dtls_transport
              ? make_ref_counted<DtlsTransport>(std::move(rtp_dtls_transport))
              : nullptr),
      rtcp_dtls_transport_(
          rtcp_dtls_transport
              ? make_ref_counted<DtlsTransport>(std::move(rtcp_dtls_transport))
              : nullptr),
      sctp_transport_(sctp_transport
                          ? make_ref_counted<::webrtc::SctpTransport>(
                                std::move(sctp_transport),
                                rtp_dtls_transport_)
                          : nullptr),
      rtcp_mux_active_callback_(std::move(rtcp_mux_active_callback)),
      remote_payload_types_(suggester),
      local_payload_types_(suggester) {
  TRACE_EVENT0("webrtc", "JsepTransport::JsepTransport");
  RTC_DCHECK(ice_transport_);
  RTC_DCHECK(rtp_dtls_transport_);
  // `rtcp_ice_transport_` must be present iff `rtcp_dtls_transport_` is
  // present.
  RTC_DCHECK_EQ((rtcp_ice_transport_ != nullptr),
                (rtcp_dtls_transport_ != nullptr));
  // Verify the "only one out of these three can be set" invariant.
  if (unencrypted_rtp_transport_) {
    RTC_DCHECK(!dtls_srtp_transport);
  } else {
    RTC_DCHECK(dtls_srtp_transport_);
    RTC_DCHECK(!unencrypted_rtp_transport);
  }
}

JsepTransport::~JsepTransport() {
  TRACE_EVENT0("webrtc", "JsepTransport::~JsepTransport");
  if (sctp_transport_) {
    sctp_transport_->Clear();
  }

  // Clear all DtlsTransports. There may be pointers to these from
  // other places, so we can't assume they'll be deleted by the destructor.
  rtp_dtls_transport_->Clear();
  if (rtcp_dtls_transport_) {
    rtcp_dtls_transport_->Clear();
  }

  // ICE will be the last transport to be deleted.
}

RTCError JsepTransport::SetLocalJsepTransportDescription(
    const JsepTransportDescription& jsep_description,
    SdpType type) {
  RTCError error;
  TRACE_EVENT0("webrtc", "JsepTransport::SetLocalJsepTransportDescription");
  RTC_DCHECK_RUN_ON(network_thread_);

  IceParameters ice_parameters =
      jsep_description.transport_desc.GetIceParameters();
  RTCError ice_parameters_result = ice_parameters.Validate();
  if (!ice_parameters_result.ok()) {
    StringBuilder sb;
    sb << "Invalid ICE parameters: " << ice_parameters_result.message();
    return RTCError(RTCErrorType::INVALID_PARAMETER, sb.Release());
  }

  if (!SetRtcpMux(jsep_description.rtcp_mux_enabled, type,
                  ContentSource::CS_LOCAL)) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Failed to setup RTCP mux.");
  }

  if (dtls_srtp_transport_) {
    RTC_DCHECK(!unencrypted_rtp_transport_);
    dtls_srtp_transport_->UpdateRecvEncryptedHeaderExtensionIds(
        jsep_description.encrypted_header_extension_ids);
  }
  bool ice_restarting =
      local_description_ != nullptr &&
      IceCredentialsChanged(local_description_->transport_desc.ice_ufrag,
                            local_description_->transport_desc.ice_pwd,
                            ice_parameters.ufrag, ice_parameters.pwd);
  local_description_.reset(new JsepTransportDescription(jsep_description));

  SSLFingerprint* local_fp =
      local_description_->transport_desc.identity_fingerprint.get();

  if (!local_fp) {
    local_certificate_ = nullptr;
  } else {
    error = VerifyCertificateFingerprint(local_certificate_.get(), local_fp);
    if (!error.ok()) {
      local_description_.reset();
      return error;
    }
  }
  RTC_DCHECK(rtp_dtls_transport_->internal());
  rtp_dtls_transport_->internal()->ice_transport()->SetIceParameters(
      ice_parameters);

  if (rtcp_dtls_transport_) {
    RTC_DCHECK(rtcp_dtls_transport_->internal());
    rtcp_dtls_transport_->internal()->ice_transport()->SetIceParameters(
        ice_parameters);
  }
  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
    error = NegotiateAndSetDtlsParameters(type);
  }
  if (!error.ok()) {
    local_description_.reset();
    return error;
  }

  if (needs_ice_restart_ && ice_restarting) {
    needs_ice_restart_ = false;
    RTC_LOG(LS_VERBOSE) << "needs-ice-restart flag cleared for transport "
                        << mid();
  }

  return RTCError::OK();
}

RTCError JsepTransport::SetRemoteJsepTransportDescription(
    const JsepTransportDescription& jsep_description,
    SdpType type) {
  TRACE_EVENT0("webrtc", "JsepTransport::SetLocalJsepTransportDescription");
  RTCError error;

  RTC_DCHECK_RUN_ON(network_thread_);

  IceParameters ice_parameters =
      jsep_description.transport_desc.GetIceParameters();
  RTCError ice_parameters_result = ice_parameters.Validate();
  if (!ice_parameters_result.ok()) {
    remote_description_.reset();
    StringBuilder sb;
    sb << "Invalid ICE parameters: " << ice_parameters_result.message();
    return RTCError(RTCErrorType::INVALID_PARAMETER, sb.Release());
  }

  if (!SetRtcpMux(jsep_description.rtcp_mux_enabled, type,
                  ContentSource::CS_REMOTE)) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Failed to setup RTCP mux.");
  }

  if (dtls_srtp_transport_) {
    RTC_DCHECK(!unencrypted_rtp_transport_);
    dtls_srtp_transport_->UpdateSendEncryptedHeaderExtensionIds(
        jsep_description.encrypted_header_extension_ids);
    dtls_srtp_transport_->CacheRtpAbsSendTimeHeaderExtension(
        jsep_description.rtp_abs_sendtime_extn_id);
  }

  remote_description_.reset(new JsepTransportDescription(jsep_description));
  RTC_DCHECK(rtp_dtls_transport());
  SetRemoteIceParameters(ice_parameters, rtp_dtls_transport()->ice_transport());

  if (rtcp_dtls_transport()) {
    SetRemoteIceParameters(ice_parameters,
                           rtcp_dtls_transport()->ice_transport());
  }

  // If PRANSWER/ANSWER is set, we should decide transport protocol type.
  if (type == SdpType::kPrAnswer || type == SdpType::kAnswer) {
    error = NegotiateAndSetDtlsParameters(SdpType::kOffer);
  }
  if (!error.ok()) {
    remote_description_.reset();
    return error;
  }
  return RTCError::OK();
}

RTCError JsepTransport::AddRemoteCandidates(const Candidates& candidates) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!remote_description_) {
    return RTCError(RTCErrorType::INVALID_STATE,
                    mid() +
                        " is not ready to use the remote candidate because the "
                        "remote description is not set.");
  }

  for (const Candidate& candidate : candidates) {
    auto transport = candidate.component() == ICE_CANDIDATE_COMPONENT_RTP
                         ? rtp_dtls_transport_
                         : rtcp_dtls_transport_;
    if (!transport) {
      return RTCError(RTCErrorType::INVALID_PARAMETER,
                      "Candidate has an unknown component: " +
                          candidate.ToSensitiveString() + " for mid " + mid());
    }
    RTC_DCHECK(transport->internal() && transport->internal()->ice_transport());
    transport->internal()->ice_transport()->AddRemoteCandidate(candidate);
  }
  return RTCError::OK();
}

void JsepTransport::SetNeedsIceRestartFlag() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!needs_ice_restart_) {
    needs_ice_restart_ = true;
    RTC_LOG(LS_VERBOSE) << "needs-ice-restart flag set for transport " << mid();
  }
}

std::optional<SSLRole> JsepTransport::GetDtlsRole() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(rtp_dtls_transport_);
  RTC_DCHECK(rtp_dtls_transport_->internal());
  SSLRole dtls_role;
  if (!rtp_dtls_transport_->internal()->GetDtlsRole(&dtls_role)) {
    return std::optional<SSLRole>();
  }

  return std::optional<SSLRole>(dtls_role);
}

bool JsepTransport::GetStats(TransportStats* stats) const {
  TRACE_EVENT0("webrtc", "JsepTransport::GetStats");
  RTC_DCHECK_RUN_ON(network_thread_);
  stats->transport_name = mid();
  stats->channel_stats.clear();
  RTC_DCHECK(rtp_dtls_transport_->internal());
  bool ret = GetTransportStats(rtp_dtls_transport_->internal(),
                               ICE_CANDIDATE_COMPONENT_RTP, stats);

  if (rtcp_dtls_transport_) {
    RTC_DCHECK(rtcp_dtls_transport_->internal());
    ret &= GetTransportStats(rtcp_dtls_transport_->internal(),
                             ICE_CANDIDATE_COMPONENT_RTCP, stats);
  }
  return ret;
}

RTCError JsepTransport::VerifyCertificateFingerprint(
    const RTCCertificate* certificate,
    const SSLFingerprint* fingerprint) const {
  TRACE_EVENT0("webrtc", "JsepTransport::VerifyCertificateFingerprint");
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!fingerprint) {
    return RTCError(RTCErrorType::INVALID_PARAMETER, "No fingerprint");
  }
  if (!certificate) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Fingerprint provided but no identity available.");
  }
  std::unique_ptr<SSLFingerprint> fp_tmp = SSLFingerprint::CreateUnique(
      fingerprint->algorithm, *certificate->identity());
  RTC_DCHECK(fp_tmp.get() != nullptr);
  if (*fp_tmp == *fingerprint) {
    return RTCError::OK();
  }
  char ss_buf[1024];
  SimpleStringBuilder desc(ss_buf);
  desc << "Local fingerprint does not match identity. Expected: ";
  desc << fp_tmp->ToString();
  desc << " Got: " << fingerprint->ToString();
  return RTCError(RTCErrorType::INVALID_PARAMETER, std::string(desc.str()));
}

void JsepTransport::SetActiveResetSrtpParams(bool active_reset_srtp_params) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (dtls_srtp_transport_) {
    RTC_LOG(LS_INFO)
        << "Setting active_reset_srtp_params of DtlsSrtpTransport to: "
        << active_reset_srtp_params;
    dtls_srtp_transport_->SetActiveResetSrtpParams(active_reset_srtp_params);
  }
}

RTCError JsepTransport::RecordPayloadTypes(bool local,
                                           SdpType type,
                                           const ContentInfo& content) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (local) {
    local_payload_types_.DisallowRedefinition();
  } else {
    remote_payload_types_.DisallowRedefinition();
  }
  RTCError result = RTCError::OK();
  for (auto codec : content.media_description()->codecs()) {
    if (local) {
      result = local_payload_types_.AddMapping(codec.id, codec);
    } else {
      result = remote_payload_types_.AddMapping(codec.id, codec);
    }
    if (!result.ok()) {
      break;
    }
  }
  if (local) {
    local_payload_types_.ReallowRedefinition();
  } else {
    remote_payload_types_.ReallowRedefinition();
  }
  return result;
}

void JsepTransport::SetRemoteIceParameters(
    const IceParameters& ice_parameters,
    IceTransportInternal* ice_transport) {
  TRACE_EVENT0("webrtc", "JsepTransport::SetRemoteIceParameters");
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(ice_transport);
  RTC_DCHECK(remote_description_);
  ice_transport->SetRemoteIceParameters(ice_parameters);
  ice_transport->SetRemoteIceMode(remote_description_->transport_desc.ice_mode);
}

RTCError JsepTransport::SetNegotiatedDtlsParameters(
    DtlsTransportInternal* dtls_transport,
    std::optional<SSLRole> dtls_role,
    SSLFingerprint* remote_fingerprint) {
  RTC_DCHECK(dtls_transport);
  return dtls_transport->SetRemoteParameters(
      remote_fingerprint->algorithm, remote_fingerprint->digest.cdata(),
      remote_fingerprint->digest.size(), dtls_role);
}

bool JsepTransport::SetRtcpMux(bool enable,
                               SdpType type,
                               ContentSource source) {
  RTC_DCHECK_RUN_ON(network_thread_);
  bool ret = false;
  switch (type) {
    case SdpType::kOffer:
      ret = rtcp_mux_negotiator_.SetOffer(enable, source);
      break;
    case SdpType::kPrAnswer:
      // This may activate RTCP muxing, but we don't yet destroy the transport
      // because the final answer may deactivate it.
      ret = rtcp_mux_negotiator_.SetProvisionalAnswer(enable, source);
      break;
    case SdpType::kAnswer:
      ret = rtcp_mux_negotiator_.SetAnswer(enable, source);
      if (ret && rtcp_mux_negotiator_.IsActive()) {
        ActivateRtcpMux();
      }
      break;
    default:
      RTC_DCHECK_NOTREACHED();
  }

  if (!ret) {
    return false;
  }

  auto transport = rtp_transport();
  transport->SetRtcpMuxEnabled(rtcp_mux_negotiator_.IsActive());
  return ret;
}

void JsepTransport::ActivateRtcpMux() {
  if (unencrypted_rtp_transport_) {
    RTC_DCHECK(!dtls_srtp_transport_);
    unencrypted_rtp_transport_->SetRtcpPacketTransport(nullptr);
  } else if (dtls_srtp_transport_) {
    RTC_DCHECK(dtls_srtp_transport_);
    RTC_DCHECK(!unencrypted_rtp_transport_);
    dtls_srtp_transport_->SetDtlsTransports(rtp_dtls_transport(),
                                            /*rtcp_dtls_transport=*/nullptr);
  }
  rtcp_dtls_transport_ = nullptr;  // Destroy this reference.
  // Notify the JsepTransportController to update the aggregate states.
  rtcp_mux_active_callback_();
}

RTCError JsepTransport::NegotiateAndSetDtlsParameters(
    SdpType local_description_type) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!local_description_ || !remote_description_) {
    return RTCError(RTCErrorType::INVALID_STATE,
                    "Applying an answer transport description "
                    "without applying any offer.");
  }
  std::unique_ptr<SSLFingerprint> remote_fingerprint;
  std::optional<SSLRole> negotiated_dtls_role;

  SSLFingerprint* local_fp =
      local_description_->transport_desc.identity_fingerprint.get();
  SSLFingerprint* remote_fp =
      remote_description_->transport_desc.identity_fingerprint.get();
  if (remote_fp && local_fp) {
    remote_fingerprint = std::make_unique<SSLFingerprint>(*remote_fp);
    RTCError error =
        NegotiateDtlsRole(local_description_type,
                          local_description_->transport_desc.connection_role,
                          remote_description_->transport_desc.connection_role,
                          &negotiated_dtls_role);
    if (!error.ok()) {
      return error;
    }
  } else if (local_fp && (local_description_type == SdpType::kAnswer)) {
    return RTCError(
        RTCErrorType::INVALID_PARAMETER,
        "Local fingerprint supplied when caller didn't offer DTLS.");
  } else {
    // We are not doing DTLS
    remote_fingerprint =
        std::make_unique<SSLFingerprint>("", ArrayView<const uint8_t>());
  }
  // Now that we have negotiated everything, push it downward.
  // Note that we cache the result so that if we have race conditions
  // between future SetRemote/SetLocal invocations and new transport
  // creation, we have the negotiation state saved until a new
  // negotiation happens.
  RTC_DCHECK(rtp_dtls_transport());
  RTCError error = SetNegotiatedDtlsParameters(
      rtp_dtls_transport(), negotiated_dtls_role, remote_fingerprint.get());
  if (!error.ok()) {
    return error;
  }

  if (rtcp_dtls_transport()) {
    error = SetNegotiatedDtlsParameters(
        rtcp_dtls_transport(), negotiated_dtls_role, remote_fingerprint.get());
  }
  return error;
}

RTCError JsepTransport::NegotiateDtlsRole(
    SdpType local_description_type,
    ConnectionRole local_connection_role,
    ConnectionRole remote_connection_role,
    std::optional<SSLRole>* negotiated_dtls_role) {
  // From RFC 4145, section-4.1, The following are the values that the
  // 'setup' attribute can take in an offer/answer exchange:
  //       Offer      Answer
  //      ________________
  //      active     passive / holdconn
  //      passive    active / holdconn
  //      actpass    active / passive / holdconn
  //      holdconn   holdconn
  //
  // Set the role that is most conformant with RFC 5763, Section 5, bullet 1
  // The endpoint MUST use the setup attribute defined in [RFC4145].
  // The endpoint that is the offerer MUST use the setup attribute
  // value of setup:actpass and be prepared to receive a client_hello
  // before it receives the answer.  The answerer MUST use either a
  // setup attribute value of setup:active or setup:passive.  Note that
  // if the answerer uses setup:passive, then the DTLS handshake will
  // not begin until the answerer is received, which adds additional
  // latency. setup:active allows the answer and the DTLS handshake to
  // occur in parallel.  Thus, setup:active is RECOMMENDED.  Whichever
  // party is active MUST initiate a DTLS handshake by sending a
  // ClientHello over each flow (host/port quartet).
  // IOW - actpass and passive modes should be treated as server and
  // active as client.
  // RFC 8842 section 5.3 updates this text, so that it is mandated
  // for the responder to handle offers with "active" and "passive"
  // as well as "actpass"
  bool is_remote_server = false;
  if (local_description_type == SdpType::kOffer) {
    if (local_connection_role != CONNECTIONROLE_ACTPASS) {
      return RTCError(RTCErrorType::INVALID_PARAMETER,
                      "Offerer must use actpass value for setup attribute.");
    }

    if (remote_connection_role == CONNECTIONROLE_ACTIVE ||
        remote_connection_role == CONNECTIONROLE_PASSIVE ||
        remote_connection_role == CONNECTIONROLE_NONE) {
      is_remote_server = (remote_connection_role == CONNECTIONROLE_PASSIVE);
    } else {
      return RTCError(RTCErrorType::INVALID_PARAMETER,
                      "Answerer must use either active or passive value "
                      "for setup attribute.");
    }
    // If remote is NONE or ACTIVE it will act as client.
  } else {
    if (remote_connection_role != CONNECTIONROLE_ACTPASS &&
        remote_connection_role != CONNECTIONROLE_NONE) {
      // Accept a remote role attribute that's not "actpass", but matches the
      // current negotiated role. This is allowed by dtls-sdp, though our
      // implementation will never generate such an offer as it's not
      // recommended.
      //
      // See https://datatracker.ietf.org/doc/html/draft-ietf-mmusic-dtls-sdp,
      // section 5.5.
      auto current_dtls_role = GetDtlsRole();
      if (!current_dtls_role) {
        // Role not assigned yet. Verify that local role fits with remote role.
        switch (remote_connection_role) {
          case CONNECTIONROLE_ACTIVE:
            if (local_connection_role != CONNECTIONROLE_PASSIVE) {
              return RTCError(
                  RTCErrorType::INVALID_PARAMETER,
                  "Answerer must be passive when offerer is active");
            }
            break;
          case CONNECTIONROLE_PASSIVE:
            if (local_connection_role != CONNECTIONROLE_ACTIVE) {
              return RTCError(
                  RTCErrorType::INVALID_PARAMETER,
                  "Answerer must be active when offerer is passive");
            }
            break;
          default:
            RTC_DCHECK_NOTREACHED();
            break;
        }
      } else {
        if ((*current_dtls_role == SSL_CLIENT &&
             remote_connection_role == CONNECTIONROLE_ACTIVE) ||
            (*current_dtls_role == SSL_SERVER &&
             remote_connection_role == CONNECTIONROLE_PASSIVE)) {
          return RTCError(RTCErrorType::INVALID_PARAMETER,
                          "Offerer must use current negotiated role for "
                          "setup attribute.");
        }
      }
    }

    if (local_connection_role == CONNECTIONROLE_ACTIVE ||
        local_connection_role == CONNECTIONROLE_PASSIVE) {
      is_remote_server = (local_connection_role == CONNECTIONROLE_ACTIVE);
    } else {
      return RTCError(RTCErrorType::INVALID_PARAMETER,
                      "Answerer must use either active or passive value "
                      "for setup attribute.");
    }

    // If local is passive, local will act as server.
  }

  *negotiated_dtls_role = (is_remote_server ? SSL_CLIENT : SSL_SERVER);
  return RTCError::OK();
}

bool JsepTransport::GetTransportStats(DtlsTransportInternal* dtls_transport,
                                      int component,
                                      TransportStats* stats) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(dtls_transport);
  TransportChannelStats substats;
  substats.component = component;
  dtls_transport->GetSslVersionBytes(&substats.ssl_version_bytes);
  dtls_transport->GetSrtpCryptoSuite(&substats.srtp_crypto_suite);
  dtls_transport->GetSslCipherSuite(&substats.ssl_cipher_suite);
  substats.tls_cipher_suite_name = dtls_transport->GetTlsCipherSuiteName();
  substats.dtls_state = dtls_transport->dtls_state();
  SSLRole dtls_role;
  if (dtls_transport->GetDtlsRole(&dtls_role)) {
    substats.dtls_role = dtls_role;
  }
  if (!dtls_transport->ice_transport()->GetStats(
          &substats.ice_transport_stats)) {
    return false;
  }
  substats.ssl_peer_signature_algorithm =
      dtls_transport->GetSslPeerSignatureAlgorithm();
  stats->channel_stats.push_back(substats);
  return true;
}

}  // namespace webrtc
