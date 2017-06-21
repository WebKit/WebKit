/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/client/basicportallocator.h"

#include <algorithm>
#include <string>
#include <vector>

#include "webrtc/api/umametrics.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/common.h"
#include "webrtc/p2p/base/port.h"
#include "webrtc/p2p/base/relayport.h"
#include "webrtc/p2p/base/stunport.h"
#include "webrtc/p2p/base/tcpport.h"
#include "webrtc/p2p/base/turnport.h"
#include "webrtc/p2p/base/udpport.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"

using rtc::CreateRandomId;

namespace {

enum {
  MSG_CONFIG_START,
  MSG_CONFIG_READY,
  MSG_ALLOCATE,
  MSG_ALLOCATION_PHASE,
  MSG_SEQUENCEOBJECTS_CREATED,
  MSG_CONFIG_STOP,
};

const int PHASE_UDP = 0;
const int PHASE_RELAY = 1;
const int PHASE_TCP = 2;
const int PHASE_SSLTCP = 3;

const int kNumPhases = 4;

// Gets protocol priority: UDP > TCP > SSLTCP.
int GetProtocolPriority(cricket::ProtocolType protocol) {
  switch (protocol) {
    case cricket::PROTO_UDP:
      return 2;
    case cricket::PROTO_TCP:
      return 1;
    case cricket::PROTO_SSLTCP:
      return 0;
    default:
      RTC_NOTREACHED();
      return 0;
  }
}
// Gets address family priority:  IPv6 > IPv4 > Unspecified.
int GetAddressFamilyPriority(int ip_family) {
  switch (ip_family) {
    case AF_INET6:
      return 2;
    case AF_INET:
      return 1;
    default:
      RTC_NOTREACHED();
      return 0;
  }
}

// Returns positive if a is better, negative if b is better, and 0 otherwise.
int ComparePort(const cricket::Port* a, const cricket::Port* b) {
  int a_protocol = GetProtocolPriority(a->GetProtocol());
  int b_protocol = GetProtocolPriority(b->GetProtocol());
  int cmp_protocol = a_protocol - b_protocol;
  if (cmp_protocol != 0) {
    return cmp_protocol;
  }

  int a_family = GetAddressFamilyPriority(a->Network()->GetBestIP().family());
  int b_family = GetAddressFamilyPriority(b->Network()->GetBestIP().family());
  return a_family - b_family;
}

}  // namespace

