/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/transportcontroller.h"

#include <algorithm>
#include <memory>

#include "p2p/base/port.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread.h"

namespace {

enum {
  MSG_ICECONNECTIONSTATE,
  MSG_RECEIVING,
  MSG_ICEGATHERINGSTATE,
  MSG_CANDIDATESGATHERED,
};

struct CandidatesData : public rtc::MessageData {
  CandidatesData(const std::string& transport_name,
                 const cricket::Candidates& candidates)
      : transport_name(transport_name), candidates(candidates) {}

  std::string transport_name;
  cricket::Candidates candidates;
};

}  // namespace

namespace cricket {

// This class groups the DTLS and ICE channels, and helps keep track of
// how many external objects (BaseChannels) reference each channel.
class TransportController::ChannelPair {
 public:
  // TODO(deadbeef): Change the types of |dtls| and |ice| to
  // DtlsTransport and P2PTransportChannelWrapper, once TransportChannelImpl is
  // removed.
  ChannelPair(DtlsTransportInternal* dtls, IceTransportInternal* ice)
      : ice_(ice), dtls_(dtls) {}

  // Currently, all ICE-related calls still go through this DTLS channel. But
  // that will change once we get rid of TransportChannelImpl, and the DTLS
  // channel interface no longer includes ICE-specific methods.
  const DtlsTransportInternal* dtls() const { return dtls_.get(); }
  DtlsTransportInternal* dtls() { return dtls_.get(); }
  const IceTransportInternal* ice() const { return ice_.get(); }
  IceTransportInternal* ice() { return ice_.get(); }

 private:
  std::unique_ptr<IceTransportInternal> ice_;
  std::unique_ptr<DtlsTransportInternal> dtls_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ChannelPair);
};

TransportController::TransportController(
    rtc::Thread* signaling_thread,
    rtc::Thread* network_thread,
    PortAllocator* port_allocator,
    bool redetermine_role_on_ice_restart,
    const rtc::CryptoOptions& crypto_options)
    : signaling_thread_(signaling_thread),
      network_thread_(network_thread),
      port_allocator_(port_allocator),
      redetermine_role_on_ice_restart_(redetermine_role_on_ice_restart),
      crypto_options_(crypto_options) {}

TransportController::~TransportController() {
  // Channel destructors may try to send packets, so this needs to happen on
  // the network thread.
  network_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&TransportController::DestroyAllChannels_n, this));
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

void TransportController::SetNeedsIceRestartFlag() {
  for (auto& kv : transports_) {
    kv.second->SetNeedsIceRestartFlag();
  }
}

bool TransportController::NeedsIceRestart(
    const std::string& transport_name) const {
  const JsepTransport* transport = GetJsepTransport(transport_name);
  if (!transport) {
    return false;
  }
  return transport->NeedsIceRestart();
}

bool TransportController::GetSslRole(const std::string& transport_name,
                                     rtc::SSLRole* role) const {
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
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const {
  if (network_thread_->IsCurrent()) {
    return GetLocalCertificate_n(transport_name, certificate);
  }
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::GetLocalCertificate_n,
                               this, transport_name, certificate));
}

std::unique_ptr<rtc::SSLCertificate>
TransportController::GetRemoteSSLCertificate(
    const std::string& transport_name) const {
  if (network_thread_->IsCurrent()) {
    return GetRemoteSSLCertificate_n(transport_name);
  }
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
    const std::string& transport_name) const {
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::ReadyForRemoteCandidates_n,
                               this, transport_name));
}

bool TransportController::GetStats(const std::string& transport_name,
                                   TransportStats* stats) {
  if (network_thread_->IsCurrent()) {
    return GetStats_n(transport_name, stats);
  }
  return network_thread_->Invoke<bool>(
      RTC_FROM_HERE,
      rtc::Bind(&TransportController::GetStats_n, this, transport_name, stats));
}

void TransportController::SetMetricsObserver(
    webrtc::MetricsObserverInterface* metrics_observer) {
  return network_thread_->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::SetMetricsObserver_n, this,
                               metrics_observer));
}

DtlsTransportInternal* TransportController::CreateDtlsTransport(
    const std::string& transport_name,
    int component) {
  return network_thread_->Invoke<DtlsTransportInternal*>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::CreateDtlsTransport_n,
                               this, transport_name, component));
}

