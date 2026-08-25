/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/candidate.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/rtc_error.h"
#include "p2p/base/p2p_constants.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_fingerprint.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::SizeIs;

constexpr absl::string_view kRawCandidate =
    "candidate:a0+B/1 1 udp 2130706432 192.168.1.5 1234 typ host generation 2";
constexpr absl::string_view kRawHostnameCandidate =
    "candidate:a0+B/1 1 udp 2130706432 a.test 1234 typ host generation 2";
constexpr char kSdpTcpActiveCandidate[] =
    "candidate:a0+B/1 1 tcp 2130706432 192.168.1.5 9 typ host "
    "tcptype active generation 2";
constexpr uint32_t kCandidatePriority = 2130706432U;  // pref = 1.0
constexpr uint32_t kCandidateGeneration = 2;
constexpr char kCandidateFoundation1[] = "a0+B/1";
}  // namespace

TEST(CandidateTest, Id) {
  Candidate c;
  EXPECT_EQ(c.id().size(), 8u);
  std::string current_id = c.id();
  // Generate a new ID.
  c.generate_id();
  EXPECT_EQ(c.id().size(), 8u);
  EXPECT_NE(current_id, c.id());
}

TEST(CandidateTest, Component) {
  Candidate c;
  EXPECT_EQ(c.component(), ICE_CANDIDATE_COMPONENT_DEFAULT);
  c.set_component(ICE_CANDIDATE_COMPONENT_RTCP);
  EXPECT_EQ(c.component(), ICE_CANDIDATE_COMPONENT_RTCP);
}

TEST(CandidateTest, TypeName) {
  Candidate c;
  // The `type_name()` property defaults to "host".
  EXPECT_EQ(c.type_name(), "host");
  EXPECT_EQ(c.type(), IceCandidateType::kHost);

  c.set_type(IceCandidateType::kSrflx);
  EXPECT_EQ(c.type_name(), "srflx");
  EXPECT_EQ(c.type(), IceCandidateType::kSrflx);

  c.set_type(IceCandidateType::kPrflx);
  EXPECT_EQ(c.type_name(), "prflx");
  EXPECT_EQ(c.type(), IceCandidateType::kPrflx);

  c.set_type(IceCandidateType::kRelay);
  EXPECT_EQ(c.type_name(), "relay");
  EXPECT_EQ(c.type(), IceCandidateType::kRelay);
}

TEST(CandidateTest, Foundation) {
  Candidate c;
  EXPECT_TRUE(c.foundation().empty());
  c.set_protocol("udp");
  c.set_relay_protocol("udp");

  SocketAddress address("99.99.98.1", 1024);
  c.set_address(address);
  c.ComputeFoundation(c.address(), 1);
  std::string foundation1 = c.foundation();
  EXPECT_FALSE(foundation1.empty());

  // Change the tiebreaker.
  c.ComputeFoundation(c.address(), 2);
  std::string foundation2 = c.foundation();
  EXPECT_NE(foundation1, foundation2);

  // Provide a different base address.
  address.SetIP("100.100.100.1");
  c.ComputeFoundation(address, 1);  // Same tiebreaker as for foundation1.
  foundation2 = c.foundation();
  EXPECT_NE(foundation1, foundation2);

  // Consistency check (just in case the algorithm ever changes to random!).
  c.ComputeFoundation(c.address(), 1);
  foundation2 = c.foundation();
  EXPECT_EQ(foundation1, foundation2);

  // Changing the protocol should affect the foundation.
  auto prev_protocol = c.protocol();
  c.set_protocol("tcp");
  ASSERT_NE(prev_protocol, c.protocol());
  c.ComputeFoundation(c.address(), 1);
  EXPECT_NE(foundation1, c.foundation());
  c.set_protocol(prev_protocol);

  // Changing the relay protocol should affect the foundation.
  prev_protocol = c.relay_protocol();
  c.set_relay_protocol("tcp");
  ASSERT_NE(prev_protocol, c.relay_protocol());
  c.ComputeFoundation(c.address(), 1);
  EXPECT_NE(foundation1, c.foundation());
}

