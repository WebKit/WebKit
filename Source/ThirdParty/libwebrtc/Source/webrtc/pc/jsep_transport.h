/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_JSEP_TRANSPORT_H_
#define PC_JSEP_TRANSPORT_H_

#include <memory>
#include <optional>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/ice_transport_interface.h"
#include "api/jsep.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/sctp/sctp_transport_internal.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/transport_description.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/dtls_transport.h"
#include "pc/rtcp_mux_filter.h"
#include "pc/rtp_transport.h"
#include "pc/rtp_transport_internal.h"
#include "pc/sctp_transport.h"
#include "pc/session_description.h"
#include "pc/transport_stats.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

struct JsepTransportDescription {
 public:
  JsepTransportDescription();
  JsepTransportDescription(
      bool rtcp_mux_enabled,
      const std::vector<int>& encrypted_header_extension_ids,
      int rtp_abs_sendtime_extn_id,
      const TransportDescription& transport_description);
  JsepTransportDescription(const JsepTransportDescription& from);
  ~JsepTransportDescription();

  JsepTransportDescription& operator=(const JsepTransportDescription& from);

  bool rtcp_mux_enabled = true;
  std::vector<int> encrypted_header_extension_ids;
  int rtp_abs_sendtime_extn_id = -1;
  // TODO(zhihuang): Add the ICE and DTLS related variables and methods from
  // TransportDescription and remove this extra layer of abstraction.
  TransportDescription transport_desc;
};

// Helper class used by JsepTransportController that processes
// TransportDescriptions. A TransportDescription represents the
// transport-specific properties of an SDP m= section, processed according to
// JSEP. Each transport consists of DTLS and ICE transport channels for RTP
// (and possibly RTCP, if rtcp-mux isn't used).
//
// On Threading: JsepTransport performs work solely on the network thread, and
// so its methods should only be called on the network thread.
class JsepTransport {
 public:
  // `local_certificate` is allowed to be null since a remote description may be
  // set before a local certificate is generated.
  JsepTransport(const scoped_refptr<RTCCertificate>& local_certificate,
                std::unique_ptr<RtpTransport> rtp_transport,
                scoped_refptr<DtlsTransport> rtp_dtls_transport,
                std::unique_ptr<SctpTransportInternal> sctp_transport,
                absl::AnyInvocable<void()> rtcp_mux_active_callback);

  ~JsepTransport();

  JsepTransport(const JsepTransport&) = delete;
  JsepTransport& operator=(const JsepTransport&) = delete;

  // Returns the name of this transport. This is used for uniquely identifying
  // the transport, logging, error reporting and transport stats.
  absl::string_view name() const {
    return dtls_transport_internal()->ice_transport()->transport_name();
  }

  // Must be called before applying local session description.
  // Needed in order to verify the local fingerprint.
  void SetLocalCertificate(
      const scoped_refptr<RTCCertificate>& local_certificate) {
    RTC_DCHECK_RUN_ON(&transport_sequence_);
    local_certificate_ = local_certificate;
  }

  // Return the local certificate provided by SetLocalCertificate.
  scoped_refptr<RTCCertificate> GetLocalCertificate() const {
    RTC_DCHECK_RUN_ON(&transport_sequence_);
    return local_certificate_;
  }

  RTCError SetLocalJsepTransportDescription(
      const JsepTransportDescription& jsep_description, SdpType type);

  // Set the remote TransportDescription to be used by DTLS and ICE channels
  // that are part of this Transport.
  RTCError SetRemoteJsepTransportDescription(
      const JsepTransportDescription& jsep_description, SdpType type);
  RTCError AddRemoteCandidates(const Candidates& candidates);

  // Set the "needs-ice-restart" flag as described in JSEP. After the flag is
  // set, offers should generate new ufrags/passwords until an ICE restart
  // occurs.
  //
  // This and `needs_ice_restart()` must be called on the network thread.
  void SetNeedsIceRestartFlag();

  // Returns true if the ICE restart flag above was set, and no ICE restart has
  // occurred yet for this transport (by applying a local description with
  // changed ufrag/password).
  bool needs_ice_restart() const {
    RTC_DCHECK_RUN_ON(&transport_sequence_);
    return needs_ice_restart_;
  }

  // Returns role if negotiated, or empty std::optional if it hasn't been
  // negotiated yet.
  std::optional<SSLRole> GetDtlsRole() const;

  bool GetStats(TransportStats* stats) const;

  const JsepTransportDescription* local_description() const {
    RTC_DCHECK_RUN_ON(&transport_sequence_);
    return local_description_.get();
  }

  const JsepTransportDescription* remote_description() const {
    RTC_DCHECK_RUN_ON(&transport_sequence_);
    return remote_description_.get();
  }

  // Returns the rtp transport, if any.
  RtpTransportInternal* rtp_transport() const { return rtp_transport_.get(); }

