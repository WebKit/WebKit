/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// P2PTransportChannel wraps up the state management of the connection between
// two P2P clients.  Clients have candidate ports for connecting, and
// connections which are combinations of candidates from each end (Alice and
// Bob each have candidates, one candidate from Alice and one candidate from
// Bob are used to make a connection, repeat to make many connections).
//
// When all of the available connections become invalid (non-writable), we
// kick off a process of determining more candidates and more connections.
//
#ifndef P2P_BASE_P2PTRANSPORTCHANNEL_H_
#define P2P_BASE_P2PTRANSPORTCHANNEL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/candidate.h"
#include "p2p/base/candidatepairinterface.h"
#include "p2p/base/icetransportinternal.h"
#include "p2p/base/portallocator.h"
#include "p2p/base/portinterface.h"
#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/random.h"
#include "rtc_base/sigslot.h"

namespace cricket {

// Enum for UMA metrics, used to record whether the channel is
// connected/connecting/disconnected when ICE restart happens.
enum class IceRestartState { CONNECTING, CONNECTED, DISCONNECTED, MAX_VALUE };

extern const int WEAK_PING_INTERVAL;
extern const int STRONG_PING_INTERVAL;
extern const int WEAK_OR_STABILIZING_WRITABLE_CONNECTION_PING_INTERVAL;
extern const int STRONG_AND_STABLE_WRITABLE_CONNECTION_PING_INTERVAL;
static const int MIN_PINGS_AT_WEAK_PING_INTERVAL = 3;

bool IceCredentialsChanged(const std::string& old_ufrag,
                           const std::string& old_pwd,
                           const std::string& new_ufrag,
                           const std::string& new_pwd);

// Adds the port on which the candidate originated.
class RemoteCandidate : public Candidate {
 public:
  RemoteCandidate(const Candidate& c, PortInterface* origin_port)
      : Candidate(c), origin_port_(origin_port) {}

  PortInterface* origin_port() { return origin_port_; }