namespace cricket {
const uint32_t DISABLE_ALL_PHASES =
    PORTALLOCATOR_DISABLE_UDP | PORTALLOCATOR_DISABLE_TCP |
    PORTALLOCATOR_DISABLE_STUN | PORTALLOCATOR_DISABLE_RELAY;

// BasicPortAllocator
BasicPortAllocator::BasicPortAllocator(rtc::NetworkManager* network_manager,
                                       rtc::PacketSocketFactory* socket_factory)
    : network_manager_(network_manager), socket_factory_(socket_factory) {
  RTC_DCHECK(network_manager_ != nullptr);
  RTC_DCHECK(socket_factory_ != nullptr);
  Construct();
}

BasicPortAllocator::BasicPortAllocator(rtc::NetworkManager* network_manager)
    : network_manager_(network_manager), socket_factory_(nullptr) {
  RTC_DCHECK(network_manager_ != nullptr);
  Construct();
}

BasicPortAllocator::BasicPortAllocator(rtc::NetworkManager* network_manager,
                                       rtc::PacketSocketFactory* socket_factory,
                                       const ServerAddresses& stun_servers)
    : network_manager_(network_manager), socket_factory_(socket_factory) {
  RTC_DCHECK(socket_factory_ != NULL);
  SetConfiguration(stun_servers, std::vector<RelayServerConfig>(), 0, false);
  Construct();
}

BasicPortAllocator::BasicPortAllocator(
    rtc::NetworkManager* network_manager,
    const ServerAddresses& stun_servers,
    const rtc::SocketAddress& relay_address_udp,
    const rtc::SocketAddress& relay_address_tcp,
    const rtc::SocketAddress& relay_address_ssl)
    : network_manager_(network_manager), socket_factory_(NULL) {
  std::vector<RelayServerConfig> turn_servers;
  RelayServerConfig config(RELAY_GTURN);
  if (!relay_address_udp.IsNil()) {
    config.ports.push_back(ProtocolAddress(relay_address_udp, PROTO_UDP));
  }
  if (!relay_address_tcp.IsNil()) {
    config.ports.push_back(ProtocolAddress(relay_address_tcp, PROTO_TCP));
  }
  if (!relay_address_ssl.IsNil()) {
    config.ports.push_back(ProtocolAddress(relay_address_ssl, PROTO_SSLTCP));
  }

  if (!config.ports.empty()) {
    turn_servers.push_back(config);
  }

  SetConfiguration(stun_servers, turn_servers, 0, false);
  Construct();
}

void BasicPortAllocator::Construct() {
  allow_tcp_listen_ = true;
}

void BasicPortAllocator::OnIceRegathering(PortAllocatorSession* session,
                                          IceRegatheringReason reason) {
  if (!metrics_observer()) {
    return;
  }
  // If the session has not been taken by an active channel, do not report the
  // metric.
  for (auto& allocator_session : pooled_sessions()) {
    if (allocator_session.get() == session) {
      return;
    }
  }

  metrics_observer()->IncrementEnumCounter(
      webrtc::kEnumCounterIceRegathering, static_cast<int>(reason),
      static_cast<int>(IceRegatheringReason::MAX_VALUE));
}

BasicPortAllocator::~BasicPortAllocator() {
  // Our created port allocator sessions depend on us, so destroy our remaining
  // pooled sessions before anything else.
  DiscardCandidatePool();
}

PortAllocatorSession* BasicPortAllocator::CreateSessionInternal(
    const std::string& content_name, int component,
    const std::string& ice_ufrag, const std::string& ice_pwd) {
  PortAllocatorSession* session = new BasicPortAllocatorSession(
      this, content_name, component, ice_ufrag, ice_pwd);
  session->SignalIceRegathering.connect(this,
                                        &BasicPortAllocator::OnIceRegathering);
  return session;
}

void BasicPortAllocator::AddTurnServer(const RelayServerConfig& turn_server) {
  std::vector<RelayServerConfig> new_turn_servers = turn_servers();
  new_turn_servers.push_back(turn_server);
  SetConfiguration(stun_servers(), new_turn_servers, candidate_pool_size(),
                   prune_turn_ports());
}

// BasicPortAllocatorSession
BasicPortAllocatorSession::BasicPortAllocatorSession(
    BasicPortAllocator* allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd)
    : PortAllocatorSession(content_name,
                           component,
                           ice_ufrag,
                           ice_pwd,
                           allocator->flags()),
      allocator_(allocator),
      network_thread_(NULL),
      socket_factory_(allocator->socket_factory()),
      allocation_started_(false),
      network_manager_started_(false),
      allocation_sequences_created_(false),
      prune_turn_ports_(allocator->prune_turn_ports()) {
  allocator_->network_manager()->SignalNetworksChanged.connect(
      this, &BasicPortAllocatorSession::OnNetworksChanged);
  allocator_->network_manager()->StartUpdating();
}

BasicPortAllocatorSession::~BasicPortAllocatorSession() {
  allocator_->network_manager()->StopUpdating();
  if (network_thread_ != NULL)
    network_thread_->Clear(this);

  for (uint32_t i = 0; i < sequences_.size(); ++i) {
    // AllocationSequence should clear it's map entry for turn ports before
    // ports are destroyed.
    sequences_[i]->Clear();
  }

  std::vector<PortData>::iterator it;
  for (it = ports_.begin(); it != ports_.end(); it++)
    delete it->port();

  for (uint32_t i = 0; i < configs_.size(); ++i)
    delete configs_[i];

  for (uint32_t i = 0; i < sequences_.size(); ++i)
    delete sequences_[i];
}

void BasicPortAllocatorSession::SetCandidateFilter(uint32_t filter) {
  if (filter == candidate_filter_) {
    return;
  }
  // We assume the filter will only change from "ALL" to something else.
  RTC_DCHECK(candidate_filter_ == CF_ALL);
  candidate_filter_ = filter;
  for (PortData& port : ports_) {
    if (!port.has_pairable_candidate()) {
      continue;
    }
    const auto& candidates = port.port()->Candidates();
    // Setting a filter may cause a ready port to become non-ready
    // if it no longer has any pairable candidates.
    if (!std::any_of(candidates.begin(), candidates.end(),
                     [this, &port](const Candidate& candidate) {
                       return CandidatePairable(candidate, port.port());
                     })) {
      port.set_has_pairable_candidate(false);
    }
  }
}

void BasicPortAllocatorSession::StartGettingPorts() {
  network_thread_ = rtc::Thread::Current();
  state_ = SessionState::GATHERING;
  if (!socket_factory_) {
    owned_socket_factory_.reset(
        new rtc::BasicPacketSocketFactory(network_thread_));
    socket_factory_ = owned_socket_factory_.get();
  }

  network_thread_->Post(RTC_FROM_HERE, this, MSG_CONFIG_START);

  LOG(LS_INFO) << "Start getting ports with prune_turn_ports "
               << (prune_turn_ports_ ? "enabled" : "disabled");
}

void BasicPortAllocatorSession::StopGettingPorts() {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  ClearGettingPorts();
  // Note: this must be called after ClearGettingPorts because both may set the
  // session state and we should set the state to STOPPED.
  state_ = SessionState::STOPPED;
}

void BasicPortAllocatorSession::ClearGettingPorts() {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  network_thread_->Clear(this, MSG_ALLOCATE);
  for (uint32_t i = 0; i < sequences_.size(); ++i) {
    sequences_[i]->Stop();
  }
  network_thread_->Post(RTC_FROM_HERE, this, MSG_CONFIG_STOP);
  state_ = SessionState::CLEARED;
}

std::vector<rtc::Network*> BasicPortAllocatorSession::GetFailedNetworks() {
  std::vector<rtc::Network*> networks = GetNetworks();

  // A network interface may have both IPv4 and IPv6 networks. Only if
  // neither of the networks has any connections, the network interface
  // is considered failed and need to be regathered on.
  std::set<std::string> networks_with_connection;
  for (const PortData& data : ports_) {
    Port* port = data.port();
    if (!port->connections().empty()) {
      networks_with_connection.insert(port->Network()->name());
    }
  }

  networks.erase(
      std::remove_if(networks.begin(), networks.end(),
                     [networks_with_connection](rtc::Network* network) {
                       // If a network does not have any connection, it is
                       // considered failed.
                       return networks_with_connection.find(network->name()) !=
                              networks_with_connection.end();
                     }),
      networks.end());
  return networks;
}

void BasicPortAllocatorSession::RegatherOnFailedNetworks() {
  // Find the list of networks that have no connection.
  std::vector<rtc::Network*> failed_networks = GetFailedNetworks();
  if (failed_networks.empty()) {
    return;
  }

  LOG(LS_INFO) << "Regather candidates on failed networks";

  // Mark a sequence as "network failed" if its network is in the list of failed
  // networks, so that it won't be considered as equivalent when the session
  // regathers ports and candidates.
  for (AllocationSequence* sequence : sequences_) {
    if (!sequence->network_failed() &&
        std::find(failed_networks.begin(), failed_networks.end(),
                  sequence->network()) != failed_networks.end()) {
      sequence->set_network_failed();
    }
  }
  // Remove ports from being used locally and send signaling to remove
  // the candidates on the remote side.
  std::vector<PortData*> ports_to_prune = GetUnprunedPorts(failed_networks);
  if (!ports_to_prune.empty()) {
    LOG(LS_INFO) << "Prune " << ports_to_prune.size()
                 << " ports because their networks failed";
    PrunePortsAndRemoveCandidates(ports_to_prune);
  }

  if (allocation_started_ && network_manager_started_ && !IsStopped()) {
    SignalIceRegathering(this, IceRegatheringReason::NETWORK_FAILURE);

    DoAllocate();
  }
}

std::vector<PortInterface*> BasicPortAllocatorSession::ReadyPorts() const {
  std::vector<PortInterface*> ret;
  for (const PortData& data : ports_) {
    if (data.ready()) {
      ret.push_back(data.port());
    }
  }
  return ret;
}

std::vector<Candidate> BasicPortAllocatorSession::ReadyCandidates() const {
  std::vector<Candidate> candidates;
  for (const PortData& data : ports_) {
    if (!data.ready()) {
      continue;
    }
    GetCandidatesFromPort(data, &candidates);
  }
  return candidates;
}

void BasicPortAllocatorSession::GetCandidatesFromPort(
    const PortData& data,
    std::vector<Candidate>* candidates) const {
  RTC_CHECK(candidates != nullptr);
  for (const Candidate& candidate : data.port()->Candidates()) {
    if (!CheckCandidateFilter(candidate)) {
      continue;
    }
    ProtocolType pvalue;
    if (!StringToProto(candidate.protocol().c_str(), &pvalue) ||
        !data.sequence()->ProtocolEnabled(pvalue)) {
      continue;
    }
    candidates->push_back(SanitizeRelatedAddress(candidate));
  }
}

Candidate BasicPortAllocatorSession::SanitizeRelatedAddress(
    const Candidate& c) const {
  Candidate copy = c;
  // If adapter enumeration is disabled or host candidates are disabled,
  // clear the raddr of STUN candidates to avoid local address leakage.
  bool filter_stun_related_address =
      ((flags() & PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION) &&
       (flags() & PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE)) ||
      !(candidate_filter_ & CF_HOST);
  // If the candidate filter doesn't allow reflexive addresses, empty TURN raddr
  // to avoid reflexive address leakage.
  bool filter_turn_related_address = !(candidate_filter_ & CF_REFLEXIVE);
  if ((c.type() == STUN_PORT_TYPE && filter_stun_related_address) ||
      (c.type() == RELAY_PORT_TYPE && filter_turn_related_address)) {
    copy.set_related_address(
        rtc::EmptySocketAddressWithFamily(copy.address().family()));
  }
  return copy;
}

bool BasicPortAllocatorSession::CandidatesAllocationDone() const {
  // Done only if all required AllocationSequence objects
  // are created.
  if (!allocation_sequences_created_) {
    return false;
  }

  // Check that all port allocation sequences are complete (not running).
  if (std::any_of(sequences_.begin(), sequences_.end(),
                  [](const AllocationSequence* sequence) {
                    return sequence->state() == AllocationSequence::kRunning;
                  })) {
    return false;
  }

  // If all allocated ports are no longer gathering, session must have got all
  // expected candidates. Session will trigger candidates allocation complete
  // signal.
  return std::none_of(ports_.begin(), ports_.end(),
                      [](const PortData& port) { return port.inprogress(); });
}

void BasicPortAllocatorSession::OnMessage(rtc::Message *message) {
  switch (message->message_id) {
  case MSG_CONFIG_START:
    RTC_DCHECK(rtc::Thread::Current() == network_thread_);
    GetPortConfigurations();
    break;
  case MSG_CONFIG_READY:
    RTC_DCHECK(rtc::Thread::Current() == network_thread_);
    OnConfigReady(static_cast<PortConfiguration*>(message->pdata));
    break;
  case MSG_ALLOCATE:
    RTC_DCHECK(rtc::Thread::Current() == network_thread_);
    OnAllocate();
    break;
  case MSG_SEQUENCEOBJECTS_CREATED:
    RTC_DCHECK(rtc::Thread::Current() == network_thread_);
    OnAllocationSequenceObjectsCreated();
    break;
  case MSG_CONFIG_STOP:
    RTC_DCHECK(rtc::Thread::Current() == network_thread_);
    OnConfigStop();
    break;
  default:
    RTC_NOTREACHED();
  }
}

void BasicPortAllocatorSession::UpdateIceParametersInternal() {
  for (PortData& port : ports_) {
    port.port()->set_content_name(content_name());
    port.port()->SetIceParameters(component(), ice_ufrag(), ice_pwd());
  }
}

void BasicPortAllocatorSession::GetPortConfigurations() {
  PortConfiguration* config = new PortConfiguration(allocator_->stun_servers(),
                                                    username(),
                                                    password());

  for (const RelayServerConfig& turn_server : allocator_->turn_servers()) {
    config->AddRelay(turn_server);
  }
  ConfigReady(config);
}

void BasicPortAllocatorSession::ConfigReady(PortConfiguration* config) {
  network_thread_->Post(RTC_FROM_HERE, this, MSG_CONFIG_READY, config);
}

// Adds a configuration to the list.
void BasicPortAllocatorSession::OnConfigReady(PortConfiguration* config) {
  if (config) {
    configs_.push_back(config);
  }

  AllocatePorts();
}

void BasicPortAllocatorSession::OnConfigStop() {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);

