/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/port.h"

#include <math.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "p2p/base/portallocator.h"
#include "rtc_base/checks.h"
#include "rtc_base/crc32.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/mdns_responder_interface.h"
#include "rtc_base/messagedigest.h"
#include "rtc_base/network.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/third_party/base64/base64.h"

namespace {

// Determines whether we have seen at least the given maximum number of
// pings fail to have a response.
inline bool TooManyFailures(
    const std::vector<cricket::Connection::SentPing>& pings_since_last_response,
    uint32_t maximum_failures,
    int rtt_estimate,
    int64_t now) {
  // If we haven't sent that many pings, then we can't have failed that many.
  if (pings_since_last_response.size() < maximum_failures)
    return false;

  // Check if the window in which we would expect a response to the ping has
  // already elapsed.
  int64_t expected_response_time =
      pings_since_last_response[maximum_failures - 1].sent_time + rtt_estimate;
  return now > expected_response_time;
}

// Determines whether we have gone too long without seeing any response.
inline bool TooLongWithoutResponse(
    const std::vector<cricket::Connection::SentPing>& pings_since_last_response,
    int64_t maximum_time,
    int64_t now) {
  if (pings_since_last_response.size() == 0)
    return false;

  auto first = pings_since_last_response[0];
  return now > (first.sent_time + maximum_time);
}

// Helper methods for converting string values of log description fields to
// enum.
webrtc::IceCandidateType GetCandidateTypeByString(const std::string& type) {
  if (type == cricket::LOCAL_PORT_TYPE) {
    return webrtc::IceCandidateType::kLocal;
  } else if (type == cricket::STUN_PORT_TYPE) {
    return webrtc::IceCandidateType::kStun;
  } else if (type == cricket::PRFLX_PORT_TYPE) {
    return webrtc::IceCandidateType::kPrflx;
  } else if (type == cricket::RELAY_PORT_TYPE) {
    return webrtc::IceCandidateType::kRelay;
  }
  return webrtc::IceCandidateType::kUnknown;
}

webrtc::IceCandidatePairProtocol GetProtocolByString(
    const std::string& protocol) {
  if (protocol == cricket::UDP_PROTOCOL_NAME) {
    return webrtc::IceCandidatePairProtocol::kUdp;
  } else if (protocol == cricket::TCP_PROTOCOL_NAME) {
    return webrtc::IceCandidatePairProtocol::kTcp;
  } else if (protocol == cricket::SSLTCP_PROTOCOL_NAME) {
    return webrtc::IceCandidatePairProtocol::kSsltcp;
  } else if (protocol == cricket::TLS_PROTOCOL_NAME) {
    return webrtc::IceCandidatePairProtocol::kTls;
  }
  return webrtc::IceCandidatePairProtocol::kUnknown;
}

webrtc::IceCandidatePairAddressFamily GetAddressFamilyByInt(
    int address_family) {
  if (address_family == AF_INET) {
    return webrtc::IceCandidatePairAddressFamily::kIpv4;
  } else if (address_family == AF_INET6) {
    return webrtc::IceCandidatePairAddressFamily::kIpv6;
  }
  return webrtc::IceCandidatePairAddressFamily::kUnknown;
}

webrtc::IceCandidateNetworkType ConvertNetworkType(rtc::AdapterType type) {
  if (type == rtc::ADAPTER_TYPE_ETHERNET) {
    return webrtc::IceCandidateNetworkType::kEthernet;
  } else if (type == rtc::ADAPTER_TYPE_LOOPBACK) {
    return webrtc::IceCandidateNetworkType::kLoopback;
  } else if (type == rtc::ADAPTER_TYPE_WIFI) {
    return webrtc::IceCandidateNetworkType::kWifi;
  } else if (type == rtc::ADAPTER_TYPE_VPN) {
    return webrtc::IceCandidateNetworkType::kVpn;
  } else if (type == rtc::ADAPTER_TYPE_CELLULAR) {
    return webrtc::IceCandidateNetworkType::kCellular;
  }
  return webrtc::IceCandidateNetworkType::kUnknown;
}

rtc::PacketInfoProtocolType ConvertProtocolTypeToPacketInfoProtocolType(
    cricket::ProtocolType type) {
  switch (type) {
    case cricket::ProtocolType::PROTO_UDP:
      return rtc::PacketInfoProtocolType::kUdp;
    case cricket::ProtocolType::PROTO_TCP:
      return rtc::PacketInfoProtocolType::kTcp;
    case cricket::ProtocolType::PROTO_SSLTCP:
      return rtc::PacketInfoProtocolType::kSsltcp;
    case cricket::ProtocolType::PROTO_TLS:
      return rtc::PacketInfoProtocolType::kTls;
    default:
      return rtc::PacketInfoProtocolType::kUnknown;
  }
}

// We will restrict RTT estimates (when used for determining state) to be
// within a reasonable range.
const int MINIMUM_RTT = 100;    // 0.1 seconds
const int MAXIMUM_RTT = 60000;  // 60 seconds

// When we don't have any RTT data, we have to pick something reasonable.  We
// use a large value just in case the connection is really slow.
const int DEFAULT_RTT = 3000;  // 3 seconds

// Computes our estimate of the RTT given the current estimate.
inline int ConservativeRTTEstimate(int rtt) {
  return rtc::SafeClamp(2 * rtt, MINIMUM_RTT, MAXIMUM_RTT);
}

// Weighting of the old rtt value to new data.
const int RTT_RATIO = 3;  // 3 : 1

// The delay before we begin checking if this port is useless. We set
// it to a little higher than a total STUN timeout.
const int kPortTimeoutDelay = cricket::STUN_TOTAL_TIMEOUT + 5000;

// For packet loss estimation.
const int64_t kConsiderPacketLostAfter = 3000;  // 3 seconds

// For packet loss estimation.
const int64_t kForgetPacketAfter = 30000;  // 30 seconds

}  // namespace

