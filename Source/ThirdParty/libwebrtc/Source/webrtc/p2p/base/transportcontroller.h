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
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/refcountedobject.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/dtlstransportchannel.h"
#include "webrtc/p2p/base/jseptransport.h"
#include "webrtc/p2p/base/p2ptransportchannel.h"

namespace rtc {
class Thread;
class PacketTransportInternal;
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
  //
  // |crypto_options| is used to determine if created DTLS transports negotiate
  // GCM crypto suites or not.
  TransportController(rtc::Thread* signaling_thread,
                      rtc::Thread* network_thread,
                      PortAllocator* port_allocator,
                      bool redetermine_role_on_ice_restart,
                      const rtc::CryptoOptions& crypto_options);

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

  // Set the "needs-ice-restart" flag as described in JSEP. After the flag is
  // set, offers should generate new ufrags/passwords until an ICE restart
  // occurs.
  void SetNeedsIceRestartFlag();
  // Returns true if the ICE restart flag above was set, and no ICE restart has
  // occurred yet for this transport (by applying a local description with
  // changed ufrag/password). If the transport has been deleted as a result of
  // bundling, returns false.
  bool NeedsIceRestart(const std::string& transport_name) const;

  bool GetSslRole(const std::string& transport_name, rtc::SSLRole* role) const;

  // Specifies the identity to use in this session.
  // Can only be called once.
  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const;
  // Caller owns returned certificate. This method mainly exists for stats
  // reporting.
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& transport_name) const;
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
  bool ReadyForRemoteCandidates(const std::string& transport_name) const;
  // TODO(deadbeef): GetStats isn't const because all the way down to
  // OpenSSLStreamAdapter,
  // GetSslCipherSuite and GetDtlsSrtpCryptoSuite are not const. Fix this.
  bool GetStats(const std::string& transport_name, TransportStats* stats);
  void SetMetricsObserver(webrtc::MetricsObserverInterface* metrics_observer);

  // Creates a channel if it doesn't exist. Otherwise, increments a reference
  // count and returns an existing channel.
  DtlsTransportInternal* CreateDtlsTransport(const std::string& transport_name,
                                             int component);
  virtual DtlsTransportInternal* CreateDtlsTransport_n(
      const std::string& transport_name,
      int component);

  // Decrements a channel's reference count, and destroys the channel if
  // nothing is referencing it.
  virtual void DestroyDtlsTransport(const std::string& transport_name,
                                    int component);
  virtual void DestroyDtlsTransport_n(const std::string& transport_name,
                                      int component);

  void use_quic() { quic_ = true; }
  bool quic() const { return quic_; }

  // TODO(deadbeef): Remove all for_testing methods!
  const rtc::scoped_refptr<rtc::RTCCertificate>& certificate_for_testing()
      const {
    return certificate_;
  }
  std::vector<std::string> transport_names_for_testing();
  std::vector<DtlsTransportInternal*> channels_for_testing();
  DtlsTransportInternal* get_channel_for_testing(
      const std::string& transport_name,
      int component);

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

  sigslot::signal1<rtc::SSLHandshakeError> SignalDtlsHandshakeError;

 protected:
  // TODO(deadbeef): Get rid of these virtual methods. Used by
  // FakeTransportController currently, but FakeTransportController shouldn't
  // even be functioning by subclassing TransportController.
  virtual IceTransportInternal* CreateIceTransportChannel_n(
      const std::string& transport_name,
      int component);
  virtual DtlsTransportInternal* CreateDtlsTransportChannel_n(
      const std::string& transport_name,
      int component,
      IceTransportInternal* ice);

 private:
  void OnMessage(rtc::Message* pmsg) override;

  class ChannelPair;
  typedef rtc::RefCountedObject<ChannelPair> RefCountedChannel;

  // Helper functions to get a channel or transport, or iterator to it (in case
  // it needs to be erased).
  std::vector<RefCountedChannel*>::iterator GetChannelIterator_n(
      const std::string& transport_name,
      int component);
  std::vector<RefCountedChannel*>::const_iterator GetChannelIterator_n(
      const std::string& transport_name,
      int component) const;
  const JsepTransport* GetJsepTransport(
      const std::string& transport_name) const;
  JsepTransport* GetJsepTransport(const std::string& transport_name);
  const RefCountedChannel* GetChannel_n(const std::string& transport_name,
                                        int component) const;
  RefCountedChannel* GetChannel_n(const std::string& transport_name,
                                  int component);

  JsepTransport* GetOrCreateJsepTransport(const std::string& transport_name);
  void DestroyAllChannels_n();

  bool SetSslMaxProtocolVersion_n(rtc::SSLProtocolVersion version);
  void SetIceConfig_n(const IceConfig& config);
  void SetIceRole_n(IceRole ice_role);
  bool GetSslRole_n(const std::string& transport_name,
                    rtc::SSLRole* role) const;
  bool SetLocalCertificate_n(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate);
  bool GetLocalCertificate_n(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const;
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate_n(
      const std::string& transport_name) const;
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
  bool ReadyForRemoteCandidates_n(const std::string& transport_name) const;
  bool GetStats_n(const std::string& transport_name, TransportStats* stats);
  void SetMetricsObserver_n(webrtc::MetricsObserverInterface* metrics_observer);

  // Handlers for signals from Transport.
  void OnChannelWritableState_n(rtc::PacketTransportInternal* transport);
  void OnChannelReceivingState_n(rtc::PacketTransportInternal* transport);
  void OnChannelGatheringState_n(IceTransportInternal* channel);
  void OnChannelCandidateGathered_n(IceTransportInternal* channel,
                                    const Candidate& candidate);
  void OnChannelCandidatesRemoved(const Candidates& candidates);
  void OnChannelCandidatesRemoved_n(IceTransportInternal* channel,
                                    const Candidates& candidates);
  void OnChannelRoleConflict_n(IceTransportInternal* channel);
  void OnChannelStateChanged_n(IceTransportInternal* channel);

  void UpdateAggregateStates_n();

  void OnDtlsHandshakeError(rtc::SSLHandshakeError error);

  rtc::Thread* const signaling_thread_ = nullptr;
  rtc::Thread* const network_thread_ = nullptr;
  PortAllocator* const port_allocator_ = nullptr;

  std::map<std::string, std::unique_ptr<JsepTransport>> transports_;
  std::vector<RefCountedChannel*> channels_;

  // Aggregate state for TransportChannelImpls.
  IceConnectionState connection_state_ = kIceConnectionConnecting;
  bool receiving_ = false;
  IceGatheringState gathering_state_ = kIceGatheringNew;

  IceConfig ice_config_;
  IceRole ice_role_ = ICEROLE_CONTROLLING;
  bool redetermine_role_on_ice_restart_;
  uint64_t ice_tiebreaker_ = rtc::CreateRandomId64();
  rtc::CryptoOptions crypto_options_;
  rtc::SSLProtocolVersion ssl_max_version_ = rtc::SSL_PROTOCOL_DTLS_12;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  rtc::AsyncInvoker invoker_;
  // True if QUIC is used instead of DTLS.
  bool quic_ = false;

  webrtc::MetricsObserverInterface* metrics_observer_ = nullptr;

  RTC_DISALLOW_COPY_AND_ASSIGN(TransportController);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TRANSPORTCONTROLLER_H_