  // If any of the allocated ports have not completed the candidates allocation,
  // mark those as error. Since session doesn't need any new candidates
  // at this stage of the allocation, it's safe to discard any new candidates.
  bool send_signal = false;
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (it->inprogress()) {
      // Updating port state to error, which didn't finish allocating candidates
      // yet.
      it->set_error();
      send_signal = true;
    }
  }

  // Did we stop any running sequences?
  for (std::vector<AllocationSequence*>::iterator it = sequences_.begin();
       it != sequences_.end() && !send_signal; ++it) {
    if ((*it)->state() == AllocationSequence::kStopped) {
      send_signal = true;
    }
  }

  // If we stopped anything that was running, send a done signal now.
  if (send_signal) {
    MaybeSignalCandidatesAllocationDone();
  }
}

void BasicPortAllocatorSession::AllocatePorts() {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  network_thread_->Post(RTC_FROM_HERE, this, MSG_ALLOCATE);
}

void BasicPortAllocatorSession::OnAllocate() {
  if (network_manager_started_ && !IsStopped())
    DoAllocate();

  allocation_started_ = true;
}

std::vector<rtc::Network*> BasicPortAllocatorSession::GetNetworks() {
  std::vector<rtc::Network*> networks;
  rtc::NetworkManager* network_manager = allocator_->network_manager();
  RTC_DCHECK(network_manager != nullptr);
  // If the network permission state is BLOCKED, we just act as if the flag has
  // been passed in.
  if (network_manager->enumeration_permission() ==
      rtc::NetworkManager::ENUMERATION_BLOCKED) {
    set_flags(flags() | PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION);
  }
  // If the adapter enumeration is disabled, we'll just bind to any address
  // instead of specific NIC. This is to ensure the same routing for http
  // traffic by OS is also used here to avoid any local or public IP leakage
  // during stun process.
  if (flags() & PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION) {
    network_manager->GetAnyAddressNetworks(&networks);
  } else {
    network_manager->GetNetworks(&networks);
    // If network enumeration fails, use the ANY address as a fallback, so we
    // can at least try gathering candidates using the default route chosen by
    // the OS. Or, if the PORTALLOCATOR_ENABLE_ANY_ADDRESS_PORTS flag is
    // set, we'll use ANY address candidates either way.
    if (networks.empty() || flags() & PORTALLOCATOR_ENABLE_ANY_ADDRESS_PORTS) {
      network_manager->GetAnyAddressNetworks(&networks);
    }
  }
  networks.erase(std::remove_if(networks.begin(), networks.end(),
                                [this](rtc::Network* network) {
                                  return allocator_->network_ignore_mask() &
                                         network->type();
                                }),
                 networks.end());

  if (flags() & PORTALLOCATOR_DISABLE_COSTLY_NETWORKS) {
    uint16_t lowest_cost = rtc::kNetworkCostMax;
    for (rtc::Network* network : networks) {
      lowest_cost = std::min<uint16_t>(lowest_cost, network->GetCost());
    }
    networks.erase(std::remove_if(networks.begin(), networks.end(),
                                  [lowest_cost](rtc::Network* network) {
                                    return network->GetCost() >
                                           lowest_cost + rtc::kNetworkCostLow;
                                  }),
                   networks.end());
  }
  return networks;
}