namespace cricket {

using webrtc::RTCErrorType;
using webrtc::RTCError;

// TODO(ronghuawu): Use "local", "srflx", "prflx" and "relay". But this requires
// the signaling part be updated correspondingly as well.
const char LOCAL_PORT_TYPE[] = "local";
const char STUN_PORT_TYPE[] = "stun";
const char PRFLX_PORT_TYPE[] = "prflx";
const char RELAY_PORT_TYPE[] = "relay";

static const char* const PROTO_NAMES[] = {UDP_PROTOCOL_NAME, TCP_PROTOCOL_NAME,
                                          SSLTCP_PROTOCOL_NAME,
                                          TLS_PROTOCOL_NAME};

const char* ProtoToString(ProtocolType proto) {
  return PROTO_NAMES[proto];
}

bool StringToProto(const char* value, ProtocolType* proto) {
  for (size_t i = 0; i <= PROTO_LAST; ++i) {
    if (absl::EqualsIgnoreCase(PROTO_NAMES[i], value)) {
      *proto = static_cast<ProtocolType>(i);
      return true;
    }
  }
  return false;
}

// RFC 6544, TCP candidate encoding rules.
const int DISCARD_PORT = 9;
const char TCPTYPE_ACTIVE_STR[] = "active";
const char TCPTYPE_PASSIVE_STR[] = "passive";
const char TCPTYPE_SIMOPEN_STR[] = "so";

// Foundation:  An arbitrary string that is the same for two candidates
//   that have the same type, base IP address, protocol (UDP, TCP,
//   etc.), and STUN or TURN server.  If any of these are different,
//   then the foundation will be different.  Two candidate pairs with
//   the same foundation pairs are likely to have similar network
//   characteristics.  Foundations are used in the frozen algorithm.
static std::string ComputeFoundation(const std::string& type,
                                     const std::string& protocol,
                                     const std::string& relay_protocol,
                                     const rtc::SocketAddress& base_address) {
  rtc::StringBuilder sb;
  sb << type << base_address.ipaddr().ToString() << protocol << relay_protocol;
  return rtc::ToString(rtc::ComputeCrc32(sb.Release()));
}

CandidateStats::CandidateStats() = default;

CandidateStats::CandidateStats(const CandidateStats&) = default;

CandidateStats::CandidateStats(Candidate candidate) {
  this->candidate = candidate;
}

CandidateStats::~CandidateStats() = default;

ConnectionInfo::ConnectionInfo()
    : best_connection(false),
      writable(false),
      receiving(false),
      timeout(false),
      new_connection(false),
      rtt(0),
      sent_total_bytes(0),
      sent_bytes_second(0),
      sent_discarded_packets(0),
      sent_total_packets(0),
      sent_ping_requests_total(0),
      sent_ping_requests_before_first_response(0),
      sent_ping_responses(0),
      recv_total_bytes(0),
      recv_bytes_second(0),
      recv_ping_requests(0),
      recv_ping_responses(0),
      key(nullptr),
      state(IceCandidatePairState::WAITING),
      priority(0),
      nominated(false),
      total_round_trip_time_ms(0) {}

ConnectionInfo::ConnectionInfo(const ConnectionInfo&) = default;

ConnectionInfo::~ConnectionInfo() = default;

Port::Port(rtc::Thread* thread,
           const std::string& type,
           rtc::PacketSocketFactory* factory,
           rtc::Network* network,
           const std::string& username_fragment,
           const std::string& password)
    : thread_(thread),
      factory_(factory),
      type_(type),
      send_retransmit_count_attribute_(false),
      network_(network),
      min_port_(0),
      max_port_(0),
      component_(ICE_CANDIDATE_COMPONENT_DEFAULT),
      generation_(0),
      ice_username_fragment_(username_fragment),
      password_(password),
      timeout_delay_(kPortTimeoutDelay),
      enable_port_packets_(false),
      ice_role_(ICEROLE_UNKNOWN),
      tiebreaker_(0),
      shared_socket_(true),
      weak_factory_(this) {
  Construct();
}

Port::Port(rtc::Thread* thread,
           const std::string& type,
           rtc::PacketSocketFactory* factory,
           rtc::Network* network,
           uint16_t min_port,
           uint16_t max_port,
           const std::string& username_fragment,
           const std::string& password)
    : thread_(thread),
      factory_(factory),
      type_(type),
      send_retransmit_count_attribute_(false),
      network_(network),
      min_port_(min_port),
      max_port_(max_port),
      component_(ICE_CANDIDATE_COMPONENT_DEFAULT),
      generation_(0),
      ice_username_fragment_(username_fragment),
      password_(password),
      timeout_delay_(kPortTimeoutDelay),
      enable_port_packets_(false),
      ice_role_(ICEROLE_UNKNOWN),
      tiebreaker_(0),
      shared_socket_(false),
      weak_factory_(this) {
  RTC_DCHECK(factory_ != NULL);
  Construct();
}

void Port::Construct() {
  // TODO(pthatcher): Remove this old behavior once we're sure no one
  // relies on it.  If the username_fragment and password are empty,
  // we should just create one.
  if (ice_username_fragment_.empty()) {
    RTC_DCHECK(password_.empty());
    ice_username_fragment_ = rtc::CreateRandomString(ICE_UFRAG_LENGTH);
    password_ = rtc::CreateRandomString(ICE_PWD_LENGTH);
  }
  network_->SignalTypeChanged.connect(this, &Port::OnNetworkTypeChanged);
  network_cost_ = network_->GetCost();

  thread_->PostDelayed(RTC_FROM_HERE, timeout_delay_, this,
                       MSG_DESTROY_IF_DEAD);
  RTC_LOG(LS_INFO) << ToString() << ": Port created with network cost "
                   << network_cost_;
}

Port::~Port() {
  // Delete all of the remaining connections.  We copy the list up front
  // because each deletion will cause it to be modified.

  std::vector<Connection*> list;

  AddressMap::iterator iter = connections_.begin();
  while (iter != connections_.end()) {
    list.push_back(iter->second);
    ++iter;
  }

  for (uint32_t i = 0; i < list.size(); i++)
    delete list[i];
}

const std::string& Port::Type() const {
  return type_;
}
rtc::Network* Port::Network() const {
  return network_;
}

IceRole Port::GetIceRole() const {
  return ice_role_;
}

void Port::SetIceRole(IceRole role) {
  ice_role_ = role;
}

void Port::SetIceTiebreaker(uint64_t tiebreaker) {
  tiebreaker_ = tiebreaker;
}
uint64_t Port::IceTiebreaker() const {
  return tiebreaker_;
}

bool Port::SharedSocket() const {
  return shared_socket_;
}

void Port::SetIceParameters(int component,
                            const std::string& username_fragment,
                            const std::string& password) {
  component_ = component;
  ice_username_fragment_ = username_fragment;
  password_ = password;
  for (Candidate& c : candidates_) {
    c.set_component(component);
    c.set_username(username_fragment);
    c.set_password(password);
  }
}

const std::vector<Candidate>& Port::Candidates() const {
  return candidates_;
}

Connection* Port::GetConnection(const rtc::SocketAddress& remote_addr) {
  AddressMap::const_iterator iter = connections_.find(remote_addr);
  if (iter != connections_.end())
    return iter->second;
  else
    return NULL;
}

void Port::AddAddress(const rtc::SocketAddress& address,
                      const rtc::SocketAddress& base_address,
                      const rtc::SocketAddress& related_address,
                      const std::string& protocol,
                      const std::string& relay_protocol,
                      const std::string& tcptype,
                      const std::string& type,
                      uint32_t type_preference,
                      uint32_t relay_preference,
                      bool is_final) {
  AddAddress(address, base_address, related_address, protocol, relay_protocol,
             tcptype, type, type_preference, relay_preference, "", is_final);
}

void Port::AddAddress(const rtc::SocketAddress& address,
                      const rtc::SocketAddress& base_address,
                      const rtc::SocketAddress& related_address,
                      const std::string& protocol,
                      const std::string& relay_protocol,
                      const std::string& tcptype,
                      const std::string& type,
                      uint32_t type_preference,
                      uint32_t relay_preference,
                      const std::string& url,
                      bool is_final) {
  if (protocol == TCP_PROTOCOL_NAME && type == LOCAL_PORT_TYPE) {
    RTC_DCHECK(!tcptype.empty());
  }

  std::string foundation =
      ComputeFoundation(type, protocol, relay_protocol, base_address);
  Candidate c(component_, protocol, address, 0U, username_fragment(), password_,
              type, generation_, foundation, network_->id(), network_cost_);
  c.set_priority(
      c.GetPriority(type_preference, network_->preference(), relay_preference));
  c.set_relay_protocol(relay_protocol);
  c.set_tcptype(tcptype);
  c.set_network_name(network_->name());
  c.set_network_type(network_->type());
  c.set_url(url);
  // TODO(bugs.webrtc.org/9723): Use a config to control the feature of IP
  // handling with mDNS.
  if (network_->GetMdnsResponder() != nullptr) {
    // Obfuscate the IP address of a host candidates by an mDNS hostname.
    if (type == LOCAL_PORT_TYPE) {
      auto weak_ptr = weak_factory_.GetWeakPtr();
      auto callback = [weak_ptr, c, is_final](const rtc::IPAddress& addr,
                                              const std::string& name) mutable {
        RTC_DCHECK(c.address().ipaddr() == addr);
        rtc::SocketAddress hostname_address(name, c.address().port());
        // In Port and Connection, we need the IP address information to
        // correctly handle the update of candidate type to prflx. The removal
        // of IP address when signaling this candidate will take place in
        // BasicPortAllocatorSession::OnCandidateReady, via SanitizeCandidate.
        hostname_address.SetResolvedIP(addr);
        c.set_address(hostname_address);
        RTC_DCHECK(c.related_address() == rtc::SocketAddress());
        if (weak_ptr != nullptr) {
          weak_ptr->set_mdns_name_registration_status(
              MdnsNameRegistrationStatus::kCompleted);
          weak_ptr->FinishAddingAddress(c, is_final);
        }
      };
      set_mdns_name_registration_status(
          MdnsNameRegistrationStatus::kInProgress);
      network_->GetMdnsResponder()->CreateNameForAddress(c.address().ipaddr(),
                                                         callback);
      return;
    }
    // For other types of candidates, the related address should be set to
    // 0.0.0.0 or ::0.
    c.set_related_address(rtc::SocketAddress());
  } else {
    c.set_related_address(related_address);
  }
  FinishAddingAddress(c, is_final);
}

void Port::FinishAddingAddress(const Candidate& c, bool is_final) {
  candidates_.push_back(c);
  SignalCandidateReady(this, c);

  PostAddAddress(is_final);
}

void Port::PostAddAddress(bool is_final) {
  if (is_final) {
    SignalPortComplete(this);
  }
}

void Port::AddOrReplaceConnection(Connection* conn) {
  auto ret = connections_.insert(
      std::make_pair(conn->remote_candidate().address(), conn));
  // If there is a different connection on the same remote address, replace
  // it with the new one and destroy the old one.
  if (ret.second == false && ret.first->second != conn) {
    RTC_LOG(LS_WARNING)
        << ToString()
        << ": A new connection was created on an existing remote address. "
           "New remote candidate: "
        << conn->remote_candidate().ToString();
    ret.first->second->SignalDestroyed.disconnect(this);
    ret.first->second->Destroy();
    ret.first->second = conn;
  }
  conn->SignalDestroyed.connect(this, &Port::OnConnectionDestroyed);
  SignalConnectionCreated(this, conn);
}

void Port::OnReadPacket(const char* data,
                        size_t size,
                        const rtc::SocketAddress& addr,
                        ProtocolType proto) {
  // If the user has enabled port packets, just hand this over.
  if (enable_port_packets_) {
    SignalReadPacket(this, data, size, addr);
    return;
  }

  // If this is an authenticated STUN request, then signal unknown address and
  // send back a proper binding response.
  std::unique_ptr<IceMessage> msg;
  std::string remote_username;
  if (!GetStunMessage(data, size, addr, &msg, &remote_username)) {
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Received non-STUN packet from unknown address: "
                      << addr.ToSensitiveString();
  } else if (!msg) {
    // STUN message handled already
  } else if (msg->type() == STUN_BINDING_REQUEST) {
    RTC_LOG(LS_INFO) << "Received STUN ping id="
                     << rtc::hex_encode(msg->transaction_id())
                     << " from unknown address " << addr.ToSensitiveString();
    // We need to signal an unknown address before we handle any role conflict
    // below. Otherwise there would be no candidate pair and TURN entry created
    // to send the error response in case of a role conflict.
    SignalUnknownAddress(this, addr, proto, msg.get(), remote_username, false);
    // Check for role conflicts.
    if (!MaybeIceRoleConflict(addr, msg.get(), remote_username)) {
      RTC_LOG(LS_INFO) << "Received conflicting role from the peer.";
      return;
    }
  } else {
    // NOTE(tschmelcher): STUN_BINDING_RESPONSE is benign. It occurs if we
    // pruned a connection for this port while it had STUN requests in flight,
    // because we then get back responses for them, which this code correctly
    // does not handle.
    if (msg->type() != STUN_BINDING_RESPONSE) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Received unexpected STUN message type: "
                        << msg->type() << " from unknown address: "
                        << addr.ToSensitiveString();
    }
  }
}

