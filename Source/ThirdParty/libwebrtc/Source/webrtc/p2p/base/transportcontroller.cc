/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/transportcontroller.h"

#include <algorithm>
#include <memory>

#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/dtlstransport.h"
#include "webrtc/p2p/base/p2ptransport.h"
#include "webrtc/p2p/base/port.h"

#ifdef HAVE_QUIC
#include "webrtc/p2p/quic/quictransport.h"
#endif  // HAVE_QUIC

namespace cricket {

enum {
  MSG_ICECONNECTIONSTATE,
  MSG_RECEIVING,
  MSG_ICEGATHERINGSTATE,
  MSG_CANDIDATESGATHERED,
};

struct CandidatesData : public rtc::MessageData {
  CandidatesData(const std::string& transport_name,
                 const Candidates& candidates)
      : transport_name(transport_name), candidates(candidates) {}

  std::string transport_name;
  Candidates candidates;
};

TransportController::TransportController(rtc::Thread* signaling_thread,
                                         rtc::Thread* network_thread,
                                         PortAllocator* port_allocator,
                                         bool redetermine_role_on_ice_restart)
    : signaling_thread_(signaling_thread),
      network_thread_(network_thread),
      port_allocator_(port_allocator),
      redetermine_role_on_ice_restart_(redetermine_role_on_ice_restart) {}

TransportController::TransportController(rtc::Thread* signaling_thread,
                                         rtc::Thread* network_thread,
                                         PortAllocator* port_allocator)
    : TransportController(signaling_thread,
                          network_thread,
                          port_allocator,
                          true) {}

TransportController::~TransportController() {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&TransportController::DestroyAllTransports_n, this));
  signaling_thread_->Clear(this);
}

bool TransportController::SetSslMaxProtocolVersion(
    rtc::SSLProtocolVersion version) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::SetSslMaxProtocolVersion_n,
                               this, version));
}

void TransportController::SetIceConfig(const IceConfig& config) {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&TransportController::SetIceConfig_n, this, config));
}

void TransportController::SetIceRole(IceRole ice_role) {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&TransportController::SetIceRole_n, this, ice_role));
}

bool TransportController::GetSslRole(const std::string& transport_name,
                                     rtc::SSLRole* role) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::GetSslRole_n, this,
                               transport_name, role));
}

bool TransportController::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::SetLocalCertificate_n,
                               this, certificate));
}

bool TransportController::GetLocalCertificate(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::GetLocalCertificate_n,
                               this, transport_name, certificate));
}

std::unique_ptr<rtc::SSLCertificate>
TransportController::GetRemoteSSLCertificate(
    const std::string& transport_name) {
  return network_thread_->Invoke<std::unique_ptr<rtc::SSLCertificate>>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::GetRemoteSSLCertificate_n,
                               this, transport_name));
}

bool TransportController::SetLocalTransportDescription(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE,
      rtc::Bind(&TransportController::SetLocalTransportDescription_n, this,
                transport_name, tdesc, action, err));
}

bool TransportController::SetRemoteTransportDescription(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE,
      rtc::Bind(&TransportController::SetRemoteTransportDescription_n, this,
                transport_name, tdesc, action, err));
}

void TransportController::MaybeStartGathering() {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&TransportController::MaybeStartGathering_n, this));
}

bool TransportController::AddRemoteCandidates(const std::string& transport_name,
                                              const Candidates& candidates,
                                              std::string* err) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::AddRemoteCandidates_n,
                               this, transport_name, candidates, err));
}

bool TransportController::RemoveRemoteCandidates(const Candidates& candidates,
                                                 std::string* err) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::RemoveRemoteCandidates_n,
                               this, candidates, err));
}

bool TransportController::ReadyForRemoteCandidates(
    const std::string& transport_name) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::ReadyForRemoteCandidates_n,
                               this, transport_name));
}

bool TransportController::GetStats(const std::string& transport_name,
                                   TransportStats* stats) {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE,
      rtc::Bind(&TransportController::GetStats_n, this, transport_name, stats));
}