// For each network, see if we have a sequence that covers it already.  If not,
// create a new sequence to create the appropriate ports.
void BasicPortAllocatorSession::DoAllocate() {
  bool done_signal_needed = false;
  std::vector<rtc::Network*> networks = GetNetworks();
  if (networks.empty()) {
    LOG(LS_WARNING) << "Machine has no networks; no ports will be allocated";
    done_signal_needed = true;
  } else {
    LOG(LS_INFO) << "Allocate ports on "<< networks.size() << " networks";
    PortConfiguration* config = configs_.empty() ? nullptr : configs_.back();
    for (uint32_t i = 0; i < networks.size(); ++i) {
      uint32_t sequence_flags = flags();
      if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {
        // If all the ports are disabled we should just fire the allocation
        // done event and return.
        done_signal_needed = true;
        break;
      }

      if (!config || config->relays.empty()) {
        // No relay ports specified in this config.
        sequence_flags |= PORTALLOCATOR_DISABLE_RELAY;
      }

      if (!(sequence_flags & PORTALLOCATOR_ENABLE_IPV6) &&
          networks[i]->GetBestIP().family() == AF_INET6) {
        // Skip IPv6 networks unless the flag's been set.
        continue;
      }

      if (!(sequence_flags & PORTALLOCATOR_ENABLE_IPV6_ON_WIFI) &&
          networks[i]->GetBestIP().family() == AF_INET6 &&
          networks[i]->type() == rtc::ADAPTER_TYPE_WIFI) {
        // Skip IPv6 Wi-Fi networks unless the flag's been set.
        continue;
      }

      // Disable phases that would only create ports equivalent to
      // ones that we have already made.
      DisableEquivalentPhases(networks[i], config, &sequence_flags);

      if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {
        // New AllocationSequence would have nothing to do, so don't make it.
        continue;
      }

      AllocationSequence* sequence =
          new AllocationSequence(this, networks[i], config, sequence_flags);
      sequence->SignalPortAllocationComplete.connect(
          this, &BasicPortAllocatorSession::OnPortAllocationComplete);
      sequence->Init();
      sequence->Start();
      sequences_.push_back(sequence);
      done_signal_needed = true;
    }
  }
  if (done_signal_needed) {
    network_thread_->Post(RTC_FROM_HERE, this, MSG_SEQUENCEOBJECTS_CREATED);
  }
}

void BasicPortAllocatorSession::OnNetworksChanged() {
  std::vector<rtc::Network*> networks = GetNetworks();
  std::vector<rtc::Network*> failed_networks;
  for (AllocationSequence* sequence : sequences_) {
    // Mark the sequence as "network failed" if its network is not in
    // |networks|.
    if (!sequence->network_failed() &&
        std::find(networks.begin(), networks.end(), sequence->network()) ==
            networks.end()) {
      sequence->OnNetworkFailed();
      failed_networks.push_back(sequence->network());
    }
  }
  std::vector<PortData*> ports_to_prune = GetUnprunedPorts(failed_networks);
  if (!ports_to_prune.empty()) {
    LOG(LS_INFO) << "Prune " << ports_to_prune.size()
                 << " ports because their networks were gone";
    PrunePortsAndRemoveCandidates(ports_to_prune);
  }

  if (allocation_started_ && !IsStopped()) {
    if (network_manager_started_) {
      // If the network manager has started, it must be regathering.
      SignalIceRegathering(this, IceRegatheringReason::NETWORK_CHANGE);
    }
    DoAllocate();
  }

  if (!network_manager_started_) {
    LOG(LS_INFO) << "Network manager has started";
    network_manager_started_ = true;
  }
}

void BasicPortAllocatorSession::DisableEquivalentPhases(
    rtc::Network* network,
    PortConfiguration* config,
    uint32_t* flags) {
  for (uint32_t i = 0; i < sequences_.size() &&
                           (*flags & DISABLE_ALL_PHASES) != DISABLE_ALL_PHASES;
       ++i) {
    sequences_[i]->DisableEquivalentPhases(network, config, flags);
  }
}

void BasicPortAllocatorSession::AddAllocatedPort(Port* port,
                                                 AllocationSequence * seq,
                                                 bool prepare_address) {
  if (!port)
    return;

  LOG(LS_INFO) << "Adding allocated port for " << content_name();
  port->set_content_name(content_name());
  port->set_component(component());
  port->set_generation(generation());
  if (allocator_->proxy().type != rtc::PROXY_NONE)
    port->set_proxy(allocator_->user_agent(), allocator_->proxy());
  port->set_send_retransmit_count_attribute(
      (flags() & PORTALLOCATOR_ENABLE_STUN_RETRANSMIT_ATTRIBUTE) != 0);

  PortData data(port, seq);
  ports_.push_back(data);

  port->SignalCandidateReady.connect(
      this, &BasicPortAllocatorSession::OnCandidateReady);
  port->SignalPortComplete.connect(this,
      &BasicPortAllocatorSession::OnPortComplete);
  port->SignalDestroyed.connect(this,
      &BasicPortAllocatorSession::OnPortDestroyed);
  port->SignalPortError.connect(
      this, &BasicPortAllocatorSession::OnPortError);
  LOG_J(LS_INFO, port) << "Added port to allocator";

  if (prepare_address)
    port->PrepareAddress();
}