void Port::OnReadyToSend() {
  AddressMap::iterator iter = connections_.begin();
  for (; iter != connections_.end(); ++iter) {
    iter->second->OnReadyToSend();
  }
}

size_t Port::AddPrflxCandidate(const Candidate& local) {
  candidates_.push_back(local);
  return (candidates_.size() - 1);
}

bool Port::GetStunMessage(const char* data,
                          size_t size,
                          const rtc::SocketAddress& addr,
                          std::unique_ptr<IceMessage>* out_msg,
                          std::string* out_username) {
  // NOTE: This could clearly be optimized to avoid allocating any memory.
  //       However, at the data rates we'll be looking at on the client side,
  //       this probably isn't worth worrying about.
  RTC_DCHECK(out_msg != NULL);
  RTC_DCHECK(out_username != NULL);
  out_username->clear();

  // Don't bother parsing the packet if we can tell it's not STUN.
  // In ICE mode, all STUN packets will have a valid fingerprint.
  if (!StunMessage::ValidateFingerprint(data, size)) {
    return false;
  }

  // Parse the request message.  If the packet is not a complete and correct
  // STUN message, then ignore it.
  std::unique_ptr<IceMessage> stun_msg(new IceMessage());
  rtc::ByteBufferReader buf(data, size);
  if (!stun_msg->Read(&buf) || (buf.Length() > 0)) {
    return false;
  }

  if (stun_msg->type() == STUN_BINDING_REQUEST) {
    // Check for the presence of USERNAME and MESSAGE-INTEGRITY (if ICE) first.
    // If not present, fail with a 400 Bad Request.
    if (!stun_msg->GetByteString(STUN_ATTR_USERNAME) ||
        !stun_msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY)) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Received STUN request without username/M-I from: "
                        << addr.ToSensitiveString();
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_BAD_REQUEST,
                               STUN_ERROR_REASON_BAD_REQUEST);
      return true;
    }

    // If the username is bad or unknown, fail with a 401 Unauthorized.
    std::string local_ufrag;
    std::string remote_ufrag;
    if (!ParseStunUsername(stun_msg.get(), &local_ufrag, &remote_ufrag) ||
        local_ufrag != username_fragment()) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Received STUN request with bad local username "
                        << local_ufrag << " from " << addr.ToSensitiveString();
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                               STUN_ERROR_REASON_UNAUTHORIZED);
      return true;
    }

    // If ICE, and the MESSAGE-INTEGRITY is bad, fail with a 401 Unauthorized
    if (!stun_msg->ValidateMessageIntegrity(data, size, password_)) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Received STUN request with bad M-I from "
                        << addr.ToSensitiveString()
                        << ", password_=" << password_;
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                               STUN_ERROR_REASON_UNAUTHORIZED);
      return true;
    }
    out_username->assign(remote_ufrag);
  } else if ((stun_msg->type() == STUN_BINDING_RESPONSE) ||
             (stun_msg->type() == STUN_BINDING_ERROR_RESPONSE)) {
    if (stun_msg->type() == STUN_BINDING_ERROR_RESPONSE) {
      if (const StunErrorCodeAttribute* error_code = stun_msg->GetErrorCode()) {
        RTC_LOG(LS_ERROR) << ToString()
                          << ": Received STUN binding error: class="
                          << error_code->eclass()
                          << " number=" << error_code->number() << " reason='"
                          << error_code->reason() << "' from "
                          << addr.ToSensitiveString();
        // Return message to allow error-specific processing
      } else {
        RTC_LOG(LS_ERROR)
            << ToString()
            << ": Received STUN binding error without a error code from "
            << addr.ToSensitiveString();
        return true;
      }
    }
    // NOTE: Username should not be used in verifying response messages.
    out_username->clear();
  } else if (stun_msg->type() == STUN_BINDING_INDICATION) {
    RTC_LOG(LS_VERBOSE) << ToString()
                        << ": Received STUN binding indication: from "
                        << addr.ToSensitiveString();
    out_username->clear();
    // No stun attributes will be verified, if it's stun indication message.
    // Returning from end of the this method.
  } else {
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Received STUN packet with invalid type ("
                      << stun_msg->type() << ") from "
                      << addr.ToSensitiveString();
    return true;
  }

  // Return the STUN message found.
  *out_msg = std::move(stun_msg);
  return true;
}

bool Port::IsCompatibleAddress(const rtc::SocketAddress& addr) {
  // Get a representative IP for the Network this port is configured to use.
  rtc::IPAddress ip = network_->GetBestIP();
  // We use single-stack sockets, so families must match.
  if (addr.family() != ip.family()) {
    return false;
  }
  // Link-local IPv6 ports can only connect to other link-local IPv6 ports.
  if (ip.family() == AF_INET6 &&
      (IPIsLinkLocal(ip) != IPIsLinkLocal(addr.ipaddr()))) {
    return false;
  }
  return true;
}

rtc::DiffServCodePoint Port::StunDscpValue() const {
  // By default, inherit from whatever the MediaChannel sends.
  return rtc::DSCP_NO_CHANGE;
}

bool Port::ParseStunUsername(const StunMessage* stun_msg,
                             std::string* local_ufrag,
                             std::string* remote_ufrag) const {
  // The packet must include a username that either begins or ends with our
  // fragment.  It should begin with our fragment if it is a request and it
  // should end with our fragment if it is a response.
  local_ufrag->clear();
  remote_ufrag->clear();
  const StunByteStringAttribute* username_attr =
      stun_msg->GetByteString(STUN_ATTR_USERNAME);
  if (username_attr == NULL)
    return false;

  // RFRAG:LFRAG
  const std::string username = username_attr->GetString();
  size_t colon_pos = username.find(":");
  if (colon_pos == std::string::npos) {
    return false;
  }

  *local_ufrag = username.substr(0, colon_pos);
  *remote_ufrag = username.substr(colon_pos + 1, username.size());
  return true;
}

bool Port::MaybeIceRoleConflict(const rtc::SocketAddress& addr,
                                IceMessage* stun_msg,
                                const std::string& remote_ufrag) {
  // Validate ICE_CONTROLLING or ICE_CONTROLLED attributes.
  bool ret = true;
  IceRole remote_ice_role = ICEROLE_UNKNOWN;
  uint64_t remote_tiebreaker = 0;
  const StunUInt64Attribute* stun_attr =
      stun_msg->GetUInt64(STUN_ATTR_ICE_CONTROLLING);
  if (stun_attr) {
    remote_ice_role = ICEROLE_CONTROLLING;
    remote_tiebreaker = stun_attr->value();
  }

  // If |remote_ufrag| is same as port local username fragment and
  // tie breaker value received in the ping message matches port
  // tiebreaker value this must be a loopback call.
  // We will treat this as valid scenario.
  if (remote_ice_role == ICEROLE_CONTROLLING &&
      username_fragment() == remote_ufrag &&
      remote_tiebreaker == IceTiebreaker()) {
    return true;
  }

  stun_attr = stun_msg->GetUInt64(STUN_ATTR_ICE_CONTROLLED);
  if (stun_attr) {
    remote_ice_role = ICEROLE_CONTROLLED;
    remote_tiebreaker = stun_attr->value();
  }

  switch (ice_role_) {
    case ICEROLE_CONTROLLING:
      if (ICEROLE_CONTROLLING == remote_ice_role) {
        if (remote_tiebreaker >= tiebreaker_) {
          SignalRoleConflict(this);
        } else {
          // Send Role Conflict (487) error response.
          SendBindingErrorResponse(stun_msg, addr, STUN_ERROR_ROLE_CONFLICT,
                                   STUN_ERROR_REASON_ROLE_CONFLICT);
          ret = false;
        }
      }
      break;
    case ICEROLE_CONTROLLED:
      if (ICEROLE_CONTROLLED == remote_ice_role) {
        if (remote_tiebreaker < tiebreaker_) {
          SignalRoleConflict(this);
        } else {
          // Send Role Conflict (487) error response.
          SendBindingErrorResponse(stun_msg, addr, STUN_ERROR_ROLE_CONFLICT,
                                   STUN_ERROR_REASON_ROLE_CONFLICT);
          ret = false;
        }
      }
      break;
    default:
      RTC_NOTREACHED();
  }
  return ret;
}

void Port::CreateStunUsername(const std::string& remote_username,
                              std::string* stun_username_attr_str) const {
  stun_username_attr_str->clear();
  *stun_username_attr_str = remote_username;
  stun_username_attr_str->append(":");
  stun_username_attr_str->append(username_fragment());
}