TEST(CandidateTest, ToCandidateAttribute) {
  SocketAddress address("192.168.1.5", 1234);
  Candidate candidate(ICE_CANDIDATE_COMPONENT_RTP, "udp", address,
                      kCandidatePriority, "", "", IceCandidateType::kHost,
                      kCandidateGeneration, kCandidateFoundation1);

  EXPECT_EQ(candidate.ToCandidateAttribute(true), kRawCandidate);

  Candidate candidate_with_ufrag(candidate);
  candidate_with_ufrag.set_username("ABC");
  EXPECT_EQ(candidate_with_ufrag.ToCandidateAttribute(true),
            std::string(kRawCandidate) + " ufrag ABC");
  EXPECT_EQ(candidate_with_ufrag.ToCandidateAttribute(false), kRawCandidate);

  Candidate candidate_with_network_info(candidate);
  candidate_with_network_info.set_network_id(1);
  EXPECT_EQ(candidate_with_network_info.ToCandidateAttribute(true),
            std::string(kRawCandidate) + " network-id 1");
  candidate_with_network_info.set_network_cost(999);
  EXPECT_EQ(candidate_with_network_info.ToCandidateAttribute(true),
            std::string(kRawCandidate) + " network-id 1 network-cost 999");
}

TEST(CandidateTest, ToCandidateAttributeHostnameCandidate) {
  SocketAddress address("a.test", 1234);
  Candidate candidate(ICE_CANDIDATE_COMPONENT_RTP, "udp", address,
                      kCandidatePriority, "", "", IceCandidateType::kHost,
                      kCandidateGeneration, kCandidateFoundation1);
  EXPECT_EQ(candidate.ToCandidateAttribute(true), kRawHostnameCandidate);
}

TEST(CandidateTest, ToCandidateAttributeTcpCandidates) {
  Candidate candidate(ICE_CANDIDATE_COMPONENT_RTP, "tcp",
                      SocketAddress("192.168.1.5", 9), kCandidatePriority, "",
                      "", IceCandidateType::kHost, kCandidateGeneration,
                      kCandidateFoundation1);
  candidate.set_tcptype(TCPTYPE_ACTIVE_STR);
  EXPECT_EQ(candidate.ToCandidateAttribute(true), kSdpTcpActiveCandidate);
}

