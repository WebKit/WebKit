/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/stun_request.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/environment/environment.h"
#include "api/test/rtc_error_matchers.h"
#include "api/transport/stun.h"
#include "api/units/time_delta.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

using ::testing::Ne;

std::unique_ptr<StunMessage> CreateStunMessage(
    StunMessageType type,
    const StunMessage* req = nullptr) {
  std::unique_ptr<StunMessage> msg = std::make_unique<StunMessage>(
      type, req ? req->transaction_id() : StunMessage::GenerateTransactionId());
  return msg;
}

int TotalDelay(int sends) {
  std::vector<int> delays = {0,    250,   750,   1750,  3750,
                             7750, 15750, 23750, 31750, 39750};
  return delays[sends];
}
}  // namespace

class StunRequestThunker;

class StunRequestTest : public ::testing::Test {
 public:
  StunRequestTest()
      : env_(CreateTestEnvironment()),
        manager_(Thread::Current(),
                 [this](const void* data, size_t size, StunRequest* request) {
                   OnSendPacket(data, size, request);
                 }),
        request_count_(0),
        response_(nullptr),
        success_(false),
        failure_(false),
        timeout_(false) {}

  std::unique_ptr<StunRequestThunker> CreateStunRequest();

  void OnSendPacket(const void* data, size_t size, StunRequest* req) {
    request_count_++;
  }

  virtual void OnResponse(StunMessage* res) {
    response_ = res;
    success_ = true;
  }
  virtual void OnErrorResponse(StunMessage* res) {
    response_ = res;
    failure_ = true;
  }
  virtual void OnTimeout() { timeout_ = true; }

 protected:
  AutoThread main_thread_;
  const Environment env_;
  StunRequestManager manager_;
  int request_count_;
  StunMessage* response_;
  bool success_;
  bool failure_;
  bool timeout_;
};

// Forwards results to the test class.
class StunRequestThunker : public StunRequest {
 public:
  StunRequestThunker(const Environment& env,
                     StunRequestManager& manager,
                     StunRequestTest* test)
      : StunRequest(env, manager, CreateStunMessage(STUN_BINDING_REQUEST)),
        test_(test) {
    SetAuthenticationRequired(false);
  }

  std::unique_ptr<StunMessage> CreateResponseMessage(StunMessageType type) {
    return CreateStunMessage(type, msg());
  }

 private:
  void OnResponse(StunMessage* res) override { test_->OnResponse(res); }
  void OnErrorResponse(StunMessage* res) override {
    test_->OnErrorResponse(res);
  }
  void OnTimeout() override { test_->OnTimeout(); }

  StunRequestTest* test_;
};

std::unique_ptr<StunRequestThunker> StunRequestTest::CreateStunRequest() {
  return std::make_unique<StunRequestThunker>(env_, manager_, this);
}