bool Port::HandleIncomingPacket(rtc::AsyncPacketSocket* socket,
                                const char* data,
                                size_t size,
                                const rtc::SocketAddress& remote_addr,
                                int64_t packet_time_us) {
  RTC_NOTREACHED();
  return false;
}

bool Port::CanHandleIncomingPacketsFrom(const rtc::SocketAddress&) const {
  return false;
}

void Port::SendBindingResponse(StunMessage* request,
                               const rtc::SocketAddress& addr) {
  RTC_DCHECK(request->type() == STUN_BINDING_REQUEST);

  // Retrieve the username from the request.
  const StunByteStringAttribute* username_attr =
      request->GetByteString(STUN_ATTR_USERNAME);
  RTC_DCHECK(username_attr != NULL);
  if (username_attr == NULL) {
    // No valid username, skip the response.
    return;
  }

  // Fill in the response message.
  StunMessage response;
  response.SetType(STUN_BINDING_RESPONSE);
  response.SetTransactionID(request->transaction_id());
  const StunUInt32Attribute* retransmit_attr =
      request->GetUInt32(STUN_ATTR_RETRANSMIT_COUNT);
  if (retransmit_attr) {
    // Inherit the incoming retransmit value in the response so the other side
    // can see our view of lost pings.
    response.AddAttribute(absl::make_unique<StunUInt32Attribute>(
        STUN_ATTR_RETRANSMIT_COUNT, retransmit_attr->value()));

    if (retransmit_attr->value() > CONNECTION_WRITE_CONNECT_FAILURES) {
      RTC_LOG(LS_INFO)
          << ToString()
          << ": Received a remote ping with high retransmit count: "
          << retransmit_attr->value();
    }
  }

  response.AddAttribute(absl::make_unique<StunXorAddressAttribute>(
      STUN_ATTR_XOR_MAPPED_ADDRESS, addr));
  response.AddMessageIntegrity(password_);
  response.AddFingerprint();

  // Send the response message.
  rtc::ByteBufferWriter buf;
  response.Write(&buf);
  rtc::PacketOptions options(StunDscpValue());
  options.info_signaled_after_sent.packet_type =
      rtc::PacketType::kIceConnectivityCheckResponse;
  auto err = SendTo(buf.Data(), buf.Length(), addr, options, false);
  if (err < 0) {
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Failed to send STUN ping response, to="
                      << addr.ToSensitiveString() << ", err=" << err
                      << ", id=" << rtc::hex_encode(response.transaction_id());
  } else {
    // Log at LS_INFO if we send a stun ping response on an unwritable
    // connection.
    Connection* conn = GetConnection(addr);
    rtc::LoggingSeverity sev =
        (conn && !conn->writable()) ? rtc::LS_INFO : rtc::LS_VERBOSE;
    RTC_LOG_V(sev) << ToString() << ": Sent STUN ping response, to="
                   << addr.ToSensitiveString()
                   << ", id=" << rtc::hex_encode(response.transaction_id());

    conn->stats_.sent_ping_responses++;
    conn->LogCandidatePairEvent(
        webrtc::IceCandidatePairEventType::kCheckResponseSent);
  }
}

void Port::SendBindingErrorResponse(StunMessage* request,
                                    const rtc::SocketAddress& addr,
                                    int error_code,
                                    const std::string& reason) {
  RTC_DCHECK(request->type() == STUN_BINDING_REQUEST);

  // Fill in the response message.
  StunMessage response;
  response.SetType(STUN_BINDING_ERROR_RESPONSE);
  response.SetTransactionID(request->transaction_id());

  // When doing GICE, we need to write out the error code incorrectly to
  // maintain backwards compatiblility.
  auto error_attr = StunAttribute::CreateErrorCode();
  error_attr->SetCode(error_code);
  error_attr->SetReason(reason);
  response.AddAttribute(std::move(error_attr));

  // Per Section 10.1.2, certain error cases don't get a MESSAGE-INTEGRITY,
  // because we don't have enough information to determine the shared secret.
  if (error_code != STUN_ERROR_BAD_REQUEST &&
      error_code != STUN_ERROR_UNAUTHORIZED)
    response.AddMessageIntegrity(password_);
  response.AddFingerprint();

  // Send the response message.
  rtc::ByteBufferWriter buf;
  response.Write(&buf);
  rtc::PacketOptions options(StunDscpValue());
  options.info_signaled_after_sent.packet_type =
      rtc::PacketType::kIceConnectivityCheckResponse;
  SendTo(buf.Data(), buf.Length(), addr, options, false);
  RTC_LOG(LS_INFO) << ToString()
                   << ": Sending STUN binding error: reason=" << reason
                   << " to " << addr.ToSensitiveString();
}

void Port::KeepAliveUntilPruned() {
  // If it is pruned, we won't bring it up again.
  if (state_ == State::INIT) {
    state_ = State::KEEP_ALIVE_UNTIL_PRUNED;
  }
}

void Port::Prune() {
  state_ = State::PRUNED;
  thread_->Post(RTC_FROM_HERE, this, MSG_DESTROY_IF_DEAD);
}

void Port::OnMessage(rtc::Message* pmsg) {
  RTC_DCHECK(pmsg->message_id == MSG_DESTROY_IF_DEAD);
  bool dead =
      (state_ == State::INIT || state_ == State::PRUNED) &&
      connections_.empty() &&
      rtc::TimeMillis() - last_time_all_connections_removed_ >= timeout_delay_;
  if (dead) {
    Destroy();
  }
}

void Port::OnNetworkTypeChanged(const rtc::Network* network) {
  RTC_DCHECK(network == network_);

  UpdateNetworkCost();
}

std::string Port::ToString() const {
  rtc::StringBuilder ss;
  ss << "Port[" << rtc::ToHex(reinterpret_cast<uintptr_t>(this)) << ":"
     << content_name_ << ":" << component_ << ":" << generation_ << ":" << type_
     << ":" << network_->ToString() << "]";
  return ss.Release();
}

// TODO(honghaiz): Make the network cost configurable from user setting.
void Port::UpdateNetworkCost() {
  uint16_t new_cost = network_->GetCost();
  if (network_cost_ == new_cost) {
    return;
  }
  RTC_LOG(LS_INFO) << "Network cost changed from " << network_cost_ << " to "
                   << new_cost
                   << ". Number of candidates created: " << candidates_.size()
                   << ". Number of connections created: "
                   << connections_.size();
  network_cost_ = new_cost;
  for (cricket::Candidate& candidate : candidates_) {
    candidate.set_network_cost(network_cost_);
  }
  // Network cost change will affect the connection selection criteria.
  // Signal the connection state change on each connection to force a
  // re-sort in P2PTransportChannel.
  for (auto kv : connections_) {
    Connection* conn = kv.second;
    conn->SignalStateChange(conn);
  }
}

void Port::EnablePortPackets() {
  enable_port_packets_ = true;
}

void Port::OnConnectionDestroyed(Connection* conn) {
  AddressMap::iterator iter =
      connections_.find(conn->remote_candidate().address());
  RTC_DCHECK(iter != connections_.end());
  connections_.erase(iter);
  HandleConnectionDestroyed(conn);

  // Ports time out after all connections fail if it is not marked as
  // "keep alive until pruned."
  // Note: If a new connection is added after this message is posted, but it
  // fails and is removed before kPortTimeoutDelay, then this message will
  // not cause the Port to be destroyed.
  if (connections_.empty()) {
    last_time_all_connections_removed_ = rtc::TimeMillis();
    thread_->PostDelayed(RTC_FROM_HERE, timeout_delay_, this,
                         MSG_DESTROY_IF_DEAD);
  }
}

void Port::Destroy() {
  RTC_DCHECK(connections_.empty());
  RTC_LOG(LS_INFO) << ToString() << ": Port deleted";
  SignalDestroyed(this);
  delete this;
}

const std::string Port::username_fragment() const {
  return ice_username_fragment_;
}

void Port::CopyPortInformationToPacketInfo(rtc::PacketInfo* info) const {
  info->protocol = ConvertProtocolTypeToPacketInfoProtocolType(GetProtocol());
  info->network_id = Network()->id();
}

// A ConnectionRequest is a simple STUN ping used to determine writability.
class ConnectionRequest : public StunRequest {
 public:
  explicit ConnectionRequest(Connection* connection)
      : StunRequest(new IceMessage()), connection_(connection) {}

