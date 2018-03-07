/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_TURNPORT_H_
#define P2P_BASE_TURNPORT_H_

#include <stdio.h>
#include <list>
#include <set>
#include <string>

#include "p2p/base/port.h"
#include "p2p/client/basicportallocator.h"
#include "rtc_base/asyncinvoker.h"
#include "rtc_base/asyncpacketsocket.h"

namespace rtc {
class AsyncResolver;
class SignalThread;
}

namespace webrtc {
class TurnCustomizer;
}

namespace cricket {

extern const char TURN_PORT_TYPE[];
class TurnAllocateRequest;
class TurnEntry;

class TurnPort : public Port {
 public:
  enum PortState {
    STATE_CONNECTING,    // Initial state, cannot send any packets.
    STATE_CONNECTED,     // Socket connected, ready to send stun requests.
    STATE_READY,         // Received allocate success, can send any packets.
    STATE_RECEIVEONLY,   // Had REFRESH_REQUEST error, cannot send any packets.
    STATE_DISCONNECTED,  // TCP connection died, cannot send/receive any
                         // packets.
  };
  // Create a TURN port using the shared UDP socket, |socket|.
  static TurnPort* Create(rtc::Thread* thread,
                          rtc::PacketSocketFactory* factory,
                          rtc::Network* network,
                          rtc::AsyncPacketSocket* socket,
                          const std::string& username,  // ice username.
                          const std::string& password,  // ice password.
                          const ProtocolAddress& server_address,
                          const RelayCredentials& credentials,
                          int server_priority,
                          const std::string& origin,
                          webrtc::TurnCustomizer* customizer) {
    return new TurnPort(thread, factory, network, socket, username, password,
                        server_address, credentials, server_priority, origin,
                        customizer);
  }

  // Create a TURN port that will use a new socket, bound to |network| and
  // using a port in the range between |min_port| and |max_port|.
  static TurnPort* Create(rtc::Thread* thread,
                          rtc::PacketSocketFactory* factory,
                          rtc::Network* network,
                          uint16_t min_port,
                          uint16_t max_port,
                          const std::string& username,  // ice username.
                          const std::string& password,  // ice password.
                          const ProtocolAddress& server_address,
                          const RelayCredentials& credentials,
                          int server_priority,
                          const std::string& origin,
                          const std::vector<std::string>& tls_alpn_protocols,
                          const std::vector<std::string>& tls_elliptic_curves,
                          webrtc::TurnCustomizer* customizer) {
    return new TurnPort(thread, factory, network, min_port, max_port, username,
                        password, server_address, credentials, server_priority,
                        origin, tls_alpn_protocols, tls_elliptic_curves,
                        customizer);
  }

  ~TurnPort() override;

  const ProtocolAddress& server_address() const { return server_address_; }
  // Returns an empty address if the local address has not been assigned.
  rtc::SocketAddress GetLocalAddress() const;

  bool ready() const { return state_ == STATE_READY; }
  bool connected() const {
    return state_ == STATE_READY || state_ == STATE_CONNECTED;
  }
  const RelayCredentials& credentials() const { return credentials_; }

  ProtocolType GetProtocol() const override;

  virtual TlsCertPolicy GetTlsCertPolicy() const;
  virtual void SetTlsCertPolicy(TlsCertPolicy tls_cert_policy);

  virtual std::vector<std::string> GetTlsAlpnProtocols() const;
  virtual std::vector<std::string> GetTlsEllipticCurves() const;

  void PrepareAddress() override;
  Connection* CreateConnection(const Candidate& c,
                               PortInterface::CandidateOrigin origin) override;
  int SendTo(const void* data,
             size_t size,
             const rtc::SocketAddress& addr,
             const rtc::PacketOptions& options,
             bool payload) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  int GetOption(rtc::Socket::Option opt, int* value) override;
  int GetError() override;