  const DtlsTransportInternal* rtp_dtls_transport() const {
    return dtls_transport_internal();
  }

  DtlsTransportInternal* rtp_dtls_transport() {
    return dtls_transport_internal();
  }

  DtlsTransportInternal* rtcp_dtls_transport() const {
    RTC_DCHECK_RUN_ON(&transport_sequence_);
    return static_cast<DtlsTransportInternal*>(
        rtp_transport_->rtcp_packet_transport());
  }

  DtlsTransportInternal* rtcp_dtls_transport() {
    RTC_DCHECK_RUN_ON(&transport_sequence_);
    return static_cast<DtlsTransportInternal*>(
        rtp_transport_->rtcp_packet_transport());
  }

  scoped_refptr<DtlsTransport> RtpDtlsTransport() {
    return rtp_dtls_transport_;
  }

  scoped_refptr<::webrtc::SctpTransport> SctpTransport() const {
    return sctp_transport_;
  }

  // TODO(bugs.webrtc.org/9719): Delete method, update callers to use
  // SctpTransport() instead.
  DataChannelTransportInterface* data_channel_transport() const {
    return sctp_transport_.get();
  }

  // TODO(deadbeef): The methods below are only public for testing. Should make
  // them utility functions or objects so they can be tested independently from
  // this class.

  // Returns an error if the certificate's identity does not match the
  // fingerprint, or either is NULL.
  RTCError VerifyCertificateFingerprint(
      const RTCCertificate* certificate,
      const SSLFingerprint* fingerprint) const;

 private:
  bool SetRtcpMux(bool enable, SdpType type, ContentSource source);

  void ActivateRtcpMux() RTC_RUN_ON(transport_sequence_);

  // Negotiates and sets the DTLS parameters based on the current local and
  // remote transport description, such as the DTLS role to use, and whether
  // DTLS should be activated.
  //
  // Called when an answer TransportDescription is applied.
  RTCError NegotiateAndSetDtlsParameters(SdpType local_description_type);

  // Negotiates the DTLS role based off the offer and answer as specified by
  // RFC 4145, section-4.1. Returns an RTCError if role cannot be determined
  // from the local description and remote description.
  RTCError NegotiateDtlsRole(SdpType local_description_type,
                             ConnectionRole local_connection_role,
                             ConnectionRole remote_connection_role,
                             std::optional<SSLRole>* negotiated_dtls_role);

  // Pushes down the ICE parameters from the remote description.
  void SetRemoteIceParameters(const IceParameters& ice_parameters,
                              IceTransportInternal* ice);

  // Pushes down the DTLS parameters obtained via negotiation.
  static RTCError SetNegotiatedDtlsParameters(
      DtlsTransportInternal* dtls_transport,
      std::optional<SSLRole> dtls_role,
      SSLFingerprint* remote_fingerprint);

  bool GetTransportStats(DtlsTransportInternal* dtls_transport,
                         int component,
                         TransportStats* stats) const;

  DtlsTransportInternal* dtls_transport_internal() const {
    // This cast is safe because JsepTransportController always creates the
    // RtpTransport with a DtlsTransportInternal as the packet transport, even
    // when encryption is disabled.
    return static_cast<DtlsTransportInternal*>(
        rtp_transport_->rtp_packet_transport());
  }

  // Owning thread, for safety checks
  RTC_NO_UNIQUE_ADDRESS SequenceChecker transport_sequence_;
  // needs-ice-restart bit as described in JSEP.
  bool needs_ice_restart_ RTC_GUARDED_BY(transport_sequence_) = false;
  scoped_refptr<RTCCertificate> local_certificate_
      RTC_GUARDED_BY(transport_sequence_);
  std::unique_ptr<JsepTransportDescription> local_description_
      RTC_GUARDED_BY(transport_sequence_);
  std::unique_ptr<JsepTransportDescription> remote_description_
      RTC_GUARDED_BY(transport_sequence_);

  // To avoid downcasting and make it type safe, keep three unique pointers for
  // different SRTP mode and only one of these is non-nullptr.
  const std::unique_ptr<RtpTransport> rtp_transport_;

  const scoped_refptr<DtlsTransport> rtp_dtls_transport_;
  // The RTCP transport is const for all usages, except that it is cleared
  // when RTCP multiplexing is turned on; this happens on the network thread.

  const scoped_refptr<::webrtc::SctpTransport> sctp_transport_;

  RtcpMuxFilter rtcp_mux_negotiator_ RTC_GUARDED_BY(transport_sequence_);

  // This is invoked when RTCP-mux becomes active and
  // `rtcp_dtls_transport_` is destroyed. The JsepTransportController will
  // receive the callback and update the aggregate transport states.
  absl::AnyInvocable<void()> rtcp_mux_active_callback_;
};

}  //  namespace webrtc

#endif  // PC_JSEP_TRANSPORT_H_