  void Prepare(StunMessage* request) override {
    request->SetType(STUN_BINDING_REQUEST);
    std::string username;
    connection_->port()->CreateStunUsername(
        connection_->remote_candidate().username(), &username);
    request->AddAttribute(absl::make_unique<StunByteStringAttribute>(
        STUN_ATTR_USERNAME, username));

    // connection_ already holds this ping, so subtract one from count.
    if (connection_->port()->send_retransmit_count_attribute()) {
      request->AddAttribute(absl::make_unique<StunUInt32Attribute>(
          STUN_ATTR_RETRANSMIT_COUNT,
          static_cast<uint32_t>(connection_->pings_since_last_response_.size() -
                                1)));
    }
    uint32_t network_info = connection_->port()->Network()->id();
    network_info = (network_info << 16) | connection_->port()->network_cost();
    request->AddAttribute(absl::make_unique<StunUInt32Attribute>(
        STUN_ATTR_NETWORK_INFO, network_info));

    // Adding ICE_CONTROLLED or ICE_CONTROLLING attribute based on the role.
    if (connection_->port()->GetIceRole() == ICEROLE_CONTROLLING) {
      request->AddAttribute(absl::make_unique<StunUInt64Attribute>(
          STUN_ATTR_ICE_CONTROLLING, connection_->port()->IceTiebreaker()));
      // We should have either USE_CANDIDATE attribute or ICE_NOMINATION
      // attribute but not both. That was enforced in p2ptransportchannel.
      if (connection_->use_candidate_attr()) {
        request->AddAttribute(absl::make_unique<StunByteStringAttribute>(
            STUN_ATTR_USE_CANDIDATE));
      }
      if (connection_->nomination() &&
          connection_->nomination() != connection_->acked_nomination()) {
        request->AddAttribute(absl::make_unique<StunUInt32Attribute>(
            STUN_ATTR_NOMINATION, connection_->nomination()));
      }
    } else if (connection_->port()->GetIceRole() == ICEROLE_CONTROLLED) {
      request->AddAttribute(absl::make_unique<StunUInt64Attribute>(
          STUN_ATTR_ICE_CONTROLLED, connection_->port()->IceTiebreaker()));
    } else {
      RTC_NOTREACHED();
    }

    // Adding PRIORITY Attribute.
    // Changing the type preference to Peer Reflexive and local preference
    // and component id information is unchanged from the original priority.
    // priority = (2^24)*(type preference) +
    //           (2^8)*(local preference) +
    //           (2^0)*(256 - component ID)
    uint32_t type_preference =
        (connection_->local_candidate().protocol() == TCP_PROTOCOL_NAME)
            ? ICE_TYPE_PREFERENCE_PRFLX_TCP
            : ICE_TYPE_PREFERENCE_PRFLX;
    uint32_t prflx_priority =
        type_preference << 24 |
        (connection_->local_candidate().priority() & 0x00FFFFFF);
    request->AddAttribute(absl::make_unique<StunUInt32Attribute>(
        STUN_ATTR_PRIORITY, prflx_priority));

    // Adding Message Integrity attribute.
    request->AddMessageIntegrity(connection_->remote_candidate().password());
    // Adding Fingerprint.
    request->AddFingerprint();
  }

  void OnResponse(StunMessage* response) override {
    connection_->OnConnectionRequestResponse(this, response);
  }

  void OnErrorResponse(StunMessage* response) override {
    connection_->OnConnectionRequestErrorResponse(this, response);
  }

  void OnTimeout() override { connection_->OnConnectionRequestTimeout(this); }

  void OnSent() override {
    connection_->OnConnectionRequestSent(this);
    // Each request is sent only once.  After a single delay , the request will
    // time out.
    timeout_ = true;
  }

  int resend_delay() override { return CONNECTION_RESPONSE_TIMEOUT; }

 private:
  Connection* connection_;
};

//
// Connection
//

Connection::Connection(Port* port,
                       size_t index,
                       const Candidate& remote_candidate)
    : id_(rtc::CreateRandomId()),
      port_(port),
      local_candidate_index_(index),
      remote_candidate_(remote_candidate),
      recv_rate_tracker_(100, 10u),
      send_rate_tracker_(100, 10u),
      write_state_(STATE_WRITE_INIT),
      receiving_(false),
      connected_(true),
      pruned_(false),
      use_candidate_attr_(false),
      remote_ice_mode_(ICEMODE_FULL),
      requests_(port->thread()),
      rtt_(DEFAULT_RTT),
      last_ping_sent_(0),
      last_ping_received_(0),
      last_data_received_(0),
      last_ping_response_received_(0),
      packet_loss_estimator_(kConsiderPacketLostAfter, kForgetPacketAfter),
      reported_(false),
      state_(IceCandidatePairState::WAITING),
      time_created_ms_(rtc::TimeMillis()) {
  // All of our connections start in WAITING state.
  // TODO(mallinath) - Start connections from STATE_FROZEN.
  // Wire up to send stun packets
  requests_.SignalSendPacket.connect(this, &Connection::OnSendStunPacket);
  RTC_LOG(LS_INFO) << ToString() << ": Connection created";
}

Connection::~Connection() {}

const Candidate& Connection::local_candidate() const {
  RTC_DCHECK(local_candidate_index_ < port_->Candidates().size());
  return port_->Candidates()[local_candidate_index_];
}

const Candidate& Connection::remote_candidate() const {
  return remote_candidate_;
}

uint64_t Connection::priority() const {
  uint64_t priority = 0;
  // RFC 5245 - 5.7.2.  Computing Pair Priority and Ordering Pairs
  // Let G be the priority for the candidate provided by the controlling
  // agent.  Let D be the priority for the candidate provided by the
  // controlled agent.
  // pair priority = 2^32*MIN(G,D) + 2*MAX(G,D) + (G>D?1:0)
  IceRole role = port_->GetIceRole();
  if (role != ICEROLE_UNKNOWN) {
    uint32_t g = 0;
    uint32_t d = 0;
    if (role == ICEROLE_CONTROLLING) {
      g = local_candidate().priority();
      d = remote_candidate_.priority();
    } else {
      g = remote_candidate_.priority();
      d = local_candidate().priority();
    }
    priority = std::min(g, d);
    priority = priority << 32;
    priority += 2 * std::max(g, d) + (g > d ? 1 : 0);
  }
  return priority;
}

void Connection::set_write_state(WriteState value) {
  WriteState old_value = write_state_;
  write_state_ = value;
  if (value != old_value) {
    RTC_LOG(LS_VERBOSE) << ToString() << ": set_write_state from: " << old_value
                        << " to " << value;
    SignalStateChange(this);
  }
}

void Connection::UpdateReceiving(int64_t now) {
  bool receiving;
  if (last_ping_sent() < last_ping_response_received()) {
    // We consider any candidate pair that has its last connectivity check
    // acknowledged by a response as receiving, particularly for backup
    // candidate pairs that send checks at a much slower pace than the selected
    // one. Otherwise, a backup candidate pair constantly becomes not receiving
    // as a side effect of a long ping interval, since we do not have a separate
    // receiving timeout for backup candidate pairs. See
    // IceConfig.ice_backup_candidate_pair_ping_interval,
    // IceConfig.ice_connection_receiving_timeout and their default value.
    receiving = true;
  } else {
    receiving =
        last_received() > 0 && now <= last_received() + receiving_timeout();
  }
  if (receiving_ == receiving) {
    return;
  }
  RTC_LOG(LS_VERBOSE) << ToString() << ": set_receiving to " << receiving;
  receiving_ = receiving;
  receiving_unchanged_since_ = now;
  SignalStateChange(this);
}

void Connection::set_state(IceCandidatePairState state) {
  IceCandidatePairState old_state = state_;
  state_ = state;
  if (state != old_state) {
    RTC_LOG(LS_VERBOSE) << ToString() << ": set_state";
  }
}

void Connection::set_connected(bool value) {
  bool old_value = connected_;
  connected_ = value;
  if (value != old_value) {
    RTC_LOG(LS_VERBOSE) << ToString() << ": Change connected_ to " << value;
    SignalStateChange(this);
  }
}

void Connection::set_use_candidate_attr(bool enable) {
  use_candidate_attr_ = enable;
}

int Connection::unwritable_timeout() const {
  return unwritable_timeout_.value_or(CONNECTION_WRITE_CONNECT_TIMEOUT);
}

int Connection::unwritable_min_checks() const {
  return unwritable_min_checks_.value_or(CONNECTION_WRITE_CONNECT_FAILURES);
}

int Connection::receiving_timeout() const {
  return receiving_timeout_.value_or(WEAK_CONNECTION_RECEIVE_TIMEOUT);
}

void Connection::OnSendStunPacket(const void* data,
                                  size_t size,
                                  StunRequest* req) {
  rtc::PacketOptions options(port_->StunDscpValue());
  options.info_signaled_after_sent.packet_type =
      rtc::PacketType::kIceConnectivityCheck;
  auto err =
      port_->SendTo(data, size, remote_candidate_.address(), options, false);
  if (err < 0) {
    RTC_LOG(LS_WARNING) << ToString()
                        << ": Failed to send STUN ping "
                           " err="
                        << err << " id=" << rtc::hex_encode(req->id());
  }
}

