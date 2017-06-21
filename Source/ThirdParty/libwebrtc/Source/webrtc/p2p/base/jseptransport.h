/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_JSEPTRANSPORT_H_
#define WEBRTC_P2P_BASE_JSEPTRANSPORT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/rtccertificate.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/p2pconstants.h"
#include "webrtc/p2p/base/sessiondescription.h"
#include "webrtc/p2p/base/transportinfo.h"

namespace cricket {

class DtlsTransportInternal;
enum class IceCandidatePairState;

typedef std::vector<Candidate> Candidates;

// TODO(deadbeef): Move all of these enums, POD types and utility methods to
// another header file.

// TODO(deadbeef): Unify with PeerConnectionInterface::IceConnectionState
// once /talk/ and /webrtc/ are combined, and also switch to ENUM_NAME naming
// style.
enum IceConnectionState {
  kIceConnectionConnecting = 0,
  kIceConnectionFailed,
  kIceConnectionConnected,  // Writable, but still checking one or more
                            // connections
  kIceConnectionCompleted,
};

enum DtlsTransportState {
  // Haven't started negotiating.
  DTLS_TRANSPORT_NEW = 0,
  // Have started negotiating.
  DTLS_TRANSPORT_CONNECTING,
  // Negotiated, and has a secure connection.
  DTLS_TRANSPORT_CONNECTED,
  // Transport is closed.
  DTLS_TRANSPORT_CLOSED,
  // Failed due to some error in the handshake process.
  DTLS_TRANSPORT_FAILED,
};

// TODO(deadbeef): Unify with PeerConnectionInterface::IceConnectionState
// once /talk/ and /webrtc/ are combined, and also switch to ENUM_NAME naming
// style.
enum IceGatheringState {
  kIceGatheringNew = 0,
  kIceGatheringGathering,
  kIceGatheringComplete,
};

enum ContinualGatheringPolicy {
  // All port allocator sessions will stop after a writable connection is found.
  GATHER_ONCE = 0,
  // The most recent port allocator session will keep on running.
  GATHER_CONTINUALLY,
  // The most recent port allocator session will keep on running, and it will
  // try to recover connectivity if the channel becomes disconnected.
  GATHER_CONTINUALLY_AND_RECOVER,
};

// Stats that we can return about the connections for a transport channel.
// TODO(hta): Rename to ConnectionStats
struct ConnectionInfo {
  ConnectionInfo();

  bool best_connection;      // Is this the best connection we have?
  bool writable;             // Has this connection received a STUN response?
  bool receiving;            // Has this connection received anything?
  bool timeout;              // Has this connection timed out?
  bool new_connection;       // Is this a newly created connection?
  size_t rtt;                // The STUN RTT for this connection.
  size_t sent_total_bytes;   // Total bytes sent on this connection.
  size_t sent_bytes_second;  // Bps over the last measurement interval.
  size_t sent_discarded_packets;  // Number of outgoing packets discarded due to
                                  // socket errors.
  size_t sent_total_packets;  // Number of total outgoing packets attempted for
                              // sending.
  size_t sent_ping_requests_total;  // Number of STUN ping request sent.
  size_t sent_ping_requests_before_first_response;  // Number of STUN ping
  // sent before receiving the first response.
  size_t sent_ping_responses;  // Number of STUN ping response sent.

  size_t recv_total_bytes;     // Total bytes received on this connection.
  size_t recv_bytes_second;    // Bps over the last measurement interval.
  size_t recv_ping_requests;   // Number of STUN ping request received.
  size_t recv_ping_responses;  // Number of STUN ping response received.
  Candidate local_candidate;   // The local candidate for this connection.
  Candidate remote_candidate;  // The remote candidate for this connection.
  void* key;                   // A static value that identifies this conn.
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-state
  IceCandidatePairState state;
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-priority
  uint64_t priority;
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-nominated
  bool nominated;
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-totalroundtriptime
  uint64_t total_round_trip_time_ms;
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-currentroundtriptime
  rtc::Optional<uint32_t> current_round_trip_time_ms;
};

// Information about all the connections of a channel.
typedef std::vector<ConnectionInfo> ConnectionInfos;

// Information about a specific channel
struct TransportChannelStats {
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
  std::string transport_name;
  TransportChannelStatsList channel_stats;
};

// ICE Nomination mode.
enum class NominationMode {
  REGULAR,         // Nominate once per ICE restart (Not implemented yet).
  AGGRESSIVE,      // Nominate every connection except that it will behave as if
                   // REGULAR when the remote is an ICE-LITE endpoint.
  SEMI_AGGRESSIVE  // Our current implementation of the nomination algorithm.
                   // The details are described in P2PTransportChannel.
};

// Information about ICE configuration.
// TODO(deadbeef): Use rtc::Optional to represent unset values, instead of
// -1.
struct IceConfig {
  // The ICE connection receiving timeout value in milliseconds.
  int receiving_timeout = -1;
  // Time interval in milliseconds to ping a backup connection when the ICE
  // channel is strongly connected.
  int backup_connection_ping_interval = -1;