DtlsTransportInternal* TransportController::CreateDtlsTransport_n(
    const std::string& transport_name,
    int component) {
  RTC_DCHECK(network_thread_->IsCurrent());

  RefCountedChannel* existing_channel = GetChannel_n(transport_name, component);
  if (existing_channel) {
    // Channel already exists; increment reference count and return.
    existing_channel->AddRef();
    return existing_channel->dtls();
  }

  // Need to create a new channel.
  JsepTransport* transport = GetOrCreateJsepTransport(transport_name);

  // Create DTLS channel wrapping ICE channel, and configure it.
  IceTransportInternal* ice =
      CreateIceTransportChannel_n(transport_name, component);
  DtlsTransportInternal* dtls =
      CreateDtlsTransportChannel_n(transport_name, component, ice);
  dtls->ice_transport()->SetMetricsObserver(metrics_observer_);
  dtls->ice_transport()->SetIceRole(ice_role_);
  dtls->ice_transport()->SetIceTiebreaker(ice_tiebreaker_);
  dtls->ice_transport()->SetIceConfig(ice_config_);
  if (certificate_) {
    bool set_cert_success = dtls->SetLocalCertificate(certificate_);
    RTC_DCHECK(set_cert_success);
  }

  // Connect to signals offered by the channels. Currently, the DTLS channel
  // forwards signals from the ICE channel, so we only need to connect to the
  // DTLS channel. In the future this won't be the case.
  dtls->SignalWritableState.connect(
      this, &TransportController::OnChannelWritableState_n);
  dtls->SignalReceivingState.connect(
      this, &TransportController::OnChannelReceivingState_n);
  dtls->SignalDtlsHandshakeError.connect(
      this, &TransportController::OnDtlsHandshakeError);
  dtls->ice_transport()->SignalGatheringState.connect(
      this, &TransportController::OnChannelGatheringState_n);
  dtls->ice_transport()->SignalCandidateGathered.connect(
      this, &TransportController::OnChannelCandidateGathered_n);
  dtls->ice_transport()->SignalCandidatesRemoved.connect(
      this, &TransportController::OnChannelCandidatesRemoved_n);
  dtls->ice_transport()->SignalRoleConflict.connect(
      this, &TransportController::OnChannelRoleConflict_n);
  dtls->ice_transport()->SignalStateChanged.connect(
      this, &TransportController::OnChannelStateChanged_n);
  RefCountedChannel* new_pair = new RefCountedChannel(dtls, ice);
  new_pair->AddRef();
  channels_.insert(channels_.end(), new_pair);
  bool channel_added = transport->AddChannel(dtls, component);
  RTC_DCHECK(channel_added);
  // Adding a channel could cause aggregate state to change.
  UpdateAggregateStates_n();
  return dtls;
}

void TransportController::DestroyDtlsTransport(
    const std::string& transport_name,
    int component) {
  network_thread_->Invoke<void>(
      RTC_FROM_HERE, rtc::Bind(&TransportController::DestroyDtlsTransport_n,
                               this, transport_name, component));
}

void TransportController::DestroyDtlsTransport_n(
    const std::string& transport_name,
    int component) {
  RTC_DCHECK(network_thread_->IsCurrent());
  auto it = GetChannelIterator_n(transport_name, component);
  if (it == channels_.end()) {
    RTC_LOG(LS_WARNING) << "Attempting to delete " << transport_name
                        << " TransportChannel " << component
                        << ", which doesn't exist.";
    return;
  }
  // Release one reference to the RefCountedChannel, and do additional cleanup
  // only if it was the last one. Matches the AddRef logic in
  // CreateDtlsTransport_n.
  if ((*it)->Release() == rtc::RefCountReleaseStatus::kOtherRefsRemained) {
    return;
  }
  channels_.erase(it);

  JsepTransport* t = GetJsepTransport(transport_name);
  bool channel_removed = t->RemoveChannel(component);
  RTC_DCHECK(channel_removed);
  // Just as we create a Transport when its first channel is created,
  // we delete it when its last channel is deleted.
  if (!t->HasChannels()) {
    transports_.erase(transport_name);
  }
  // Removing a channel could cause aggregate state to change.
  UpdateAggregateStates_n();
}

std::vector<std::string> TransportController::transport_names_for_testing() {
  std::vector<std::string> ret;
  for (const auto& kv : transports_) {
    ret.push_back(kv.first);
  }
  return ret;
}

std::vector<DtlsTransportInternal*>
TransportController::channels_for_testing() {
  std::vector<DtlsTransportInternal*> ret;
  for (RefCountedChannel* channel : channels_) {
    ret.push_back(channel->dtls());
  }
  return ret;
}

DtlsTransportInternal* TransportController::get_channel_for_testing(
    const std::string& transport_name,
    int component) {
  RefCountedChannel* ch = GetChannel_n(transport_name, component);
  return ch ? ch->dtls() : nullptr;
}

