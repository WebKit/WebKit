/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_CONNECTION_H_
#define P2P_BASE_CONNECTION_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/environment/environment.h"
#include "api/rtc_error.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/stun.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/ice_logger.h"
#include "p2p/base/candidate_pair_interface.h"
#include "p2p/base/connection_info.h"
#include "p2p/base/p2p_transport_channel_ice_field_trials.h"
#include "p2p/base/port_interface.h"
#include "p2p/base/stun_request.h"
#include "p2p/base/transport_description.h"
#include "p2p/dtls/dtls_stun_piggyback_callbacks.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/bitrate_tracker.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/network.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/numerics/event_based_exponential_moving_average.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

// Version number for GOOG_PING, this is added to have the option of
// adding other flavors in the future.
constexpr int kGoogPingVersion = 1;
// 1200 is the "commonly used" MTU. Subtract M-I attribute (20+4) and FP (4+4).
constexpr int kMaxStunBindingLength = 1200 - 24 - 8;

// Represents a communication link between a port on the local client and a
// port on the remote client.
class RTC_EXPORT Connection : public CandidatePairInterface {
 public:
  struct SentPing {
    SentPing(absl::string_view id, Timestamp sent_time, uint32_t nomination)
        : id(id), sent_time(sent_time), nomination(nomination) {}

    std::string id;
    Timestamp sent_time;
    uint32_t nomination;
  };

  ~Connection() override;

  // A unique ID assigned when the connection is created.
  uint32_t id() const { return id_; }

  TaskQueueBase* network_thread() const;

  // Implementation of virtual methods in CandidatePairInterface.
  // Returns the description of the local port
  const Candidate& local_candidate() const override;
  // Returns the description of the remote port to which we communicate.
  const Candidate& remote_candidate() const override;

  // Return local network for this connection.
  virtual const Network* network() const;
  // Return generation for this connection.
  virtual int generation() const;

  // Returns the pair priority.
  virtual uint64_t priority() const;

  enum WriteState {
    STATE_WRITABLE = 0,          // we have received ping responses recently
    STATE_WRITE_UNRELIABLE = 1,  // we have had a few ping failures
    STATE_WRITE_INIT = 2,        // we have yet to receive a ping response
    STATE_WRITE_TIMEOUT = 3,     // we have had a large number of ping failures
  };

  WriteState write_state() const;
  bool writable() const;
  bool receiving() const;