  ContinualGatheringPolicy continual_gathering_policy = GATHER_ONCE;

  bool gather_continually() const {
    return continual_gathering_policy == GATHER_CONTINUALLY ||
           continual_gathering_policy == GATHER_CONTINUALLY_AND_RECOVER;
  }

  // Whether we should prioritize Relay/Relay candidate when nothing
  // is writable yet.
  bool prioritize_most_likely_candidate_pairs = false;

  // Writable connections are pinged at a slower rate once stablized.
  int stable_writable_connection_ping_interval = -1;

  // If set to true, this means the ICE transport should presume TURN-to-TURN
  // candidate pairs will succeed, even before a binding response is received.
  bool presume_writable_when_fully_relayed = false;

  // Interval to check on all networks and to perform ICE regathering on any
  // active network having no connection on it.
  rtc::Optional<int> regather_on_failed_networks_interval;

  // The time period in which we will not switch the selected connection
  // when a new connection becomes receiving but the selected connection is not
  // in case that the selected connection may become receiving soon.
  rtc::Optional<int> receiving_switching_delay;

  // TODO(honghaiz): Change the default to regular nomination.
  // Default nomination mode if the remote does not support renomination.
  NominationMode default_nomination_mode = NominationMode::SEMI_AGGRESSIVE;

  // ICE checks (STUN pings) will not be sent at higher rate (lower interval)
  // than this, no matter what other settings there are.
  // Measure in milliseconds.
  rtc::Optional<int> ice_check_min_interval;

  IceConfig() {}
  IceConfig(int receiving_timeout_ms,
            int backup_connection_ping_interval,
            ContinualGatheringPolicy gathering_policy,
            bool prioritize_most_likely_candidate_pairs,
            int stable_writable_connection_ping_interval_ms,
            bool presume_writable_when_fully_relayed,
            int regather_on_failed_networks_interval_ms,
            int receiving_switching_delay_ms)
      : receiving_timeout(receiving_timeout_ms),
        backup_connection_ping_interval(backup_connection_ping_interval),
        continual_gathering_policy(gathering_policy),
        prioritize_most_likely_candidate_pairs(
            prioritize_most_likely_candidate_pairs),
        stable_writable_connection_ping_interval(
            stable_writable_connection_ping_interval_ms),
        presume_writable_when_fully_relayed(
            presume_writable_when_fully_relayed),
        regather_on_failed_networks_interval(
            regather_on_failed_networks_interval_ms),
        receiving_switching_delay(receiving_switching_delay_ms) {}
};

bool BadTransportDescription(const std::string& desc, std::string* err_desc);

bool IceCredentialsChanged(const std::string& old_ufrag,
                           const std::string& old_pwd,
                           const std::string& new_ufrag,
                           const std::string& new_pwd);

// If a candidate is not acceptable, returns false and sets error.
bool VerifyCandidate(const Candidate& candidate, std::string* error);
bool VerifyCandidates(const Candidates& candidates, std::string* error);

// Helper class used by TransportController that processes
// TransportDescriptions. A TransportDescription represents the
// transport-specific properties of an SDP m= section, processed according to
// JSEP. Each transport consists of DTLS and ICE transport channels for RTP
// (and possibly RTCP, if rtcp-mux isn't used).
//
// On Threading:  Transport performs work solely on the network thread, and so
// its methods should only be called on the network thread.
//
// TODO(deadbeef): Move this into /pc/ and out of /p2p/base/, since it's
// PeerConnection-specific.
class JsepTransport : public sigslot::has_slots<> {
 public:
  // |mid| is just used for log statements in order to identify the Transport.
  // Note that |certificate| is allowed to be null since a remote description
  // may be set before a local certificate is generated.
  JsepTransport(const std::string& mid,
                const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);

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
                                    ContentAction action,
                                    std::string* error_desc);

  // Set the remote TransportDescription to be used by DTLS and ICE channels
  // that are part of this Transport.
  bool SetRemoteTransportDescription(const TransportDescription& description,
                                     ContentAction action,
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
  bool NegotiateTransportDescription(ContentAction local_description_type,
                                     std::string* error_desc);

  // Negotiates the SSL role based off the offer and answer as specified by
  // RFC 4145, section-4.1. Returns false if the SSL role cannot be determined
  // from the local description and remote description.
  bool NegotiateRole(ContentAction local_description_type,
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

#endif  // WEBRTC_P2P_BASE_JSEPTRANSPORT_H_
