/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_CANDIDATE_H_
#define WEBRTC_P2P_BASE_CANDIDATE_H_

#include <limits.h>
#include <math.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

#include "webrtc/p2p/base/p2pconstants.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/network.h"
#include "webrtc/base/socketaddress.h"

namespace cricket {

// Candidate for ICE based connection discovery.

class Candidate {
 public:
  // TODO: Match the ordering and param list as per RFC 5245
  // candidate-attribute syntax. http://tools.ietf.org/html/rfc5245#section-15.1
  Candidate()
      : id_(rtc::CreateRandomString(8)),
        component_(0),
        priority_(0),
        network_type_(rtc::ADAPTER_TYPE_UNKNOWN),
        generation_(0),
        network_id_(0),
        network_cost_(0) {}

  Candidate(int component,
            const std::string& protocol,
            const rtc::SocketAddress& address,
            uint32_t priority,
            const std::string& username,
            const std::string& password,
            const std::string& type,
            uint32_t generation,
            const std::string& foundation,
            uint16_t network_id = 0,
            uint16_t network_cost = 0)
      : id_(rtc::CreateRandomString(8)),
        component_(component),
        protocol_(protocol),
        address_(address),
        priority_(priority),
        username_(username),
        password_(password),
        type_(type),
        network_type_(rtc::ADAPTER_TYPE_UNKNOWN),
        generation_(generation),
        foundation_(foundation),
        network_id_(network_id),
        network_cost_(network_cost) {}

  const std::string & id() const { return id_; }
  void set_id(const std::string & id) { id_ = id; }

  int component() const { return component_; }
  void set_component(int component) { component_ = component; }

  const std::string & protocol() const { return protocol_; }
  void set_protocol(const std::string & protocol) { protocol_ = protocol; }

  // The protocol used to talk to relay.
  const std::string& relay_protocol() const { return relay_protocol_; }
  void set_relay_protocol(const std::string& protocol) {
    relay_protocol_ = protocol;
  }

  const rtc::SocketAddress & address() const { return address_; }
  void set_address(const rtc::SocketAddress & address) {
    address_ = address;
  }

  uint32_t priority() const { return priority_; }
  void set_priority(const uint32_t priority) { priority_ = priority; }

  // TODO(pthatcher): Remove once Chromium's jingle/glue/utils.cc
  // doesn't use it.
  // Maps old preference (which was 0.0-1.0) to match priority (which
  // is 0-2^32-1) to to match RFC 5245, section 4.1.2.1.  Also see
  // https://docs.google.com/a/google.com/document/d/
  // 1iNQDiwDKMh0NQOrCqbj3DKKRT0Dn5_5UJYhmZO-t7Uc/edit
  float preference() const {
    // The preference value is clamped to two decimal precision.
    return static_cast<float>(((priority_ >> 24) * 100 / 127) / 100.0);
  }

  // TODO(pthatcher): Remove once Chromium's jingle/glue/utils.cc
  // doesn't use it.
  void set_preference(float preference) {
    // Limiting priority to UINT_MAX when value exceeds uint32_t max.
    // This can happen for e.g. when preference = 3.
    uint64_t prio_val = static_cast<uint64_t>(preference * 127) << 24;
    priority_ = static_cast<uint32_t>(
        std::min(prio_val, static_cast<uint64_t>(UINT_MAX)));
  }

  // TODO(honghaiz): Change to usernameFragment or ufrag.
  const std::string & username() const { return username_; }
  void set_username(const std::string & username) { username_ = username; }

  const std::string & password() const { return password_; }
  void set_password(const std::string & password) { password_ = password; }

  const std::string & type() const { return type_; }
  void set_type(const std::string & type) { type_ = type; }

  const std::string & network_name() const { return network_name_; }
  void set_network_name(const std::string & network_name) {
    network_name_ = network_name;
  }

  rtc::AdapterType network_type() const { return network_type_; }
  void set_network_type(rtc::AdapterType network_type) {
    network_type_ = network_type;
  }

  // Candidates in a new generation replace those in the old generation.
  uint32_t generation() const { return generation_; }
  void set_generation(uint32_t generation) { generation_ = generation; }
  const std::string generation_str() const {
    std::ostringstream ost;
    ost << generation_;
    return ost.str();
  }
  void set_generation_str(const std::string& str) {
    std::istringstream ist(str);
    ist >> generation_;
  }

  // |network_cost| measures the cost/penalty of using this candidate. A network
  // cost of 0 indicates this candidate can be used freely. A value of
  // rtc::kNetworkCostMax indicates it should be used only as the last resort.
  void set_network_cost(uint16_t network_cost) {
    ASSERT(network_cost <= rtc::kNetworkCostMax);
    network_cost_ = network_cost;
  }
  uint16_t network_cost() const { return network_cost_; }

  // An ID assigned to the network hosting the candidate.
  uint16_t network_id() const { return network_id_; }
  void set_network_id(uint16_t network_id) { network_id_ = network_id; }