  const PortInterface* port() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return port_.get();
  }

  // Determines whether the connection has finished connecting.  This can only
  // be false for TCP connections.
  bool connected() const;
  bool weak() const;
  bool active() const;
  bool pending_delete() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return !port_;
  }

  // A connection is dead if it can be safely deleted.
  bool dead(Timestamp now) const;

  TimeDelta Rtt() const;

  TimeDelta UnwritableTimeout() const;
  void SetUnwritableTimeout(std::optional<TimeDelta> value);
  int unwritable_min_checks() const;
  void set_unwritable_min_checks(const std::optional<int>& value);

  TimeDelta InactiveTimeout() const;
  void SetInactiveTimeout(std::optional<TimeDelta> value);

  // Gets the `ConnectionInfo` stats, where `best_connection` has not been
  // populated (default value false).
  ConnectionInfo stats();

  [[deprecated("Use SubscribeStateChange(void* tag, ...)")]]
  void SubscribeStateChange(
      absl::AnyInvocable<void(Connection* connection)> callback) {
    state_change_callbacks_.AddReceiver(std::move(callback));
  }
  void SubscribeStateChange(
      void* tag,
      absl::AnyInvocable<void(Connection* connection)> callback) {
    state_change_callbacks_.AddReceiver(tag, std::move(callback));
  }
  // Sent when the connection has decided that it is no longer of value.  It
  // will delete itself immediately after this call.
  void SubscribeDestroyed(
      void* tag,
      absl::AnyInvocable<void(Connection* connection)> callback) {
    destroyed_callbacks_.AddReceiver(tag, std::move(callback));
  }
  void UnsubscribeDestroyed(void* tag) {
    destroyed_callbacks_.RemoveReceivers(tag);
  }

  // The connection can send and receive packets asynchronously.  This matches
  // the interface of AsyncPacketSocket, which may use UDP or TCP under the
  // covers.
  virtual int Send(const void* data,
                   size_t size,
                   const AsyncSocketPacketOptions& options) = 0;

  // Error if Send() returns < 0
  virtual int GetError() = 0;

  // Register as a recipient of received packets. There can only be one.
  void RegisterReceivedPacketCallback(
      absl::AnyInvocable<void(Connection*, const ReceivedIpPacket&)>
          received_packet_callback);
  void DeregisterReceivedPacketCallback();

  [[deprecated("Use SubscribeReadyToSend(void* tag, ...)")]]
  void SubscribeReadyToSend(
      absl::AnyInvocable<void(Connection* connection)> callback) {
    ready_to_send_callbacks_.AddReceiver(std::move(callback));
  }
  void SubscribeReadyToSend(
      void* tag,
      absl::AnyInvocable<void(Connection* connection)> callback) {
    ready_to_send_callbacks_.AddReceiver(tag, std::move(callback));
  }
  // Called when a packet is received on this connection.
  void OnReadPacket(const ReceivedIpPacket& packet);
  [[deprecated("Pass a ReceivedIpPacket")]] void
  OnReadPacket(const char* data, size_t size, int64_t packet_time_us);

  // Called when the socket is currently able to send.
  void OnReadyToSend();

  // Called when a connection is determined to be no longer useful to us.  We
  // still keep it around in case the other side wants to use it.  But we can
  // safely stop pinging on it and we can allow it to time out if the other
  // side stops using it as well.
  bool pruned() const;
  void Prune();

  bool use_candidate_attr() const;
  void set_use_candidate_attr(bool enable);

  void set_nomination(uint32_t value);

  uint32_t remote_nomination() const;
  // One or several pairs may be nominated based on if Regular or Aggressive
  // Nomination is used. https://tools.ietf.org/html/rfc5245#section-8
  // `nominated` is defined both for the controlling or controlled agent based
  // on if a nomination has been pinged or acknowledged. The controlled agent
  // gets its `remote_nomination_` set when pinged by the controlling agent with
  // a nomination value. The controlling agent gets its `acked_nomination_` set
  // when receiving a response to a nominating ping.
  bool nominated() const;

  TimeDelta ReceivingTimeout() const;
  void SetReceivingTimeout(std::optional<TimeDelta> receiving_timeout);

  // Deletes a `Connection` instance is by calling the `DestroyConnection`
  // method in `Port`.
  // Note: When the function returns, the object has been deleted.
  void Destroy();

  // Signals object destruction, releases outstanding references and performs
  // final logging.
  // The function will return `true` when shutdown was performed, signals
  // emitted and outstanding references released. If the function returns
  // `false`, `Shutdown()` has previously been called.
  bool Shutdown();

  // Prunes the connection and sets its state to STATE_FAILED,
  // It will not be used or send pings although it can still receive packets.
  void FailAndPrune();

  // Checks that the state of this connection is up-to-date.  The argument is
  // the current time, which is compared against various timeouts.
  void UpdateState(Timestamp now);

  void UpdateLocalIceParameters(int component,
                                absl::string_view username_fragment,
                                absl::string_view password);

  // Called when this connection should try checking writability again.
  Timestamp LastPingSent() const;

  void Ping();
  void Ping(Timestamp now,
            std::unique_ptr<StunByteStringAttribute> delta = nullptr);
  void ReceivedPingResponse(
      TimeDelta rtt,
      absl::string_view request_id,
      const std::optional<uint32_t>& nomination = std::nullopt);
  std::unique_ptr<IceMessage> BuildPingRequest(
      std::unique_ptr<StunByteStringAttribute> delta)
      RTC_RUN_ON(network_thread_);

  Timestamp LastPingResponseReceived() const;
  const std::optional<std::string>& last_ping_id_received() const;

  // Used to check if any STUN ping response has been received.
  int rtt_samples() const;

  Timestamp LastPingReceived() const;

  void ReceivedPing(
      const std::optional<std::string>& request_id = std::nullopt);
  // Handles the binding request; sends a response if this is a valid request.
  void HandleStunBindingOrGoogPingRequest(IceMessage* msg);
  // Handles the piggyback acknowledgement of the lastest connectivity check
  // that the remote peer has received, if it is indicated in the incoming
  // connectivity check from the peer.
  void HandlePiggybackCheckAcknowledgementIfAny(StunMessage* msg);
  // Timestamp when data was last sent (or attempted to be sent).
  Timestamp LastSendData() const;
  Timestamp LastDataReceived() const;

  // Debugging description of this connection
  std::string ToDebugId() const;
  std::string ToString() const;
  std::string ToSensitiveString() const;
  // Structured description of this candidate pair.
  const IceCandidatePairDescription& ToLogDescription();
  void set_ice_event_log(IceEventLog* ice_event_log);

  // Prints pings_since_last_response_ into a string.
  void PrintPingsSinceLastResponse(std::string* pings, size_t max);

  // `set_selected` is only used for logging in ToString above.  The flag is
  // set true by P2PTransportChannel for its selected candidate pair.
  // TODO(tommi): Remove `selected()` once not referenced downstream.
  bool selected() const;
  void set_selected(bool selected);

  // This signal will be fired if this connection is nominated by the
  // controlling side.
  [[deprecated("Use SubscribeNominated(void* tag, ...)")]]
  void SubscribeNominated(
      absl::AnyInvocable<void(Connection* connection)> callback) {
    nominated_callbacks_.AddReceiver(std::move(callback));
  }

  void SubscribeNominated(
      void* tag,
      absl::AnyInvocable<void(Connection* connection)> callback) {
    nominated_callbacks_.AddReceiver(tag, std::move(callback));
  }
  IceCandidatePairState state() const;

  int num_pings_sent() const;

  uint32_t ComputeNetworkCost() const;

  // Update the ICE password and/or generation of the remote candidate if the
  // ufrag in `params` matches the candidate's ufrag, and the
  // candidate's password and/or ufrag has not been set.
  void MaybeSetRemoteIceParametersAndGeneration(const IceParameters& params,
                                                int generation);

  // If `remote_candidate_` is peer reflexive and is equivalent to
  // `new_candidate` except the type, update `remote_candidate_` to
  // `new_candidate`.
  void MaybeUpdatePeerReflexiveCandidate(const Candidate& new_candidate);

  // Returns the last received time of any data, stun request, or stun
  // response in milliseconds
  Timestamp LastReceived() const;

  // Returns the last time when the connection changed its receiving state.
  Timestamp ReceivingUnchangedSince() const;

  // Constructs the prflx priority as described in
  // https://datatracker.ietf.org/doc/html/rfc5245#section-4.1.2.1
  uint32_t prflx_priority() const;

  bool stable(Timestamp now) const;

  // Check if we sent `val` pings without receving a response.
  bool TooManyOutstandingPings(const std::optional<int>& val) const;

  // Called by Port when the network cost changes.
  void SetLocalCandidateNetworkCost(uint16_t cost);

  void SetIceFieldTrials(const IceFieldTrials* field_trials);
  const EventBasedExponentialMovingAverage& GetRttEstimate() const {
    return rtt_estimate_;
  }

  // Reset the connection to a state of a newly connected.
  // - STATE_WRITE_INIT
  // - receving = false
  // - throw away all pending request
  // - reset RttEstimate
  //
  // Keep the following unchanged:
  // - connected
  // - remote_candidate
  // - statistics
  //
  // Does not trigger SignalStateChange
  void ForgetLearnedState();

  void SendStunBindingResponse(const StunMessage* message);
  void SendGoogPingResponse(const StunMessage* message);
  void SendResponseMessage(const StunMessage& response);

  // An accessor for unit tests.
  PortInterface* PortForTest() {
    RTC_DCHECK_RUN_ON(network_thread_);
    return port_.get();
  }
  const PortInterface* PortForTest() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return port_.get();
  }

  std::unique_ptr<IceMessage> BuildPingRequestForTest() {
    RTC_DCHECK_RUN_ON(network_thread_);
    return BuildPingRequest(nullptr);
  }

  // Public for unit tests.
  uint32_t acked_nomination() const;
  void set_remote_nomination(uint32_t remote_nomination);

  const std::string& remote_password_for_test() const {
    return remote_candidate().password();
  }
  void set_remote_password_for_test(absl::string_view pwd) {
    remote_candidate_.set_password(pwd);
  }

  void SetStunDictConsumer(
      std::function<std::unique_ptr<StunAttribute>(
          const StunByteStringAttribute*)> goog_delta_consumer,
      std::function<void(RTCErrorOr<const StunUInt64Attribute*>)>
          goog_delta_ack_consumer) {
    goog_delta_consumer_ = std::move(goog_delta_consumer);
    goog_delta_ack_consumer_ = std::move(goog_delta_ack_consumer);
  }

  void ClearStunDictConsumer() {
    goog_delta_consumer_ = std::nullopt;
    goog_delta_ack_consumer_ = std::nullopt;
  }

  void RegisterDtlsPiggyback(DtlsStunPiggybackCallbacks&& callbacks) {
    dtls_stun_piggyback_callbacks_ = std::move(callbacks);
  }

  void DeregisterDtlsPiggyback() { dtls_stun_piggyback_callbacks_.reset(); }

  void NotifyNominatedForTesting(Connection* connection) {
    NotifyNominated(connection);
  }

  bool set_writable_for_fake_ice_lite() const;

 protected:
  // A ConnectionRequest is a simple STUN ping used to determine writability.
  class ConnectionRequest;

  // Constructs a new connection to the given remote port.
  Connection(const Environment& env,
             WeakPtr<PortInterface> port,
             size_t index,
             const Candidate& candidate);

  // Called back when StunRequestManager has a stun packet to send
  void OnSendStunPacket(const void* data, size_t size, StunRequest* req);

  // Callbacks from ConnectionRequest
  virtual void OnConnectionRequestResponse(StunRequest* req,
                                           StunMessage* response);
  void OnConnectionRequestErrorResponse(ConnectionRequest* req,
                                        StunMessage* response)
      RTC_RUN_ON(network_thread_);
  void OnConnectionRequestTimeout(ConnectionRequest* req)
      RTC_RUN_ON(network_thread_);
  void OnConnectionRequestSent(ConnectionRequest* req)
      RTC_RUN_ON(network_thread_);

  bool rtt_converged() const;

  // If the response is not received within 2 * RTT, the response is assumed to
  // be missing.
  bool missing_responses(Timestamp now) const;

  // Changes the state and signals if necessary.
  void set_write_state(WriteState value);
  void UpdateReceiving(Timestamp now);

  void set_state(IceCandidatePairState state);
  void set_connected(bool value);

  // The local port where this connection sends and receives packets.
  PortInterface* port() {
    RTC_DCHECK_RUN_ON(network_thread_);
    return port_.get();
  }

  const Environment& env() { return env_; }
  ConnectionInfo& mutable_stats() { return stats_; }
  void AddSentBytesToStats(int size, Timestamp now) {
    send_rate_tracker_.Update(size, now);
    stats_.sent_total_bytes += size;
  }
  void set_last_send_data(Timestamp now) { last_send_data_ = now; }

 private:
  // Update the local candidate based on the mapped address attribute.
  // If the local candidate changed, fires SignalStateChange.
  void MaybeUpdateLocalCandidate(StunRequest* request, StunMessage* response)
      RTC_RUN_ON(network_thread_);

  void LogCandidatePairConfig(IceCandidatePairConfigType type)
      RTC_RUN_ON(network_thread_);
  void LogCandidatePairEvent(IceCandidatePairEventType type,
                             uint32_t transaction_id)
      RTC_RUN_ON(network_thread_);

  // Check if this IceMessage is identical
  // to last message ack:ed STUN_BINDING_REQUEST.
  bool ShouldSendGoogPing(const StunMessage* message)
      RTC_RUN_ON(network_thread_);
  // Firing of callbacks.
  void NotifyStateChange(Connection* connection) {
    state_change_callbacks_.Send(connection);
  }
  void NotifyDestroyed(Connection* connection) {
    destroyed_callbacks_.Send(connection);
  }
  void NotifyReadyToSend(Connection* connection) {
    ready_to_send_callbacks_.Send(connection);
  }
  void NotifyNominated(Connection* connection) {
    nominated_callbacks_.Send(connection);
  }

  const Environment env_;

  // NOTE: A pointer to the network thread is held by `port_` so in theory we
  // shouldn't need to hold on to this pointer here, but rather defer to
  // port_->thread(). However, some tests delete the classes in the wrong order
  // so `port_` may be deleted before an instance of this class is deleted.
  // TODO(tommi): This ^^^ should be fixed.
  TaskQueueBase* const network_thread_;
  const uint32_t id_;
  WeakPtr<PortInterface> port_ RTC_GUARDED_BY(network_thread_);
  Candidate local_candidate_ RTC_GUARDED_BY(network_thread_);
  Candidate remote_candidate_;

  ConnectionInfo stats_;
  BitrateTracker recv_rate_tracker_;
  BitrateTracker send_rate_tracker_;
  Timestamp last_send_data_;

  WriteState write_state_ RTC_GUARDED_BY(network_thread_);
  bool receiving_ RTC_GUARDED_BY(network_thread_);
  bool connected_ RTC_GUARDED_BY(network_thread_);
  bool pruned_ RTC_GUARDED_BY(network_thread_);
  bool selected_ RTC_GUARDED_BY(network_thread_) = false;
  // By default `use_candidate_attr_` flag will be true,
  // as we will be using aggressive nomination.
  // But when peer is ice-lite, this flag "must" be initialized to false and
  // turn on when connection becomes "best connection".
  bool use_candidate_attr_ RTC_GUARDED_BY(network_thread_);
  // Used by the controlling side to indicate that this connection will be
  // selected for transmission if the peer supports ICE-renomination when this
  // value is positive. A larger-value indicates that a connection is nominated
  // later and should be selected by the controlled side with higher precedence.
  // A zero-value indicates not nominating this connection.
  uint32_t nomination_ RTC_GUARDED_BY(network_thread_) = 0;
  // The last nomination that has been acknowledged.
  uint32_t acked_nomination_ RTC_GUARDED_BY(network_thread_) = 0;
  // Used by the controlled side to remember the nomination value received from
  // the controlling side. When the peer does not support ICE re-nomination, its
  // value will be 1 if the connection has been nominated.
  uint32_t remote_nomination_ RTC_GUARDED_BY(network_thread_) = 0;

  StunRequestManager requests_ RTC_GUARDED_BY(network_thread_);
  TimeDelta rtt_ RTC_GUARDED_BY(network_thread_);
  int rtt_samples_ RTC_GUARDED_BY(network_thread_) = 0;
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-totalroundtriptime
  TimeDelta total_round_trip_time_ RTC_GUARDED_BY(network_thread_);
  // https://w3c.github.io/webrtc-stats/#dom-rtcicecandidatepairstats-currentroundtriptime
  std::optional<TimeDelta> current_round_trip_time_
      RTC_GUARDED_BY(network_thread_);
  // last time we sent a ping to the other side
  Timestamp last_ping_sent_ RTC_GUARDED_BY(network_thread_);
  // last time we received a ping from the other side
  Timestamp last_ping_received_ RTC_GUARDED_BY(network_thread_);
  Timestamp last_data_received_ RTC_GUARDED_BY(network_thread_);
  Timestamp last_ping_response_received_ RTC_GUARDED_BY(network_thread_);
  Timestamp receiving_unchanged_since_ RTC_GUARDED_BY(network_thread_);
  std::vector<SentPing> pings_since_last_response_
      RTC_GUARDED_BY(network_thread_);
  // Transaction ID of the last connectivity check received. Null if having not
  // received a ping yet.
  std::optional<std::string> last_ping_id_received_
      RTC_GUARDED_BY(network_thread_);

  std::optional<TimeDelta> unwritable_timeout_ RTC_GUARDED_BY(network_thread_);
  std::optional<int> unwritable_min_checks_ RTC_GUARDED_BY(network_thread_);
  std::optional<TimeDelta> inactive_timeout_ RTC_GUARDED_BY(network_thread_);

  IceCandidatePairState state_ RTC_GUARDED_BY(network_thread_);
  // Time duration to switch from receiving to not receiving.
  std::optional<TimeDelta> receiving_timeout_ RTC_GUARDED_BY(network_thread_);
  const Timestamp time_created_ RTC_GUARDED_BY(network_thread_);
  const TimeDelta delta_internal_unix_epoch_ RTC_GUARDED_BY(network_thread_);
  int num_pings_sent_ RTC_GUARDED_BY(network_thread_) = 0;

  std::optional<IceCandidatePairDescription> log_description_
      RTC_GUARDED_BY(network_thread_);
  IceEventLog* ice_event_log_ RTC_GUARDED_BY(network_thread_) = nullptr;

  // GOOG_PING_REQUEST is sent in place of STUN_BINDING_REQUEST
  // if configured via field trial, the remote peer supports it (signaled
  // in STUN_BINDING) and if the last STUN BINDING is identical to the one
  // that is about to be sent.
  std::optional<bool> remote_support_goog_ping_ RTC_GUARDED_BY(network_thread_);
  std::unique_ptr<StunMessage> cached_stun_binding_
      RTC_GUARDED_BY(network_thread_);

  const IceFieldTrials* field_trials_;
  EventBasedExponentialMovingAverage rtt_estimate_
      RTC_GUARDED_BY(network_thread_);

  std::optional<std::function<std::unique_ptr<StunAttribute>(
      const StunByteStringAttribute*)>>
      goog_delta_consumer_;
  std::optional<std::function<void(RTCErrorOr<const StunUInt64Attribute*>)>>
      goog_delta_ack_consumer_;
  absl::AnyInvocable<void(Connection*, const ReceivedIpPacket&)>
      received_packet_callback_;

  void MaybeAddDtlsPiggybackingAttributes(StunMessage* msg);
  void MaybeHandleDtlsPiggybackingAttributes(
      const StunMessage* msg,
      const StunRequest* original_request);
  DtlsStunPiggybackCallbacks dtls_stun_piggyback_callbacks_;

  CallbackList<Connection*> state_change_callbacks_;
  CallbackList<Connection*> destroyed_callbacks_;
  CallbackList<Connection*> ready_to_send_callbacks_;
  CallbackList<Connection*> nominated_callbacks_;
};

// ProxyConnection defers all the interesting work to the port.
class ProxyConnection : public Connection {
 public:
  ProxyConnection(const Environment& env,
                  WeakPtr<PortInterface> port,
                  size_t index,
                  const Candidate& remote_candidate);

  int Send(const void* data,
           size_t size,
           const AsyncSocketPacketOptions& options) override;
  int GetError() override;

 private:
  int error_ = 0;
};

}  //  namespace webrtc


#endif  // P2P_BASE_CONNECTION_H_
