/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// A Transport manages a set of named channels of the same type.
//
// Subclasses choose the appropriate class to instantiate for each channel;
// however, this base class keeps track of the channels by name, watches their
// state changes (in order to update the manager's state), and forwards
// requests to begin connecting or to reset to each of the channels.
//
// On Threading:  Transport performs work solely on the network thread, and so
// its methods should only be called on the network thread.
//
// Note: Subclasses must call DestroyChannels() in their own destructors.
// It is not possible to do so here because the subclass destructor will
// already have run.

#ifndef WEBRTC_P2P_BASE_TRANSPORT_H_
#define WEBRTC_P2P_BASE_TRANSPORT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/p2pconstants.h"
#include "webrtc/p2p/base/sessiondescription.h"
#include "webrtc/p2p/base/transportinfo.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/rtccertificate.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslstreamadapter.h"

namespace cricket {

class PortAllocator;
class TransportChannel;
class TransportChannelImpl;

typedef std::vector<Candidate> Candidates;

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
  ConnectionInfo()
      : best_connection(false),
        writable(false),
        receiving(false),
        timeout(false),
        new_connection(false),
        rtt(0),
        sent_total_bytes(0),
        sent_bytes_second(0),
        sent_discarded_packets(0),
        sent_total_packets(0),
        sent_ping_requests_total(0),
        sent_ping_requests_before_first_response(0),
        sent_ping_responses(0),
        recv_total_bytes(0),
        recv_bytes_second(0),
        recv_ping_requests(0),
        recv_ping_responses(0),
        key(NULL) {}

  bool best_connection;        // Is this the best connection we have?
  bool writable;               // Has this connection received a STUN response?
  bool receiving;              // Has this connection received anything?
  bool timeout;                // Has this connection timed out?
  bool new_connection;         // Is this a newly created connection?
  size_t rtt;                  // The STUN RTT for this connection.
  size_t sent_total_bytes;     // Total bytes sent on this connection.
  size_t sent_bytes_second;    // Bps over the last measurement interval.
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
};

// Information about all the connections of a channel.
typedef std::vector<ConnectionInfo> ConnectionInfos;

// Information about a specific channel
struct TransportChannelStats {
  int component = 0;
  ConnectionInfos connection_infos;
  int srtp_crypto_suite = rtc::SRTP_INVALID_CRYPTO_SUITE;
  int ssl_cipher_suite = rtc::TLS_NULL_WITH_NULL_NULL;
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

class Transport : public sigslot::has_slots<> {
 public:
  Transport(const std::string& name, PortAllocator* allocator);
  virtual ~Transport();

  // Returns the name of this transport.
  const std::string& name() const { return name_; }

  // Returns the port allocator object for this transport.
  PortAllocator* port_allocator() { return allocator_; }

  bool ready_for_remote_candidates() const {
    return local_description_set_ && remote_description_set_;
  }

  void SetIceRole(IceRole role);
  IceRole ice_role() const { return ice_role_; }

  void SetIceTiebreaker(uint64_t IceTiebreaker) { tiebreaker_ = IceTiebreaker; }
  uint64_t IceTiebreaker() { return tiebreaker_; }

  void SetIceConfig(const IceConfig& config);

  // Must be called before applying local session description.
  virtual void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>&) {}

  // Get a copy of the local certificate provided by SetLocalCertificate.
  virtual bool GetLocalCertificate(
      rtc::scoped_refptr<rtc::RTCCertificate>*) {
    return false;
  }

  // Get a copy of the remote certificate in use by the specified channel.
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate();

  // Create, destroy, and lookup the channels of this type by their components.
  TransportChannelImpl* CreateChannel(int component);

  TransportChannelImpl* GetChannel(int component);

  bool HasChannel(int component) {
    return (NULL != GetChannel(component));
  }
  bool HasChannels();

  void DestroyChannel(int component);

  // Set the local TransportDescription to be used by TransportChannels.
  bool SetLocalTransportDescription(const TransportDescription& description,
                                    ContentAction action,
                                    std::string* error_desc);

  // Set the remote TransportDescription to be used by TransportChannels.
  bool SetRemoteTransportDescription(const TransportDescription& description,
                                     ContentAction action,
                                     std::string* error_desc);