TEST(CandidateTest, ToCandidateAttributeTlsCandidates) {
  const uint8_t kDigest[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                             0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  Candidate candidate(
      ICE_CANDIDATE_COMPONENT_RTP, "tls", SocketAddress("192.168.1.5", 443),
      kCandidatePriority, "", "", IceCandidateType::kHost, kCandidateGeneration,
      kCandidateFoundation1, 0, 0, SSLFingerprint("sha-256", kDigest));
  candidate.set_tcptype(TCPTYPE_PASSIVE_STR);
  EXPECT_EQ(
      candidate.ToCandidateAttribute(true),
      "candidate:a0+B/1 1 tls 2130706432 192.168.1.5 443 typ host tcptype "
      "passive generation 2 fingerprint sha-256;"
      "01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F:10:"
      "11:12:13:14:15:16:17:18:19:1A:1B:1C:1D:1E:1F:20");
}

TEST(CandidateTest, TypeToString) {
  EXPECT_EQ(IceCandidateTypeToString(IceCandidateType::kHost), "host");
  EXPECT_EQ(IceCandidateTypeToString(IceCandidateType::kSrflx), "srflx");
  EXPECT_EQ(IceCandidateTypeToString(IceCandidateType::kPrflx), "prflx");
  EXPECT_EQ(IceCandidateTypeToString(IceCandidateType::kRelay), "relay");
}

TEST(CandidateTest, StringToType) {
  EXPECT_EQ(*StringToIceCandidateType("host"), IceCandidateType::kHost);
  EXPECT_EQ(*StringToIceCandidateType("srflx"), IceCandidateType::kSrflx);
  EXPECT_EQ(*StringToIceCandidateType("prflx"), IceCandidateType::kPrflx);
  EXPECT_EQ(*StringToIceCandidateType("relay"), IceCandidateType::kRelay);
  EXPECT_FALSE(StringToIceCandidateType("blah"));
  EXPECT_FALSE(StringToIceCandidateType(""));
}

TEST(CandidateTest, NetworkSlice) {
  Candidate default_initialized_candidate;
  EXPECT_EQ(default_initialized_candidate.network_slice(),
            NetworkSlice::NO_SLICE);

  SocketAddress address("192.168.1.5", 1234);
  Candidate candidate(ICE_CANDIDATE_COMPONENT_RTP, "udp", address,
                      kCandidatePriority, "", "", IceCandidateType::kHost,
                      kCandidateGeneration, kCandidateFoundation1);
  EXPECT_EQ(candidate.network_slice(), NetworkSlice::NO_SLICE);

  candidate.set_network_slice(NetworkSlice::UNIFIED_COMMUNICATIONS);
  EXPECT_EQ(candidate.network_slice(), NetworkSlice::UNIFIED_COMMUNICATIONS);

  Candidate copied_candidate(candidate);
  EXPECT_EQ(copied_candidate.network_slice(),
            NetworkSlice::UNIFIED_COMMUNICATIONS);
}

TEST(CandidateTest, Parse) {
  constexpr char kCand1[] =
      "candidate:a0+B/1 1 udp 2130706432 192.168.1.5 1234 typ host "
      "generation 2";
  RTCErrorOr<Candidate> ret = Candidate::ParseCandidateString(kCand1);
  ASSERT_TRUE(ret.ok());
  Candidate c = ret.MoveValue();
  EXPECT_FALSE(c.id().empty());
  EXPECT_EQ(c.foundation(), "a0+B/1");
  EXPECT_EQ(c.component(), 1);
  EXPECT_EQ(c.protocol(), "udp");
  EXPECT_EQ(c.priority(), 2130706432u);  // 0x7F000000
  EXPECT_EQ(c.address().ToString(), "192.168.1.5:1234");
  EXPECT_EQ(c.type(), IceCandidateType::kHost);
  EXPECT_EQ(c.generation(), 2u);

  // Test compatibility with the same string as an attribute line.
  ret = Candidate::ParseCandidateString(std::string("a=") + kCand1);
  ASSERT_TRUE(ret.ok());
  EXPECT_TRUE(ret.value().IsEquivalent(c));

  // Test some bogus strings.
  EXPECT_FALSE(Candidate::ParseCandidateString("").ok());
  EXPECT_FALSE(
      Candidate::ParseCandidateString(std::string("x=") + kCand1).ok());
  EXPECT_FALSE(Candidate::ParseCandidateString("a=").ok());

  // Run through a few more test strings that should all pass.
  struct Expectation {
    absl::string_view candidate_string;
    IceCandidateType type;
    absl::string_view foundation;
    absl::string_view protocol;
    absl::string_view address_str;
    absl::string_view related_address_str = "";
    int component;
    uint32_t priority;
    uint32_t generation;
    std::optional<uint16_t> network_id;
    std::optional<uint16_t> network_cost;
  } const test_candidates[] = {
      {.candidate_string =
           "candidate:a0+B/1 1 udp 2130706432 192.168.1.5 1234 typ host "
           "generation 2",
       .type = IceCandidateType::kHost,
       .foundation = "a0+B/1",
       .protocol = "udp",
       .address_str = "192.168.1.5:1234",
       .component = 1,
       .priority = 2130706432u,
       .generation = 2u},
      {.candidate_string =
           "candidate:a0+B/1 2 udp 2130706432 192.168.1.5 1235 typ host "
           "generation 2",
       .type = IceCandidateType::kHost,
       .foundation = "a0+B/1",
       .protocol = "udp",
       .address_str = "192.168.1.5:1235",
       .component = 2,
       .priority = 2130706432u,
       .generation = 2u},
      {.candidate_string =
           "candidate:a0+B/2 1 udp 2130706432 ::1 1238 typ host generation 2",
       .type = IceCandidateType::kHost,
       .foundation = "a0+B/2",
       .protocol = "udp",
       .address_str = "[::1]:1238",
       .component = 1,
       .priority = 2130706432u,
       .generation = 2u},
      {.candidate_string =
           "candidate:a0+B/3 1 udp 2130706432 74.125.127.126 2345 typ srflx "
           "raddr 192.168.1.5 rport 2346 generation 2",
       .type = IceCandidateType::kSrflx,
       .foundation = "a0+B/3",
       .protocol = "udp",
       .address_str = "74.125.127.126:2345",
       .related_address_str = "192.168.1.5:2346",
       .component = 1,
       .priority = 2130706432u,
       .generation = 2u},
      {.candidate_string =
           "candidate:a0+B/3 1 udp 16785663 74.125.127.126 2345 typ relay "
           "raddr 192.168.1.5 rport 2346 generation 1 network-id 77",
       .type = IceCandidateType::kRelay,
       .foundation = "a0+B/3",
       .protocol = "udp",
       .address_str = "74.125.127.126:2345",
       .related_address_str = "192.168.1.5:2346",
       .component = 1,
       .priority = 16785663u,
       .generation = 1u,
       .network_id = 77u},
      {.candidate_string =
           "candidate:a0+B/3 1 udp 16785663 74.125.127.126 2345 typ relay "
           "raddr 192.168.1.5 rport 2346 generation 1 network-cost 765",
       .type = IceCandidateType::kRelay,
       .foundation = "a0+B/3",
       .protocol = "udp",
       .address_str = "74.125.127.126:2345",
       .related_address_str = "192.168.1.5:2346",
       .component = 1,
       .priority = 16785663u,
       .generation = 1u,
       .network_cost = 765u},
  };

  for (const auto& test : test_candidates) {
    ret = Candidate::ParseCandidateString(test.candidate_string);
    ASSERT_TRUE(ret.ok()) << test.candidate_string;
    c = ret.MoveValue();
    EXPECT_FALSE(c.id().empty());

    // Verify parsed attributes.
    EXPECT_EQ(c.foundation(), test.foundation);
    EXPECT_EQ(c.component(), test.component);
    EXPECT_EQ(c.protocol(), test.protocol);
    EXPECT_EQ(c.priority(), test.priority);
    EXPECT_EQ(c.address().ToString(), test.address_str);
    EXPECT_EQ(c.type(), test.type);
    EXPECT_EQ(c.generation(), test.generation);
    if (!test.related_address_str.empty()) {
      EXPECT_EQ(c.related_address().ToString(), test.related_address_str);
    }

    // Verify optional extension attributes.
    EXPECT_EQ(c.network_id(), test.network_id.value_or(0));
    EXPECT_EQ(c.network_cost(), test.network_cost.value_or(0));

    // Verify default-initialized attributes.
    EXPECT_EQ(c.network_type(), AdapterType::ADAPTER_TYPE_UNKNOWN);
    EXPECT_EQ(c.underlying_type_for_vpn(), AdapterType::ADAPTER_TYPE_UNKNOWN);
    EXPECT_EQ(c.network_slice(), NetworkSlice::NO_SLICE);
  }
}

TEST(CandidateTest, FingerprintIsAbsentByDefault) {
  Candidate c;
  EXPECT_FALSE(c.fingerprint().has_value());
}

TEST(CandidateTest, SetAndGetFingerprint) {
  const uint8_t kDigest[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
                             0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
                             0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
                             0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
  Candidate c;
  c.set_fingerprint(SSLFingerprint("sha-256", kDigest));
  ASSERT_TRUE(c.fingerprint().has_value());
  EXPECT_EQ(c.fingerprint()->algorithm, "sha-256");
  EXPECT_THAT(c.fingerprint()->digest, SizeIs(32));
}

TEST(CandidateTest, CopyConstructorDeepCopiesFingerprint) {
  const uint8_t kDigest[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                             0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  Candidate original;
  original.set_fingerprint(SSLFingerprint("sha-256", kDigest));
  Candidate duplicate(original);

  ASSERT_TRUE(original.fingerprint().has_value());
  ASSERT_TRUE(duplicate.fingerprint().has_value());
  EXPECT_EQ(*duplicate.fingerprint(), *original.fingerprint());
}

TEST(CandidateTest, AssignmentOperatorDeepCopiesFingerprint) {
  const uint8_t kDigest[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04,
                             0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                             0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14,
                             0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C};
  Candidate original;
  original.set_fingerprint(SSLFingerprint("sha-256", kDigest));

  Candidate assigned;
  EXPECT_FALSE(assigned.fingerprint().has_value());
  assigned = original;

  ASSERT_TRUE(original.fingerprint().has_value());
  ASSERT_TRUE(assigned.fingerprint().has_value());
  EXPECT_EQ(*assigned.fingerprint(), *original.fingerprint());
}

TEST(CandidateTest, CopyOfCandidateWithoutFingerprintHasNoFingerprint) {
  Candidate original;
  EXPECT_FALSE(original.fingerprint().has_value());

  Candidate duplicate(original);
  EXPECT_FALSE(duplicate.fingerprint().has_value());

  Candidate assigned;
  assigned = original;
  EXPECT_FALSE(assigned.fingerprint().has_value());
}

TEST(CandidateTest, ParseTlsCandidateWithFingerprint) {
  // TLS candidate with fingerprint extension in "algorithm;digest" format.
  constexpr char kTlsCandidate[] =
      "candidate:a0+B/1 1 tls 2130706432 192.168.1.5 443 typ host "
      "generation 2 "
      "fingerprint sha-256;"
      "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:"
      "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99";
  RTCErrorOr<Candidate> ret = Candidate::ParseCandidateString(kTlsCandidate);
  ASSERT_TRUE(ret.ok()) << ret.error().message();
  Candidate c = ret.MoveValue();

  EXPECT_EQ(c.protocol(), "tls");
  EXPECT_EQ(c.address().ToString(), "192.168.1.5:443");
  ASSERT_TRUE(c.fingerprint().has_value());
  EXPECT_EQ(c.fingerprint()->algorithm, "sha-256");
  EXPECT_THAT(c.fingerprint()->digest, SizeIs(32));
}

TEST(CandidateTest, ParseTlsCandidateWithoutFingerprint) {
  constexpr char kTlsCandidate[] =
      "candidate:a0+B/1 1 tls 2130706432 192.168.1.5 443 typ host "
      "generation 2";
  RTCErrorOr<Candidate> ret = Candidate::ParseCandidateString(kTlsCandidate);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ret.error().type(), RTCErrorType::SYNTAX_ERROR);
  EXPECT_STREQ(ret.error().message(),
               "Missing fingerprint extension for TLS candidate");
}

TEST(CandidateTest, ParseTlsCandidateWithInvalidFingerprintFormat) {
  constexpr char kTlsCandidate[] =
      "candidate:a0+B/1 1 tls 2130706432 192.168.1.5 443 typ host "
      "generation 2 fingerprint sha-256";
  RTCErrorOr<Candidate> ret = Candidate::ParseCandidateString(kTlsCandidate);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ret.error().type(), RTCErrorType::SYNTAX_ERROR);
  EXPECT_STREQ(ret.error().message(), "Invalid fingerprint");
}

TEST(CandidateTest, ParseTlsCandidateWithInvalidFingerprintAlgorithm) {
  constexpr char kTlsCandidate[] =
      "candidate:a0+B/1 1 tls 2130706432 192.168.1.5 443 typ host "
      "generation 2 "
      "fingerprint md5;"
      "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:"
      "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99";
  RTCErrorOr<Candidate> ret = Candidate::ParseCandidateString(kTlsCandidate);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ret.error().type(), RTCErrorType::SYNTAX_ERROR);
  EXPECT_STREQ(ret.error().message(),
               "Failed to create fingerprint from the digest.");
}

