/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_TRANSPORTCONTROLLER_H_
#define WEBRTC_P2P_BASE_TRANSPORTCONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/transport.h"

namespace rtc {
class Thread;
class PacketTransportInterface;
}
namespace webrtc {
class MetricsObserverInterface;
}

namespace cricket {

class TransportController : public sigslot::has_slots<>,
                            public rtc::MessageHandler {
 public:
  // If |redetermine_role_on_ice_restart| is true, ICE role is redetermined
  // upon setting a local transport description that indicates an ICE restart.
  // For the constructor that doesn't take this parameter, it defaults to true.
  TransportController(rtc::Thread* signaling_thread,
                      rtc::Thread* network_thread,
                      PortAllocator* port_allocator,
                      bool redetermine_role_on_ice_restart);

  TransportController(rtc::Thread* signaling_thread,
                      rtc::Thread* network_thread,
                      PortAllocator* port_allocator);

  virtual ~TransportController();

  rtc::Thread* signaling_thread() const { return signaling_thread_; }
  rtc::Thread* network_thread() const { return network_thread_; }

  PortAllocator* port_allocator() const { return port_allocator_; }

  // Can only be set before transports are created.
  // TODO(deadbeef): Make this an argument to the constructor once BaseSession
  // and WebRtcSession are combined
  bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version);

  void SetIceConfig(const IceConfig& config);
  void SetIceRole(IceRole ice_role);

  bool GetSslRole(const std::string& transport_name, rtc::SSLRole* role);