void Connection::OnReadPacket(const char* data,
                              size_t size,
                              int64_t packet_time_us) {
  std::unique_ptr<IceMessage> msg;
  std::string remote_ufrag;
  const rtc::SocketAddress& addr(remote_candidate_.address());
  if (!port_->GetStunMessage(data, size, addr, &msg, &remote_ufrag)) {
    // The packet did not parse as a valid STUN message
    // This is a data packet, pass it along.
    last_data_received_ = rtc::TimeMillis();
    UpdateReceiving(last_data_received_);
    recv_rate_tracker_.AddSamples(size);
    SignalReadPacket(this, data, size, packet_time_us);

    // If timed out sending writability checks, start up again
    if (!pruned_ && (write_state_ == STATE_WRITE_TIMEOUT)) {
      RTC_LOG(LS_WARNING)
          << "Received a data packet on a timed-out Connection. "
             "Resetting state to STATE_WRITE_INIT.";
      set_write_state(STATE_WRITE_INIT);
    }
  } else if (!msg) {
    // The packet was STUN, but failed a check and was handled internally.
  } else {
    // The packet is STUN and passed the Port checks.
    // Perform our own checks to ensure this packet is valid.
    // If this is a STUN request, then update the receiving bit and respond.
    // If this is a STUN response, then update the writable bit.
    // Log at LS_INFO if we receive a ping on an unwritable connection.
    rtc::LoggingSeverity sev = (!writable() ? rtc::LS_INFO : rtc::LS_VERBOSE);
    switch (msg->type()) {
      case STUN_BINDING_REQUEST:
        RTC_LOG_V(sev) << ToString() << ": Received STUN ping, id="
                       << rtc::hex_encode(msg->transaction_id());

        if (remote_ufrag == remote_candidate_.username()) {
          HandleBindingRequest(msg.get());
        } else {
          // The packet had the right local username, but the remote username
          // was not the right one for the remote address.
          RTC_LOG(LS_ERROR)
              << ToString()
              << ": Received STUN request with bad remote username "
              << remote_ufrag;
          port_->SendBindingErrorResponse(msg.get(), addr,
                                          STUN_ERROR_UNAUTHORIZED,
                                          STUN_ERROR_REASON_UNAUTHORIZED);
        }
        break;

      // Response from remote peer. Does it match request sent?
      // This doesn't just check, it makes callbacks if transaction
      // id's match.
      case STUN_BINDING_RESPONSE:
      case STUN_BINDING_ERROR_RESPONSE:
        if (msg->ValidateMessageIntegrity(data, size,
                                          remote_candidate().password())) {
          requests_.CheckResponse(msg.get());
        }
        // Otherwise silently discard the response message.
        break;

      // Remote end point sent an STUN indication instead of regular binding
      // request. In this case |last_ping_received_| will be updated but no
      // response will be sent.
      case STUN_BINDING_INDICATION:
        ReceivedPing();
        break;

      default:
        RTC_NOTREACHED();
        break;
    }
  }
}

void Connection::HandleBindingRequest(IceMessage* msg) {
  // This connection should now be receiving.
  ReceivedPing();

  const rtc::SocketAddress& remote_addr = remote_candidate_.address();
  const std::string& remote_ufrag = remote_candidate_.username();
  // Check for role conflicts.
  if (!port_->MaybeIceRoleConflict(remote_addr, msg, remote_ufrag)) {
    // Received conflicting role from the peer.
    RTC_LOG(LS_INFO) << "Received conflicting role from the peer.";
    return;
  }

  stats_.recv_ping_requests++;
  LogCandidatePairEvent(webrtc::IceCandidatePairEventType::kCheckReceived);

  // This is a validated stun request from remote peer.
  port_->SendBindingResponse(msg, remote_addr);

  // If it timed out on writing check, start up again
  if (!pruned_ && write_state_ == STATE_WRITE_TIMEOUT) {
    set_write_state(STATE_WRITE_INIT);
  }

  if (port_->GetIceRole() == ICEROLE_CONTROLLED) {
    const StunUInt32Attribute* nomination_attr =
        msg->GetUInt32(STUN_ATTR_NOMINATION);
    uint32_t nomination = 0;
    if (nomination_attr) {
      nomination = nomination_attr->value();
      if (nomination == 0) {
        RTC_LOG(LS_ERROR) << "Invalid nomination: " << nomination;
      }
    } else {
      const StunByteStringAttribute* use_candidate_attr =
          msg->GetByteString(STUN_ATTR_USE_CANDIDATE);
      if (use_candidate_attr) {
        nomination = 1;
      }
    }
    // We don't un-nominate a connection, so we only keep a larger nomination.
    if (nomination > remote_nomination_) {
      set_remote_nomination(nomination);
      SignalNominated(this);
    }
  }
  // Set the remote cost if the network_info attribute is available.
  // Note: If packets are re-ordered, we may get incorrect network cost
  // temporarily, but it should get the correct value shortly after that.
  const StunUInt32Attribute* network_attr =
      msg->GetUInt32(STUN_ATTR_NETWORK_INFO);
  if (network_attr) {
    uint32_t network_info = network_attr->value();
    uint16_t network_cost = static_cast<uint16_t>(network_info);
    if (network_cost != remote_candidate_.network_cost()) {
      remote_candidate_.set_network_cost(network_cost);
      // Network cost change will affect the connection ranking, so signal
      // state change to force a re-sort in P2PTransportChannel.
      SignalStateChange(this);
    }
  }
}

void Connection::OnReadyToSend() {
  SignalReadyToSend(this);
}

void Connection::Prune() {
  if (!pruned_ || active()) {
    RTC_LOG(LS_INFO) << ToString() << ": Connection pruned";
    pruned_ = true;
    requests_.Clear();
    set_write_state(STATE_WRITE_TIMEOUT);
  }
}

void Connection::Destroy() {
  // TODO(deadbeef, nisse): This may leak if an application closes a
  // PeerConnection and then quickly destroys the PeerConnectionFactory (along
  // with the networking thread on which this message is posted). Also affects
  // tests, with a workaround in
  // AutoSocketServerThread::~AutoSocketServerThread.
  RTC_LOG(LS_VERBOSE) << ToString() << ": Connection destroyed";
  port_->thread()->Post(RTC_FROM_HERE, this, MSG_DELETE);
  LogCandidatePairConfig(webrtc::IceCandidatePairConfigType::kDestroyed);
}

void Connection::FailAndDestroy() {
  set_state(IceCandidatePairState::FAILED);
  Destroy();
}

void Connection::FailAndPrune() {
  set_state(IceCandidatePairState::FAILED);
  Prune();
}

void Connection::PrintPingsSinceLastResponse(std::string* s, size_t max) {
  rtc::StringBuilder oss;
  if (pings_since_last_response_.size() > max) {
    for (size_t i = 0; i < max; i++) {
      const SentPing& ping = pings_since_last_response_[i];
      oss << rtc::hex_encode(ping.id) << " ";
    }
    oss << "... " << (pings_since_last_response_.size() - max) << " more";
  } else {
    for (const SentPing& ping : pings_since_last_response_) {
      oss << rtc::hex_encode(ping.id) << " ";
    }
  }
  *s = oss.str();
}

void Connection::UpdateState(int64_t now) {
  int rtt = ConservativeRTTEstimate(rtt_);

  if (RTC_LOG_CHECK_LEVEL(LS_VERBOSE)) {
    std::string pings;
    PrintPingsSinceLastResponse(&pings, 5);
    RTC_LOG(LS_VERBOSE) << ToString()
                        << ": UpdateState()"
                           ", ms since last received response="
                        << now - last_ping_response_received_
                        << ", ms since last received data="
                        << now - last_data_received_ << ", rtt=" << rtt
                        << ", pings_since_last_response=" << pings;
  }

  // Check the writable state.  (The order of these checks is important.)
  //
  // Before becoming unwritable, we allow for a fixed number of pings to fail
  // (i.e., receive no response).  We also have to give the response time to
  // get back, so we include a conservative estimate of this.
  //
  // Before timing out writability, we give a fixed amount of time.  This is to
  // allow for changes in network conditions.

  if ((write_state_ == STATE_WRITABLE) &&
      TooManyFailures(pings_since_last_response_, unwritable_min_checks(), rtt,
                      now) &&
      TooLongWithoutResponse(pings_since_last_response_, unwritable_timeout(),
                             now)) {
    uint32_t max_pings = unwritable_min_checks();
    RTC_LOG(LS_INFO) << ToString() << ": Unwritable after " << max_pings
                     << " ping failures and "
                     << now - pings_since_last_response_[0].sent_time
                     << " ms without a response,"
                        " ms since last received ping="
                     << now - last_ping_received_
                     << " ms since last received data="
                     << now - last_data_received_ << " rtt=" << rtt;
    set_write_state(STATE_WRITE_UNRELIABLE);
  }
  if ((write_state_ == STATE_WRITE_UNRELIABLE ||
       write_state_ == STATE_WRITE_INIT) &&
      TooLongWithoutResponse(pings_since_last_response_,
                             CONNECTION_WRITE_TIMEOUT, now)) {
    RTC_LOG(LS_INFO) << ToString() << ": Timed out after "
                     << now - pings_since_last_response_[0].sent_time
                     << " ms without a response, rtt=" << rtt;
    set_write_state(STATE_WRITE_TIMEOUT);
  }

  // Update the receiving state.
  UpdateReceiving(now);
  if (dead(now)) {
    Destroy();
  }
}

void Connection::Ping(int64_t now) {
  last_ping_sent_ = now;
  ConnectionRequest* req = new ConnectionRequest(this);
  // If not using renomination, we use "1" to mean "nominated" and "0" to mean
  // "not nominated". If using renomination, values greater than 1 are used for
  // re-nominated pairs.
  int nomination = use_candidate_attr_ ? 1 : 0;
  if (nomination_ > 0) {
    nomination = nomination_;
  }
  pings_since_last_response_.push_back(SentPing(req->id(), now, nomination));
  packet_loss_estimator_.ExpectResponse(req->id(), now);
  RTC_LOG(LS_VERBOSE) << ToString() << ": Sending STUN ping, id="
                      << rtc::hex_encode(req->id())
                      << ", nomination=" << nomination_;
  requests_.Send(req);
  state_ = IceCandidatePairState::IN_PROGRESS;
  num_pings_sent_++;
}