void BasicPortAllocatorSession::OnAllocationSequenceObjectsCreated() {
  allocation_sequences_created_ = true;
  // Send candidate allocation complete signal if we have no sequences.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnCandidateReady(
    Port* port, const Candidate& c) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  PortData* data = FindPort(port);
  RTC_DCHECK(data != NULL);
  LOG_J(LS_INFO, port) << "Gathered candidate: " << c.ToSensitiveString();
  // Discarding any candidate signal if port allocation status is
  // already done with gathering.
  if (!data->inprogress()) {
    LOG(LS_WARNING)
        << "Discarding candidate because port is already done gathering.";
    return;
  }

  // Mark that the port has a pairable candidate, either because we have a
  // usable candidate from the port, or simply because the port is bound to the
  // any address and therefore has no host candidate. This will trigger the port
  // to start creating candidate pairs (connections) and issue connectivity
  // checks. If port has already been marked as having a pairable candidate,
  // do nothing here.
  // Note: We should check whether any candidates may become ready after this
  // because there we will check whether the candidate is generated by the ready
  // ports, which may include this port.
  bool pruned = false;
  if (CandidatePairable(c, port) && !data->has_pairable_candidate()) {
    data->set_has_pairable_candidate(true);

    if (prune_turn_ports_ && port->Type() == RELAY_PORT_TYPE) {
      pruned = PruneTurnPorts(port);
    }
    // If the current port is not pruned yet, SignalPortReady.
    if (!data->pruned()) {
      LOG_J(LS_INFO, port) << "Port ready.";
      SignalPortReady(this, port);
      port->KeepAliveUntilPruned();
    }
  }

  ProtocolType pvalue;
  bool candidate_protocol_enabled =
      StringToProto(c.protocol().c_str(), &pvalue) &&
      data->sequence()->ProtocolEnabled(pvalue);

  if (data->ready() && CheckCandidateFilter(c) && candidate_protocol_enabled) {
    std::vector<Candidate> candidates;
    candidates.push_back(SanitizeRelatedAddress(c));
    SignalCandidatesReady(this, candidates);
  } else if (!candidate_protocol_enabled) {
    LOG(LS_INFO)
        << "Not yet signaling candidate because protocol is not yet enabled.";
  } else {
    LOG(LS_INFO) << "Discarding candidate because it doesn't match filter.";
  }

  // If we have pruned any port, maybe need to signal port allocation done.
  if (pruned) {
    MaybeSignalCandidatesAllocationDone();
  }
}

Port* BasicPortAllocatorSession::GetBestTurnPortForNetwork(
    const std::string& network_name) const {
  Port* best_turn_port = nullptr;
  for (const PortData& data : ports_) {
    if (data.port()->Network()->name() == network_name &&
        data.port()->Type() == RELAY_PORT_TYPE && data.ready() &&
        (!best_turn_port || ComparePort(data.port(), best_turn_port) > 0)) {
      best_turn_port = data.port();
    }
  }
  return best_turn_port;
}

bool BasicPortAllocatorSession::PruneTurnPorts(Port* newly_pairable_turn_port) {
  // Note: We determine the same network based only on their network names. So
  // if an IPv4 address and an IPv6 address have the same network name, they
  // are considered the same network here.
  const std::string& network_name = newly_pairable_turn_port->Network()->name();
  Port* best_turn_port = GetBestTurnPortForNetwork(network_name);
  // |port| is already in the list of ports, so the best port cannot be nullptr.
  RTC_CHECK(best_turn_port != nullptr);

  bool pruned = false;
  std::vector<PortData*> ports_to_prune;
  for (PortData& data : ports_) {
    if (data.port()->Network()->name() == network_name &&
        data.port()->Type() == RELAY_PORT_TYPE && !data.pruned() &&
        ComparePort(data.port(), best_turn_port) < 0) {
      pruned = true;
      if (data.port() != newly_pairable_turn_port) {
        // These ports will be pruned in PrunePortsAndRemoveCandidates.
        ports_to_prune.push_back(&data);
      } else {
        data.Prune();
      }
    }
  }

  if (!ports_to_prune.empty()) {
    LOG(LS_INFO) << "Prune " << ports_to_prune.size()
                 << " low-priority TURN ports";
    PrunePortsAndRemoveCandidates(ports_to_prune);
  }
  return pruned;
}

void BasicPortAllocatorSession::PruneAllPorts() {
  for (PortData& data : ports_) {
    data.Prune();
  }
}

void BasicPortAllocatorSession::OnPortComplete(Port* port) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  LOG_J(LS_INFO, port) << "Port completed gathering candidates.";
  PortData* data = FindPort(port);
  RTC_DCHECK(data != NULL);

  // Ignore any late signals.
  if (!data->inprogress()) {
    return;
  }

  // Moving to COMPLETE state.
  data->set_complete();
  // Send candidate allocation complete signal if this was the last port.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnPortError(Port* port) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  LOG_J(LS_INFO, port) << "Port encountered error while gathering candidates.";
  PortData* data = FindPort(port);
  RTC_DCHECK(data != NULL);
  // We might have already given up on this port and stopped it.
  if (!data->inprogress()) {
    return;
  }

  // SignalAddressError is currently sent from StunPort/TurnPort.
  // But this signal itself is generic.
  data->set_error();
  // Send candidate allocation complete signal if this was the last port.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnProtocolEnabled(AllocationSequence* seq,
                                                  ProtocolType proto) {
  std::vector<Candidate> candidates;
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (it->sequence() != seq)
      continue;

    const std::vector<Candidate>& potentials = it->port()->Candidates();
    for (size_t i = 0; i < potentials.size(); ++i) {
      if (!CheckCandidateFilter(potentials[i])) {
        continue;
      }
      ProtocolType pvalue;
      bool candidate_protocol_enabled =
          StringToProto(potentials[i].protocol().c_str(), &pvalue) &&
          pvalue == proto;
      if (candidate_protocol_enabled) {
        LOG(LS_INFO) << "Signaling candidate because protocol was enabled: "
                     << potentials[i].ToSensitiveString();
        candidates.push_back(potentials[i]);
      }
    }
  }

  if (!candidates.empty()) {
    SignalCandidatesReady(this, candidates);
  }
}

bool BasicPortAllocatorSession::CheckCandidateFilter(const Candidate& c) const {
  uint32_t filter = candidate_filter_;

  // When binding to any address, before sending packets out, the getsockname
  // returns all 0s, but after sending packets, it'll be the NIC used to
  // send. All 0s is not a valid ICE candidate address and should be filtered
  // out.
  if (c.address().IsAnyIP()) {
    return false;
  }

  if (c.type() == RELAY_PORT_TYPE) {
    return ((filter & CF_RELAY) != 0);
  } else if (c.type() == STUN_PORT_TYPE) {
    return ((filter & CF_REFLEXIVE) != 0);
  } else if (c.type() == LOCAL_PORT_TYPE) {
    if ((filter & CF_REFLEXIVE) && !c.address().IsPrivateIP()) {
      // We allow host candidates if the filter allows server-reflexive
      // candidates and the candidate is a public IP. Because we don't generate
      // server-reflexive candidates if they have the same IP as the host
      // candidate (i.e. when the host candidate is a public IP), filtering to
      // only server-reflexive candidates won't work right when the host
      // candidates have public IPs.
      return true;
    }

    return ((filter & CF_HOST) != 0);
  }
  return false;
}