  // Specifies the identity to use in this session.
  // Can only be called once.
  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate);
  // Caller owns returned certificate
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& transport_name);
  bool SetLocalTransportDescription(const std::string& transport_name,
                                    const TransportDescription& tdesc,
                                    ContentAction action,
                                    std::string* err);
  bool SetRemoteTransportDescription(const std::string& transport_name,
                                     const TransportDescription& tdesc,
                                     ContentAction action,
                                     std::string* err);
  // Start gathering candidates for any new transports, or transports doing an
  // ICE restart.
  void MaybeStartGathering();
  bool AddRemoteCandidates(const std::string& transport_name,
                           const Candidates& candidates,
                           std::string* err);
  bool RemoveRemoteCandidates(const Candidates& candidates, std::string* err);
  bool ReadyForRemoteCandidates(const std::string& transport_name);
  bool GetStats(const std::string& transport_name, TransportStats* stats);

  // Creates a channel if it doesn't exist. Otherwise, increments a reference
  // count and returns an existing channel.
  virtual TransportChannel* CreateTransportChannel_n(
      const std::string& transport_name,
      int component);

  // Decrements a channel's reference count, and destroys the channel if
  // nothing is referencing it.
  virtual void DestroyTransportChannel_n(const std::string& transport_name,
                                         int component);

  void use_quic() { quic_ = true; }
  bool quic() const { return quic_; }

  // All of these signals are fired on the signalling thread.

  // If any transport failed => failed,
  // Else if all completed => completed,
  // Else if all connected => connected,
  // Else => connecting
  sigslot::signal1<IceConnectionState> SignalConnectionState;

  // Receiving if any transport is receiving
  sigslot::signal1<bool> SignalReceiving;

  // If all transports done gathering => complete,
  // Else if any are gathering => gathering,
  // Else => new
  sigslot::signal1<IceGatheringState> SignalGatheringState;

  // (transport_name, candidates)
  sigslot::signal2<const std::string&, const Candidates&>
      SignalCandidatesGathered;

  sigslot::signal1<const Candidates&> SignalCandidatesRemoved;

  // for unit test
  const rtc::scoped_refptr<rtc::RTCCertificate>& certificate_for_testing();

  sigslot::signal1<rtc::SSLHandshakeError> SignalDtlsHandshakeError;

  void SetMetricsObserver(webrtc::MetricsObserverInterface* metrics_observer);

 protected:
  // Protected and virtual so we can override it in unit tests.
  virtual Transport* CreateTransport_n(const std::string& transport_name);

  // For unit tests
  const std::map<std::string, Transport*>& transports() { return transports_; }
  Transport* GetTransport_n(const std::string& transport_name);

 private:
  void OnMessage(rtc::Message* pmsg) override;

  // It's the Transport that's currently responsible for creating/destroying
  // channels, but the TransportController keeps track of how many external
  // objects (BaseChannels) reference each channel.
  struct RefCountedChannel {
    RefCountedChannel() : impl_(nullptr), ref_(0) {}
    explicit RefCountedChannel(TransportChannelImpl* impl)
        : impl_(impl), ref_(0) {}

    void AddRef() { ++ref_; }
    void DecRef() {
      ASSERT(ref_ > 0);
      --ref_;
    }
    int ref() const { return ref_; }

    TransportChannelImpl* get() const { return impl_; }
    TransportChannelImpl* operator->() const { return impl_; }

   private:
    TransportChannelImpl* impl_;
    int ref_;
  };

  std::vector<RefCountedChannel>::iterator FindChannel_n(
      const std::string& transport_name,
      int component);

  Transport* GetOrCreateTransport_n(const std::string& transport_name);
  void DestroyTransport_n(const std::string& transport_name);
  void DestroyAllTransports_n();

  bool SetSslMaxProtocolVersion_n(rtc::SSLProtocolVersion version);
  void SetIceConfig_n(const IceConfig& config);
  void SetIceRole_n(IceRole ice_role);
  bool GetSslRole_n(const std::string& transport_name, rtc::SSLRole* role);
  bool SetLocalCertificate_n(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  bool GetLocalCertificate_n(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate);
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate_n(
      const std::string& transport_name);
  bool SetLocalTransportDescription_n(const std::string& transport_name,
                                      const TransportDescription& tdesc,
                                      ContentAction action,
                                      std::string* err);
  bool SetRemoteTransportDescription_n(const std::string& transport_name,
                                       const TransportDescription& tdesc,
                                       ContentAction action,
                                       std::string* err);
  void MaybeStartGathering_n();
  bool AddRemoteCandidates_n(const std::string& transport_name,
                             const Candidates& candidates,
                             std::string* err);
  bool RemoveRemoteCandidates_n(const Candidates& candidates, std::string* err);
  bool ReadyForRemoteCandidates_n(const std::string& transport_name);
  bool GetStats_n(const std::string& transport_name, TransportStats* stats);

  // Handlers for signals from Transport.
  void OnChannelWritableState_n(rtc::PacketTransportInterface* transport);
  void OnChannelReceivingState_n(rtc::PacketTransportInterface* transport);
  void OnChannelGatheringState_n(TransportChannelImpl* channel);
  void OnChannelCandidateGathered_n(TransportChannelImpl* channel,
                                    const Candidate& candidate);
  void OnChannelCandidatesRemoved(const Candidates& candidates);
  void OnChannelCandidatesRemoved_n(TransportChannelImpl* channel,
                                    const Candidates& candidates);
  void OnChannelRoleConflict_n(TransportChannelImpl* channel);
  void OnChannelStateChanged_n(TransportChannelImpl* channel);

  void UpdateAggregateStates_n();

  void OnDtlsHandshakeError(rtc::SSLHandshakeError error);

  rtc::Thread* const signaling_thread_ = nullptr;
  rtc::Thread* const network_thread_ = nullptr;
  typedef std::map<std::string, Transport*> TransportMap;
  TransportMap transports_;

  std::vector<RefCountedChannel> channels_;

  PortAllocator* const port_allocator_ = nullptr;
  rtc::SSLProtocolVersion ssl_max_version_ = rtc::SSL_PROTOCOL_DTLS_12;

  // Aggregate state for TransportChannelImpls.
  IceConnectionState connection_state_ = kIceConnectionConnecting;
  bool receiving_ = false;
  IceGatheringState gathering_state_ = kIceGatheringNew;

  // TODO(deadbeef): Move the fields below down to the transports themselves
  IceConfig ice_config_;
  IceRole ice_role_ = ICEROLE_CONTROLLING;
  bool redetermine_role_on_ice_restart_;
  uint64_t ice_tiebreaker_ = rtc::CreateRandomId64();
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  rtc::AsyncInvoker invoker_;
  // True if QUIC is used instead of DTLS.
  bool quic_ = false;

  webrtc::MetricsObserverInterface* metrics_observer_ = nullptr;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TRANSPORTCONTROLLER_H_
