/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <set>
#include <string>

#include "p2p/base/mdns_message.h"
#include "rtc_base/bytebuffer.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ipaddress.h"
#include "rtc_base/socketaddress.h"
#include "test/gmock.h"

#define ReadMDnsMessage(X, Y) ReadMDnsMessageTestCase(X, Y, sizeof(Y))
#define WriteMDnsMessageAndCompare(X, Y) \
  WriteMDnsMessageAndCompareWithTestCast(X, Y, sizeof(Y))

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace webrtc {

namespace {

const uint8_t kSingleQuestionForIPv4AddrWithUnicastResponse[] = {
    0x12, 0x34,                                // ID
    0x00, 0x00,                                // flags
    0x00, 0x01,                                // number of questions
    0x00, 0x00,                                // number of answer rr
    0x00, 0x00,                                // number of name server rr
    0x00, 0x00,                                // number of additional rr
    0x06, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63,  // webrtc
    0x03, 0x6f, 0x72, 0x67,                    // org
    0x00,                                      // null label
    0x00, 0x01,                                // type A Record
    0x80, 0x01,                                // class IN, unicast response
};

const uint8_t kTwoQuestionsForIPv4AndIPv6AddrWithMulticastResponse[] = {
    0x12, 0x34,                                      // ID
    0x00, 0x00,                                      // flags
    0x00, 0x02,                                      // number of questions
    0x00, 0x00,                                      // number of answer rr
    0x00, 0x00,                                      // number of name server rr
    0x00, 0x00,                                      // number of additional rr
    0x07, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63, 0x34,  // webrtc4
    0x03, 0x6f, 0x72, 0x67,                          // org
    0x00,                                            // null label
    0x00, 0x01,                                      // type A Record
    0x00, 0x01,  // class IN, multicast response
    0x07, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63, 0x36,  // webrtc6
    0x03, 0x6f, 0x72, 0x67,                          // org
    0x00,                                            // null label
    0x00, 0x1C,                                      // type AAAA Record
    0x00, 0x01,  // class IN, multicast response
};

const uint8_t
    kTwoQuestionsForIPv4AndIPv6AddrWithMulticastResponseAndNameCompression[] = {
        0x12, 0x34,                                // ID
        0x00, 0x00,                                // flags
        0x00, 0x02,                                // number of questions
        0x00, 0x00,                                // number of answer rr
        0x00, 0x00,                                // number of name server rr
        0x00, 0x00,                                // number of additional rr
        0x03, 0x77, 0x77, 0x77,                    // www
        0x06, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63,  // webrtc
        0x03, 0x6f, 0x72, 0x67,                    // org
        0x00,                                      // null label
        0x00, 0x01,                                // type A Record
        0x00, 0x01,                    // class IN, multicast response
        0x04, 0x6d, 0x64, 0x6e, 0x73,  // mdns
        0xc0, 0x10,                    // offset 16, webrtc.org.
        0x00, 0x1C,                    // type AAAA Record
        0x00, 0x01,                    // class IN, multicast response
};

const uint8_t kThreeQuestionsWithTwoPointersToTheSameNameSuffix[] = {
    0x12, 0x34,                                // ID
    0x00, 0x00,                                // flags
    0x00, 0x03,                                // number of questions
    0x00, 0x00,                                // number of answer rr
    0x00, 0x00,                                // number of name server rr
    0x00, 0x00,                                // number of additional rr
    0x03, 0x77, 0x77, 0x77,                    // www
    0x06, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63,  // webrtc
    0x03, 0x6f, 0x72, 0x67,                    // org
    0x00,                                      // null label
    0x00, 0x01,                                // type A Record
    0x00, 0x01,                                // class IN, multicast response
    0x04, 0x6d, 0x64, 0x6e, 0x73,              // mdns
    0xc0, 0x10,                                // offset 16, webrtc.org.
    0x00, 0x1C,                                // type AAAA Record
    0x00, 0x01,                                // class IN, multicast response
    0xc0, 0x10,                                // offset 16, webrtc.org.
    0x00, 0x01,                                // type A Record
    0x00, 0x01,                                // class IN, multicast response
};

const uint8_t kThreeQuestionsWithPointerToNameSuffixContainingAnotherPointer[] =
    {
        0x12, 0x34,                                // ID
        0x00, 0x00,                                // flags
        0x00, 0x03,                                // number of questions
        0x00, 0x00,                                // number of answer rr
        0x00, 0x00,                                // number of name server rr
        0x00, 0x00,                                // number of additional rr
        0x03, 0x77, 0x77, 0x77,                    // www
        0x06, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63,  // webrtc
        0x03, 0x6f, 0x72, 0x67,                    // org
        0x00,                                      // null label
        0x00, 0x01,                                // type A Record
        0x00, 0x01,                    // class IN, multicast response
        0x04, 0x6d, 0x64, 0x6e, 0x73,  // mdns
        0xc0, 0x10,                    // offset 16, webrtc.org.
        0x00, 0x1C,                    // type AAAA Record
        0x00, 0x01,                    // class IN, multicast response
        0x03, 0x77, 0x77, 0x77,        // www
        0xc0, 0x20,                    // offset 32, mdns.webrtc.org.
        0x00, 0x01,                    // type A Record
        0x00, 0x01,                    // class IN, multicast response
};

const uint8_t kCorruptedQuestionWithNameCompression1[] = {
    0x12, 0x34,  // ID
    0x84, 0x00,  // flags
    0x00, 0x01,  // number of questions
    0x00, 0x00,  // number of answer rr
    0x00, 0x00,  // number of name server rr
    0x00, 0x00,  // number of additional rr
    0xc0, 0x0c,  // offset 12,
    0x00, 0x01,  // type A Record
    0x00, 0x01,  // class IN
};

const uint8_t kCorruptedQuestionWithNameCompression2[] = {
    0x12, 0x34,  // ID
    0x84, 0x00,  // flags
    0x00, 0x01,  // number of questions
    0x00, 0x00,  // number of answer rr
    0x00, 0x00,  // number of name server rr
    0x00, 0x00,  // number of additional rr
    0x01, 0x77,  // w
    0xc0, 0x0c,  // offset 12,
    0x00, 0x01,  // type A Record
    0x00, 0x01,  // class IN
};

const uint8_t kSingleAuthoritativeAnswerWithIPv4Addr[] = {
    0x12, 0x34,                                // ID
    0x84, 0x00,                                // flags
    0x00, 0x00,                                // number of questions
    0x00, 0x01,                                // number of answer rr
    0x00, 0x00,                                // number of name server rr
    0x00, 0x00,                                // number of additional rr
    0x06, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63,  // webrtc
    0x03, 0x6f, 0x72, 0x67,                    // org
    0x00,                                      // null label
    0x00, 0x01,                                // type A Record
    0x00, 0x01,                                // class IN
    0x00, 0x00, 0x00, 0x78,                    // TTL, 120 seconds
    0x00, 0x04,                                // rdlength, 32 bits
    0xC0, 0xA8, 0x00, 0x01,                    // 192.168.0.1
};

const uint8_t kTwoAuthoritativeAnswersWithIPv4AndIPv6Addr[] = {
    0x12, 0x34,                                      // ID
    0x84, 0x00,                                      // flags
    0x00, 0x00,                                      // number of questions
    0x00, 0x02,                                      // number of answer rr
    0x00, 0x00,                                      // number of name server rr
    0x00, 0x00,                                      // number of additional rr
    0x07, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63, 0x34,  // webrtc4
    0x03, 0x6f, 0x72, 0x67,                          // org
    0x00,                                            // null label
    0x00, 0x01,                                      // type A Record
    0x00, 0x01,                                      // class IN
    0x00, 0x00, 0x00, 0x3c,                          // TTL, 60 seconds
    0x00, 0x04,                                      // rdlength, 32 bits
    0xC0, 0xA8, 0x00, 0x01,                          // 192.168.0.1
    0x07, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63, 0x36,  // webrtc6
    0x03, 0x6f, 0x72, 0x67,                          // org
    0x00,                                            // null label
    0x00, 0x1C,                                      // type AAAA Record
    0x00, 0x01,                                      // class IN
    0x00, 0x00, 0x00, 0x78,                          // TTL, 120 seconds
    0x00, 0x10,                                      // rdlength, 128 bits
    0xfd, 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0x01,  // fd12:3456:789a:1::1
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

const uint8_t kTwoAuthoritativeAnswersWithIPv4AndIPv6AddrWithNameCompression[] =
    {
        0x12, 0x34,                                // ID
        0x84, 0x00,                                // flags
        0x00, 0x00,                                // number of questions
        0x00, 0x02,                                // number of answer rr
        0x00, 0x00,                                // number of name server rr
        0x00, 0x00,                                // number of additional rr
        0x03, 0x77, 0x77, 0x77,                    // www
        0x06, 0x77, 0x65, 0x62, 0x72, 0x74, 0x63,  // webrtc
        0x03, 0x6f, 0x72, 0x67,                    // org
        0x00,                                      // null label
        0x00, 0x01,                                // type A Record
        0x00, 0x01,                                // class IN
        0x00, 0x00, 0x00, 0x3c,                    // TTL, 60 seconds
        0x00, 0x04,                                // rdlength, 32 bits
        0xc0, 0xA8, 0x00, 0x01,                    // 192.168.0.1
        0xc0, 0x10,                                // offset 16, webrtc.org.
        0x00, 0x1C,                                // type AAAA Record
        0x00, 0x01,                                // class IN
        0x00, 0x00, 0x00, 0x78,                    // TTL, 120 seconds
        0x00, 0x10,                                // rdlength, 128 bits
        0xfd, 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0x01,  // fd12:3456:789a:1::1
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

const uint8_t kCorruptedAnswerWithNameCompression1[] = {
    0x12, 0x34,              // ID
    0x84, 0x00,              // flags
    0x00, 0x00,              // number of questions
    0x00, 0x01,              // number of answer rr
    0x00, 0x00,              // number of name server rr
    0x00, 0x00,              // number of additional rr
    0xc0, 0x0c,              // offset 12,
    0x00, 0x01,              // type A Record
    0x00, 0x01,              // class IN
    0x00, 0x00, 0x00, 0x3c,  // TTL, 60 seconds
    0x00, 0x04,              // rdlength, 32 bits
    0xc0, 0xA8, 0x00, 0x01,  // 192.168.0.1
};

const uint8_t kCorruptedAnswerWithNameCompression2[] = {
    0x12, 0x34,              // ID
    0x84, 0x00,              // flags
    0x00, 0x00,              // number of questions
    0x00, 0x01,              // number of answer rr
    0x00, 0x00,              // number of name server rr
    0x00, 0x00,              // number of additional rr
    0x01, 0x77,              // w
    0xc0, 0x0c,              // offset 12,
    0x00, 0x01,              // type A Record
    0x00, 0x01,              // class IN
    0x00, 0x00, 0x00, 0x3c,  // TTL, 60 seconds
    0x00, 0x04,              // rdlength, 32 bits
    0xc0, 0xA8, 0x00, 0x01,  // 192.168.0.1
};

bool ReadMDnsMessageTestCase(MDnsMessage* msg,
                             const uint8_t* testcase,
                             size_t size) {
  MessageBufferReader buf(reinterpret_cast<const char*>(testcase), size);
  return msg->Read(&buf);
}

void WriteMDnsMessageAndCompareWithTestCast(MDnsMessage* msg,
                                            const uint8_t* testcase,
                                            size_t size) {
  rtc::ByteBufferWriter out;
  EXPECT_TRUE(msg->Write(&out));
  EXPECT_EQ(size, out.Length());
  int len = static_cast<int>(out.Length());
  rtc::ByteBufferReader read_buf(out);
  std::string bytes;
  read_buf.ReadString(&bytes, len);
  std::string testcase_bytes(reinterpret_cast<const char*>(testcase), size);
  EXPECT_EQ(testcase_bytes, bytes);
}

bool GetQueriedNames(MDnsMessage* msg, std::set<std::string>* names) {
  if (!msg->IsQuery() || msg->question_section().empty()) {
    return false;
  }
  for (const auto& question : msg->question_section()) {
    names->insert(question.GetName());
  }
  return true;
}

bool GetResolution(MDnsMessage* msg,
                   std::map<std::string, rtc::IPAddress>* names) {
  if (msg->IsQuery() || msg->answer_section().empty()) {
    return false;
  }
  for (const auto& answer : msg->answer_section()) {
    rtc::IPAddress resolved_addr;
    if (!answer.GetIPAddressFromRecordData(&resolved_addr)) {
      return false;
    }
    (*names)[answer.GetName()] = resolved_addr;
  }
  return true;
}

}  // namespace

TEST(MDnsMessageTest, ReadSingleQuestionForIPv4Address) {
  MDnsMessage msg;
  ASSERT_TRUE(
      ReadMDnsMessage(&msg, kSingleQuestionForIPv4AddrWithUnicastResponse));
  EXPECT_TRUE(msg.IsQuery());
  EXPECT_EQ(0x1234, msg.GetId());
  ASSERT_EQ(1u, msg.question_section().size());
  EXPECT_EQ(0u, msg.answer_section().size());
  EXPECT_EQ(0u, msg.authority_section().size());
  EXPECT_EQ(0u, msg.additional_section().size());
  EXPECT_TRUE(msg.ShouldUnicastResponse());

  const auto& question = msg.question_section()[0];
  EXPECT_EQ(SectionEntryType::kA, question.GetType());

  std::set<std::string> queried_names;
  EXPECT_TRUE(GetQueriedNames(&msg, &queried_names));
  EXPECT_THAT(queried_names, ElementsAre("webrtc.org."));
}

TEST(MDnsMessageTest, ReadTwoQuestionsForIPv4AndIPv6Addr) {
  MDnsMessage msg;
  ASSERT_TRUE(ReadMDnsMessage(
      &msg, kTwoQuestionsForIPv4AndIPv6AddrWithMulticastResponse));
  EXPECT_TRUE(msg.IsQuery());
  EXPECT_EQ(0x1234, msg.GetId());
  ASSERT_EQ(2u, msg.question_section().size());
  EXPECT_EQ(0u, msg.answer_section().size());
  EXPECT_EQ(0u, msg.authority_section().size());
  EXPECT_EQ(0u, msg.additional_section().size());

  const auto& question1 = msg.question_section()[0];
  const auto& question2 = msg.question_section()[1];
  EXPECT_EQ(SectionEntryType::kA, question1.GetType());
  EXPECT_EQ(SectionEntryType::kAAAA, question2.GetType());

  std::set<std::string> queried_names;
  EXPECT_TRUE(GetQueriedNames(&msg, &queried_names));
  EXPECT_THAT(queried_names,
              UnorderedElementsAre("webrtc4.org.", "webrtc6.org."));
}

TEST(MDnsMessageTest, ReadTwoQuestionsForIPv4AndIPv6AddrWithNameCompression) {
  MDnsMessage msg;
  ASSERT_TRUE(ReadMDnsMessage(
      &msg,
      kTwoQuestionsForIPv4AndIPv6AddrWithMulticastResponseAndNameCompression));

  ASSERT_EQ(2u, msg.question_section().size());
  const auto& question1 = msg.question_section()[0];
  const auto& question2 = msg.question_section()[1];
  EXPECT_EQ(SectionEntryType::kA, question1.GetType());
  EXPECT_EQ(SectionEntryType::kAAAA, question2.GetType());

  std::set<std::string> queried_names;
  EXPECT_TRUE(GetQueriedNames(&msg, &queried_names));
  EXPECT_THAT(queried_names,
              UnorderedElementsAre("www.webrtc.org.", "mdns.webrtc.org."));
}

TEST(MDnsMessageTest, ReadThreeQuestionsWithTwoPointersToTheSameNameSuffix) {
  MDnsMessage msg;
  ASSERT_TRUE(
      ReadMDnsMessage(&msg, kThreeQuestionsWithTwoPointersToTheSameNameSuffix));

  ASSERT_EQ(3u, msg.question_section().size());
  const auto& question1 = msg.question_section()[0];
  const auto& question2 = msg.question_section()[1];
  const auto& question3 = msg.question_section()[2];
  EXPECT_EQ(SectionEntryType::kA, question1.GetType());
  EXPECT_EQ(SectionEntryType::kAAAA, question2.GetType());
  EXPECT_EQ(SectionEntryType::kA, question3.GetType());

  std::set<std::string> queried_names;
  EXPECT_TRUE(GetQueriedNames(&msg, &queried_names));
  EXPECT_THAT(queried_names,
              UnorderedElementsAre("www.webrtc.org.", "mdns.webrtc.org.",
                                   "webrtc.org."));
}

TEST(MDnsMessageTest,
     ReadThreeQuestionsWithPointerToNameSuffixContainingAnotherPointer) {
  MDnsMessage msg;
  ASSERT_TRUE(ReadMDnsMessage(
      &msg, kThreeQuestionsWithPointerToNameSuffixContainingAnotherPointer));

  ASSERT_EQ(3u, msg.question_section().size());
  const auto& question1 = msg.question_section()[0];
  const auto& question2 = msg.question_section()[1];
  const auto& question3 = msg.question_section()[2];
  EXPECT_EQ(SectionEntryType::kA, question1.GetType());
  EXPECT_EQ(SectionEntryType::kAAAA, question2.GetType());
  EXPECT_EQ(SectionEntryType::kA, question3.GetType());

  std::set<std::string> queried_names;
  EXPECT_TRUE(GetQueriedNames(&msg, &queried_names));
  EXPECT_THAT(queried_names,
              UnorderedElementsAre("www.webrtc.org.", "mdns.webrtc.org.",
                                   "www.mdns.webrtc.org."));
}

TEST(MDnsMessageTest,
     ReadQuestionWithCorruptedPointerInNameCompressionShouldFail) {
  MDnsMessage msg;
  EXPECT_FALSE(ReadMDnsMessage(&msg, kCorruptedQuestionWithNameCompression1));
  EXPECT_FALSE(ReadMDnsMessage(&msg, kCorruptedQuestionWithNameCompression2));
}

TEST(MDnsMessageTest, ReadSingleAnswerForIPv4Addr) {
  MDnsMessage msg;
  ASSERT_TRUE(ReadMDnsMessage(&msg, kSingleAuthoritativeAnswerWithIPv4Addr));
  EXPECT_FALSE(msg.IsQuery());
  EXPECT_TRUE(msg.IsAuthoritative());
  EXPECT_EQ(0x1234, msg.GetId());
  EXPECT_EQ(0u, msg.question_section().size());
  ASSERT_EQ(1u, msg.answer_section().size());
  EXPECT_EQ(0u, msg.authority_section().size());
  EXPECT_EQ(0u, msg.additional_section().size());

  const auto& answer = msg.answer_section()[0];
  EXPECT_EQ(SectionEntryType::kA, answer.GetType());
  EXPECT_EQ(120u, answer.GetTtlSeconds());

  std::map<std::string, rtc::IPAddress> resolution;
  EXPECT_TRUE(GetResolution(&msg, &resolution));
  rtc::IPAddress expected_addr(rtc::SocketAddress("192.168.0.1", 0).ipaddr());
  EXPECT_THAT(resolution, ElementsAre(Pair("webrtc.org.", expected_addr)));
}

TEST(MDnsMessageTest, ReadTwoAnswersForIPv4AndIPv6Addr) {
  MDnsMessage msg;
  ASSERT_TRUE(
      ReadMDnsMessage(&msg, kTwoAuthoritativeAnswersWithIPv4AndIPv6Addr));
  EXPECT_FALSE(msg.IsQuery());
  EXPECT_TRUE(msg.IsAuthoritative());
  EXPECT_EQ(0x1234, msg.GetId());
  EXPECT_EQ(0u, msg.question_section().size());
  ASSERT_EQ(2u, msg.answer_section().size());
  EXPECT_EQ(0u, msg.authority_section().size());
  EXPECT_EQ(0u, msg.additional_section().size());

  const auto& answer1 = msg.answer_section()[0];
  const auto& answer2 = msg.answer_section()[1];
  EXPECT_EQ(SectionEntryType::kA, answer1.GetType());
  EXPECT_EQ(SectionEntryType::kAAAA, answer2.GetType());
  EXPECT_EQ(60u, answer1.GetTtlSeconds());
  EXPECT_EQ(120u, answer2.GetTtlSeconds());

  std::map<std::string, rtc::IPAddress> resolution;
  EXPECT_TRUE(GetResolution(&msg, &resolution));
  rtc::IPAddress expected_addr_ipv4(
      rtc::SocketAddress("192.168.0.1", 0).ipaddr());
  rtc::IPAddress expected_addr_ipv6(
      rtc::SocketAddress("fd12:3456:789a:1::1", 0).ipaddr());
  EXPECT_THAT(resolution,
              UnorderedElementsAre(Pair("webrtc4.org.", expected_addr_ipv4),
                                   Pair("webrtc6.org.", expected_addr_ipv6)));
}

TEST(MDnsMessageTest, ReadTwoAnswersForIPv4AndIPv6AddrWithNameCompression) {
  MDnsMessage msg;
  ASSERT_TRUE(ReadMDnsMessage(
      &msg, kTwoAuthoritativeAnswersWithIPv4AndIPv6AddrWithNameCompression));

  std::map<std::string, rtc::IPAddress> resolution;
  EXPECT_TRUE(GetResolution(&msg, &resolution));
  rtc::IPAddress expected_addr_ipv4(
      rtc::SocketAddress("192.168.0.1", 0).ipaddr());
  rtc::IPAddress expected_addr_ipv6(
      rtc::SocketAddress("fd12:3456:789a:1::1", 0).ipaddr());
  EXPECT_THAT(resolution,
              UnorderedElementsAre(Pair("www.webrtc.org.", expected_addr_ipv4),
                                   Pair("webrtc.org.", expected_addr_ipv6)));
}

TEST(MDnsMessageTest,
     ReadAnswerWithCorruptedPointerInNameCompressionShouldFail) {
  MDnsMessage msg;
  EXPECT_FALSE(ReadMDnsMessage(&msg, kCorruptedAnswerWithNameCompression1));
  EXPECT_FALSE(ReadMDnsMessage(&msg, kCorruptedAnswerWithNameCompression2));
}

TEST(MDnsMessageTest, WriteSingleQuestionForIPv4Addr) {
  MDnsMessage msg;
  msg.SetId(0x1234);
  msg.SetQueryOrResponse(true);

  MDnsQuestion question;
  question.SetName("webrtc.org.");
  question.SetType(SectionEntryType::kA);
  question.SetClass(SectionEntryClass::kIN);
  question.SetUnicastResponse(true);
  msg.AddQuestion(question);

  WriteMDnsMessageAndCompare(&msg,
                             kSingleQuestionForIPv4AddrWithUnicastResponse);
}

TEST(MDnsMessageTest, WriteTwoQuestionsForIPv4AndIPv6Addr) {
  MDnsMessage msg;
  msg.SetId(0x1234);
  msg.SetQueryOrResponse(true);

  MDnsQuestion question1;
  question1.SetName("webrtc4.org.");
  question1.SetType(SectionEntryType::kA);
  question1.SetClass(SectionEntryClass::kIN);
  msg.AddQuestion(question1);

  MDnsQuestion question2;
  question2.SetName("webrtc6.org.");
  question2.SetType(SectionEntryType::kAAAA);
  question2.SetClass(SectionEntryClass::kIN);
  msg.AddQuestion(question2);

  WriteMDnsMessageAndCompare(
      &msg, kTwoQuestionsForIPv4AndIPv6AddrWithMulticastResponse);
}

TEST(MDnsMessageTest, WriteSingleAnswerToIPv4Addr) {
  MDnsMessage msg;
  msg.SetId(0x1234);
  msg.SetQueryOrResponse(false);
  msg.SetAuthoritative(true);

  MDnsResourceRecord answer;
  answer.SetName("webrtc.org.");
  answer.SetType(SectionEntryType::kA);
  answer.SetClass(SectionEntryClass::kIN);
  EXPECT_TRUE(answer.SetIPAddressInRecordData(
      rtc::SocketAddress("192.168.0.1", 0).ipaddr()));
  answer.SetTtlSeconds(120);
  msg.AddAnswerRecord(answer);

  WriteMDnsMessageAndCompare(&msg, kSingleAuthoritativeAnswerWithIPv4Addr);
}

TEST(MDnsMessageTest, WriteTwoAnswersToIPv4AndIPv6Addr) {
  MDnsMessage msg;
  msg.SetId(0x1234);
  msg.SetQueryOrResponse(false);
  msg.SetAuthoritative(true);

  MDnsResourceRecord answer1;
  answer1.SetName("webrtc4.org.");
  answer1.SetType(SectionEntryType::kA);
  answer1.SetClass(SectionEntryClass::kIN);
  answer1.SetIPAddressInRecordData(
      rtc::SocketAddress("192.168.0.1", 0).ipaddr());
  answer1.SetTtlSeconds(60);
  msg.AddAnswerRecord(answer1);

  MDnsResourceRecord answer2;
  answer2.SetName("webrtc6.org.");
  answer2.SetType(SectionEntryType::kAAAA);
  answer2.SetClass(SectionEntryClass::kIN);
  answer2.SetIPAddressInRecordData(
      rtc::SocketAddress("fd12:3456:789a:1::1", 0).ipaddr());
  answer2.SetTtlSeconds(120);
  msg.AddAnswerRecord(answer2);

  WriteMDnsMessageAndCompare(&msg, kTwoAuthoritativeAnswersWithIPv4AndIPv6Addr);
}

}  // namespace webrtc