IceTransportInternal* TransportController::CreateIceTransportChannel_n(
    const std::string& transport_name,
    int component) {
  return new P2PTransportChannel(transport_name, component, port_allocator_);
}

DtlsTransportInternal* TransportController::CreateDtlsTransportChannel_n(
    const std::string&,
    int,
    IceTransportInternal* ice) {
  DtlsTransport* dtls = new DtlsTransport(ice, crypto_options_);
  dtls->SetSslMaxProtocolVersion(ssl_max_version_);
  return dtls;
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
      RTC_NOTREACHED();
  }
}

std::vector<TransportController::RefCountedChannel*>::iterator
TransportController::GetChannelIterator_n(const std::string& transport_name,
                                          int component) {
  RTC_DCHECK(network_thread_->IsCurrent());
  return std::find_if(channels_.begin(), channels_.end(),
                      [transport_name, component](RefCountedChannel* channel) {
                        return channel->dtls()->transport_name() ==
                                   transport_name &&
                               channel->dtls()->component() == component;
                      });
}

std::vector<TransportController::RefCountedChannel*>::const_iterator
TransportController::GetChannelIterator_n(const std::string& transport_name,
                                          int component) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  return std::find_if(
      channels_.begin(), channels_.end(),
      [transport_name, component](const RefCountedChannel* channel) {
        return channel->dtls()->transport_name() == transport_name &&
               channel->dtls()->component() == component;
      });
}

const JsepTransport* TransportController::GetJsepTransport(
    const std::string& transport_name) const {
  auto it = transports_.find(transport_name);
  return (it == transports_.end()) ? nullptr : it->second.get();
}

JsepTransport* TransportController::GetJsepTransport(
    const std::string& transport_name) {
  auto it = transports_.find(transport_name);
  return (it == transports_.end()) ? nullptr : it->second.get();
}

const TransportController::RefCountedChannel* TransportController::GetChannel_n(
    const std::string& transport_name,
    int component) const {
  RTC_DCHECK(network_thread_->IsCurrent());
  auto it = GetChannelIterator_n(transport_name, component);
  return (it == channels_.end()) ? nullptr : *it;
}

TransportController::RefCountedChannel* TransportController::GetChannel_n(
    const std::string& transport_name,
    int component) {
  RTC_DCHECK(network_thread_->IsCurrent());
  auto it = GetChannelIterator_n(transport_name, component);
  return (it == channels_.end()) ? nullptr : *it;
}

JsepTransport* TransportController::GetOrCreateJsepTransport(
    const std::string& transport_name) {
  RTC_DCHECK(network_thread_->IsCurrent());

  JsepTransport* transport = GetJsepTransport(transport_name);
  if (transport) {
    return transport;
  }

  transport = new JsepTransport(transport_name, certificate_);
  transports_[transport_name] = std::unique_ptr<JsepTransport>(transport);
  return transport;
}

void TransportController::DestroyAllChannels_n() {
  RTC_DCHECK(network_thread_->IsCurrent());
  transports_.clear();
  // TODO(nisse): If |channels_| were a vector of scoped_refptr, we
  // wouldn't need this strange hack.
  for (RefCountedChannel* channel : channels_) {
    // Even though these objects are normally ref-counted, if
    // TransportController is deleted while they still have references, just
    // remove all references.
    while (channel->Release() ==
           rtc::RefCountReleaseStatus::kOtherRefsRemained) {
    }
  }
  channels_.clear();
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
  for (auto& channel : channels_) {
    channel->dtls()->ice_transport()->SetIceConfig(ice_config_);
  }
}

void TransportController::SetIceRole_n(IceRole ice_role) {
  RTC_DCHECK(network_thread_->IsCurrent());

  ice_role_ = ice_role;
  for (auto& channel : channels_) {
    channel->dtls()->ice_transport()->SetIceRole(ice_role_);
  }
}

bool TransportController::GetSslRole_n(const std::string& transport_name,
                                       rtc::SSLRole* role) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  const JsepTransport* t = GetJsepTransport(transport_name);
  if (!t) {
    return false;
  }
  rtc::Optional<rtc::SSLRole> current_role = t->GetSslRole();
  if (!current_role) {
    return false;
  }
  *role = *current_role;
  return true;
}

bool TransportController::SetLocalCertificate_n(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Can't change a certificate, or set a null certificate.
  if (certificate_ || !certificate) {
    return false;
  }
  certificate_ = certificate;

  // Set certificate for JsepTransport, which verifies it matches the
  // fingerprint in SDP, and DTLS transport.
  // Fallback from DTLS to SDES is not supported.
  for (auto& kv : transports_) {
    kv.second->SetLocalCertificate(certificate_);
  }
  for (auto& channel : channels_) {
    bool set_cert_success = channel->dtls()->SetLocalCertificate(certificate_);
    RTC_DCHECK(set_cert_success);
  }
  return true;
}