TEST(CandidateTest, ParseTlsCandidateWithUppercaseFingerprintAlgorithm) {
  constexpr char kTlsCandidate[] =
      "candidate:a0+B/1 1 tls 2130706432 192.168.1.5 443 typ host "
      "generation 2 "
      "fingerprint SHA-256;"
      "01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F:10:"
      "11:12:13:14:15:16:17:18:19:1A:1B:1C:1D:1E:1F:20";
  RTCErrorOr<Candidate> ret = Candidate::ParseCandidateString(kTlsCandidate);
  ASSERT_TRUE(ret.ok()) << ret.error().message();
  Candidate c = ret.MoveValue();

  ASSERT_TRUE(c.fingerprint().has_value());
  EXPECT_EQ(c.fingerprint()->algorithm, "sha-256");
}

TEST(CandidateTest, ParseCandidateWithFingerprintRoundTrip) {
  // Parse a TLS candidate with fingerprint, serialize it, and parse again
  // to verify the fingerprint survives a full round trip.
  constexpr char kTlsCandidate[] =
      "candidate:a0+B/1 1 tls 2130706432 192.168.1.5 443 typ host "
      "generation 2 "
      "fingerprint sha-256;"
      "01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F:10:"
      "11:12:13:14:15:16:17:18:19:1A:1B:1C:1D:1E:1F:20";
  RTCErrorOr<Candidate> ret = Candidate::ParseCandidateString(kTlsCandidate);
  ASSERT_TRUE(ret.ok());
  Candidate original = ret.MoveValue();

  std::string serialized =
      original.ToCandidateAttribute(/*include_ufrag=*/false);
  RTCErrorOr<Candidate> reparsed = Candidate::ParseCandidateString(serialized);
  ASSERT_TRUE(reparsed.ok()) << reparsed.error().message();
  Candidate result = reparsed.MoveValue();

  ASSERT_TRUE(original.fingerprint().has_value());
  ASSERT_TRUE(result.fingerprint().has_value());
  EXPECT_EQ(result.fingerprint()->algorithm, "sha-256");
  EXPECT_EQ(*result.fingerprint(), *original.fingerprint());
}

