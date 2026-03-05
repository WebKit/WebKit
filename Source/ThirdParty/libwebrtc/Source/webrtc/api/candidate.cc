/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/candidate.h"

#include <algorithm>  // IWYU pragma: keep
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "api/rtc_error.h"
#include "p2p/base/p2p_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/crc32.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"

using webrtc::IceCandidateType;

namespace webrtc {
namespace {
constexpr char kLineTypeAttributes = 'a';
constexpr char kAttributeCandidate[] = "candidate";
constexpr char kAttributeCandidateTyp[] = "typ";
constexpr char kAttributeCandidateRaddr[] = "raddr";
constexpr char kAttributeCandidateRport[] = "rport";
constexpr char kAttributeCandidateUfrag[] = "ufrag";
constexpr char kAttributeCandidateGeneration[] = "generation";
constexpr char kAttributeCandidateNetworkId[] = "network-id";
constexpr char kAttributeCandidateNetworkCost[] = "network-cost";
constexpr char kAttributeCandidatePwd[] = "pwd";

constexpr absl::string_view kSdpDelimiterColon = ":";
constexpr char kSdpDelimiterColonChar = kSdpDelimiterColon[0];
constexpr char kSdpDelimiterSpaceChar = ' ';
constexpr char kSdpDelimiterEqualChar = '=';
constexpr char kNewLineChar = '\n';
constexpr char kReturnChar = '\r';

constexpr char kCandidateHost[] = "host";
constexpr char kCandidateSrflx[] = "srflx";
constexpr char kCandidatePrflx[] = "prflx";
constexpr char kCandidateRelay[] = "relay";
// Backwards compatibility.
constexpr char kTcpCandidateType[] = "tcptype";

absl::string_view TrimReturnChar(absl::string_view line) {
  if (!line.empty() && line.back() == kReturnChar) {
    line.remove_suffix(1);
  }
  return line;
}

bool IsValidPort(int port) {
  return port >= 0 && port <= 65535;
}

// Returns the `candidate-attribute` as described in:
// https://www.rfc-editor.org/rfc/rfc5245#section-15.1
std::string BuildCandidate(const Candidate& candidate, bool include_ufrag) {
  StringBuilder os;
  os << kAttributeCandidate;

  absl::string_view type = candidate.type_name();
  os << kSdpDelimiterColon << candidate.foundation() << " "
     << candidate.component() << " " << candidate.protocol() << " "
     << candidate.priority() << " "
     << (candidate.address().ipaddr().IsNil()
             ? candidate.address().hostname()
             : candidate.address().ipaddr().ToString())
     << " " << candidate.address().PortAsString() << " "
     << kAttributeCandidateTyp << " " << type << " ";

  // Related address
  if (!candidate.related_address().IsNil()) {
    os << kAttributeCandidateRaddr << " "
       << candidate.related_address().ipaddr().ToString() << " "
       << kAttributeCandidateRport << " "
       << candidate.related_address().PortAsString() << " ";
  }

  // Note that we allow the tcptype to be missing, for backwards
  // compatibility; the implementation treats this as a passive candidate.
  // TODO(bugs.webrtc.org/11466): Treat a missing tcptype as an error?
  if (candidate.protocol() == TCP_PROTOCOL_NAME &&
      !candidate.tcptype().empty()) {
    os << kTcpCandidateType << " " << candidate.tcptype() << " ";
  }

  // Extensions
  os << kAttributeCandidateGeneration << " " << candidate.generation();
  if (include_ufrag && !candidate.username().empty()) {
    os << " " << kAttributeCandidateUfrag << " " << candidate.username();
  }
  if (candidate.network_id() > 0) {
    os << " " << kAttributeCandidateNetworkId << " " << candidate.network_id();
  }
  if (candidate.network_cost() > 0) {
    os << " " << kAttributeCandidateNetworkCost << " "
       << candidate.network_cost();
  }

  return os.str();
}

// From WebRTC draft section 4.8.1.1 candidate-attribute should be
// candidate:<candidate> when trickled.
RTCErrorOr<Candidate> ParseCandidate(absl::string_view message) {
  // Makes sure `message` contains only one line.
  absl::string_view first_line;
  size_t line_end = message.find(kNewLineChar);
  if (line_end == absl::string_view::npos) {
    first_line = message;
  } else if (line_end + 1 == message.size()) {
    first_line = message.substr(0, line_end);
  } else {
    return RTCError(RTCErrorType::INVALID_PARAMETER, "Expect one line only");
  }

  // For backwards compatibility, don't fail if the supplied string is in the
  // form of "a=candidate...". If we encounter that, ignore the first 2
  // characters and continue.
  if (first_line.length() > 2 && first_line[0] == kLineTypeAttributes &&
      first_line[1] == kSdpDelimiterEqualChar) {
    first_line = first_line.substr(2);
  }

  // Trim return char, if any.
  first_line = TrimReturnChar(first_line);

  std::string attribute_candidate;
  std::string candidate_value;

  // `first_line` must be in the form of "candidate:<value>".
  if (!tokenize_first(first_line, kSdpDelimiterColonChar, &attribute_candidate,
                      &candidate_value) ||
      attribute_candidate != kAttributeCandidate) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    absl::StrCat("Expected ", kAttributeCandidate, " got ",
                                 attribute_candidate));
  }