void Connection::ReceivedPing() {
  last_ping_received_ = rtc::TimeMillis();
  UpdateReceiving(last_ping_received_);
}

void Connection::ReceivedPingResponse(int rtt, const std::string& request_id) {
  RTC_DCHECK_GE(rtt, 0);
  // We've already validated that this is a STUN binding response with
  // the correct local and remote username for this connection.
  // So if we're not already, become writable. We may be bringing a pruned
  // connection back to life, but if we don't really want it, we can always
  // prune it again.
  auto iter = std::find_if(
      pings_since_last_response_.begin(), pings_since_last_response_.end(),
      [request_id](const SentPing& ping) { return ping.id == request_id; });
  if (iter != pings_since_last_response_.end() &&
      iter->nomination > acked_nomination_) {
    acked_nomination_ = iter->nomination;
  }

  total_round_trip_time_ms_ += rtt;
  current_round_trip_time_ms_ = static_cast<uint32_t>(rtt);

  pings_since_last_response_.clear();
  last_ping_response_received_ = rtc::TimeMillis();
  UpdateReceiving(last_ping_response_received_);
  set_write_state(STATE_WRITABLE);
  set_state(IceCandidatePairState::SUCCEEDED);
  if (rtt_samples_ > 0) {
    rtt_ = rtc::GetNextMovingAverage(rtt_, rtt, RTT_RATIO);
  } else {
    rtt_ = rtt;
  }
  rtt_samples_++;
}

bool Connection::dead(int64_t now) const {
  if (last_received() > 0) {
    // If it has ever received anything, we keep it alive until it hasn't
    // received anything for DEAD_CONNECTION_RECEIVE_TIMEOUT. This covers the
    // normal case of a successfully used connection that stops working. This
    // also allows a remote peer to continue pinging over a locally inactive
    // (pruned) connection.
    return (now > (last_received() + DEAD_CONNECTION_RECEIVE_TIMEOUT));
  }

  if (active()) {
    // If it has never received anything, keep it alive as long as it is
    // actively pinging and not pruned. Otherwise, the connection might be
    // deleted before it has a chance to ping. This is the normal case for a
    // new connection that is pinging but hasn't received anything yet.
    return false;
  }

  // If it has never received anything and is not actively pinging (pruned), we
  // keep it around for at least MIN_CONNECTION_LIFETIME to prevent connections
  // from being pruned too quickly during a network change event when two
  // networks would be up simultaneously but only for a brief period.
  return now > (time_created_ms_ + MIN_CONNECTION_LIFETIME);
}

bool Connection::stable(int64_t now) const {
  // A connection is stable if it's RTT has converged and it isn't missing any
  // responses.  We should send pings at a higher rate until the RTT converges
  // and whenever a ping response is missing (so that we can detect
  // unwritability faster)
  return rtt_converged() && !missing_responses(now);
}

std::string Connection::ToDebugId() const {
  return rtc::ToHex(reinterpret_cast<uintptr_t>(this));
}

uint32_t Connection::ComputeNetworkCost() const {
  // TODO(honghaiz): Will add rtt as part of the network cost.
  return port()->network_cost() + remote_candidate_.network_cost();
}

std::string Connection::ToString() const {
  const absl::string_view CONNECT_STATE_ABBREV[2] = {
      "-",  // not connected (false)
      "C",  // connected (true)
  };
  const absl::string_view RECEIVE_STATE_ABBREV[2] = {
      "-",  // not receiving (false)
      "R",  // receiving (true)
  };
  const absl::string_view WRITE_STATE_ABBREV[4] = {
      "W",  // STATE_WRITABLE
      "w",  // STATE_WRITE_UNRELIABLE
      "-",  // STATE_WRITE_INIT
      "x",  // STATE_WRITE_TIMEOUT
  };
  const absl::string_view ICESTATE[4] = {
      "W",  // STATE_WAITING
      "I",  // STATE_INPROGRESS
      "S",  // STATE_SUCCEEDED
      "F"   // STATE_FAILED
  };
  const absl::string_view SELECTED_STATE_ABBREV[2] = {
      "-",  // candidate pair not selected (false)
      "S",  // selected (true)
  };
  const Candidate& local = local_candidate();
  const Candidate& remote = remote_candidate();
  rtc::StringBuilder ss;
  ss << "Conn[" << ToDebugId() << ":" << port_->content_name() << ":"
     << port_->Network()->ToString() << ":" << local.id() << ":"
     << local.component() << ":" << local.generation() << ":" << local.type()
     << ":" << local.protocol() << ":" << local.address().ToSensitiveString()
     << "->" << remote.id() << ":" << remote.component() << ":"
     << remote.priority() << ":" << remote.type() << ":" << remote.protocol()
     << ":" << remote.address().ToSensitiveString() << "|"
     << CONNECT_STATE_ABBREV[connected()] << RECEIVE_STATE_ABBREV[receiving()]
     << WRITE_STATE_ABBREV[write_state()] << ICESTATE[static_cast<int>(state())]
     << "|" << SELECTED_STATE_ABBREV[selected()] << "|" << remote_nomination()
     << "|" << nomination() << "|" << priority() << "|";
  if (rtt_ < DEFAULT_RTT) {
    ss << rtt_ << "]";
  } else {
    ss << "-]";
  }
  return ss.Release();
}

std::string Connection::ToSensitiveString() const {
  return ToString();
}

const webrtc::IceCandidatePairDescription& Connection::ToLogDescription() {
  if (log_description_.has_value()) {
    return log_description_.value();
  }
  const Candidate& local = local_candidate();
  const Candidate& remote = remote_candidate();
  const rtc::Network* network = port()->Network();
  log_description_ = webrtc::IceCandidatePairDescription();
  log_description_->local_candidate_type =
      GetCandidateTypeByString(local.type());
  log_description_->local_relay_protocol =
      GetProtocolByString(local.relay_protocol());
  log_description_->local_network_type = ConvertNetworkType(network->type());
  log_description_->local_address_family =
      GetAddressFamilyByInt(local.address().family());
  log_description_->remote_candidate_type =
      GetCandidateTypeByString(remote.type());
  log_description_->remote_address_family =
      GetAddressFamilyByInt(remote.address().family());
  log_description_->candidate_pair_protocol =
      GetProtocolByString(local.protocol());
  return log_description_.value();
}

void Connection::LogCandidatePairConfig(
    webrtc::IceCandidatePairConfigType type) {
  if (ice_event_log_ == nullptr) {
    return;
  }
  ice_event_log_->LogCandidatePairConfig(type, id(), ToLogDescription());
}

void Connection::LogCandidatePairEvent(webrtc::IceCandidatePairEventType type) {
  if (ice_event_log_ == nullptr) {
    return;
  }
  ice_event_log_->LogCandidatePairEvent(type, id());
}

void Connection::OnConnectionRequestResponse(ConnectionRequest* request,
                                             StunMessage* response) {
  // Log at LS_INFO if we receive a ping response on an unwritable
  // connection.
  rtc::LoggingSeverity sev = !writable() ? rtc::LS_INFO : rtc::LS_VERBOSE;

  int rtt = request->Elapsed();

  if (RTC_LOG_CHECK_LEVEL_V(sev)) {
    std::string pings;
    PrintPingsSinceLastResponse(&pings, 5);
    RTC_LOG_V(sev) << ToString() << ": Received STUN ping response, id="
                   << rtc::hex_encode(request->id())
                   << ", code=0"  // Makes logging easier to parse.
                      ", rtt="
                   << rtt << ", pings_since_last_response=" << pings;
  }
  ReceivedPingResponse(rtt, request->id());

  int64_t time_received = rtc::TimeMillis();
  packet_loss_estimator_.ReceivedResponse(request->id(), time_received);

  stats_.recv_ping_responses++;
  LogCandidatePairEvent(
      webrtc::IceCandidatePairEventType::kCheckResponseReceived);

  MaybeUpdateLocalCandidate(request, response);
}

void Connection::OnConnectionRequestErrorResponse(ConnectionRequest* request,
                                                  StunMessage* response) {
  int error_code = response->GetErrorCodeValue();
  RTC_LOG(LS_WARNING) << ToString() << ": Received STUN error response id="
                      << rtc::hex_encode(request->id())
                      << " code=" << error_code
                      << " rtt=" << request->Elapsed();

  if (error_code == STUN_ERROR_UNKNOWN_ATTRIBUTE ||
      error_code == STUN_ERROR_SERVER_ERROR ||
      error_code == STUN_ERROR_UNAUTHORIZED) {
    // Recoverable error, retry
  } else if (error_code == STUN_ERROR_STALE_CREDENTIALS) {
    // Race failure, retry
  } else if (error_code == STUN_ERROR_ROLE_CONFLICT) {
    HandleRoleConflictFromPeer();
  } else {
    // This is not a valid connection.
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Received STUN error response, code=" << error_code
                      << "; killing connection";
    FailAndDestroy();
  }
}

void Connection::OnConnectionRequestTimeout(ConnectionRequest* request) {
  // Log at LS_INFO if we miss a ping on a writable connection.
  rtc::LoggingSeverity sev = writable() ? rtc::LS_INFO : rtc::LS_VERBOSE;
  RTC_LOG_V(sev) << ToString() << ": Timing-out STUN ping "
                 << rtc::hex_encode(request->id()) << " after "
                 << request->Elapsed() << " ms";
}