TEST(CandidateTest, ConstructorWithFingerprint) {
  const uint8_t kDigest[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                             0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  auto fp = SSLFingerprint("sha-256", kDigest);
  SocketAddress address("192.168.1.5", 443);
  Candidate c(ICE_CANDIDATE_COMPONENT_RTP, "tls", address, kCandidatePriority,
              "", "", IceCandidateType::kHost, kCandidateGeneration,
              kCandidateFoundation1, 0, 0, std::move(fp));
  ASSERT_TRUE(c.fingerprint().has_value());
  EXPECT_EQ(c.fingerprint()->algorithm, "sha-256");
  EXPECT_THAT(c.fingerprint()->digest, SizeIs(32));
}

TEST(CandidateTest, EqualityBothFingerprintsNull) {
  Candidate a;
  a.set_address(SocketAddress("1.2.3.4", 1234));
  Candidate b(a);
  EXPECT_FALSE(a.fingerprint().has_value());
  EXPECT_FALSE(b.fingerprint().has_value());
  EXPECT_EQ(a, b);
}

TEST(CandidateTest, EqualityBothFingerprintsSameValue) {
  const uint8_t kDigest[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                             0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  Candidate a;
  a.set_address(SocketAddress("1.2.3.4", 1234));
  a.set_fingerprint(SSLFingerprint("sha-256", kDigest));
  Candidate b(a);
  EXPECT_EQ(a, b);
}

TEST(CandidateTest, EqualityDifferentFingerprints) {
  const uint8_t kDigest1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                              0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                              0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  const uint8_t kDigest2[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8,
                              0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
                              0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8,
                              0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0};
  Candidate a;
  a.set_address(SocketAddress("1.2.3.4", 1234));
  a.set_fingerprint(SSLFingerprint("sha-256", kDigest1));
  Candidate b(a);
  b.set_fingerprint(SSLFingerprint("sha-256", kDigest2));
  EXPECT_NE(a, b);
}

TEST(CandidateTest, EqualityOneFingerprintNull) {
  const uint8_t kDigest[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                             0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  Candidate a;
  a.set_address(SocketAddress("1.2.3.4", 1234));
  Candidate b(a);
  b.set_fingerprint(SSLFingerprint("sha-256", kDigest));
  EXPECT_NE(a, b);
  EXPECT_NE(b, a);
}

TEST(CandidateTest, IsEquivalentSameFingerprint) {
  const uint8_t kDigest[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                             0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  Candidate a(ICE_CANDIDATE_COMPONENT_RTP, "tls",
              SocketAddress("192.168.1.5", 443), kCandidatePriority, "", "",
              IceCandidateType::kHost, kCandidateGeneration,
              kCandidateFoundation1, 0, 0, SSLFingerprint("sha-256", kDigest));
  Candidate b(a);
  EXPECT_TRUE(a.IsEquivalent(b));
}

TEST(CandidateTest, WithDifferentFingerprintsAreNotEquivalent) {
  const uint8_t kDigest1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                              0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                              0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  const uint8_t kDigest2[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8,
                              0xF7, 0xF6, 0xF5, 0xF4, 0xF3, 0xF2, 0xF1, 0xF0,
                              0xEF, 0xEE, 0xED, 0xEC, 0xEB, 0xEA, 0xE9, 0xE8,
                              0xE7, 0xE6, 0xE5, 0xE4, 0xE3, 0xE2, 0xE1, 0xE0};
  Candidate a(ICE_CANDIDATE_COMPONENT_RTP, "tls",
              SocketAddress("192.168.1.5", 443), kCandidatePriority, "", "",
              IceCandidateType::kHost, kCandidateGeneration,
              kCandidateFoundation1, 0, 0, SSLFingerprint("sha-256", kDigest1));
  Candidate b(a);
  b.set_fingerprint(SSLFingerprint("sha-256", kDigest2));
  EXPECT_FALSE(a.IsEquivalent(b));
}

TEST(CandidateTest, IsEquivalentOneFingerprintNull) {
  const uint8_t kDigest[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                             0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                             0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
  Candidate a(ICE_CANDIDATE_COMPONENT_RTP, "tls",
              SocketAddress("192.168.1.5", 443), kCandidatePriority, "", "",
              IceCandidateType::kHost, kCandidateGeneration,
              kCandidateFoundation1);
  Candidate b(a);
  b.set_fingerprint(SSLFingerprint("sha-256", kDigest));
  EXPECT_FALSE(a.IsEquivalent(b));
}

TEST(CandidateTest, SerializationRoundTripWithFingerprint) {
  const uint8_t kDigest[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
                             0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
                             0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
                             0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
  Candidate original(ICE_CANDIDATE_COMPONENT_RTP, "tls",
                     SocketAddress("192.168.1.5", 443), kCandidatePriority,
                     "user", "pass", IceCandidateType::kHost,
                     kCandidateGeneration, kCandidateFoundation1, 0, 0,
                     SSLFingerprint("sha-256", kDigest));
  original.set_tcptype("passive");

  // Serialize to candidate attribute string and parse back.
  std::string serialized =
      original.ToCandidateAttribute(/*include_ufrag=*/true);
  RTCErrorOr<Candidate> parsed = Candidate::ParseCandidateString(serialized);
  ASSERT_TRUE(parsed.ok()) << parsed.error().message();
  Candidate result = parsed.MoveValue();

  EXPECT_EQ(result.protocol(), "tls");
  EXPECT_EQ(result.tcptype(), TCPTYPE_PASSIVE_STR);
  ASSERT_TRUE(original.fingerprint().has_value());
  ASSERT_TRUE(result.fingerprint().has_value());
  EXPECT_EQ(result.fingerprint()->algorithm, "sha-256");
  EXPECT_EQ(*result.fingerprint(), *original.fingerprint());
}

TEST(CandidateTest, SerializeTlsCandidateWithoutFingerprintFailsOnParse) {
  Candidate original(ICE_CANDIDATE_COMPONENT_RTP, "tls",
                     SocketAddress("192.168.1.5", 443), kCandidatePriority,
                     "user", "pass", IceCandidateType::kHost,
                     kCandidateGeneration, kCandidateFoundation1);
  original.set_tcptype("passive");

  std::string serialized =
      original.ToCandidateAttribute(/*include_ufrag=*/true);
  RTCErrorOr<Candidate> parsed = Candidate::ParseCandidateString(serialized);
  ASSERT_FALSE(parsed.ok());
  EXPECT_EQ(parsed.error().type(), RTCErrorType::SYNTAX_ERROR);
  EXPECT_STREQ(parsed.error().message(),
               "Missing fingerprint extension for TLS candidate");
}

}  // namespace webrtc