 private:
  PortInterface* origin_port_;
};

// P2PTransportChannel manages the candidates and connection process to keep
// two P2P clients connected to each other.
class P2PTransportChannel : public IceTransportInternal,
                            public rtc::MessageHandler {
 public:
  P2PTransportChannel(const std::string& transport_name,
                      int component,
                      PortAllocator* allocator);
  ~P2PTransportChannel() override;

  // From TransportChannelImpl:
  IceTransportState GetState() const override;
  const std::string& transport_name() const override;
  int component() const override;
  bool writable() const override;
  bool receiving() const override;
  void SetIceRole(IceRole role) override;
  IceRole GetIceRole() const override;
  void SetIceTiebreaker(uint64_t tiebreaker) override;
  void SetIceParameters(const IceParameters& ice_params) override;
  void SetRemoteIceParameters(const IceParameters& ice_params) override;
  void SetRemoteIceMode(IceMode mode) override;
  // TODO(deadbeef): Deprecated. Remove when Chromium's
  // IceTransportChannel does not depend on this.
  void Connect() {}
  void MaybeStartGathering() override;
  IceGatheringState gathering_state() const override;
  void AddRemoteCandidate(const Candidate& candidate) override;
  void RemoveRemoteCandidate(const Candidate& candidate) override;
  // Sets the parameters in IceConfig. We do not set them blindly. Instead, we
  // only update the parameter if it is considered set in |config|. For example,
  // a negative value of receiving_timeout will be considered "not set" and we
  // will not use it to update the respective parameter in |config_|.
  // TODO(deadbeef): Use rtc::Optional instead of negative values.
  void SetIceConfig(const IceConfig& config) override;
  const IceConfig& config() const;
  void SetMetricsObserver(webrtc::MetricsObserverInterface* observer) override;

  // From TransportChannel:
  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  bool GetOption(rtc::Socket::Option opt, int* value) override;
  int GetError() override;
  bool GetStats(std::vector<ConnectionInfo>* stats) override;
  rtc::Optional<int> GetRttEstimate() override;

  // TODO(honghaiz): Remove this method once the reference of it in
  // Chromoting is removed.
  const Connection* best_connection() const { return selected_connection_; }

  const Connection* selected_connection() const { return selected_connection_; }
  void set_incoming_only(bool value) { incoming_only_ = value; }

  // Note: These are only for testing purpose.
  // |ports_| and |pruned_ports| should not be changed from outside.
  const std::vector<PortInterface*>& ports() { return ports_; }
  const std::vector<PortInterface*>& pruned_ports() { return pruned_ports_; }

  IceMode remote_ice_mode() const { return remote_ice_mode_; }

  void PruneAllPorts();
  int receiving_timeout() const { return config_.receiving_timeout; }
  int check_receiving_interval() const { return check_receiving_interval_; }

  rtc::Optional<rtc::NetworkRoute> network_route() const override;

  // Helper method used only in unittest.
  rtc::DiffServCodePoint DefaultDscpValue() const;

  // Public for unit tests.
  Connection* FindNextPingableConnection();
  void MarkConnectionPinged(Connection* conn);

  // Public for unit tests.
  const std::vector<Connection*>& connections() const { return connections_; }

  // Public for unit tests.
  PortAllocatorSession* allocator_session() {
    return allocator_sessions_.back().get();
  }

  // Public for unit tests.
  const std::vector<RemoteCandidate>& remote_candidates() const {
    return remote_candidates_;
  }

  std::string ToString() const {
    const char RECEIVING_ABBREV[2] = {'_', 'R'};
    const char WRITABLE_ABBREV[2] = {'_', 'W'};
    std::stringstream ss;
    ss << "Channel[" << transport_name_ << "|" << component_ << "|"
       << RECEIVING_ABBREV[receiving_] << WRITABLE_ABBREV[writable_] << "]";
    return ss.str();
  }

 private:
  rtc::Thread* thread() const { return network_thread_; }
  bool IsGettingPorts() { return allocator_session()->IsGettingPorts(); }

  // A transport channel is weak if the current best connection is either
  // not receiving or not writable, or if there is no best connection at all.
  bool weak() const;

  int weak_ping_interval() const {
    if (config_.ice_check_min_interval &&
        weak_ping_interval_ < *config_.ice_check_min_interval) {
      return *config_.ice_check_min_interval;
    }
    return weak_ping_interval_;
  }

  int strong_ping_interval() const {
    if (config_.ice_check_min_interval &&
        STRONG_PING_INTERVAL < *config_.ice_check_min_interval) {
      return *config_.ice_check_min_interval;
    }
    return STRONG_PING_INTERVAL;
  }

  // Returns true if it's possible to send packets on |connection|.
  bool ReadyToSend(Connection* connection) const;
  void UpdateConnectionStates();
  void RequestSortAndStateUpdate();
  // Start pinging if we haven't already started, and we now have a connection
  // that's pingable.
  void MaybeStartPinging();

  // The methods below return a positive value if |a| is preferable to |b|,
  // a negative value if |b| is preferable, and 0 if they're equally preferable.
  // If |receiving_unchanged_threshold| is set, then when |b| is receiving and
  // |a| is not, returns a negative value only if |b| has been in receiving
  // state and |a| has been in not receiving state since
  // |receiving_unchanged_threshold| and sets
  // |missed_receiving_unchanged_threshold| to true otherwise.
  int CompareConnectionStates(
      const cricket::Connection* a,
      const cricket::Connection* b,
      rtc::Optional<int64_t> receiving_unchanged_threshold,
      bool* missed_receiving_unchanged_threshold) const;
  int CompareConnectionCandidates(const cricket::Connection* a,
                                  const cricket::Connection* b) const;
  // Compares two connections based on the connection states
  // (writable/receiving/connected), nomination states, last data received time,
  // and static preferences. Does not include latency. Used by both sorting
  // and ShouldSwitchSelectedConnection().
  // Returns a positive value if |a| is better than |b|.
  int CompareConnections(const cricket::Connection* a,
                         const cricket::Connection* b,
                         rtc::Optional<int64_t> receiving_unchanged_threshold,
                         bool* missed_receiving_unchanged_threshold) const;

  bool PresumedWritable(const cricket::Connection* conn) const;

  void SortConnectionsAndUpdateState();
  void SwitchSelectedConnection(Connection* conn);
  void UpdateState();
  void HandleAllTimedOut();
  void MaybeStopPortAllocatorSessions();
  IceTransportState ComputeState() const;

  Connection* GetBestConnectionOnNetwork(rtc::Network* network) const;
  bool CreateConnections(const Candidate& remote_candidate,
                         PortInterface* origin_port);
  bool CreateConnection(PortInterface* port,
                        const Candidate& remote_candidate,
                        PortInterface* origin_port);
  bool FindConnection(cricket::Connection* connection) const;

  uint32_t GetRemoteCandidateGeneration(const Candidate& candidate);
  bool IsDuplicateRemoteCandidate(const Candidate& candidate);
  void RememberRemoteCandidate(const Candidate& remote_candidate,
                               PortInterface* origin_port);
  bool IsPingable(const Connection* conn, int64_t now) const;
  // Whether a writable connection is past its ping interval and needs to be
  // pinged again.
  bool WritableConnectionPastPingInterval(const Connection* conn,
                                          int64_t now) const;
  int CalculateActiveWritablePingInterval(const Connection* conn,
                                          int64_t now) const;
  void PingConnection(Connection* conn);
  void AddAllocatorSession(std::unique_ptr<PortAllocatorSession> session);
  void AddConnection(Connection* connection);

  void OnPortReady(PortAllocatorSession *session, PortInterface* port);
  void OnPortsPruned(PortAllocatorSession* session,
                     const std::vector<PortInterface*>& ports);
  void OnCandidatesReady(PortAllocatorSession *session,
                         const std::vector<Candidate>& candidates);
  void OnCandidatesRemoved(PortAllocatorSession* session,
                           const std::vector<Candidate>& candidates);
  void OnCandidatesAllocationDone(PortAllocatorSession* session);
  void OnUnknownAddress(PortInterface* port,
                        const rtc::SocketAddress& addr,
                        ProtocolType proto,
                        IceMessage* stun_msg,
                        const std::string& remote_username,
                        bool port_muxed);

  // When a port is destroyed, remove it from both lists |ports_|
  // and |pruned_ports_|.
  void OnPortDestroyed(PortInterface* port);
  // When pruning a port, move it from |ports_| to |pruned_ports_|.
  // Returns true if the port is found and removed from |ports_|.
  bool PrunePort(PortInterface* port);
  void OnRoleConflict(PortInterface* port);

  void OnConnectionStateChange(Connection* connection);
  void OnReadPacket(Connection *connection, const char *data, size_t len,
                    const rtc::PacketTime& packet_time);
  void OnSentPacket(const rtc::SentPacket& sent_packet);
  void OnReadyToSend(Connection* connection);
  void OnConnectionDestroyed(Connection *connection);

  void OnNominated(Connection* conn);

  void OnMessage(rtc::Message* pmsg) override;
  void OnCheckAndPing();
  void OnRegatherOnFailedNetworks();
  void OnRegatherOnAllNetworks();

  uint32_t GetNominationAttr(Connection* conn) const;
  bool GetUseCandidateAttr(Connection* conn, NominationMode mode) const;

  // Returns true if we should switch to the new connection.
  // sets |missed_receiving_unchanged_threshold| to true if either
  // the selected connection or the new connection missed its
  // receiving-unchanged-threshold.
  bool ShouldSwitchSelectedConnection(
      Connection* new_connection,
      bool* missed_receiving_unchanged_threshold) const;
  // Returns true if the new_connection is selected for transmission.
  bool MaybeSwitchSelectedConnection(Connection* new_connection,
                                     const std::string& reason);
  // Gets the best connection for each network.
  std::map<rtc::Network*, Connection*> GetBestConnectionByNetwork() const;
  std::vector<Connection*> GetBestWritableConnectionPerNetwork() const;
  void PruneConnections();
  bool IsBackupConnection(const Connection* conn) const;

  Connection* FindOldestConnectionNeedingTriggeredCheck(int64_t now);
  // Between |conn1| and |conn2|, this function returns the one which should
  // be pinged first.
  Connection* MorePingable(Connection* conn1, Connection* conn2);
  // Select the connection which is Relay/Relay. If both of them are,
  // UDP relay protocol takes precedence.
  Connection* MostLikelyToWork(Connection* conn1, Connection* conn2);
  // Compare the last_ping_sent time and return the one least recently pinged.
  Connection* LeastRecentlyPinged(Connection* conn1, Connection* conn2);

  // Returns the latest remote ICE parameters or nullptr if there are no remote
  // ICE parameters yet.
  IceParameters* remote_ice() {
    return remote_ice_parameters_.empty() ? nullptr
                                          : &remote_ice_parameters_.back();
  }
  // Returns the remote IceParameters and generation that match |ufrag|
  // if found, and returns nullptr otherwise.
  const IceParameters* FindRemoteIceFromUfrag(const std::string& ufrag,
                                              uint32_t* generation);
  // Returns the index of the latest remote ICE parameters, or 0 if no remote
  // ICE parameters have been received.
  uint32_t remote_ice_generation() {
    return remote_ice_parameters_.empty()
               ? 0
               : static_cast<uint32_t>(remote_ice_parameters_.size() - 1);
  }

  // Samples a delay from the uniform distribution defined by the
  // regather_on_all_networks_interval ICE configuration pair.
  int SampleRegatherAllNetworksInterval();

  // Indicates if the given local port has been pruned.
  bool IsPortPruned(const Port* port) const;

  // Indicates if the given remote candidate has been pruned.
  bool IsRemoteCandidatePruned(const Candidate& cand) const;

  // Sets the writable state, signaling if necessary.
  void set_writable(bool writable);
  // Sets the receiving state, signaling if necessary.
  void set_receiving(bool receiving);

  std::string transport_name_;
  int component_;
  PortAllocator* allocator_;
  rtc::Thread* network_thread_;
  bool incoming_only_;
  int error_;
  std::vector<std::unique_ptr<PortAllocatorSession>> allocator_sessions_;
  // |ports_| contains ports that are used to form new connections when
  // new remote candidates are added.
  std::vector<PortInterface*> ports_;
  // |pruned_ports_| contains ports that have been removed from |ports_| and
  // are not being used to form new connections, but that aren't yet destroyed.
  // They may have existing connections, and they still fire signals such as
  // SignalUnknownAddress.
  std::vector<PortInterface*> pruned_ports_;

  // |connections_| is a sorted list with the first one always be the
  // |selected_connection_| when it's not nullptr. The combination of
  // |pinged_connections_| and |unpinged_connections_| has the same
  // connections as |connections_|. These 2 sets maintain whether a
  // connection should be pinged next or not.
  std::vector<Connection *> connections_;
  std::set<Connection*> pinged_connections_;
  std::set<Connection*> unpinged_connections_;

  Connection* selected_connection_ = nullptr;

  std::vector<RemoteCandidate> remote_candidates_;
  bool sort_dirty_;  // indicates whether another sort is needed right now
  bool had_connection_ = false;  // if connections_ has ever been nonempty
  typedef std::map<rtc::Socket::Option, int> OptionMap;
  OptionMap options_;
  IceParameters ice_parameters_;
  std::vector<IceParameters> remote_ice_parameters_;
  IceMode remote_ice_mode_;
  IceRole ice_role_;
  uint64_t tiebreaker_;
  IceGatheringState gathering_state_;

  // Used to generate random intervals for regather_all_networks_interval_range.
  webrtc::Random rand_;

  int check_receiving_interval_;
  int64_t last_ping_sent_ms_ = 0;
  int weak_ping_interval_ = WEAK_PING_INTERVAL;
  IceTransportState state_ = IceTransportState::STATE_INIT;
  IceConfig config_;
  int last_sent_packet_id_ = -1;  // -1 indicates no packet was sent before.
  bool started_pinging_ = false;
  // The value put in the "nomination" attribute for the next nominated
  // connection. A zero-value indicates the connection will not be nominated.
  uint32_t nomination_ = 0;
  bool receiving_ = false;
  bool writable_ = false;

  webrtc::MetricsObserverInterface* metrics_observer_ = nullptr;

  rtc::Optional<rtc::NetworkRoute> network_route_;

  RTC_DISALLOW_COPY_AND_ASSIGN(P2PTransportChannel);
};

}  // namespace cricket

#endif  // P2P_BASE_P2PTRANSPORTCHANNEL_H_