  bool HandleIncomingPacket(rtc::AsyncPacketSocket* socket,
                            const char* data,
                            size_t size,
                            const rtc::SocketAddress& remote_addr,
                            const rtc::PacketTime& packet_time) override;
  virtual void OnReadPacket(rtc::AsyncPacketSocket* socket,
                            const char* data, size_t size,
                            const rtc::SocketAddress& remote_addr,
                            const rtc::PacketTime& packet_time);

  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet) override;
  virtual void OnReadyToSend(rtc::AsyncPacketSocket* socket);
  bool SupportsProtocol(const std::string& protocol) const override;

  void OnSocketConnect(rtc::AsyncPacketSocket* socket);
  void OnSocketClose(rtc::AsyncPacketSocket* socket, int error);


  const std::string& hash() const { return hash_; }
  const std::string& nonce() const { return nonce_; }

  int error() const { return error_; }

  void OnAllocateMismatch();

  rtc::AsyncPacketSocket* socket() const {
    return socket_;
  }

  // For testing only.
  rtc::AsyncInvoker* invoker() { return &invoker_; }

  // Signal with resolved server address.
  // Parameters are port, server address and resolved server address.
  // This signal will be sent only if server address is resolved successfully.
  sigslot::signal3<TurnPort*,
                   const rtc::SocketAddress&,
                   const rtc::SocketAddress&> SignalResolvedServerAddress;

  // All public methods/signals below are for testing only.
  sigslot::signal2<TurnPort*, int> SignalTurnRefreshResult;
  sigslot::signal3<TurnPort*, const rtc::SocketAddress&, int>
      SignalCreatePermissionResult;
  void FlushRequests(int msg_type) { request_manager_.Flush(msg_type); }
  bool HasRequests() { return !request_manager_.empty(); }
  void set_credentials(RelayCredentials& credentials) {
    credentials_ = credentials;
  }
  // Finds the turn entry with |address| and sets its channel id.
  // Returns true if the entry is found.
  bool SetEntryChannelId(const rtc::SocketAddress& address, int channel_id);
  // Visible for testing.
  // Shuts down the turn port, usually because of some fatal errors.
  void Close();

 protected:
  TurnPort(rtc::Thread* thread,
           rtc::PacketSocketFactory* factory,
           rtc::Network* network,
           rtc::AsyncPacketSocket* socket,
           const std::string& username,
           const std::string& password,
           const ProtocolAddress& server_address,
           const RelayCredentials& credentials,
           int server_priority,
           const std::string& origin,
           webrtc::TurnCustomizer* customizer);

  TurnPort(rtc::Thread* thread,
           rtc::PacketSocketFactory* factory,
           rtc::Network* network,
           uint16_t min_port,
           uint16_t max_port,
           const std::string& username,
           const std::string& password,
           const ProtocolAddress& server_address,
           const RelayCredentials& credentials,
           int server_priority,
           const std::string& origin,
           const std::vector<std::string>& tls_alpn_protocols,
           const std::vector<std::string>& tls_elliptic_curves,
           webrtc::TurnCustomizer* customizer);

 private:
  enum {
    MSG_ALLOCATE_ERROR = MSG_FIRST_AVAILABLE,
    MSG_ALLOCATE_MISMATCH,
    MSG_TRY_ALTERNATE_SERVER,
    MSG_REFRESH_ERROR
  };

  typedef std::list<TurnEntry*> EntryList;
  typedef std::map<rtc::Socket::Option, int> SocketOptionsMap;
  typedef std::set<rtc::SocketAddress> AttemptedServerSet;

  void OnMessage(rtc::Message* pmsg) override;
  void HandleConnectionDestroyed(Connection* conn) override;

  bool CreateTurnClientSocket();

  void set_nonce(const std::string& nonce) { nonce_ = nonce; }
  void set_realm(const std::string& realm) {
    if (realm != realm_) {
      realm_ = realm;
      UpdateHash();
    }
  }