  const std::string& foundation() const {
    return foundation_;
  }

  void set_foundation(const std::string& foundation) {
    foundation_ = foundation;
  }

  const rtc::SocketAddress & related_address() const {
    return related_address_;
  }
  void set_related_address(
      const rtc::SocketAddress & related_address) {
    related_address_ = related_address;
  }
  const std::string& tcptype() const { return tcptype_; }
  void set_tcptype(const std::string& tcptype){
    tcptype_ = tcptype;
  }

  // The name of the transport channel of this candidate.
  const std::string& transport_name() const { return transport_name_; }
  void set_transport_name(const std::string& transport_name) {
    transport_name_ = transport_name;
  }

  // Determines whether this candidate is equivalent to the given one.
  bool IsEquivalent(const Candidate& c) const {
    // We ignore the network name, since that is just debug information, and
    // the priority and the network cost, since they should be the same if the
    // rest are.
    return (component_ == c.component_) && (protocol_ == c.protocol_) &&
           (address_ == c.address_) && (username_ == c.username_) &&
           (password_ == c.password_) && (type_ == c.type_) &&
           (generation_ == c.generation_) && (foundation_ == c.foundation_) &&
           (related_address_ == c.related_address_) &&
           (network_id_ == c.network_id_);
  }

  // Determines whether this candidate can be considered equivalent to the
  // given one when looking for a matching candidate to remove.
  bool MatchesForRemoval(const Candidate& c) const {
    return component_ == c.component_ && protocol_ == c.protocol_ &&
           address_ == c.address_;
  }

  std::string ToString() const {
    return ToStringInternal(false);
  }

  std::string ToSensitiveString() const {
    return ToStringInternal(true);
  }

  uint32_t GetPriority(uint32_t type_preference,
                       int network_adapter_preference,
                       int relay_preference) const {
    // RFC 5245 - 4.1.2.1.
    // priority = (2^24)*(type preference) +
    //            (2^8)*(local preference) +
    //            (2^0)*(256 - component ID)

    // |local_preference| length is 2 bytes, 0-65535 inclusive.
    // In our implemenation we will partion local_preference into
    //              0                 1
    //       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //      |  NIC Pref     |    Addr Pref  |
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // NIC Type - Type of the network adapter e.g. 3G/Wifi/Wired.
    // Addr Pref - Address preference value as per RFC 3484.
    // local preference =  (NIC Type << 8 | Addr_Pref) - relay preference.

    int addr_pref = IPAddressPrecedence(address_.ipaddr());
    int local_preference = ((network_adapter_preference << 8) | addr_pref) +
        relay_preference;

    return (type_preference << 24) |
           (local_preference << 8) |
           (256 - component_);
  }

  bool operator==(const Candidate& o) const {
    return id_ == o.id_ && component_ == o.component_ &&
           protocol_ == o.protocol_ && relay_protocol_ == o.relay_protocol_ &&
           address_ == o.address_ && priority_ == o.priority_ &&
           username_ == o.username_ && password_ == o.password_ &&
           type_ == o.type_ && network_name_ == o.network_name_ &&
           network_type_ == o.network_type_ && generation_ == o.generation_ &&
           foundation_ == o.foundation_ &&
           related_address_ == o.related_address_ && tcptype_ == o.tcptype_ &&
           transport_name_ == o.transport_name_ && network_id_ == o.network_id_;
  }
  bool operator!=(const Candidate& o) const { return !(*this == o); }

 private:
  std::string ToStringInternal(bool sensitive) const {
    std::ostringstream ost;
    std::string address = sensitive ? address_.ToSensitiveString() :
                                      address_.ToString();
    ost << "Cand[" << transport_name_ << ":" << foundation_ << ":" << component_
        << ":" << protocol_ << ":" << priority_ << ":" << address << ":"
        << type_ << ":" << related_address_ << ":" << username_ << ":"
        << password_ << ":" << network_id_ << ":" << network_cost_ << ":"
        << generation_ << "]";
    return ost.str();
  }

  std::string id_;
  int component_;
  std::string protocol_;
  std::string relay_protocol_;
  rtc::SocketAddress address_;
  uint32_t priority_;
  std::string username_;
  std::string password_;
  std::string type_;
  std::string network_name_;
  rtc::AdapterType network_type_;
  uint32_t generation_;
  std::string foundation_;
  rtc::SocketAddress related_address_;
  std::string tcptype_;
  std::string transport_name_;
  uint16_t network_id_;
  uint16_t network_cost_;
};

// Used during parsing and writing to map component to channel name
// and back.  This is primarily for converting old G-ICE candidate
// signalling to new ICE candidate classes.
class CandidateTranslator {
 public:
  virtual ~CandidateTranslator() {}
  virtual bool GetChannelNameFromComponent(
      int component, std::string* channel_name) const = 0;
  virtual bool GetComponentFromChannelName(
      const std::string& channel_name, int* component) const = 0;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_CANDIDATE_H_