bool BasicPortAllocatorSession::CandidatePairable(const Candidate& c,
                                                  const Port* port) const {
  bool candidate_signalable = CheckCandidateFilter(c);

  // When device enumeration is disabled (to prevent non-default IP addresses
  // from leaking), we ping from some local candidates even though we don't
  // signal them. However, if host candidates are also disabled (for example, to
  // prevent even default IP addresses from leaking), we still don't want to
  // ping from them, even if device enumeration is disabled.  Thus, we check for
  // both device enumeration and host candidates being disabled.
  bool network_enumeration_disabled = c.address().IsAnyIP();
  bool can_ping_from_candidate =
      (port->SharedSocket() || c.protocol() == TCP_PROTOCOL_NAME);
  bool host_candidates_disabled = !(candidate_filter_ & CF_HOST);

  return candidate_signalable ||
         (network_enumeration_disabled && can_ping_from_candidate &&
          !host_candidates_disabled);
}

void BasicPortAllocatorSession::OnPortAllocationComplete(
    AllocationSequence* seq) {
  // Send candidate allocation complete signal if all ports are done.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::MaybeSignalCandidatesAllocationDone() {
  if (CandidatesAllocationDone()) {
    if (pooled()) {
      LOG(LS_INFO) << "All candidates gathered for pooled session.";
    } else {
      LOG(LS_INFO) << "All candidates gathered for " << content_name() << ":"
                   << component() << ":" << generation();
    }
    SignalCandidatesAllocationDone(this);
  }
}

void BasicPortAllocatorSession::OnPortDestroyed(
    PortInterface* port) {
  RTC_DCHECK(rtc::Thread::Current() == network_thread_);
  for (std::vector<PortData>::iterator iter = ports_.begin();
       iter != ports_.end(); ++iter) {
    if (port == iter->port()) {
      ports_.erase(iter);
      LOG_J(LS_INFO, port) << "Removed port from allocator ("
                           << static_cast<int>(ports_.size()) << " remaining)";
      return;
    }
  }
  RTC_NOTREACHED();
}

BasicPortAllocatorSession::PortData* BasicPortAllocatorSession::FindPort(
    Port* port) {
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (it->port() == port) {
      return &*it;
    }
  }
  return NULL;
}

std::vector<BasicPortAllocatorSession::PortData*>
BasicPortAllocatorSession::GetUnprunedPorts(
    const std::vector<rtc::Network*>& networks) {
  std::vector<PortData*> unpruned_ports;
  for (PortData& port : ports_) {
    if (!port.pruned() &&
        std::find(networks.begin(), networks.end(),
                  port.sequence()->network()) != networks.end()) {
      unpruned_ports.push_back(&port);
    }
  }
  return unpruned_ports;
}

void BasicPortAllocatorSession::PrunePortsAndRemoveCandidates(
    const std::vector<PortData*>& port_data_list) {
  std::vector<PortInterface*> pruned_ports;
  std::vector<Candidate> removed_candidates;
  for (PortData* data : port_data_list) {
    // Prune the port so that it may be destroyed.
    data->Prune();
    pruned_ports.push_back(data->port());
    if (data->has_pairable_candidate()) {
      GetCandidatesFromPort(*data, &removed_candidates);
      // Mark the port as having no pairable candidates so that its candidates
      // won't be removed multiple times.
      data->set_has_pairable_candidate(false);
    }
  }
  if (!pruned_ports.empty()) {
    SignalPortsPruned(this, pruned_ports);
  }
  if (!removed_candidates.empty()) {
    LOG(LS_INFO) << "Removed " << removed_candidates.size() << " candidates";
    SignalCandidatesRemoved(this, removed_candidates);
  }
}

// AllocationSequence

AllocationSequence::AllocationSequence(BasicPortAllocatorSession* session,
                                       rtc::Network* network,
                                       PortConfiguration* config,
                                       uint32_t flags)
    : session_(session),
      network_(network),
      ip_(network->GetBestIP()),
      config_(config),
      state_(kInit),
      flags_(flags),
      udp_socket_(),
      udp_port_(NULL),
      phase_(0) {
}

void AllocationSequence::Init() {
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    udp_socket_.reset(session_->socket_factory()->CreateUdpSocket(
        rtc::SocketAddress(ip_, 0), session_->allocator()->min_port(),
        session_->allocator()->max_port()));
    if (udp_socket_) {
      udp_socket_->SignalReadPacket.connect(
          this, &AllocationSequence::OnReadPacket);
    }
    // Continuing if |udp_socket_| is NULL, as local TCP and RelayPort using TCP
    // are next available options to setup a communication channel.
  }
}

void AllocationSequence::Clear() {
  udp_port_ = NULL;
  turn_ports_.clear();
}

void AllocationSequence::OnNetworkFailed() {
  RTC_DCHECK(!network_failed_);
  network_failed_ = true;
  // Stop the allocation sequence if its network failed.
  Stop();
}

AllocationSequence::~AllocationSequence() {
  session_->network_thread()->Clear(this);
}

void AllocationSequence::DisableEquivalentPhases(rtc::Network* network,
    PortConfiguration* config, uint32_t* flags) {
  if (network_failed_) {
    // If the network of this allocation sequence has ever become failed,
    // it won't be equivalent to the new network.
    return;
  }

  if (!((network == network_) && (ip_ == network->GetBestIP()))) {
    // Different network setup; nothing is equivalent.
    return;
  }

  // Else turn off the stuff that we've already got covered.

  // Every config implicitly specifies local, so turn that off right away.
  *flags |= PORTALLOCATOR_DISABLE_UDP;
  *flags |= PORTALLOCATOR_DISABLE_TCP;

  if (config_ && config) {
    if (config_->StunServers() == config->StunServers()) {
      // Already got this STUN servers covered.
      *flags |= PORTALLOCATOR_DISABLE_STUN;
    }
    if (!config_->relays.empty()) {
      // Already got relays covered.
      // NOTE: This will even skip a _different_ set of relay servers if we
      // were to be given one, but that never happens in our codebase. Should
      // probably get rid of the list in PortConfiguration and just keep a
      // single relay server in each one.
      *flags |= PORTALLOCATOR_DISABLE_RELAY;
    }
  }
}

void AllocationSequence::Start() {
  state_ = kRunning;
  session_->network_thread()->Post(RTC_FROM_HERE, this, MSG_ALLOCATION_PHASE);
}

void AllocationSequence::Stop() {
  // If the port is completed, don't set it to stopped.
  if (state_ == kRunning) {
    state_ = kStopped;
    session_->network_thread()->Clear(this, MSG_ALLOCATION_PHASE);
  }
}