TransportChannel* TransportController::CreateTransportChannel_n(
    const std::string& transport_name,
    int component) {
  RTC_DCHECK(network_thread_->IsCurrent());

  auto it = FindChannel_n(transport_name, component);
  if (it != channels_.end()) {
    // Channel already exists; increment reference count and return.
    it->AddRef();
    return it->get();
  }

  // Need to create a new channel.
  Transport* transport = GetOrCreateTransport_n(transport_name);
  TransportChannelImpl* channel = transport->CreateChannel(component);
  channel->SetMetricsObserver(metrics_observer_);
  channel->SignalWritableState.connect(
      this, &TransportController::OnChannelWritableState_n);
  channel->SignalReceivingState.connect(
      this, &TransportController::OnChannelReceivingState_n);
  channel->SignalGatheringState.connect(
      this, &TransportController::OnChannelGatheringState_n);
  channel->SignalCandidateGathered.connect(
      this, &TransportController::OnChannelCandidateGathered_n);
  channel->SignalCandidatesRemoved.connect(
      this, &TransportController::OnChannelCandidatesRemoved_n);
  channel->SignalRoleConflict.connect(
      this, &TransportController::OnChannelRoleConflict_n);
  channel->SignalStateChanged.connect(
      this, &TransportController::OnChannelStateChanged_n);
  channel->SignalDtlsHandshakeError.connect(
      this, &TransportController::OnDtlsHandshakeError);
  channels_.insert(channels_.end(), RefCountedChannel(channel))->AddRef();
  // Adding a channel could cause aggregate state to change.
  UpdateAggregateStates_n();
  return channel;
}

void TransportController::DestroyTransportChannel_n(
    const std::string& transport_name,
    int component) {
  RTC_DCHECK(network_thread_->IsCurrent());

  auto it = FindChannel_n(transport_name, component);
  if (it == channels_.end()) {
    LOG(LS_WARNING) << "Attempting to delete " << transport_name
                    << " TransportChannel " << component
                    << ", which doesn't exist.";
    return;
  }

  it->DecRef();
  if (it->ref() > 0) {
    return;
  }

  channels_.erase(it);
  Transport* transport = GetTransport_n(transport_name);
  transport->DestroyChannel(component);
  // Just as we create a Transport when its first channel is created,
  // we delete it when its last channel is deleted.
  if (!transport->HasChannels()) {
    DestroyTransport_n(transport_name);
  }
  // Removing a channel could cause aggregate state to change.
  UpdateAggregateStates_n();
}

const rtc::scoped_refptr<rtc::RTCCertificate>&
TransportController::certificate_for_testing() {
  return certificate_;
}

Transport* TransportController::CreateTransport_n(
    const std::string& transport_name) {
  RTC_DCHECK(network_thread_->IsCurrent());

#ifdef HAVE_QUIC
  if (quic_) {
    return new QuicTransport(transport_name, port_allocator(), certificate_);
  }
#endif  // HAVE_QUIC
  Transport* transport = new DtlsTransport<P2PTransport>(
      transport_name, port_allocator(), certificate_);
  return transport;
}

Transport* TransportController::GetTransport_n(
    const std::string& transport_name) {
  RTC_DCHECK(network_thread_->IsCurrent());

  auto iter = transports_.find(transport_name);
  return (iter != transports_.end()) ? iter->second : nullptr;
}

void TransportController::OnMessage(rtc::Message* pmsg) {
  RTC_DCHECK(signaling_thread_->IsCurrent());

  switch (pmsg->message_id) {
    case MSG_ICECONNECTIONSTATE: {
      rtc::TypedMessageData<IceConnectionState>* data =
          static_cast<rtc::TypedMessageData<IceConnectionState>*>(pmsg->pdata);
      SignalConnectionState(data->data());
      delete data;
      break;
    }
    case MSG_RECEIVING: {
      rtc::TypedMessageData<bool>* data =
          static_cast<rtc::TypedMessageData<bool>*>(pmsg->pdata);
      SignalReceiving(data->data());
      delete data;
      break;
    }
    case MSG_ICEGATHERINGSTATE: {
      rtc::TypedMessageData<IceGatheringState>* data =
          static_cast<rtc::TypedMessageData<IceGatheringState>*>(pmsg->pdata);
      SignalGatheringState(data->data());
      delete data;
      break;
    }
    case MSG_CANDIDATESGATHERED: {
      CandidatesData* data = static_cast<CandidatesData*>(pmsg->pdata);
      SignalCandidatesGathered(data->transport_name, data->candidates);
      delete data;
      break;
    }
    default:
      ASSERT(false);
  }
}