void Connection::OnConnectionRequestSent(ConnectionRequest* request) {
  // Log at LS_INFO if we send a ping on an unwritable connection.
  rtc::LoggingSeverity sev = !writable() ? rtc::LS_INFO : rtc::LS_VERBOSE;
  RTC_LOG_V(sev) << ToString()
                 << ": Sent STUN ping, id=" << rtc::hex_encode(request->id())
                 << ", use_candidate=" << use_candidate_attr()
                 << ", nomination=" << nomination();
  stats_.sent_ping_requests_total++;
  LogCandidatePairEvent(webrtc::IceCandidatePairEventType::kCheckSent);
  if (stats_.recv_ping_responses == 0) {
    stats_.sent_ping_requests_before_first_response++;
  }
}

void Connection::HandleRoleConflictFromPeer() {
  port_->SignalRoleConflict(port_);
}

void Connection::MaybeSetRemoteIceParametersAndGeneration(
    const IceParameters& ice_params,
    int generation) {
  if (remote_candidate_.username() == ice_params.ufrag &&
      remote_candidate_.password().empty()) {
    remote_candidate_.set_password(ice_params.pwd);
  }
  // TODO(deadbeef): A value of '0' for the generation is used for both
  // generation 0 and "generation unknown". It should be changed to an
  // absl::optional to fix this.
  if (remote_candidate_.username() == ice_params.ufrag &&
      remote_candidate_.password() == ice_params.pwd &&
      remote_candidate_.generation() == 0) {
    remote_candidate_.set_generation(generation);
  }
}

void Connection::MaybeUpdatePeerReflexiveCandidate(
    const Candidate& new_candidate) {
  if (remote_candidate_.type() == PRFLX_PORT_TYPE &&
      new_candidate.type() != PRFLX_PORT_TYPE &&
      remote_candidate_.protocol() == new_candidate.protocol() &&
      remote_candidate_.address() == new_candidate.address() &&
      remote_candidate_.username() == new_candidate.username() &&
      remote_candidate_.password() == new_candidate.password() &&
      remote_candidate_.generation() == new_candidate.generation()) {
    remote_candidate_ = new_candidate;
  }
}

void Connection::OnMessage(rtc::Message* pmsg) {
  RTC_DCHECK(pmsg->message_id == MSG_DELETE);
  RTC_LOG(LS_INFO) << "Connection deleted with number of pings sent: "
                   << num_pings_sent_;
  SignalDestroyed(this);
  delete this;
}

int64_t Connection::last_received() const {
  return std::max(last_data_received_,
                  std::max(last_ping_received_, last_ping_response_received_));
}

ConnectionInfo Connection::stats() {
  stats_.recv_bytes_second = round(recv_rate_tracker_.ComputeRate());
  stats_.recv_total_bytes = recv_rate_tracker_.TotalSampleCount();
  stats_.sent_bytes_second = round(send_rate_tracker_.ComputeRate());
  stats_.sent_total_bytes = send_rate_tracker_.TotalSampleCount();
  stats_.receiving = receiving_;
  stats_.writable = write_state_ == STATE_WRITABLE;
  stats_.timeout = write_state_ == STATE_WRITE_TIMEOUT;
  stats_.new_connection = !reported_;
  stats_.rtt = rtt_;
  stats_.key = this;
  stats_.state = state_;
  stats_.priority = priority();
  stats_.nominated = nominated();
  stats_.total_round_trip_time_ms = total_round_trip_time_ms_;
  stats_.current_round_trip_time_ms = current_round_trip_time_ms_;
  CopyCandidatesToStatsAndSanitizeIfNecessary();
  return stats_;
}

void Connection::MaybeUpdateLocalCandidate(ConnectionRequest* request,
                                           StunMessage* response) {
  // RFC 5245
  // The agent checks the mapped address from the STUN response.  If the
  // transport address does not match any of the local candidates that the
  // agent knows about, the mapped address represents a new candidate -- a
  // peer reflexive candidate.
  const StunAddressAttribute* addr =
      response->GetAddress(STUN_ATTR_XOR_MAPPED_ADDRESS);
  if (!addr) {
    RTC_LOG(LS_WARNING)
        << "Connection::OnConnectionRequestResponse - "
           "No MAPPED-ADDRESS or XOR-MAPPED-ADDRESS found in the "
           "stun response message";
    return;
  }

  for (size_t i = 0; i < port_->Candidates().size(); ++i) {
    if (port_->Candidates()[i].address() == addr->GetAddress()) {
      if (local_candidate_index_ != i) {
        RTC_LOG(LS_INFO) << ToString()
                         << ": Updating local candidate type to srflx.";
        local_candidate_index_ = i;
        // SignalStateChange to force a re-sort in P2PTransportChannel as this
        // Connection's local candidate has changed.
        SignalStateChange(this);
      }
      return;
    }
  }

  // RFC 5245
  // Its priority is set equal to the value of the PRIORITY attribute
  // in the Binding request.
  const StunUInt32Attribute* priority_attr =
      request->msg()->GetUInt32(STUN_ATTR_PRIORITY);
  if (!priority_attr) {
    RTC_LOG(LS_WARNING) << "Connection::OnConnectionRequestResponse - "
                           "No STUN_ATTR_PRIORITY found in the "
                           "stun response message";
    return;
  }
  const uint32_t priority = priority_attr->value();
  std::string id = rtc::CreateRandomString(8);

  Candidate new_local_candidate;
  new_local_candidate.set_id(id);
  new_local_candidate.set_component(local_candidate().component());
  new_local_candidate.set_type(PRFLX_PORT_TYPE);
  new_local_candidate.set_protocol(local_candidate().protocol());
  new_local_candidate.set_address(addr->GetAddress());
  new_local_candidate.set_priority(priority);
  new_local_candidate.set_username(local_candidate().username());
  new_local_candidate.set_password(local_candidate().password());
  new_local_candidate.set_network_name(local_candidate().network_name());
  new_local_candidate.set_network_type(local_candidate().network_type());
  new_local_candidate.set_related_address(local_candidate().address());
  new_local_candidate.set_generation(local_candidate().generation());
  new_local_candidate.set_foundation(ComputeFoundation(
      PRFLX_PORT_TYPE, local_candidate().protocol(),
      local_candidate().relay_protocol(), local_candidate().address()));
  new_local_candidate.set_network_id(local_candidate().network_id());
  new_local_candidate.set_network_cost(local_candidate().network_cost());

  // Change the local candidate of this Connection to the new prflx candidate.
  RTC_LOG(LS_INFO) << ToString() << ": Updating local candidate type to prflx.";
  local_candidate_index_ = port_->AddPrflxCandidate(new_local_candidate);

  // SignalStateChange to force a re-sort in P2PTransportChannel as this
  // Connection's local candidate has changed.
  SignalStateChange(this);
}

void Connection::CopyCandidatesToStatsAndSanitizeIfNecessary() {
  auto get_sanitized_copy = [](const Candidate& c) {
    bool use_hostname_address = c.type() == LOCAL_PORT_TYPE;
    bool filter_related_address = c.type() == STUN_PORT_TYPE;
    return c.ToSanitizedCopy(use_hostname_address, filter_related_address);
  };

  if (port_->Network()->GetMdnsResponder() != nullptr) {
    // When the mDNS obfuscation of local IPs is enabled, we sanitize local
    // candidates.
    stats_.local_candidate = get_sanitized_copy(local_candidate());
  } else {
    stats_.local_candidate = local_candidate();
  }

  if (!remote_candidate().address().hostname().empty()) {
    // If the remote endpoint signaled us a hostname candidate, we assume it is
    // supposed to be sanitized in the stats.
    //
    // A prflx remote candidate should not have a hostname set.
    RTC_DCHECK(remote_candidate().type() != PRFLX_PORT_TYPE);
    // A remote hostname candidate should have a resolved IP before we can form
    // a candidate pair.
    RTC_DCHECK(!remote_candidate().address().IsUnresolvedIP());
    stats_.remote_candidate = get_sanitized_copy(remote_candidate());
  } else {
    stats_.remote_candidate = remote_candidate();
  }
}

bool Connection::rtt_converged() const {
  return rtt_samples_ > (RTT_RATIO + 1);
}

bool Connection::missing_responses(int64_t now) const {
  if (pings_since_last_response_.empty()) {
    return false;
  }

  int64_t waiting = now - pings_since_last_response_[0].sent_time;
  return waiting > 2 * rtt();
}

ProxyConnection::ProxyConnection(Port* port,
                                 size_t index,
                                 const Candidate& remote_candidate)
    : Connection(port, index, remote_candidate) {}

int ProxyConnection::Send(const void* data,
                          size_t size,
                          const rtc::PacketOptions& options) {
  stats_.sent_total_packets++;
  int sent =
      port_->SendTo(data, size, remote_candidate_.address(), options, true);
  if (sent <= 0) {
    RTC_DCHECK(sent < 0);
    error_ = port_->GetError();
    stats_.sent_discarded_packets++;
  } else {
    send_rate_tracker_.AddSamples(sent);
  }
  return sent;
}

int ProxyConnection::GetError() {
  return error_;
}

}  // namespace cricket