  std::vector<absl::string_view> fields =
      split(candidate_value, kSdpDelimiterSpaceChar);

  // RFC 5245
  // a=candidate:<foundation> <component-id> <transport> <priority>
  // <connection-address> <port> typ <candidate-types>
  // [raddr <connection-address>] [rport <port>]
  // *(SP extension-att-name SP extension-att-value)
  const size_t expected_min_fields = 8;
  if (fields.size() < expected_min_fields ||
      (fields[6] != kAttributeCandidateTyp)) {
    return RTCError(
        RTCErrorType::INVALID_PARAMETER,
        absl::StrCat("Expect at least ", expected_min_fields, " fields."));
  }
  const absl::string_view foundation = fields[0];

  int component_id = 0;
  if (!FromString(fields[1], &component_id)) {
    return RTCError(RTCErrorType::SYNTAX_ERROR, "Invalid component id");
  }
  const absl::string_view transport = fields[2];
  uint32_t priority = 0;
  if (!FromString(fields[3], &priority)) {
    return RTCError(RTCErrorType::SYNTAX_ERROR, "Invalid priority");
  }
  int port = 0;
  if (!FromString(fields[5], &port) || !IsValidPort(port)) {
    return RTCError(RTCErrorType::SYNTAX_ERROR, "Invalid port");
  }
  const absl::string_view connection_address = fields[4];
  SocketAddress address(connection_address, port);