std::vector<TransportController::RefCountedChannel>::iterator
TransportController::FindChannel_n(const std::string& transport_name,
                                   int component) {
  return std::find_if(
      channels_.begin(), channels_.end(),
      [transport_name, component](const RefCountedChannel& channel) {
        return channel->transport_name() == transport_name &&
               channel->component() == component;
      });
}

Transport* TransportController::GetOrCreateTransport_n(
    const std::string& transport_name) {
  RTC_DCHECK(network_thread_->IsCurrent());

  Transport* transport = GetTransport_n(transport_name);
  if (transport) {
    return transport;
  }

  transport = CreateTransport_n(transport_name);
  // The stuff below happens outside of CreateTransport_w so that unit tests
  // can override CreateTransport_w to return a different type of transport.
  transport->SetSslMaxProtocolVersion(ssl_max_version_);
  transport->SetIceConfig(ice_config_);
  transport->SetIceRole(ice_role_);
  transport->SetIceTiebreaker(ice_tiebreaker_);
  if (certificate_) {
    transport->SetLocalCertificate(certificate_);
  }
  transports_[transport_name] = transport;

  return transport;
}

void TransportController::DestroyTransport_n(
    const std::string& transport_name) {
  RTC_DCHECK(network_thread_->IsCurrent());

  auto iter = transports_.find(transport_name);
  if (iter != transports_.end()) {
    delete iter->second;
    transports_.erase(transport_name);
  }
}

void TransportController::DestroyAllTransports_n() {
  RTC_DCHECK(network_thread_->IsCurrent());

  for (const auto& kv : transports_) {
    delete kv.second;
  }
  transports_.clear();
}

bool TransportController::SetSslMaxProtocolVersion_n(
    rtc::SSLProtocolVersion version) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Max SSL version can only be set before transports are created.
  if (!transports_.empty()) {
    return false;
  }

  ssl_max_version_ = version;
  return true;
}

void TransportController::SetIceConfig_n(const IceConfig& config) {
  RTC_DCHECK(network_thread_->IsCurrent());
  ice_config_ = config;
  for (const auto& kv : transports_) {
    kv.second->SetIceConfig(ice_config_);
  }
}

void TransportController::SetIceRole_n(IceRole ice_role) {
  RTC_DCHECK(network_thread_->IsCurrent());
  ice_role_ = ice_role;
  for (const auto& kv : transports_) {
    kv.second->SetIceRole(ice_role_);
  }
}

bool TransportController::GetSslRole_n(const std::string& transport_name,
                                       rtc::SSLRole* role) {
  RTC_DCHECK(network_thread_->IsCurrent());

  Transport* t = GetTransport_n(transport_name);
  if (!t) {
    return false;
  }

  return t->GetSslRole(role);
}

bool TransportController::SetLocalCertificate_n(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  RTC_DCHECK(network_thread_->IsCurrent());

  if (certificate_) {
    return false;
  }
  if (!certificate) {
    return false;
  }
  certificate_ = certificate;

  for (const auto& kv : transports_) {
    kv.second->SetLocalCertificate(certificate_);
  }
  return true;
}

bool TransportController::GetLocalCertificate_n(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
  RTC_DCHECK(network_thread_->IsCurrent());

  Transport* t = GetTransport_n(transport_name);
  if (!t) {
    return false;
  }

  return t->GetLocalCertificate(certificate);
}

std::unique_ptr<rtc::SSLCertificate>
TransportController::GetRemoteSSLCertificate_n(
    const std::string& transport_name) {
  RTC_DCHECK(network_thread_->IsCurrent());

  Transport* t = GetTransport_n(transport_name);
  if (!t) {
    return nullptr;
  }

  return t->GetRemoteSSLCertificate();
}

