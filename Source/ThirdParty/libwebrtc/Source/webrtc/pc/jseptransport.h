/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_JSEPTRANSPORT_H_
#define PC_JSEPTRANSPORT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/jsep.h"
#include "api/media_transport_interface.h"
#include "p2p/base/dtlstransport.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/transportinfo.h"
#include "pc/dtlssrtptransport.h"
#include "pc/rtcpmuxfilter.h"
#include "pc/rtptransport.h"
#include "pc/sessiondescription.h"
#include "pc/srtpfilter.h"
#include "pc/srtptransport.h"
#include "pc/transportstats.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/messagequeue.h"
#include "rtc_base/rtccertificate.h"
#include "rtc_base/sslstreamadapter.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace cricket {

class DtlsTransportInternal;

struct JsepTransportDescription {
 public:
  JsepTransportDescription();
  JsepTransportDescription(
      bool rtcp_mux_enabled,
      const std::vector<CryptoParams>& cryptos,
      const std::vector<int>& encrypted_header_extension_ids,
      int rtp_abs_sendtime_extn_id,
      const TransportDescription& transport_description);
  JsepTransportDescription(const JsepTransportDescription& from);
  ~JsepTransportDescription();

  JsepTransportDescription& operator=(const JsepTransportDescription& from);

  bool rtcp_mux_enabled = true;
  std::vector<CryptoParams> cryptos;
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
class JsepTransport : public sigslot::has_slots<>,
                      public webrtc::MediaTransportStateCallback {
 public:
  // |mid| is just used for log statements in order to identify the Transport.
  // Note that |local_certificate| is allowed to be null since a remote
  // description may be set before a local certificate is generated.
  //
  // |media_trasport| is optional (experimental). If available it will be used
  // to send / receive encoded audio and video frames instead of RTP.
  // Currently |media_transport| can co-exist with RTP / RTCP transports.
  JsepTransport(
      const std::string& mid,
      const rtc::scoped_refptr<rtc::RTCCertificate>& local_certificate,
      std::unique_ptr<webrtc::RtpTransport> unencrypted_rtp_transport,
      std::unique_ptr<webrtc::SrtpTransport> sdes_transport,
      std::unique_ptr<webrtc::DtlsSrtpTransport> dtls_srtp_transport,
      std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport,
      std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport,
      std::unique_ptr<webrtc::MediaTransportInterface> media_transport);

  ~JsepTransport() override;

  // Returns the MID of this transport. This is only used for logging.
  const std::string& mid() const { return mid_; }

  // Must be called before applying local session description.
  // Needed in order to verify the local fingerprint.
  void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& local_certificate) {
    local_certificate_ = local_certificate;
  }

  // Return the local certificate provided by SetLocalCertificate.
  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const {
    return local_certificate_;
  }

  webrtc::RTCError SetLocalJsepTransportDescription(
      const JsepTransportDescription& jsep_description,
      webrtc::SdpType type);

  // Set the remote TransportDescription to be used by DTLS and ICE channels
  // that are part of this Transport.
  webrtc::RTCError SetRemoteJsepTransportDescription(
      const JsepTransportDescription& jsep_description,
      webrtc::SdpType type);

  webrtc::RTCError AddRemoteCandidates(const Candidates& candidates);

  // Set the "needs-ice-restart" flag as described in JSEP. After the flag is
  // set, offers should generate new ufrags/passwords until an ICE restart
  // occurs.
  //
  // This and the below method can be called safely from any thread as long as
  // SetXTransportDescription is not in progress.
  void SetNeedsIceRestartFlag();
  // Returns true if the ICE restart flag above was set, and no ICE restart has
  // occurred yet for this transport (by applying a local description with
  // changed ufrag/password).
  bool needs_ice_restart() const { return needs_ice_restart_; }

  // Returns role if negotiated, or empty absl::optional if it hasn't been
  // negotiated yet.
  absl::optional<rtc::SSLRole> GetDtlsRole() const;

  // TODO(deadbeef): Make this const. See comment in transportcontroller.h.
  bool GetStats(TransportStats* stats);

  const JsepTransportDescription* local_description() const {
    return local_description_.get();
  }

  const JsepTransportDescription* remote_description() const {
    return remote_description_.get();
  }

  webrtc::RtpTransportInternal* rtp_transport() const {
    if (dtls_srtp_transport_) {
      return dtls_srtp_transport_.get();
    } else if (sdes_transport_) {
      return sdes_transport_.get();
    } else {
      return unencrypted_rtp_transport_.get();
    }
  }

  DtlsTransportInternal* rtp_dtls_transport() const {
    return rtp_dtls_transport_.get();
  }

