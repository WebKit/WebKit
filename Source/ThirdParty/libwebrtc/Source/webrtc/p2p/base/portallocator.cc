/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/portallocator.h"

#include <utility>

#include "rtc_base/checks.h"

namespace cricket {

RelayServerConfig::RelayServerConfig(RelayType type) : type(type) {}

RelayServerConfig::RelayServerConfig(const rtc::SocketAddress& address,
                                     const std::string& username,
                                     const std::string& password,
                                     ProtocolType proto)
    : type(RELAY_TURN), credentials(username, password) {
  ports.push_back(ProtocolAddress(address, proto));
}

RelayServerConfig::RelayServerConfig(const std::string& address,
                                     int port,
                                     const std::string& username,
                                     const std::string& password,
                                     ProtocolType proto)
    : RelayServerConfig(rtc::SocketAddress(address, port),
                        username,
                        password,
                        proto) {}

// Legacy constructor where "secure" and PROTO_TCP implies PROTO_TLS.
RelayServerConfig::RelayServerConfig(const std::string& address,
                                     int port,
                                     const std::string& username,
                                     const std::string& password,
                                     ProtocolType proto,
                                     bool secure)
    : RelayServerConfig(address,
                        port,
                        username,
                        password,
                        (proto == PROTO_TCP && secure ? PROTO_TLS : proto)) {}

RelayServerConfig::RelayServerConfig(const RelayServerConfig&) = default;

RelayServerConfig::~RelayServerConfig() = default;

PortAllocatorSession::PortAllocatorSession(const std::string& content_name,
                                           int component,
                                           const std::string& ice_ufrag,
                                           const std::string& ice_pwd,
                                           uint32_t flags)
    : flags_(flags),
      generation_(0),
      content_name_(content_name),
      component_(component),
      ice_ufrag_(ice_ufrag),
      ice_pwd_(ice_pwd) {
  // Pooled sessions are allowed to be created with empty content name,
  // component, ufrag and password.
  RTC_DCHECK(ice_ufrag.empty() == ice_pwd.empty());
}

PortAllocatorSession::~PortAllocatorSession() = default;

bool PortAllocatorSession::IsCleared() const {
  return false;
}

bool PortAllocatorSession::IsStopped() const {
  return false;
}

uint32_t PortAllocatorSession::generation() {
  return generation_;
}

void PortAllocatorSession::set_generation(uint32_t generation) {
  generation_ = generation;
}

PortAllocator::PortAllocator()
    : flags_(kDefaultPortAllocatorFlags),
      min_port_(0),
      max_port_(0),
      max_ipv6_networks_(kDefaultMaxIPv6Networks),
      step_delay_(kDefaultStepDelay),
      allow_tcp_listen_(true),
      candidate_filter_(CF_ALL) {}

PortAllocator::~PortAllocator() = default;

bool PortAllocator::SetConfiguration(
    const ServerAddresses& stun_servers,
    const std::vector<RelayServerConfig>& turn_servers,
    int candidate_pool_size,
    bool prune_turn_ports,
    webrtc::TurnCustomizer* turn_customizer) {
  bool ice_servers_changed =
      (stun_servers != stun_servers_ || turn_servers != turn_servers_);
  stun_servers_ = stun_servers;
  turn_servers_ = turn_servers;
  prune_turn_ports_ = prune_turn_ports;

  if (candidate_pool_frozen_) {
    if (candidate_pool_size != candidate_pool_size_) {
      RTC_LOG(LS_ERROR)
          << "Trying to change candidate pool size after pool was "
          << "frozen.";
      return false;
    }
    return true;
  }

  if (candidate_pool_size < 0) {
    RTC_LOG(LS_ERROR) << "Can't set negative pool size.";
    return false;
  }

  candidate_pool_size_ = candidate_pool_size;

  // If ICE servers changed, throw away any existing pooled sessions and create
  // new ones.
  if (ice_servers_changed) {
    pooled_sessions_.clear();
  }

  turn_customizer_ = turn_customizer;

  // If |candidate_pool_size_| is less than the number of pooled sessions, get
  // rid of the extras.
  while (candidate_pool_size_ < static_cast<int>(pooled_sessions_.size())) {
    pooled_sessions_.front().reset(nullptr);
    pooled_sessions_.pop_front();
  }

  // If |candidate_pool_size_| is greater than the number of pooled sessions,
  // create new sessions.
  while (static_cast<int>(pooled_sessions_.size()) < candidate_pool_size_) {
    PortAllocatorSession* pooled_session = CreateSessionInternal("", 0, "", "");
    pooled_session->StartGettingPorts();
    pooled_sessions_.push_back(
        std::unique_ptr<PortAllocatorSession>(pooled_session));
  }
  return true;
}

std::unique_ptr<PortAllocatorSession> PortAllocator::CreateSession(
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd) {
  auto session = std::unique_ptr<PortAllocatorSession>(
      CreateSessionInternal(content_name, component, ice_ufrag, ice_pwd));
  session->SetCandidateFilter(candidate_filter());
  return session;
}

std::unique_ptr<PortAllocatorSession> PortAllocator::TakePooledSession(
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd) {
  RTC_DCHECK(!ice_ufrag.empty());
  RTC_DCHECK(!ice_pwd.empty());
  if (pooled_sessions_.empty()) {
    return nullptr;
  }
  std::unique_ptr<PortAllocatorSession> ret =
      std::move(pooled_sessions_.front());
  ret->SetIceParameters(content_name, component, ice_ufrag, ice_pwd);
  // According to JSEP, a pooled session should filter candidates only after
  // it's taken out of the pool.
  ret->SetCandidateFilter(candidate_filter());
  pooled_sessions_.pop_front();
  return ret;
}

const PortAllocatorSession* PortAllocator::GetPooledSession() const {
  if (pooled_sessions_.empty()) {
    return nullptr;
  }
  return pooled_sessions_.front().get();
}

void PortAllocator::FreezeCandidatePool() {
  candidate_pool_frozen_ = true;
}

void PortAllocator::DiscardCandidatePool() {
  pooled_sessions_.clear();
}

}  // namespace cricket