  std::optional<ProtocolType> protocol = StringToProto(transport);
  if (!protocol) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Unsupported transport type");
  }
  bool tcp_protocol = false;
  switch (*protocol) {
    // Supported protocols.
    case PROTO_UDP:
      break;
    case PROTO_TCP:
    case PROTO_SSLTCP:
      tcp_protocol = true;
      break;
    default:
      return RTCError(RTCErrorType::INVALID_PARAMETER, "Unsupported protocol");
  }

  IceCandidateType candidate_type;
  const absl::string_view type = fields[7];
  if (type == kCandidateHost) {
    candidate_type = IceCandidateType::kHost;
  } else if (type == kCandidateSrflx) {
    candidate_type = IceCandidateType::kSrflx;
  } else if (type == kCandidateRelay) {
    candidate_type = IceCandidateType::kRelay;
  } else if (type == kCandidatePrflx) {
    candidate_type = IceCandidateType::kPrflx;
  } else {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Unsupported candidate type");
  }

  size_t current_position = expected_min_fields;
  SocketAddress related_address;
  // The 2 optional fields for related address
  // [raddr <connection-address>] [rport <port>]
  if (fields.size() >= (current_position + 2) &&
      fields[current_position] == kAttributeCandidateRaddr) {
    related_address.SetIP(fields[++current_position]);
    ++current_position;
  }
  if (fields.size() >= (current_position + 2) &&
      fields[current_position] == kAttributeCandidateRport) {
    int related_port = 0;
    if (!FromString(fields[++current_position], &related_port) ||
        !IsValidPort(related_port)) {
      return RTCError(RTCErrorType::SYNTAX_ERROR, "Invalid port");
    }
    related_address.SetPort(related_port);
    ++current_position;
  }

  // If this is a TCP candidate, it has additional extension as defined in
  // RFC 6544.
  absl::string_view tcptype;
  if (fields.size() >= (current_position + 2) &&
      fields[current_position] == kTcpCandidateType) {
    tcptype = fields[++current_position];
    ++current_position;

    if (tcptype != TCPTYPE_ACTIVE_STR && tcptype != TCPTYPE_PASSIVE_STR &&
        tcptype != TCPTYPE_SIMOPEN_STR) {
      return RTCError(RTCErrorType::SYNTAX_ERROR, "Invalid TCP candidate type");
    }

    if (!tcp_protocol) {
      return RTCError(RTCErrorType::SYNTAX_ERROR, "Invalid non-TCP candidate");
    }
  } else if (tcp_protocol) {
    // We allow the tcptype to be missing, for backwards compatibility,
    // treating it as a passive candidate.
    // TODO(bugs.webrtc.org/11466): Treat a missing tcptype as an error?
    tcptype = TCPTYPE_PASSIVE_STR;
  }

  // Extension
  // Though non-standard, we support the ICE ufrag and pwd being signaled on
  // the candidate to avoid issues with confusing which generation a candidate
  // belongs to when trickling multiple generations at the same time.
  absl::string_view username;
  absl::string_view password;
  uint32_t generation = 0;
  uint16_t network_id = 0;
  uint16_t network_cost = 0;
  for (size_t i = current_position; i + 1 < fields.size(); ++i) {
    // RFC 5245
    // *(SP extension-att-name SP extension-att-value)
    if (fields[i] == kAttributeCandidateGeneration) {
      if (!FromString(fields[++i], &generation)) {
        return RTCError(
            RTCErrorType::SYNTAX_ERROR,
            absl::StrCat("Invalid ", kAttributeCandidateGeneration));
      }
    } else if (fields[i] == kAttributeCandidateUfrag) {
      username = fields[++i];
    } else if (fields[i] == kAttributeCandidatePwd) {
      password = fields[++i];
    } else if (fields[i] == kAttributeCandidateNetworkId) {
      if (!FromString(fields[++i], &network_id)) {
        return RTCError(RTCErrorType::SYNTAX_ERROR,
                        absl::StrCat("Invalid ", kAttributeCandidateNetworkId));
      }
    } else if (fields[i] == kAttributeCandidateNetworkCost) {
      if (!FromString(fields[++i], &network_cost)) {
        return RTCError(
            RTCErrorType::SYNTAX_ERROR,
            absl::StrCat("Invalid ", kAttributeCandidateNetworkCost));
      }
      network_cost = std::min(network_cost, kNetworkCostMax);
    } else {
      // Skip the unknown extension.
      ++i;
    }
  }

  Candidate candidate(component_id, ProtoToString(*protocol), address, priority,
                      username, password, candidate_type, generation,
                      foundation, network_id, network_cost);
  candidate.set_related_address(related_address);
  candidate.set_tcptype(tcptype);
  return candidate;
}

}  // namespace

absl::string_view IceCandidateTypeToString(IceCandidateType type) {
  switch (type) {
    case IceCandidateType::kHost:
      return kCandidateHost;
    case IceCandidateType::kSrflx:
      return kCandidateSrflx;
    case IceCandidateType::kPrflx:
      return kCandidatePrflx;
    case IceCandidateType::kRelay:
      return kCandidateRelay;
  }
}

std::optional<IceCandidateType> StringToIceCandidateType(
    absl::string_view type) {
  if (type == kCandidateHost) {
    return IceCandidateType::kHost;
  } else if (type == kCandidateSrflx) {
    return IceCandidateType::kSrflx;
  } else if (type == kCandidatePrflx) {
    return IceCandidateType::kPrflx;
  } else if (type == kCandidateRelay) {
    return IceCandidateType::kRelay;
  }
  return std::nullopt;
}

// static
RTCErrorOr<Candidate> Candidate::ParseCandidateString(
    absl::string_view message) {
  return ParseCandidate(message);
}

Candidate::Candidate()
    : id_(CreateRandomString(8)),
      component_(ICE_CANDIDATE_COMPONENT_DEFAULT),
      priority_(0),
      network_type_(ADAPTER_TYPE_UNKNOWN),
      underlying_type_for_vpn_(ADAPTER_TYPE_UNKNOWN),
      generation_(0),
      network_id_(0),
      network_cost_(0) {}