bool TransportController::SetLocalTransportDescription_n(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());

  Transport* transport = GetTransport_n(transport_name);
  if (!transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    // TODO(deadbeef): Make callers smarter so they won't attempt to set a
    // description on a deleted transport.
    return true;
  }

  // Older versions of Chrome expect the ICE role to be re-determined when an
  // ICE restart occurs, and also don't perform conflict resolution correctly,
  // so for now we can't safely stop doing this, unless the application opts in
  // by setting |redetermine_role_on_ice_restart_| to false.
  // See: https://bugs.chromium.org/p/chromium/issues/detail?id=628676
  // TODO(deadbeef): Remove this when these old versions of Chrome reach a low
  // enough population.
  if (redetermine_role_on_ice_restart_ && transport->local_description() &&
      IceCredentialsChanged(transport->local_description()->ice_ufrag,
                            transport->local_description()->ice_pwd,
                            tdesc.ice_ufrag, tdesc.ice_pwd)) {
    IceRole new_ice_role =
        (action == CA_OFFER) ? ICEROLE_CONTROLLING : ICEROLE_CONTROLLED;
    SetIceRole(new_ice_role);
  }

  LOG(LS_INFO) << "Set local transport description on " << transport_name;
  return transport->SetLocalTransportDescription(tdesc, action, err);
}

bool TransportController::SetRemoteTransportDescription_n(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());

  Transport* transport = GetTransport_n(transport_name);
  if (!transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    // TODO(deadbeef): Make callers smarter so they won't attempt to set a
    // description on a deleted transport.
    return true;
  }

  LOG(LS_INFO) << "Set remote transport description on " << transport_name;
  return transport->SetRemoteTransportDescription(tdesc, action, err);
}

void TransportController::MaybeStartGathering_n() {
  for (const auto& kv : transports_) {
    kv.second->MaybeStartGathering();
  }
}

bool TransportController::AddRemoteCandidates_n(
    const std::string& transport_name,
    const Candidates& candidates,
    std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());

  Transport* transport = GetTransport_n(transport_name);
  if (!transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    return true;
  }

  return transport->AddRemoteCandidates(candidates, err);
}

bool TransportController::RemoveRemoteCandidates_n(const Candidates& candidates,
                                                   std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());
  std::map<std::string, Candidates> candidates_by_transport_name;
  for (const Candidate& cand : candidates) {
    RTC_DCHECK(!cand.transport_name().empty());
    candidates_by_transport_name[cand.transport_name()].push_back(cand);
  }

  bool result = true;
  for (auto kv : candidates_by_transport_name) {
    Transport* transport = GetTransport_n(kv.first);
    if (!transport) {
      // If we didn't find a transport, that's not an error;
      // it could have been deleted as a result of bundling.
      continue;
    }
    result &= transport->RemoveRemoteCandidates(kv.second, err);
  }
  return result;
}

bool TransportController::ReadyForRemoteCandidates_n(
    const std::string& transport_name) {
  RTC_DCHECK(network_thread_->IsCurrent());

  Transport* transport = GetTransport_n(transport_name);
  if (!transport) {
    return false;
  }
  return transport->ready_for_remote_candidates();
}

bool TransportController::GetStats_n(const std::string& transport_name,
                                     TransportStats* stats) {
  RTC_DCHECK(network_thread_->IsCurrent());

  Transport* transport = GetTransport_n(transport_name);
  if (!transport) {
    return false;
  }
  return transport->GetStats(stats);
}

void TransportController::OnChannelWritableState_n(
    rtc::PacketTransportInterface* transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  LOG(LS_INFO) << " TransportChannel " << transport->debug_name()
               << " writability changed to " << transport->writable() << ".";
  UpdateAggregateStates_n();
}

void TransportController::OnChannelReceivingState_n(
    rtc::PacketTransportInterface* transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  UpdateAggregateStates_n();
}

void TransportController::OnChannelGatheringState_n(
    TransportChannelImpl* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  UpdateAggregateStates_n();
}

void TransportController::OnChannelCandidateGathered_n(
    TransportChannelImpl* channel,
    const Candidate& candidate) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // We should never signal peer-reflexive candidates.
  if (candidate.type() == PRFLX_PORT_TYPE) {
    RTC_DCHECK(false);
    return;
  }
  std::vector<Candidate> candidates;
  candidates.push_back(candidate);
  CandidatesData* data =
      new CandidatesData(channel->transport_name(), candidates);
  signaling_thread_->Post(RTC_FROM_HERE, this, MSG_CANDIDATESGATHERED, data);
}

