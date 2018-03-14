/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
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

#include "api/candidate.h"
#include "api/jsep.h"
#include "api/optional.h"
#include "p2p/base/dtlstransport.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/transportinfo.h"
#include "pc/sessiondescription.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/messagequeue.h"
#include "rtc_base/rtccertificate.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/sslstreamadapter.h"

namespace cricket {

class DtlsTransportInternal;

struct TransportChannelStats {
  TransportChannelStats();
  TransportChannelStats(const TransportChannelStats&);
  ~TransportChannelStats();

  int component = 0;
  ConnectionInfos connection_infos;
  int srtp_crypto_suite = rtc::SRTP_INVALID_CRYPTO_SUITE;
  int ssl_cipher_suite = rtc::TLS_NULL_WITH_NULL_NULL;
  DtlsTransportState dtls_state = DTLS_TRANSPORT_NEW;
};

// Information about all the channels of a transport.
// TODO(hta): Consider if a simple vector is as good as a map.
typedef std::vector<TransportChannelStats> TransportChannelStatsList;

// Information about the stats of a transport.
struct TransportStats {
  TransportStats();
  ~TransportStats();

  std::string transport_name;
  TransportChannelStatsList channel_stats;
};

bool BadTransportDescription(const std::string& desc, std::string* err_desc);

// Helper class used by TransportController that processes
// TransportDescriptions. A TransportDescription represents the
// transport-specific properties of an SDP m= section, processed according to
// JSEP. Each transport consists of DTLS and ICE transport channels for RTP
// (and possibly RTCP, if rtcp-mux isn't used).
//
// On Threading:  Transport performs work solely on the network thread, and so
// its methods should only be called on the network thread.
class JsepTransport : public sigslot::has_slots<> {
 public:
  // |mid| is just used for log statements in order to identify the Transport.
  // Note that |certificate| is allowed to be null since a remote description
  // may be set before a local certificate is generated.
  JsepTransport(const std::string& mid,
                const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  ~JsepTransport() override;

  // Returns the MID of this transport.
  const std::string& mid() const { return mid_; }

  // Add or remove channel that is affected when a local/remote transport
  // description is set on this transport. Need to add all channels before
  // setting a transport description.
  bool AddChannel(DtlsTransportInternal* dtls, int component);
  bool RemoveChannel(int component);
  bool HasChannels() const;

  bool ready_for_remote_candidates() const {
    return local_description_set_ && remote_description_set_;
  }

  // Must be called before applying local session description.
  // Needed in order to verify the local fingerprint.
  void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);

  // Get a copy of the local certificate provided by SetLocalCertificate.
  bool GetLocalCertificate(
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const;

  // Set the local TransportDescription to be used by DTLS and ICE channels
  // that are part of this Transport.
  bool SetLocalTransportDescription(const TransportDescription& description,
                                    webrtc::SdpType type,
                                    std::string* error_desc);

  // Set the remote TransportDescription to be used by DTLS and ICE channels
  // that are part of this Transport.
  bool SetRemoteTransportDescription(const TransportDescription& description,
                                     webrtc::SdpType type,
                                     std::string* error_desc);

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
  bool NeedsIceRestart() const;

  // Returns role if negotiated, or empty Optional if it hasn't been negotiated
  // yet.
  rtc::Optional<rtc::SSLRole> GetSslRole() const;

  // TODO(deadbeef): Make this const. See comment in transportcontroller.h.
  bool GetStats(TransportStats* stats);

  // The current local transport description, possibly used
  // by the transport controller.
  const TransportDescription* local_description() const {
    return local_description_.get();
  }

  // The current remote transport description, possibly used
  // by the transport controller.
  const TransportDescription* remote_description() const {
    return remote_description_.get();
  }

  // TODO(deadbeef): The methods below are only public for testing. Should make
  // them utility functions or objects so they can be tested independently from
  // this class.

  // Returns false if the certificate's identity does not match the fingerprint,
  // or either is NULL.
  bool VerifyCertificateFingerprint(const rtc::RTCCertificate* certificate,
                                    const rtc::SSLFingerprint* fingerprint,
                                    std::string* error_desc) const;

 private:
  // Negotiates the transport parameters based on the current local and remote
  // transport description, such as the ICE role to use, and whether DTLS
  // should be activated.
  //
  // Called when an answer TransportDescription is applied.
  bool NegotiateTransportDescription(webrtc::SdpType local_description_type,
                                     std::string* error_desc);

  // Negotiates the SSL role based off the offer and answer as specified by
  // RFC 4145, section-4.1. Returns false if the SSL role cannot be determined
  // from the local description and remote description.
  bool NegotiateRole(webrtc::SdpType local_description_type,
                     std::string* error_desc);

  // Pushes down the transport parameters from the local description, such
  // as the ICE ufrag and pwd.
  bool ApplyLocalTransportDescription(DtlsTransportInternal* dtls_transport,
                                      std::string* error_desc);

  // Pushes down the transport parameters from the remote description to the
  // transport channel.
  bool ApplyRemoteTransportDescription(DtlsTransportInternal* dtls_transport,
                                       std::string* error_desc);

  // Pushes down the transport parameters obtained via negotiation.
  bool ApplyNegotiatedTransportDescription(
      DtlsTransportInternal* dtls_transport,
      std::string* error_desc);

  const std::string mid_;
  // needs-ice-restart bit as described in JSEP.
  bool needs_ice_restart_ = false;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  rtc::Optional<rtc::SSLRole> ssl_role_;
  std::unique_ptr<rtc::SSLFingerprint> remote_fingerprint_;
  std::unique_ptr<TransportDescription> local_description_;
  std::unique_ptr<TransportDescription> remote_description_;
  bool local_description_set_ = false;
  bool remote_description_set_ = false;

  // Candidate component => DTLS channel
  std::map<int, DtlsTransportInternal*> channels_;

  RTC_DISALLOW_COPY_AND_ASSIGN(JsepTransport);
};

}  // namespace cricket

#endif  // PC_JSEPTRANSPORT_H_