void AllocationSequence::OnMessage(rtc::Message* msg) {
  RTC_DCHECK(rtc::Thread::Current() == session_->network_thread());
  RTC_DCHECK(msg->message_id == MSG_ALLOCATION_PHASE);

  const char* const PHASE_NAMES[kNumPhases] = {
    "Udp", "Relay", "Tcp", "SslTcp"
  };

  // Perform all of the phases in the current step.
  LOG_J(LS_INFO, network_) << "Allocation Phase="
                           << PHASE_NAMES[phase_];

  switch (phase_) {
    case PHASE_UDP:
      CreateUDPPorts();
      CreateStunPorts();
      EnableProtocol(PROTO_UDP);
      break;

    case PHASE_RELAY:
      CreateRelayPorts();
      break;

    case PHASE_TCP:
      CreateTCPPorts();
      EnableProtocol(PROTO_TCP);
      break;

    case PHASE_SSLTCP:
      state_ = kCompleted;
      EnableProtocol(PROTO_SSLTCP);
      break;

    default:
      RTC_NOTREACHED();
  }

  if (state() == kRunning) {
    ++phase_;
    session_->network_thread()->PostDelayed(RTC_FROM_HERE,
                                            session_->allocator()->step_delay(),
                                            this, MSG_ALLOCATION_PHASE);
  } else {
    // If all phases in AllocationSequence are completed, no allocation
    // steps needed further. Canceling  pending signal.
    session_->network_thread()->Clear(this, MSG_ALLOCATION_PHASE);
    SignalPortAllocationComplete(this);
  }
}

void AllocationSequence::EnableProtocol(ProtocolType proto) {
  if (!ProtocolEnabled(proto)) {
    protocols_.push_back(proto);
    session_->OnProtocolEnabled(this, proto);
  }
}

bool AllocationSequence::ProtocolEnabled(ProtocolType proto) const {
  for (ProtocolList::const_iterator it = protocols_.begin();
       it != protocols_.end(); ++it) {
    if (*it == proto)
      return true;
  }
  return false;
}

void AllocationSequence::CreateUDPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_UDP)) {
    LOG(LS_VERBOSE) << "AllocationSequence: UDP ports disabled, skipping.";
    return;
  }

  // TODO(mallinath) - Remove UDPPort creating socket after shared socket
  // is enabled completely.
  UDPPort* port = NULL;
  bool emit_local_candidate_for_anyaddress =
      !IsFlagSet(PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE);
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) && udp_socket_) {
    port = UDPPort::Create(
        session_->network_thread(), session_->socket_factory(), network_,
        udp_socket_.get(), session_->username(), session_->password(),
        session_->allocator()->origin(), emit_local_candidate_for_anyaddress);
  } else {
    port = UDPPort::Create(
        session_->network_thread(), session_->socket_factory(), network_, ip_,
        session_->allocator()->min_port(), session_->allocator()->max_port(),
        session_->username(), session_->password(),
        session_->allocator()->origin(), emit_local_candidate_for_anyaddress);
  }

  if (port) {
    // If shared socket is enabled, STUN candidate will be allocated by the
    // UDPPort.
    if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
      udp_port_ = port;
      port->SignalDestroyed.connect(this, &AllocationSequence::OnPortDestroyed);

      // If STUN is not disabled, setting stun server address to port.
      if (!IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
        if (config_ && !config_->StunServers().empty()) {
          LOG(LS_INFO) << "AllocationSequence: UDPPort will be handling the "
                       <<  "STUN candidate generation.";
          port->set_server_addresses(config_->StunServers());
        }
      }
    }

    session_->AddAllocatedPort(port, this, true);
  }
}

void AllocationSequence::CreateTCPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_TCP)) {
    LOG(LS_VERBOSE) << "AllocationSequence: TCP ports disabled, skipping.";
    return;
  }

  Port* port = TCPPort::Create(session_->network_thread(),
                               session_->socket_factory(),
                               network_, ip_,
                               session_->allocator()->min_port(),
                               session_->allocator()->max_port(),
                               session_->username(), session_->password(),
                               session_->allocator()->allow_tcp_listen());
  if (port) {
    session_->AddAllocatedPort(port, this, true);
    // Since TCPPort is not created using shared socket, |port| will not be
    // added to the dequeue.
  }
}

void AllocationSequence::CreateStunPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
    LOG(LS_VERBOSE) << "AllocationSequence: STUN ports disabled, skipping.";
    return;
  }

  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    return;
  }

  if (!(config_ && !config_->StunServers().empty())) {
    LOG(LS_WARNING)
        << "AllocationSequence: No STUN server configured, skipping.";
    return;
  }

  StunPort* port = StunPort::Create(session_->network_thread(),
                                session_->socket_factory(),
                                network_, ip_,
                                session_->allocator()->min_port(),
                                session_->allocator()->max_port(),
                                session_->username(), session_->password(),
                                config_->StunServers(),
                                session_->allocator()->origin());
  if (port) {
    session_->AddAllocatedPort(port, this, true);
    // Since StunPort is not created using shared socket, |port| will not be
    // added to the dequeue.
  }
}

void AllocationSequence::CreateRelayPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_RELAY)) {
     LOG(LS_VERBOSE) << "AllocationSequence: Relay ports disabled, skipping.";
     return;
  }

  // If BasicPortAllocatorSession::OnAllocate left relay ports enabled then we
  // ought to have a relay list for them here.
  RTC_DCHECK(config_ && !config_->relays.empty());
  if (!(config_ && !config_->relays.empty())) {
    LOG(LS_WARNING)
        << "AllocationSequence: No relay server configured, skipping.";
    return;
  }

  for (RelayServerConfig& relay : config_->relays) {
    if (relay.type == RELAY_GTURN) {
      CreateGturnPort(relay);
    } else if (relay.type == RELAY_TURN) {
      CreateTurnPort(relay);
    } else {
      RTC_NOTREACHED();
    }
  }
}

void AllocationSequence::CreateGturnPort(const RelayServerConfig& config) {
  // TODO(mallinath) - Rename RelayPort to GTurnPort.
  RelayPort* port = RelayPort::Create(session_->network_thread(),
                                      session_->socket_factory(),
                                      network_, ip_,
                                      session_->allocator()->min_port(),
                                      session_->allocator()->max_port(),
                                      config_->username, config_->password);
  if (port) {
    // Since RelayPort is not created using shared socket, |port| will not be
    // added to the dequeue.
    // Note: We must add the allocated port before we add addresses because
    //       the latter will create candidates that need name and preference
    //       settings.  However, we also can't prepare the address (normally
    //       done by AddAllocatedPort) until we have these addresses.  So we
    //       wait to do that until below.
    session_->AddAllocatedPort(port, this, false);

    // Add the addresses of this protocol.
    PortList::const_iterator relay_port;
    for (relay_port = config.ports.begin();
         relay_port != config.ports.end();
         ++relay_port) {
      port->AddServerAddress(*relay_port);
      port->AddExternalAddress(*relay_port);
    }
    // Start fetching an address for this port.
    port->PrepareAddress();
  }
}

