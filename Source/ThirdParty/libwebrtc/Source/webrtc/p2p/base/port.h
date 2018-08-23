/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_PORT_H_
#define P2P_BASE_PORT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/rtcerror.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/icelogger.h"
#include "p2p/base/candidatepairinterface.h"
#include "p2p/base/p2pconstants.h"
#include "p2p/base/packetlossestimator.h"
#include "p2p/base/packetsocketfactory.h"
#include "p2p/base/portinterface.h"
#include "p2p/base/stun.h"
#include "p2p/base/stunrequest.h"
#include "rtc_base/asyncpacketsocket.h"
#include "rtc_base/checks.h"
#include "rtc_base/nethelper.h"
#include "rtc_base/network.h"
#include "rtc_base/proxyinfo.h"
#include "rtc_base/ratetracker.h"
#include "rtc_base/socketaddress.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"

namespace cricket {

class Connection;
class ConnectionRequest;

extern const char LOCAL_PORT_TYPE[];
extern const char STUN_PORT_TYPE[];
extern const char PRFLX_PORT_TYPE[];
extern const char RELAY_PORT_TYPE[];

// RFC 6544, TCP candidate encoding rules.
extern const int DISCARD_PORT;
extern const char TCPTYPE_ACTIVE_STR[];
extern const char TCPTYPE_PASSIVE_STR[];
extern const char TCPTYPE_SIMOPEN_STR[];

enum RelayType {
  RELAY_GTURN,  // Legacy google relay service.
  RELAY_TURN    // Standard (TURN) relay service.
};

enum IcePriorityValue {
  ICE_TYPE_PREFERENCE_RELAY_TLS = 0,
  ICE_TYPE_PREFERENCE_RELAY_TCP = 1,
  ICE_TYPE_PREFERENCE_RELAY_UDP = 2,
  ICE_TYPE_PREFERENCE_PRFLX_TCP = 80,
  ICE_TYPE_PREFERENCE_HOST_TCP = 90,
  ICE_TYPE_PREFERENCE_SRFLX = 100,
  ICE_TYPE_PREFERENCE_PRFLX = 110,
  ICE_TYPE_PREFERENCE_HOST = 126
};

// States are from RFC 5245. http://tools.ietf.org/html/rfc5245#section-5.7.4
enum class IceCandidatePairState {
  WAITING = 0,  // Check has not been performed, Waiting pair on CL.
  IN_PROGRESS,  // Check has been sent, transaction is in progress.
  SUCCEEDED,    // Check already done, produced a successful result.
  FAILED,       // Check for this connection failed.
  // According to spec there should also be a frozen state, but nothing is ever
  // frozen because we have not implemented ICE freezing logic.
};

// Stats that we can return about the port of a STUN candidate.
class StunStats {
 public:
  StunStats() = default;
  StunStats(const StunStats&) = default;
  ~StunStats() = default;

  StunStats& operator=(const StunStats& other) = default;

  int stun_binding_requests_sent = 0;
  int stun_binding_responses_received = 0;
  double stun_binding_rtt_ms_total = 0;
  double stun_binding_rtt_ms_squared_total = 0;
};

// Stats that we can return about a candidate.
class CandidateStats {
 public:
  CandidateStats();
  explicit CandidateStats(Candidate candidate);
  CandidateStats(const CandidateStats&);
  ~CandidateStats();

  Candidate candidate;
  // STUN port stats if this candidate is a STUN candidate.
  absl::optional<StunStats> stun_stats;
};

typedef std::vector<CandidateStats> CandidateStatsList;

// Stats that we can return about the connections for a transport channel.
// TODO(hta): Rename to ConnectionStats
struct ConnectionInfo {
  ConnectionInfo();
  ConnectionInfo(const ConnectionInfo&);
  ~ConnectionInfo();

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
  absl::optional<uint32_t> current_round_trip_time_ms;
};

// Information about all the candidate pairs of a channel.
typedef std::vector<ConnectionInfo> ConnectionInfos;

const char* ProtoToString(ProtocolType proto);
bool StringToProto(const char* value, ProtocolType* proto);

struct ProtocolAddress {
  rtc::SocketAddress address;
  ProtocolType proto;