  DtlsTransportInternal* rtcp_dtls_transport() const {
    return rtcp_dtls_transport_.get();
  }

  // Returns media transport, if available.
  // Note that media transport is owned by jseptransport and the pointer
  // to media transport will becomes invalid after destruction of jseptransport.
  webrtc::MediaTransportInterface* media_transport() const {
    return media_transport_.get();
  }

  // Returns the latest media transport state.
  webrtc::MediaTransportState media_transport_state() const {
    return media_transport_state_;
  }

  // This is signaled when RTCP-mux becomes active and
  // |rtcp_dtls_transport_| is destroyed. The JsepTransportController will
  // handle the signal and update the aggregate transport states.
  sigslot::signal<> SignalRtcpMuxActive;

  // This is signaled for changes in |media_transport_| state.
  sigslot::signal<> SignalMediaTransportStateChanged;

  // TODO(deadbeef): The methods below are only public for testing. Should make
  // them utility functions or objects so they can be tested independently from
  // this class.

  // Returns an error if the certificate's identity does not match the
  // fingerprint, or either is NULL.
  webrtc::RTCError VerifyCertificateFingerprint(
      const rtc::RTCCertificate* certificate,
      const rtc::SSLFingerprint* fingerprint) const;

  void SetActiveResetSrtpParams(bool active_reset_srtp_params);

 private:
  bool SetRtcpMux(bool enable, webrtc::SdpType type, ContentSource source);

  void ActivateRtcpMux();

  bool SetSdes(const std::vector<CryptoParams>& cryptos,
               const std::vector<int>& encrypted_extension_ids,
               webrtc::SdpType type,
               ContentSource source);

  // Negotiates and sets the DTLS parameters based on the current local and
  // remote transport description, such as the DTLS role to use, and whether
  // DTLS should be activated.
  //
  // Called when an answer TransportDescription is applied.
  webrtc::RTCError NegotiateAndSetDtlsParameters(
      webrtc::SdpType local_description_type);

  // Negotiates the DTLS role based off the offer and answer as specified by
  // RFC 4145, section-4.1. Returns an RTCError if role cannot be determined
  // from the local description and remote description.
  webrtc::RTCError NegotiateDtlsRole(
      webrtc::SdpType local_description_type,
      ConnectionRole local_connection_role,
      ConnectionRole remote_connection_role,
      absl::optional<rtc::SSLRole>* negotiated_dtls_role);

  // Pushes down the ICE parameters from the local description, such
  // as the ICE ufrag and pwd.
  void SetLocalIceParameters(IceTransportInternal* ice);

  // Pushes down the ICE parameters from the remote description.
  void SetRemoteIceParameters(IceTransportInternal* ice);

  // Pushes down the DTLS parameters obtained via negotiation.
  webrtc::RTCError SetNegotiatedDtlsParameters(
      DtlsTransportInternal* dtls_transport,
      absl::optional<rtc::SSLRole> dtls_role,
      rtc::SSLFingerprint* remote_fingerprint);

  bool GetTransportStats(DtlsTransportInternal* dtls_transport,
                         TransportStats* stats);

  // Invoked whenever the state of the media transport changes.
  void OnStateChanged(webrtc::MediaTransportState state) override;

  const std::string mid_;
  // needs-ice-restart bit as described in JSEP.
  bool needs_ice_restart_ = false;
  rtc::scoped_refptr<rtc::RTCCertificate> local_certificate_;
  std::unique_ptr<JsepTransportDescription> local_description_;
  std::unique_ptr<JsepTransportDescription> remote_description_;

  // To avoid downcasting and make it type safe, keep three unique pointers for
  // different SRTP mode and only one of these is non-nullptr.
  std::unique_ptr<webrtc::RtpTransport> unencrypted_rtp_transport_;
  std::unique_ptr<webrtc::SrtpTransport> sdes_transport_;
  std::unique_ptr<webrtc::DtlsSrtpTransport> dtls_srtp_transport_;

  std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport_;
  std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport_;

  SrtpFilter sdes_negotiator_;
  RtcpMuxFilter rtcp_mux_negotiator_;

  // Cache the encrypted header extension IDs for SDES negoitation.
  absl::optional<std::vector<int>> send_extension_ids_;
  absl::optional<std::vector<int>> recv_extension_ids_;

  // Optional media transport (experimental).
  std::unique_ptr<webrtc::MediaTransportInterface> media_transport_;

  // If |media_transport_| is provided, this variable represents the state of
  // media transport.
  webrtc::MediaTransportState media_transport_state_ =
      webrtc::MediaTransportState::kPending;

  RTC_DISALLOW_COPY_AND_ASSIGN(JsepTransport);
};

}  // namespace cricket

#endif  // PC_JSEPTRANSPORT_H_