Candidate::Candidate(int component,
                     absl::string_view protocol,
                     const SocketAddress& address,
                     uint32_t priority,
                     absl::string_view username,
                     absl::string_view password,
                     IceCandidateType type,
                     uint32_t generation,
                     absl::string_view foundation,
                     uint16_t network_id /*= 0*/,
                     uint16_t network_cost /*= 0*/)
    : id_(CreateRandomString(8)),
      component_(component),
      protocol_(protocol),
      address_(address),
      priority_(priority),
      username_(username),
      password_(password),
      type_(type),
      network_type_(ADAPTER_TYPE_UNKNOWN),
      underlying_type_for_vpn_(ADAPTER_TYPE_UNKNOWN),
      generation_(generation),
      foundation_(foundation),
      network_id_(network_id),
      network_cost_(network_cost) {}

Candidate::Candidate(const Candidate&) = default;

Candidate::~Candidate() = default;

void Candidate::generate_id() {
  id_ = CreateRandomString(8);
}

bool Candidate::is_local() const {
  return type_ == IceCandidateType::kHost;
}
bool Candidate::is_stun() const {
  return type_ == IceCandidateType::kSrflx;
}
bool Candidate::is_prflx() const {
  return type_ == IceCandidateType::kPrflx;
}
bool Candidate::is_relay() const {
  return type_ == IceCandidateType::kRelay;
}

absl::string_view Candidate::type_name() const {
  return IceCandidateTypeToString(type_);
}