bool TransportController::GetLocalCertificate_n(
    const std::string& transport_name,
    rtc::scoped_refptr<rtc::RTCCertificate>* certificate) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  const JsepTransport* t = GetJsepTransport(transport_name);
  if (!t) {
    return false;
  }
  return t->GetLocalCertificate(certificate);
}

std::unique_ptr<rtc::SSLCertificate>
TransportController::GetRemoteSSLCertificate_n(
    const std::string& transport_name) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Get the certificate from the RTP channel's DTLS handshake. Should be
  // identical to the RTCP channel's, since they were given the same remote
  // fingerprint.
  const RefCountedChannel* ch = GetChannel_n(transport_name, 1);
  if (!ch) {
    return nullptr;
  }
  return ch->dtls()->GetRemoteSSLCertificate();
}

bool TransportController::SetLocalTransportDescription_n(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());

  JsepTransport* transport = GetJsepTransport(transport_name);
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
                            tdesc.ice_ufrag, tdesc.ice_pwd) &&
      // Don't change the ICE role if the remote endpoint is ICE lite; we
      // should always be controlling in that case.
      (!transport->remote_description() ||
       transport->remote_description()->ice_mode != ICEMODE_LITE)) {
    IceRole new_ice_role =
        (action == CA_OFFER) ? ICEROLE_CONTROLLING : ICEROLE_CONTROLLED;
    SetIceRole(new_ice_role);
  }

  RTC_LOG(LS_INFO) << "Set local transport description on " << transport_name;
  return transport->SetLocalTransportDescription(tdesc, action, err);
}

bool TransportController::SetRemoteTransportDescription_n(
    const std::string& transport_name,
    const TransportDescription& tdesc,
    ContentAction action,
    std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // If our role is ICEROLE_CONTROLLED and the remote endpoint supports only
  // ice_lite, this local endpoint should take the CONTROLLING role.
  // TODO(deadbeef): This is a session-level attribute, so it really shouldn't
  // be in a TransportDescription in the first place...
  if (ice_role_ == ICEROLE_CONTROLLED && tdesc.ice_mode == ICEMODE_LITE) {
    SetIceRole_n(ICEROLE_CONTROLLING);
  }

  JsepTransport* transport = GetJsepTransport(transport_name);
  if (!transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    // TODO(deadbeef): Make callers smarter so they won't attempt to set a
    // description on a deleted transport.
    return true;
  }

  RTC_LOG(LS_INFO) << "Set remote transport description on " << transport_name;
  return transport->SetRemoteTransportDescription(tdesc, action, err);
}

void TransportController::MaybeStartGathering_n() {
  for (auto& channel : channels_) {
    channel->dtls()->ice_transport()->MaybeStartGathering();
  }
}

bool TransportController::AddRemoteCandidates_n(
    const std::string& transport_name,
    const Candidates& candidates,
    std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Verify each candidate before passing down to the transport layer.
  if (!VerifyCandidates(candidates, err)) {
    return false;
  }

  JsepTransport* transport = GetJsepTransport(transport_name);
  if (!transport) {
    // If we didn't find a transport, that's not an error;
    // it could have been deleted as a result of bundling.
    return true;
  }

  for (const Candidate& candidate : candidates) {
    RefCountedChannel* channel =
        GetChannel_n(transport_name, candidate.component());
    if (!channel) {
      *err = "Candidate has an unknown component: " + candidate.ToString() +
             " for content: " + transport_name;
      return false;
    }
    channel->dtls()->ice_transport()->AddRemoteCandidate(candidate);
  }
  return true;
}

bool TransportController::RemoveRemoteCandidates_n(const Candidates& candidates,
                                                   std::string* err) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // Verify each candidate before passing down to the transport layer.
  if (!VerifyCandidates(candidates, err)) {
    return false;
  }

  std::map<std::string, Candidates> candidates_by_transport_name;
  for (const Candidate& cand : candidates) {
    if (!cand.transport_name().empty()) {
      candidates_by_transport_name[cand.transport_name()].push_back(cand);
    } else {
      RTC_LOG(LS_ERROR) << "Not removing candidate because it does not have a "
                           "transport name set: "
                        << cand.ToString();
    }
  }

  bool result = true;
  for (const auto& kv : candidates_by_transport_name) {
    const std::string& transport_name = kv.first;
    const Candidates& candidates = kv.second;
    JsepTransport* transport = GetJsepTransport(transport_name);
    if (!transport) {
      // If we didn't find a transport, that's not an error;
      // it could have been deleted as a result of bundling.
      continue;
    }
    for (const Candidate& candidate : candidates) {
      RefCountedChannel* channel =
          GetChannel_n(transport_name, candidate.component());
      if (channel) {
        channel->dtls()->ice_transport()->RemoveRemoteCandidate(candidate);
      }
    }
  }
  return result;
}

