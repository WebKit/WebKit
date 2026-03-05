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
#include <string>

#include "absl/strings/string_view.h"
#include "api/rtc_error.h"
#include "p2p/base/p2p_constants.h"
#include "rtc_base/socket_address.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
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
  };

  for (const auto& test : test_candidates) {
    ret = Candidate::ParseCandidateString(test.candidate_string);
    ASSERT_TRUE(ret.ok()) << test.candidate_string;
    c = ret.MoveValue();
    EXPECT_FALSE(c.id().empty());
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
  }
}

}  // namespace webrtc