bool Candidate::IsEquivalent(const Candidate& c) const {
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

bool Candidate::MatchesForRemoval(const Candidate& c) const {
  return component_ == c.component_ && protocol_ == c.protocol_ &&
         address_ == c.address_;
}

std::string Candidate::ToStringInternal(bool sensitive) const {
  StringBuilder ost;
  std::string address =
      sensitive ? address_.ToSensitiveString() : address_.ToString();
  std::string related_address = sensitive ? related_address_.ToSensitiveString()
                                          : related_address_.ToString();
  ost << "Cand[:" << foundation_ << ":" << component_ << ":" << protocol_ << ":"
      << priority_ << ":" << address << ":" << type_name() << ":"
      << related_address << ":" << username_ << ":" << password_ << ":"
      << network_id_ << ":" << network_cost_ << ":" << generation_ << "]";
  return ost.Release();
}

std::string Candidate::ToCandidateAttribute(bool include_ufrag) const {
  return BuildCandidate(*this, include_ufrag);
}

uint32_t Candidate::GetPriority(uint32_t type_preference,
                                int network_adapter_preference,
                                int relay_preference,
                                bool adjust_local_preference) const {
  // RFC 5245 - 4.1.2.1.
  // priority = (2^24)*(type preference) +
  //            (2^8)*(local preference) +
  //            (2^0)*(256 - component ID)

  // `local_preference` length is 2 bytes, 0-65535 inclusive.
  // In our implemenation we will partion local_preference into
  //              0                 1
  //       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
  //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //      |  NIC Pref     |    Addr Pref  |
  //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // NIC Type - Type of the network adapter e.g. 3G/Wifi/Wired.
  // Addr Pref - Address preference value as per RFC 3484.
  // local preference =  (NIC Type << 8 | Addr_Pref) + relay preference.
  // The relay preference is based on the number of TURN servers, the
  // first TURN server gets the highest preference.
  int addr_pref = IPAddressPrecedence(address_.ipaddr());
  int local_preference =
      ((network_adapter_preference << 8) | addr_pref) + relay_preference;

  // Ensure that the added relay preference will not result in a relay candidate
  // whose STUN priority attribute has a higher priority than a server-reflexive
  // candidate.
  // The STUN priority attribute is calculated as
  // (peer-reflexive type preference) << 24 | (priority & 0x00FFFFFF)
  // as described in
  // https://www.rfc-editor.org/rfc/rfc5245#section-7.1.2.1
  // To satisfy that condition, add kMaxTurnServers to the local preference.
  // This can not overflow the field width since the highest "NIC pref"
  // assigned is kHighestNetworkPreference = 127
  RTC_DCHECK_LT(local_preference + kMaxTurnServers, 0x10000);
  if (adjust_local_preference && relay_protocol_.empty()) {
    local_preference += kMaxTurnServers;
  }

  return (type_preference << 24) | (local_preference << 8) | (256 - component_);
}

bool Candidate::operator==(const Candidate& o) const {
  return id_ == o.id_ && component_ == o.component_ &&
         protocol_ == o.protocol_ && relay_protocol_ == o.relay_protocol_ &&
         address_ == o.address_ && priority_ == o.priority_ &&
         username_ == o.username_ && password_ == o.password_ &&
         type_ == o.type_ && network_name_ == o.network_name_ &&
         network_type_ == o.network_type_ && generation_ == o.generation_ &&
         foundation_ == o.foundation_ &&
         related_address_ == o.related_address_ && tcptype_ == o.tcptype_ &&
         network_id_ == o.network_id_;
}

bool Candidate::operator!=(const Candidate& o) const {
  return !(*this == o);
}

Candidate Candidate::ToSanitizedCopy(bool use_hostname_address,
                                     bool filter_related_address) const {
  return ToSanitizedCopy(use_hostname_address, filter_related_address, false);
}

Candidate Candidate::ToSanitizedCopy(bool use_hostname_address,
                                     bool filter_related_address,
                                     bool filter_ufrag) const {
  Candidate copy(*this);
  if (use_hostname_address) {
    IPAddress ip;
    if (address().hostname().empty()) {
      // IP needs to be redacted, but no hostname available.
      SocketAddress redacted_addr("redacted-ip.invalid", address().port());
      copy.set_address(redacted_addr);
    } else if (IPFromString(address().hostname(), &ip)) {
      // The hostname is an IP literal, and needs to be redacted too.
      SocketAddress redacted_addr("redacted-literal.invalid", address().port());
      copy.set_address(redacted_addr);
    } else {
      SocketAddress hostname_only_addr(address().hostname(), address().port());
      copy.set_address(hostname_only_addr);
    }
  }
  if (filter_related_address) {
    copy.set_related_address(
        EmptySocketAddressWithFamily(copy.address().family()));
  }
  if (filter_ufrag) {
    copy.set_username("");
  }

  return copy;
}

void Candidate::ComputeFoundation(const SocketAddress& base_address,
                                  uint64_t tie_breaker) {
  // https://www.rfc-editor.org/rfc/rfc5245#section-4.1.1.3
  // The foundation is an identifier, scoped within a session.  Two candidates
  // MUST have the same foundation ID when all of the following are true:
  //
  // o they are of the same type.
  // o their bases have the same IP address (the ports can be different).
  // o for reflexive and relayed candidates, the STUN or TURN servers used to
  //   obtain them have the same IP address.
  // o they were obtained using the same transport protocol (TCP, UDP, etc.).
  //
  // Similarly, two candidates MUST have different foundations if their
  // types are different, their bases have different IP addresses, the STUN or
  // TURN servers used to obtain them have different IP addresses, or their
  // transport protocols are different.

  StringBuilder sb;
  sb << type_name() << base_address.ipaddr().ToString() << protocol_
     << relay_protocol_;

  // https://www.rfc-editor.org/rfc/rfc5245#section-5.2
  // [...] it is possible for both agents to mistakenly believe they are
  // controlled or controlling. To resolve this, each agent MUST select a random
  // number, called the tie-breaker, uniformly distributed between 0 and (2**64)
  // - 1 (that is, a 64-bit positive integer).  This number is used in
  // connectivity checks to detect and repair this case [...]
  sb << absl::StrCat(tie_breaker);
  foundation_ = absl::StrCat(ComputeCrc32(sb.Release()));
}

void Candidate::ComputePrflxFoundation() {
  RTC_DCHECK(is_prflx());
  RTC_DCHECK(!id_.empty());
  foundation_ = absl::StrCat(ComputeCrc32(id_));
}

void Candidate::Assign(std::string& s, absl::string_view view) {
  // Assigning via a temporary object, like s = std::string(view), results in
  // binary size bloat. To avoid that, extract pointer and size from the
  // string view, and use std::string::assign method.
  s.assign(view.data(), view.size());
}

}  // namespace webrtc