bool TransportController::ReadyForRemoteCandidates_n(
    const std::string& transport_name) const {
  RTC_DCHECK(network_thread_->IsCurrent());

  const JsepTransport* transport = GetJsepTransport(transport_name);
  if (!transport) {
    return false;
  }
  return transport->ready_for_remote_candidates();
}

bool TransportController::GetStats_n(const std::string& transport_name,
                                     TransportStats* stats) {
  RTC_DCHECK(network_thread_->IsCurrent());

  JsepTransport* transport = GetJsepTransport(transport_name);
  if (!transport) {
    return false;
  }
  return transport->GetStats(stats);
}

void TransportController::SetMetricsObserver_n(
    webrtc::MetricsObserverInterface* metrics_observer) {
  RTC_DCHECK(network_thread_->IsCurrent());
  metrics_observer_ = metrics_observer;
  for (auto& channel : channels_) {
    channel->dtls()->ice_transport()->SetMetricsObserver(metrics_observer);
  }
}

void TransportController::OnChannelWritableState_n(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  RTC_LOG(LS_INFO) << " Transport " << transport->transport_name()
                   << " writability changed to " << transport->writable()
                   << ".";
  UpdateAggregateStates_n();
}

void TransportController::OnChannelReceivingState_n(
    rtc::PacketTransportInternal* transport) {
  RTC_DCHECK(network_thread_->IsCurrent());
  UpdateAggregateStates_n();
}

void TransportController::OnChannelGatheringState_n(
    IceTransportInternal* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  UpdateAggregateStates_n();
}

void TransportController::OnChannelCandidateGathered_n(
    IceTransportInternal* channel,
    const Candidate& candidate) {
  RTC_DCHECK(network_thread_->IsCurrent());

  // We should never signal peer-reflexive candidates.
  if (candidate.type() == PRFLX_PORT_TYPE) {
    RTC_NOTREACHED();
    return;
  }
  std::vector<Candidate> candidates;
  candidates.push_back(candidate);
  CandidatesData* data =
      new CandidatesData(channel->transport_name(), candidates);
  signaling_thread_->Post(RTC_FROM_HERE, this, MSG_CANDIDATESGATHERED, data);
}

void TransportController::OnChannelCandidatesRemoved_n(
    IceTransportInternal* channel,
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
    IceTransportInternal* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  // Note: since the role conflict is handled entirely on the network thread,
  // we don't need to worry about role conflicts occurring on two ports at once.
  // The first one encountered should immediately reverse the role.
  IceRole reversed_role = (ice_role_ == ICEROLE_CONTROLLING)
                              ? ICEROLE_CONTROLLED
                              : ICEROLE_CONTROLLING;
  RTC_LOG(LS_INFO) << "Got role conflict; switching to "
                   << (reversed_role == ICEROLE_CONTROLLING ? "controlling"
                                                            : "controlled")
                   << " role.";
  SetIceRole_n(reversed_role);
}

void TransportController::OnChannelStateChanged_n(
    IceTransportInternal* channel) {
  RTC_DCHECK(network_thread_->IsCurrent());
  RTC_LOG(LS_INFO) << channel->transport_name() << " TransportChannel "
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
    any_receiving = any_receiving || channel->dtls()->receiving();
    any_failed = any_failed || channel->dtls()->ice_transport()->GetState() ==
                                   IceTransportState::STATE_FAILED;
    all_connected = all_connected && channel->dtls()->writable();
    all_completed =
        all_completed && channel->dtls()->writable() &&
        channel->dtls()->ice_transport()->GetState() ==
            IceTransportState::STATE_COMPLETED &&
        channel->dtls()->ice_transport()->GetIceRole() == ICEROLE_CONTROLLING &&
        channel->dtls()->ice_transport()->gathering_state() ==
            kIceGatheringComplete;
    any_gathering =
        any_gathering ||
        channel->dtls()->ice_transport()->gathering_state() != kIceGatheringNew;
    all_done_gathering = all_done_gathering &&
                         channel->dtls()->ice_transport()->gathering_state() ==
                             kIceGatheringComplete;
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

}  // namespace cricket