void AllocationSequence::CreateTurnPort(const RelayServerConfig& config) {
  PortList::const_iterator relay_port;
  for (relay_port = config.ports.begin();
       relay_port != config.ports.end(); ++relay_port) {
    TurnPort* port = NULL;

    // Skip UDP connections to relay servers if it's disallowed.
    if (IsFlagSet(PORTALLOCATOR_DISABLE_UDP_RELAY) &&
        relay_port->proto == PROTO_UDP) {
      continue;
    }

    // Do not create a port if the server address family is known and does
    // not match the local IP address family.
    int server_ip_family = relay_port->address.ipaddr().family();
    int local_ip_family = ip_.family();
    if (server_ip_family != AF_UNSPEC && server_ip_family != local_ip_family) {
      LOG(LS_INFO) << "Server and local address families are not compatible. "
                   << "Server address: "
                   << relay_port->address.ipaddr().ToString()
                   << " Local address: " << ip_.ToString();
      continue;
    }


    // Shared socket mode must be enabled only for UDP based ports. Hence
    // don't pass shared socket for ports which will create TCP sockets.
    // TODO(mallinath) - Enable shared socket mode for TURN ports. Disabled
    // due to webrtc bug https://code.google.com/p/webrtc/issues/detail?id=3537
    if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) &&
        relay_port->proto == PROTO_UDP && udp_socket_) {
      port = TurnPort::Create(session_->network_thread(),
                              session_->socket_factory(),
                              network_, udp_socket_.get(),
                              session_->username(), session_->password(),
                              *relay_port, config.credentials, config.priority,
                              session_->allocator()->origin());
      turn_ports_.push_back(port);
      // Listen to the port destroyed signal, to allow AllocationSequence to
      // remove entrt from it's map.
      port->SignalDestroyed.connect(this, &AllocationSequence::OnPortDestroyed);
    } else {
      port = TurnPort::Create(session_->network_thread(),
                              session_->socket_factory(),
                              network_, ip_,
                              session_->allocator()->min_port(),
                              session_->allocator()->max_port(),
                              session_->username(),
                              session_->password(),
                              *relay_port, config.credentials, config.priority,
                              session_->allocator()->origin());
    }
    RTC_DCHECK(port != NULL);
    port->SetTlsCertPolicy(config.tls_cert_policy);
    session_->AddAllocatedPort(port, this, true);
  }
}

void AllocationSequence::OnReadPacket(
    rtc::AsyncPacketSocket* socket, const char* data, size_t size,
    const rtc::SocketAddress& remote_addr,
    const rtc::PacketTime& packet_time) {
  RTC_DCHECK(socket == udp_socket_.get());

  bool turn_port_found = false;

  // Try to find the TurnPort that matches the remote address. Note that the
  // message could be a STUN binding response if the TURN server is also used as
  // a STUN server. We don't want to parse every message here to check if it is
  // a STUN binding response, so we pass the message to TurnPort regardless of
  // the message type. The TurnPort will just ignore the message since it will
  // not find any request by transaction ID.
  for (TurnPort* port : turn_ports_) {
    if (port->server_address().address == remote_addr) {
      if (port->HandleIncomingPacket(socket, data, size, remote_addr,
                                     packet_time)) {
        return;
      }
      turn_port_found = true;
    }
  }

  if (udp_port_) {
    const ServerAddresses& stun_servers = udp_port_->server_addresses();

    // Pass the packet to the UdpPort if there is no matching TurnPort, or if
    // the TURN server is also a STUN server.
    if (!turn_port_found ||
        stun_servers.find(remote_addr) != stun_servers.end()) {
      RTC_DCHECK(udp_port_->SharedSocket());
      udp_port_->HandleIncomingPacket(socket, data, size, remote_addr,
                                      packet_time);
    }
  }
}

void AllocationSequence::OnPortDestroyed(PortInterface* port) {
  if (udp_port_ == port) {
    udp_port_ = NULL;
    return;
  }

  auto it = std::find(turn_ports_.begin(), turn_ports_.end(), port);
  if (it != turn_ports_.end()) {
    turn_ports_.erase(it);
  } else {
    LOG(LS_ERROR) << "Unexpected OnPortDestroyed for nonexistent port.";
    RTC_NOTREACHED();
  }
}

// PortConfiguration
PortConfiguration::PortConfiguration(
    const rtc::SocketAddress& stun_address,
    const std::string& username,
    const std::string& password)
    : stun_address(stun_address), username(username), password(password) {
  if (!stun_address.IsNil())
    stun_servers.insert(stun_address);
}

PortConfiguration::PortConfiguration(const ServerAddresses& stun_servers,
                                     const std::string& username,
                                     const std::string& password)
    : stun_servers(stun_servers),
      username(username),
      password(password) {
  if (!stun_servers.empty())
    stun_address = *(stun_servers.begin());
}

ServerAddresses PortConfiguration::StunServers() {
  if (!stun_address.IsNil() &&
      stun_servers.find(stun_address) == stun_servers.end()) {
    stun_servers.insert(stun_address);
  }
  // Every UDP TURN server should also be used as a STUN server.
  ServerAddresses turn_servers = GetRelayServerAddresses(RELAY_TURN, PROTO_UDP);
  for (const rtc::SocketAddress& turn_server : turn_servers) {
    if (stun_servers.find(turn_server) == stun_servers.end()) {
      stun_servers.insert(turn_server);
    }
  }
  return stun_servers;
}

void PortConfiguration::AddRelay(const RelayServerConfig& config) {
  relays.push_back(config);
}

bool PortConfiguration::SupportsProtocol(
    const RelayServerConfig& relay, ProtocolType type) const {
  PortList::const_iterator relay_port;
  for (relay_port = relay.ports.begin();
        relay_port != relay.ports.end();
        ++relay_port) {
    if (relay_port->proto == type)
      return true;
  }
  return false;
}

bool PortConfiguration::SupportsProtocol(RelayType turn_type,
                                         ProtocolType type) const {
  for (size_t i = 0; i < relays.size(); ++i) {
    if (relays[i].type == turn_type &&
        SupportsProtocol(relays[i], type))
      return true;
  }
  return false;
}

ServerAddresses PortConfiguration::GetRelayServerAddresses(
    RelayType turn_type, ProtocolType type) const {
  ServerAddresses servers;
  for (size_t i = 0; i < relays.size(); ++i) {
    if (relays[i].type == turn_type && SupportsProtocol(relays[i], type)) {
      servers.insert(relays[i].ports.front().address);
    }
  }
  return servers;
}

}  // namespace cricket