  void OnRefreshError();
  void HandleRefreshError();
  bool SetAlternateServer(const rtc::SocketAddress& address);
  void ResolveTurnAddress(const rtc::SocketAddress& address);
  void OnResolveResult(rtc::AsyncResolverInterface* resolver);

  void AddRequestAuthInfo(StunMessage* msg);
  void OnSendStunPacket(const void* data, size_t size, StunRequest* request);
  // Stun address from allocate success response.
  // Currently used only for testing.
  void OnStunAddress(const rtc::SocketAddress& address);
  void OnAllocateSuccess(const rtc::SocketAddress& address,
                         const rtc::SocketAddress& stun_address);
  void OnAllocateError();
  void OnAllocateRequestTimeout();

  void HandleDataIndication(const char* data, size_t size,
                            const rtc::PacketTime& packet_time);
  void HandleChannelData(int channel_id, const char* data, size_t size,
                         const rtc::PacketTime& packet_time);
  void DispatchPacket(const char* data, size_t size,
      const rtc::SocketAddress& remote_addr,
      ProtocolType proto, const rtc::PacketTime& packet_time);

  bool ScheduleRefresh(int lifetime);
  void SendRequest(StunRequest* request, int delay);
  int Send(const void* data, size_t size,
           const rtc::PacketOptions& options);
  void UpdateHash();
  bool UpdateNonce(StunMessage* response);
  void ResetNonce();

  bool HasPermission(const rtc::IPAddress& ipaddr) const;
  TurnEntry* FindEntry(const rtc::SocketAddress& address) const;
  TurnEntry* FindEntry(int channel_id) const;
  bool EntryExists(TurnEntry* e);
  void CreateOrRefreshEntry(const rtc::SocketAddress& address);
  void DestroyEntry(TurnEntry* entry);
  // Destroys the entry only if |timestamp| matches the destruction timestamp
  // in |entry|.
  void DestroyEntryIfNotCancelled(TurnEntry* entry, int64_t timestamp);
  void ScheduleEntryDestruction(TurnEntry* entry);

  // Marks the connection with remote address |address| failed and
  // pruned (a.k.a. write-timed-out). Returns true if a connection is found.
  bool FailAndPruneConnection(const rtc::SocketAddress& address);

  // Reconstruct the URL of the server which the candidate is gathered from.
  std::string ReconstructedServerUrl();

  void TurnCustomizerMaybeModifyOutgoingStunMessage(StunMessage* message);
  bool TurnCustomizerAllowChannelData(const void* data,
                                      size_t size, bool payload);

  ProtocolAddress server_address_;
  TlsCertPolicy tls_cert_policy_ = TlsCertPolicy::TLS_CERT_POLICY_SECURE;
  std::vector<std::string> tls_alpn_protocols_;
  std::vector<std::string> tls_elliptic_curves_;
  RelayCredentials credentials_;
  AttemptedServerSet attempted_server_addresses_;

  rtc::AsyncPacketSocket* socket_;
  SocketOptionsMap socket_options_;
  rtc::AsyncResolverInterface* resolver_;
  int error_;

  StunRequestManager request_manager_;
  std::string realm_;       // From 401/438 response message.
  std::string nonce_;       // From 401/438 response message.
  std::string hash_;        // Digest of username:realm:password

  int next_channel_number_;
  EntryList entries_;

  PortState state_;
  // By default the value will be set to 0. This value will be used in
  // calculating the candidate priority.
  int server_priority_;

  // The number of retries made due to allocate mismatch error.
  size_t allocate_mismatch_retries_;

  rtc::AsyncInvoker invoker_;

  // Optional TurnCustomizer that can modify outgoing messages.
  webrtc::TurnCustomizer *turn_customizer_ = nullptr;

  friend class TurnEntry;
  friend class TurnAllocateRequest;
  friend class TurnRefreshRequest;
  friend class TurnCreatePermissionRequest;
  friend class TurnChannelBindRequest;
};

}  // namespace cricket

#endif  // P2P_BASE_TURNPORT_H_