// Test handling of a normal binding response.
TEST_F(StunRequestTest, TestSuccess) {
  std::unique_ptr<StunRequestThunker> request = CreateStunRequest();
  std::unique_ptr<StunMessage> res =
      request->CreateResponseMessage(STUN_BINDING_RESPONSE);
  manager_.Send(std::move(request));
  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

// Test handling of an error binding response.
TEST_F(StunRequestTest, TestError) {
  std::unique_ptr<StunRequestThunker> request = CreateStunRequest();
  std::unique_ptr<StunMessage> res =
      request->CreateResponseMessage(STUN_BINDING_ERROR_RESPONSE);
  manager_.Send(std::move(request));
  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_FALSE(success_);
  EXPECT_TRUE(failure_);
  EXPECT_FALSE(timeout_);
}

// Test handling of a binding response with the wrong transaction id.
TEST_F(StunRequestTest, TestUnexpected) {
  std::unique_ptr<StunRequestThunker> request = CreateStunRequest();
  std::unique_ptr<StunMessage> res = CreateStunMessage(STUN_BINDING_RESPONSE);

  manager_.Send(std::move(request));
  EXPECT_FALSE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == nullptr);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

// Test that requests are sent at the right times.
TEST_F(StunRequestTest, TestBackoff) {
  ScopedFakeClock fake_clock;
  std::unique_ptr<StunRequestThunker> request = CreateStunRequest();
  std::unique_ptr<StunMessage> res =
      request->CreateResponseMessage(STUN_BINDING_RESPONSE);

  int64_t start = env_.clock().TimeInMilliseconds();
  manager_.Send(std::move(request));
  for (int i = 0; i < 9; ++i) {
    EXPECT_THAT(WaitUntil([&] { return request_count_; }, Ne(i),
                          {.timeout = TimeDelta::Millis(STUN_TOTAL_TIMEOUT),
                           .clock = &fake_clock}),
                IsRtcOk());
    int64_t elapsed = env_.clock().TimeInMilliseconds() - start;
    RTC_DLOG(LS_INFO) << "STUN request #" << (i + 1) << " sent at " << elapsed
                      << " ms";
    EXPECT_EQ(TotalDelay(i), elapsed);
  }
  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

// Test that we timeout properly if no response is received.
TEST_F(StunRequestTest, TestTimeout) {
  ScopedFakeClock fake_clock;
  std::unique_ptr<StunRequestThunker> request = CreateStunRequest();
  std::unique_ptr<StunMessage> res =
      request->CreateResponseMessage(STUN_BINDING_RESPONSE);

  manager_.Send(std::move(request));
  SIMULATED_WAIT(false, STUN_TOTAL_TIMEOUT, fake_clock);

  EXPECT_FALSE(manager_.CheckResponse(res.get()));
  EXPECT_TRUE(response_ == nullptr);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_TRUE(timeout_);
}

// Regression test for specific crash where we receive a response with the
// same id as a request that doesn't have an underlying StunMessage yet.
TEST_F(StunRequestTest, TestNoEmptyRequest) {
  std::unique_ptr<StunRequestThunker> request = CreateStunRequest();
  std::string request_id = request->id();

  manager_.Send(std::move(request), /*delay=*/TimeDelta::Millis(100));

  StunMessage dummy_req(0, request_id);
  std::unique_ptr<StunMessage> res =
      CreateStunMessage(STUN_BINDING_RESPONSE, &dummy_req);

  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

// If the response contains an attribute in the "comprehension required" range
// which is not recognized, the transaction should be considered a failure and
// the response should be ignored.
TEST_F(StunRequestTest, TestUnrecognizedComprehensionRequiredAttribute) {
  std::unique_ptr<StunRequestThunker> request = CreateStunRequest();
  std::unique_ptr<StunMessage> res =
      request->CreateResponseMessage(STUN_BINDING_ERROR_RESPONSE);

  manager_.Send(std::move(request));
  res->AddAttribute(StunAttribute::CreateUInt32(0x7777));
  EXPECT_FALSE(manager_.CheckResponse(res.get()));

  EXPECT_EQ(nullptr, response_);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

class StunRequestReentranceTest : public StunRequestTest {
 public:
  void OnResponse(StunMessage* res) override {
    manager_.Clear();
    StunRequestTest::OnResponse(res);
  }
  void OnErrorResponse(StunMessage* res) override {
    manager_.Clear();
    StunRequestTest::OnErrorResponse(res);
  }
};

TEST_F(StunRequestReentranceTest, TestSuccess) {
  std::unique_ptr<StunRequestThunker> request = CreateStunRequest();
  std::unique_ptr<StunMessage> res =
      request->CreateResponseMessage(STUN_BINDING_RESPONSE);
  manager_.Send(std::move(request));
  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
}

TEST_F(StunRequestReentranceTest, TestError) {
  std::unique_ptr<StunRequestThunker> request = CreateStunRequest();
  std::unique_ptr<StunMessage> res =
      request->CreateResponseMessage(STUN_BINDING_ERROR_RESPONSE);
  manager_.Send(std::move(request));
  EXPECT_TRUE(manager_.CheckResponse(res.get()));

  EXPECT_TRUE(response_ == res.get());
  EXPECT_FALSE(success_);
  EXPECT_TRUE(failure_);
  EXPECT_FALSE(timeout_);
}

}  // namespace webrtc