  // Tells channels to start gathering candidates if necessary.
  // Should be called after ConnectChannels() has been called at least once,
  // which will happen in SetLocalTransportDescription.
  void MaybeStartGathering();

  // Resets all of the channels back to their initial state.  They are no
  // longer connecting.
  void ResetChannels();

  // Destroys every channel created so far.
  void DestroyAllChannels();

  bool GetStats(TransportStats* stats);

  // Called when one or more candidates are ready from the remote peer.
  bool AddRemoteCandidates(const std::vector<Candidate>& candidates,
                           std::string* error);
  bool RemoveRemoteCandidates(const std::vector<Candidate>& candidates,
                              std::string* error);

  virtual bool GetSslRole(rtc::SSLRole*) const { return false; }

  // Must be called before channel is starting to connect.
  virtual bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion) {
    return false;
  }

  // The current local transport description, for use by derived classes
  // when performing transport description negotiation, and possibly used
  // by the transport controller.
  const TransportDescription* local_description() const {
    return local_description_.get();
  }

  // The current remote transport description, for use by derived classes
  // when performing transport description negotiation, and possibly used
  // by the transport controller.
  const TransportDescription* remote_description() const {
    return remote_description_.get();
  }

 protected:
  // These are called by Create/DestroyChannel above in order to create or
  // destroy the appropriate type of channel.
  virtual TransportChannelImpl* CreateTransportChannel(int component) = 0;
  virtual void DestroyTransportChannel(TransportChannelImpl* channel) = 0;

  // Pushes down the transport parameters from the local description, such
  // as the ICE ufrag and pwd.
  // Derived classes can override, but must call the base as well.
  virtual bool ApplyLocalTransportDescription(TransportChannelImpl* channel,
                                              std::string* error_desc);

  // Pushes down remote ice credentials from the remote description to the
  // transport channel.
  virtual bool ApplyRemoteTransportDescription(TransportChannelImpl* ch,
                                               std::string* error_desc);

  // Negotiates the transport parameters based on the current local and remote
  // transport description, such as the ICE role to use, and whether DTLS
  // should be activated.
  // Derived classes can negotiate their specific parameters here, but must call
  // the base as well.
  virtual bool NegotiateTransportDescription(ContentAction local_role,
                                             std::string* error_desc);

  // Pushes down the transport parameters obtained via negotiation.
  // Derived classes can set their specific parameters here, but must call the
  // base as well.
  virtual bool ApplyNegotiatedTransportDescription(
      TransportChannelImpl* channel,
      std::string* error_desc);

  // Returns false if the certificate's identity does not match the fingerprint,
  // or either is NULL.
  virtual bool VerifyCertificateFingerprint(
      const rtc::RTCCertificate* certificate,
      const rtc::SSLFingerprint* fingerprint,
      std::string* error_desc) const;

  // Negotiates the SSL role based off the offer and answer as specified by
  // RFC 4145, section-4.1. Returns false if the SSL role cannot be determined
  // from the local description and remote description.
  virtual bool NegotiateRole(ContentAction local_role,
                             rtc::SSLRole* ssl_role,
                             std::string* error_desc) const;

 private:
  // If a candidate is not acceptable, returns false and sets error.
  // Call this before calling OnRemoteCandidates.
  bool VerifyCandidate(const Candidate& candidate, std::string* error);
  bool VerifyCandidates(const Candidates& candidates, std::string* error);

  // Candidate component => TransportChannelImpl*
  typedef std::map<int, TransportChannelImpl*> ChannelMap;

  // Helper function that invokes the given function on every channel.
  typedef void (TransportChannelImpl::* TransportChannelFunc)();
  void CallChannels(TransportChannelFunc func);

  const std::string name_;
  PortAllocator* const allocator_;
  bool channels_destroyed_ = false;
  IceRole ice_role_ = ICEROLE_UNKNOWN;
  uint64_t tiebreaker_ = 0;
  IceMode remote_ice_mode_ = ICEMODE_FULL;
  IceConfig ice_config_;
  std::unique_ptr<TransportDescription> local_description_;
  std::unique_ptr<TransportDescription> remote_description_;
  bool local_description_set_ = false;
  bool remote_description_set_ = false;

  ChannelMap channels_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Transport);
};


}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TRANSPORT_H_