void TransportController::OnChannelCandidatesRemoved_n(
    TransportChannelImpl* channel,
    const Candidates& candidates) {
  invoker_.AsyncInvoke<void>(
      RTC_FROM_HERE, signaling_thread_,
      rtc::Bind(&TransportController::OnChannelCandidatesRemoved, this,
                candidates));
}

void TransportController::OnChannelCandidatesRemoved(
    const Candidates& candidates) {
  RTC_DCHECK(signaling_thread_->IsCurrent());
  SignalCandidatesRemoved(candidates);
}

void TransportController::OnChannelRoleConflict_n(
    TransportChannelImpl* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  // Note: since the role conflict is handled entirely on the network thread,
  // we don't need to worry about role conflicts occurring on two ports at once.
  // The first one encountered should immediately reverse the role.
  IceRole reversed_role = (ice_role_ == ICEROLE_CONTROLLING)
                              ? ICEROLE_CONTROLLED
                              : ICEROLE_CONTROLLING;
  LOG(LS_INFO) << "Got role conflict; switching to "
               << (reversed_role == ICEROLE_CONTROLLING ? "controlling"
                                                        : "controlled")
               << " role.";
  SetIceRole_n(reversed_role);
}

void TransportController::OnChannelStateChanged_n(
    TransportChannelImpl* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  LOG(LS_INFO) << channel->transport_name() << " TransportChannel "
               << channel->component()
               << " state changed. Check if state is complete.";
  UpdateAggregateStates_n();
}

void TransportController::UpdateAggregateStates_n() {
  RTC_DCHECK(network_thread_->IsCurrent());

  IceConnectionState new_connection_state = kIceConnectionConnecting;
  IceGatheringState new_gathering_state = kIceGatheringNew;
  bool any_receiving = false;
  bool any_failed = false;
  bool all_connected = !channels_.empty();
  bool all_completed = !channels_.empty();
  bool any_gathering = false;
  bool all_done_gathering = !channels_.empty();
  for (const auto& channel : channels_) {
    any_receiving = any_receiving || channel->receiving();
    any_failed = any_failed ||
                 channel->GetState() == TransportChannelState::STATE_FAILED;
    all_connected = all_connected && channel->writable();
    all_completed =
        all_completed && channel->writable() &&
        channel->GetState() == TransportChannelState::STATE_COMPLETED &&
        channel->GetIceRole() == ICEROLE_CONTROLLING &&
        channel->gathering_state() == kIceGatheringComplete;
    any_gathering =
        any_gathering || channel->gathering_state() != kIceGatheringNew;
    all_done_gathering = all_done_gathering &&
                         channel->gathering_state() == kIceGatheringComplete;
  }

  if (any_failed) {
    new_connection_state = kIceConnectionFailed;
  } else if (all_completed) {
    new_connection_state = kIceConnectionCompleted;
  } else if (all_connected) {
    new_connection_state = kIceConnectionConnected;
  }
  if (connection_state_ != new_connection_state) {
    connection_state_ = new_connection_state;
    signaling_thread_->Post(
        RTC_FROM_HERE, this, MSG_ICECONNECTIONSTATE,
        new rtc::TypedMessageData<IceConnectionState>(new_connection_state));
  }

  if (receiving_ != any_receiving) {
    receiving_ = any_receiving;
    signaling_thread_->Post(RTC_FROM_HERE, this, MSG_RECEIVING,
                            new rtc::TypedMessageData<bool>(any_receiving));
  }

  if (all_done_gathering) {
    new_gathering_state = kIceGatheringComplete;
  } else if (any_gathering) {
    new_gathering_state = kIceGatheringGathering;
  }
  if (gathering_state_ != new_gathering_state) {
    gathering_state_ = new_gathering_state;
    signaling_thread_->Post(
        RTC_FROM_HERE, this, MSG_ICEGATHERINGSTATE,
        new rtc::TypedMessageData<IceGatheringState>(new_gathering_state));
  }
}

void TransportController::OnDtlsHandshakeError(rtc::SSLHandshakeError error) {
  SignalDtlsHandshakeError(error);
}

void TransportController::SetMetricsObserver(
    webrtc::MetricsObserverInterface* metrics_observer) {
  metrics_observer_ = metrics_observer;
  for (auto channel : channels_) {
    channel->SetMetricsObserver(metrics_observer);
  }
}

}  // namespace cricket