  ProtocolAddress(const rtc::SocketAddress& a, ProtocolType p)
      : address(a), proto(p) {}

  bool operator==(const ProtocolAddress& o) const {
    return address == o.address && proto == o.proto;
  }
  bool operator!=(const ProtocolAddress& o) const { return !(*this == o); }
};

typedef std::set<rtc::SocketAddress> ServerAddresses;

// Represents a local communication mechanism that can be used to create
// connections to similar mechanisms of the other client.  Subclasses of this
// one add support for specific mechanisms like local UDP ports.
class Port : public PortInterface,
             public rtc::MessageHandler,
             public sigslot::has_slots<> {
 public:
  // INIT: The state when a port is just created.
  // KEEP_ALIVE_UNTIL_PRUNED: A port should not be destroyed even if no
  // connection is using it.
  // PRUNED: It will be destroyed if no connection is using it for a period of
  // 30 seconds.
  enum class State { INIT, KEEP_ALIVE_UNTIL_PRUNED, PRUNED };
  Port(rtc::Thread* thread,
       const std::string& type,
       rtc::PacketSocketFactory* factory,
       rtc::Network* network,
       const std::string& username_fragment,
       const std::string& password);
  Port(rtc::Thread* thread,
       const std::string& type,
       rtc::PacketSocketFactory* factory,
       rtc::Network* network,
       uint16_t min_port,
       uint16_t max_port,
       const std::string& username_fragment,
       const std::string& password);
  ~Port() override;

  // Note that the port type does NOT uniquely identify different subclasses of
  // Port. Use the 2-tuple of the port type AND the protocol (GetProtocol()) to
  // uniquely identify subclasses. Whenever a new subclass of Port introduces a
  // conflit in the value of the 2-tuple, make sure that the implementation that
  // relies on this 2-tuple for RTTI is properly changed.
  const std::string& Type() const override;
  rtc::Network* Network() const override;

  // Methods to set/get ICE role and tiebreaker values.
  IceRole GetIceRole() const override;
  void SetIceRole(IceRole role) override;

  void SetIceTiebreaker(uint64_t tiebreaker) override;
  uint64_t IceTiebreaker() const override;

  bool SharedSocket() const override;
  void ResetSharedSocket() { shared_socket_ = false; }

  // Should not destroy the port even if no connection is using it. Called when
  // a port is ready to use.
  void KeepAliveUntilPruned();
  // Allows a port to be destroyed if no connection is using it.
  void Prune();

  // The thread on which this port performs its I/O.
  rtc::Thread* thread() { return thread_; }

  // The factory used to create the sockets of this port.
  rtc::PacketSocketFactory* socket_factory() const { return factory_; }
  void set_socket_factory(rtc::PacketSocketFactory* factory) {
    factory_ = factory;
  }

  // For debugging purposes.
  const std::string& content_name() const { return content_name_; }
  void set_content_name(const std::string& content_name) {
    content_name_ = content_name;
  }

  int component() const { return component_; }
  void set_component(int component) { component_ = component; }

  bool send_retransmit_count_attribute() const {
    return send_retransmit_count_attribute_;
  }
  void set_send_retransmit_count_attribute(bool enable) {
    send_retransmit_count_attribute_ = enable;
  }

  // Identifies the generation that this port was created in.
  uint32_t generation() const { return generation_; }
  void set_generation(uint32_t generation) { generation_ = generation; }

  const std::string username_fragment() const;
  const std::string& password() const { return password_; }

  // May be called when this port was initially created by a pooled
  // PortAllocatorSession, and is now being assigned to an ICE transport.
  // Updates the information for candidates as well.
  void SetIceParameters(int component,
                        const std::string& username_fragment,
                        const std::string& password);

  // Fired when candidates are discovered by the port. When all candidates
  // are discovered that belong to port SignalAddressReady is fired.
  sigslot::signal2<Port*, const Candidate&> SignalCandidateReady;

  // Provides all of the above information in one handy object.
  const std::vector<Candidate>& Candidates() const override;

  // SignalPortComplete is sent when port completes the task of candidates
  // allocation.
  sigslot::signal1<Port*> SignalPortComplete;
  // This signal sent when port fails to allocate candidates and this port
  // can't be used in establishing the connections. When port is in shared mode
  // and port fails to allocate one of the candidates, port shouldn't send
  // this signal as other candidates might be usefull in establishing the
  // connection.
  sigslot::signal1<Port*> SignalPortError;

  // Returns a map containing all of the connections of this port, keyed by the
  // remote address.
  typedef std::map<rtc::SocketAddress, Connection*> AddressMap;
  const AddressMap& connections() { return connections_; }

  // Returns the connection to the given address or NULL if none exists.
  Connection* GetConnection(const rtc::SocketAddress& remote_addr) override;

  // Called each time a connection is created.
  sigslot::signal2<Port*, Connection*> SignalConnectionCreated;

  // In a shared socket mode each port which shares the socket will decide
  // to accept the packet based on the |remote_addr|. Currently only UDP
  // port implemented this method.
  // TODO(mallinath) - Make it pure virtual.
  virtual bool HandleIncomingPacket(rtc::AsyncPacketSocket* socket,
                                    const char* data,
                                    size_t size,
                                    const rtc::SocketAddress& remote_addr,
                                    const rtc::PacketTime& packet_time);

  // Shall the port handle packet from this |remote_addr|.
  // This method is overridden by TurnPort.
  virtual bool CanHandleIncomingPacketsFrom(
      const rtc::SocketAddress& remote_addr) const;

  // Sends a response message (normal or error) to the given request.  One of
  // these methods should be called as a response to SignalUnknownAddress.
  // NOTE: You MUST call CreateConnection BEFORE SendBindingResponse.
  void SendBindingResponse(StunMessage* request,
                           const rtc::SocketAddress& addr) override;
  void SendBindingErrorResponse(StunMessage* request,
                                const rtc::SocketAddress& addr,
                                int error_code,
                                const std::string& reason) override;

  void set_proxy(const std::string& user_agent, const rtc::ProxyInfo& proxy) {
    user_agent_ = user_agent;
    proxy_ = proxy;
  }
  const std::string& user_agent() { return user_agent_; }
  const rtc::ProxyInfo& proxy() { return proxy_; }

  void EnablePortPackets() override;

  // Called if the port has no connections and is no longer useful.
  void Destroy();

  void OnMessage(rtc::Message* pmsg) override;

  // Debugging description of this port
  std::string ToString() const override;
  uint16_t min_port() { return min_port_; }
  uint16_t max_port() { return max_port_; }

  // Timeout shortening function to speed up unit tests.
  void set_timeout_delay(int delay) { timeout_delay_ = delay; }

  // This method will return local and remote username fragements from the
  // stun username attribute if present.
  bool ParseStunUsername(const StunMessage* stun_msg,
                         std::string* local_username,
                         std::string* remote_username) const;
  void CreateStunUsername(const std::string& remote_username,
                          std::string* stun_username_attr_str) const;

  bool MaybeIceRoleConflict(const rtc::SocketAddress& addr,
                            IceMessage* stun_msg,
                            const std::string& remote_ufrag);

  // Called when a packet has been sent to the socket.
  // This is made pure virtual to notify subclasses of Port that they MUST
  // listen to AsyncPacketSocket::SignalSentPacket and then call
  // PortInterface::OnSentPacket.
  virtual void OnSentPacket(rtc::AsyncPacketSocket* socket,
                            const rtc::SentPacket& sent_packet) = 0;

  // Called when the socket is currently able to send.
  void OnReadyToSend();

  // Called when the Connection discovers a local peer reflexive candidate.
  // Returns the index of the new local candidate.
  size_t AddPrflxCandidate(const Candidate& local);

  int16_t network_cost() const { return network_cost_; }

  void GetStunStats(absl::optional<StunStats>* stats) override{};

 protected:
  enum { MSG_DESTROY_IF_DEAD = 0, MSG_FIRST_AVAILABLE };

  virtual void UpdateNetworkCost();

  void set_type(const std::string& type) { type_ = type; }

  // Deprecated. Use the AddAddress() method below with "url" instead.
  // TODO(zhihuang): Remove this after downstream applications stop using it.
  void AddAddress(const rtc::SocketAddress& address,
                  const rtc::SocketAddress& base_address,
                  const rtc::SocketAddress& related_address,
                  const std::string& protocol,
                  const std::string& relay_protocol,
                  const std::string& tcptype,
                  const std::string& type,
                  uint32_t type_preference,
                  uint32_t relay_preference,
                  bool final);

  void AddAddress(const rtc::SocketAddress& address,
                  const rtc::SocketAddress& base_address,
                  const rtc::SocketAddress& related_address,
                  const std::string& protocol,
                  const std::string& relay_protocol,
                  const std::string& tcptype,
                  const std::string& type,
                  uint32_t type_preference,
                  uint32_t relay_preference,
                  const std::string& url,
                  bool final);

  // Adds the given connection to the map keyed by the remote candidate address.
  // If an existing connection has the same address, the existing one will be
  // replaced and destroyed.
  void AddOrReplaceConnection(Connection* conn);

  // Called when a packet is received from an unknown address that is not
  // currently a connection.  If this is an authenticated STUN binding request,
  // then we will signal the client.
  void OnReadPacket(const char* data,
                    size_t size,
                    const rtc::SocketAddress& addr,
                    ProtocolType proto);

  // If the given data comprises a complete and correct STUN message then the
  // return value is true, otherwise false. If the message username corresponds
  // with this port's username fragment, msg will contain the parsed STUN
  // message.  Otherwise, the function may send a STUN response internally.
  // remote_username contains the remote fragment of the STUN username.
  bool GetStunMessage(const char* data,
                      size_t size,
                      const rtc::SocketAddress& addr,
                      std::unique_ptr<IceMessage>* out_msg,
                      std::string* out_username);

  // Checks if the address in addr is compatible with the port's ip.
  bool IsCompatibleAddress(const rtc::SocketAddress& addr);

  // Returns default DSCP value.
  rtc::DiffServCodePoint DefaultDscpValue() const {
    // No change from what MediaChannel set.
    return rtc::DSCP_NO_CHANGE;
  }

  // Extra work to be done in subclasses when a connection is destroyed.
  virtual void HandleConnectionDestroyed(Connection* conn) {}

  void CopyPortInformationToPacketInfo(rtc::PacketInfo* info) const;

 private:
  void Construct();
  // Called when one of our connections deletes itself.
  void OnConnectionDestroyed(Connection* conn);

  void OnNetworkTypeChanged(const rtc::Network* network);

  rtc::Thread* thread_;
  rtc::PacketSocketFactory* factory_;
  std::string type_;
  bool send_retransmit_count_attribute_;
  rtc::Network* network_;
  uint16_t min_port_;
  uint16_t max_port_;
  std::string content_name_;
  int component_;
  uint32_t generation_;
  // In order to establish a connection to this Port (so that real data can be
  // sent through), the other side must send us a STUN binding request that is
  // authenticated with this username_fragment and password.
  // PortAllocatorSession will provide these username_fragment and password.
  //
  // Note: we should always use username_fragment() instead of using
  // |ice_username_fragment_| directly. For the details see the comment on
  // username_fragment().
  std::string ice_username_fragment_;
  std::string password_;
  std::vector<Candidate> candidates_;
  AddressMap connections_;
  int timeout_delay_;
  bool enable_port_packets_;
  IceRole ice_role_;
  uint64_t tiebreaker_;
  bool shared_socket_;
  // Information to use when going through a proxy.
  std::string user_agent_;
  rtc::ProxyInfo proxy_;

  // A virtual cost perceived by the user, usually based on the network type
  // (WiFi. vs. Cellular). It takes precedence over the priority when
  // comparing two connections.
  int16_t network_cost_;
  State state_ = State::INIT;
  int64_t last_time_all_connections_removed_ = 0;

  friend class Connection;
};

// Represents a communication link between a port on the local client and a
// port on the remote client.
class Connection : public CandidatePairInterface,
                   public rtc::MessageHandler,
                   public sigslot::has_slots<> {
 public:
  struct SentPing {
    SentPing(const std::string id, int64_t sent_time, uint32_t nomination)
        : id(id), sent_time(sent_time), nomination(nomination) {}

    std::string id;
    int64_t sent_time;
    uint32_t nomination;
  };

  ~Connection() override;

  // A unique ID assigned when the connection is created.
  uint32_t id() { return id_; }

  // The local port where this connection sends and receives packets.
  Port* port() { return port_; }
  const Port* port() const { return port_; }

  // Implementation of virtual methods in CandidatePairInterface.
  // Returns the description of the local port
  const Candidate& local_candidate() const override;
  // Returns the description of the remote port to which we communicate.
  const Candidate& remote_candidate() const override;

  // Returns the pair priority.
  uint64_t priority() const;

  enum WriteState {
    STATE_WRITABLE = 0,          // we have received ping responses recently
    STATE_WRITE_UNRELIABLE = 1,  // we have had a few ping failures
    STATE_WRITE_INIT = 2,        // we have yet to receive a ping response
    STATE_WRITE_TIMEOUT = 3,     // we have had a large number of ping failures
  };

  WriteState write_state() const { return write_state_; }
  bool writable() const { return write_state_ == STATE_WRITABLE; }
  bool receiving() const { return receiving_; }

  // Determines whether the connection has finished connecting.  This can only
  // be false for TCP connections.
  bool connected() const { return connected_; }
  bool weak() const { return !(writable() && receiving() && connected()); }
  bool active() const { return write_state_ != STATE_WRITE_TIMEOUT; }

  // A connection is dead if it can be safely deleted.
  bool dead(int64_t now) const;

  // Estimate of the round-trip time over this connection.
  int rtt() const { return rtt_; }

  int unwritable_timeout() const;
  void set_unwritable_timeout(const absl::optional<int>& value_ms) {
    unwritable_timeout_ = value_ms;
  }
  int unwritable_min_checks() const;
  void set_unwritable_min_checks(const absl::optional<int>& value) {
    unwritable_min_checks_ = value;
  }

  // Gets the |ConnectionInfo| stats, where |best_connection| has not been
  // populated (default value false).
  ConnectionInfo stats();

  sigslot::signal1<Connection*> SignalStateChange;

  // Sent when the connection has decided that it is no longer of value.  It
  // will delete itself immediately after this call.
  sigslot::signal1<Connection*> SignalDestroyed;

  // The connection can send and receive packets asynchronously.  This matches
  // the interface of AsyncPacketSocket, which may use UDP or TCP under the
  // covers.
  virtual int Send(const void* data,
                   size_t size,
                   const rtc::PacketOptions& options) = 0;

  // Error if Send() returns < 0
  virtual int GetError() = 0;

  sigslot::signal4<Connection*, const char*, size_t, const rtc::PacketTime&>
      SignalReadPacket;

  sigslot::signal1<Connection*> SignalReadyToSend;

  // Called when a packet is received on this connection.
  void OnReadPacket(const char* data,
                    size_t size,
                    const rtc::PacketTime& packet_time);

  // Called when the socket is currently able to send.
  void OnReadyToSend();

  // Called when a connection is determined to be no longer useful to us.  We
  // still keep it around in case the other side wants to use it.  But we can
  // safely stop pinging on it and we can allow it to time out if the other
  // side stops using it as well.
  bool pruned() const { return pruned_; }
  void Prune();

  bool use_candidate_attr() const { return use_candidate_attr_; }
  void set_use_candidate_attr(bool enable);

  void set_nomination(uint32_t value) { nomination_ = value; }

  uint32_t remote_nomination() const { return remote_nomination_; }
  // One or several pairs may be nominated based on if Regular or Aggressive
  // Nomination is used. https://tools.ietf.org/html/rfc5245#section-8
  // |nominated| is defined both for the controlling or controlled agent based
  // on if a nomination has been pinged or acknowledged. The controlled agent
  // gets its |remote_nomination_| set when pinged by the controlling agent with
  // a nomination value. The controlling agent gets its |acked_nomination_| set
  // when receiving a response to a nominating ping.
  bool nominated() const { return acked_nomination_ || remote_nomination_; }
  // Public for unit tests.
  void set_remote_nomination(uint32_t remote_nomination) {
    remote_nomination_ = remote_nomination;
  }
  // Public for unit tests.
  uint32_t acked_nomination() const { return acked_nomination_; }

  void set_remote_ice_mode(IceMode mode) { remote_ice_mode_ = mode; }

  int receiving_timeout() const;
  void set_receiving_timeout(absl::optional<int> receiving_timeout_ms) {
    receiving_timeout_ = receiving_timeout_ms;
  }

  // Makes the connection go away.
  void Destroy();

  // Makes the connection go away, in a failed state.
  void FailAndDestroy();

  // Prunes the connection and sets its state to STATE_FAILED,
  // It will not be used or send pings although it can still receive packets.
  void FailAndPrune();

  // Checks that the state of this connection is up-to-date.  The argument is
  // the current time, which is compared against various timeouts.
  void UpdateState(int64_t now);

  // Called when this connection should try checking writability again.
  int64_t last_ping_sent() const { return last_ping_sent_; }
  void Ping(int64_t now);
  void ReceivedPingResponse(int rtt, const std::string& request_id);
  int64_t last_ping_response_received() const {
    return last_ping_response_received_;
  }
  // Used to check if any STUN ping response has been received.
  int rtt_samples() const { return rtt_samples_; }

  // Called whenever a valid ping is received on this connection.  This is
  // public because the connection intercepts the first ping for us.
  int64_t last_ping_received() const { return last_ping_received_; }
  void ReceivedPing();
  // Handles the binding request; sends a response if this is a valid request.
  void HandleBindingRequest(IceMessage* msg);

  int64_t last_data_received() const { return last_data_received_; }

  // Debugging description of this connection
  std::string ToDebugId() const;
  std::string ToString() const;
  std::string ToSensitiveString() const;
  // Structured description of this candidate pair.
  const webrtc::IceCandidatePairDescription& ToLogDescription();
  void set_ice_event_log(webrtc::IceEventLog* ice_event_log) {
    ice_event_log_ = ice_event_log;
  }
  // Prints pings_since_last_response_ into a string.
  void PrintPingsSinceLastResponse(std::string* pings, size_t max);

  bool reported() const { return reported_; }
  void set_reported(bool reported) { reported_ = reported; }
  // The following two methods are only used for logging in ToString above, and
  // this flag is set true by P2PTransportChannel for its selected candidate
  // pair.
  bool selected() const { return selected_; }
  void set_selected(bool selected) { selected_ = selected; }

  // This signal will be fired if this connection is nominated by the
  // controlling side.
  sigslot::signal1<Connection*> SignalNominated;

  // Invoked when Connection receives STUN error response with 487 code.
  void HandleRoleConflictFromPeer();

  IceCandidatePairState state() const { return state_; }

  int num_pings_sent() const { return num_pings_sent_; }

  IceMode remote_ice_mode() const { return remote_ice_mode_; }

  uint32_t ComputeNetworkCost() const;

  // Update the ICE password and/or generation of the remote candidate if the
  // ufrag in |params| matches the candidate's ufrag, and the
  // candidate's password and/or ufrag has not been set.
  void MaybeSetRemoteIceParametersAndGeneration(const IceParameters& params,
                                                int generation);

  // If |remote_candidate_| is peer reflexive and is equivalent to
  // |new_candidate| except the type, update |remote_candidate_| to
  // |new_candidate|.
  void MaybeUpdatePeerReflexiveCandidate(const Candidate& new_candidate);

  // Returns the last received time of any data, stun request, or stun
  // response in milliseconds
  int64_t last_received() const;
  // Returns the last time when the connection changed its receiving state.
  int64_t receiving_unchanged_since() const {
    return receiving_unchanged_since_;
  }

  bool stable(int64_t now) const;

 protected:
  enum { MSG_DELETE = 0, MSG_FIRST_AVAILABLE };

  // Constructs a new connection to the given remote port.
  Connection(Port* port, size_t index, const Candidate& candidate);

  // Called back when StunRequestManager has a stun packet to send
  void OnSendStunPacket(const void* data, size_t size, StunRequest* req);

  // Callbacks from ConnectionRequest
  virtual void OnConnectionRequestResponse(ConnectionRequest* req,
                                           StunMessage* response);
  void OnConnectionRequestErrorResponse(ConnectionRequest* req,
                                        StunMessage* response);
  void OnConnectionRequestTimeout(ConnectionRequest* req);
  void OnConnectionRequestSent(ConnectionRequest* req);

  bool rtt_converged() const;

  // If the response is not received within 2 * RTT, the response is assumed to
  // be missing.
  bool missing_responses(int64_t now) const;

  // Changes the state and signals if necessary.
  void set_write_state(WriteState value);
  void UpdateReceiving(int64_t now);
  void set_state(IceCandidatePairState state);
  void set_connected(bool value);

  uint32_t nomination() const { return nomination_; }

  void OnMessage(rtc::Message* pmsg) override;

  uint32_t id_;
  Port* port_;
  size_t local_candidate_index_;
  Candidate remote_candidate_;

  ConnectionInfo stats_;
  rtc::RateTracker recv_rate_tracker_;
  rtc::RateTracker send_rate_tracker_;

 private:
  // Update the local candidate based on the mapped address attribute.
  // If the local candidate changed, fires SignalStateChange.
  void MaybeUpdateLocalCandidate(ConnectionRequest* request,
                                 StunMessage* response);

  void LogCandidatePairConfig(webrtc::IceCandidatePairConfigType type);
  void LogCandidatePairEvent(webrtc::IceCandidatePairEventType type);

  WriteState write_state_;
  bool receiving_;
  bool connected_;
  bool pruned_;
  bool selected_ = false;
  // By default |use_candidate_attr_| flag will be true,
  // as we will be using aggressive nomination.
  // But when peer is ice-lite, this flag "must" be initialized to false and
  // turn on when connection becomes "best connection".
  bool use_candidate_attr_;
  // Used by the controlling side to indicate that this connection will be
  // selected for transmission if the peer supports ICE-renomination when this
  // value is positive. A larger-value indicates that a connection is nominated
  // later and should be selected by the controlled side with higher precedence.
  // A zero-value indicates not nominating this connection.
  uint32_t nomination_ = 0;
  // The last nomination that has been acknowledged.
  uint32_t acked_nomination_ = 0;
  // Used by the controlled side to remember the nomination value received from
  // the controlling side. When the peer does not support ICE re-nomination,
  // its value will be 1 if the connection has been nominated.
  uint32_t remote_nomination_ = 0;

  IceMode remote_ice_mode_;
  StunRequestManager requests_;
  int rtt_;
  int rtt_samples_ = 0;
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-totalroundtriptime
  uint64_t total_round_trip_time_ms_ = 0;
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-currentroundtriptime
  absl::optional<uint32_t> current_round_trip_time_ms_;
  int64_t last_ping_sent_;      // last time we sent a ping to the other side
  int64_t last_ping_received_;  // last time we received a ping from the other
                                // side
  int64_t last_data_received_;
  int64_t last_ping_response_received_;
  int64_t receiving_unchanged_since_ = 0;
  std::vector<SentPing> pings_since_last_response_;

  PacketLossEstimator packet_loss_estimator_;

  absl::optional<int> unwritable_timeout_;
  absl::optional<int> unwritable_min_checks_;

  bool reported_;
  IceCandidatePairState state_;
  // Time duration to switch from receiving to not receiving.
  absl::optional<int> receiving_timeout_;
  int64_t time_created_ms_;
  int num_pings_sent_ = 0;

  absl::optional<webrtc::IceCandidatePairDescription> log_description_;
  webrtc::IceEventLog* ice_event_log_ = nullptr;

  friend class Port;
  friend class ConnectionRequest;
};

// ProxyConnection defers all the interesting work to the port.
class ProxyConnection : public Connection {
 public:
  ProxyConnection(Port* port, size_t index, const Candidate& remote_candidate);

  int Send(const void* data,
           size_t size,
           const rtc::PacketOptions& options) override;
  int GetError() override;

 private:
  int error_ = 0;
};

}  // namespace cricket

#endif  // P2P_BASE_PORT_H_
