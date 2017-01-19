/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/stunrequest.h"
#include "webrtc/base/fakeclock.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/timeutils.h"

using namespace cricket;

class StunRequestTest : public testing::Test,
                        public sigslot::has_slots<> {
 public:
  StunRequestTest()
      : manager_(rtc::Thread::Current()),
        request_count_(0), response_(NULL),
        success_(false), failure_(false), timeout_(false) {
    manager_.SignalSendPacket.connect(this, &StunRequestTest::OnSendPacket);
  }

  void OnSendPacket(const void* data, size_t size, StunRequest* req) {
    request_count_++;
  }

  void OnResponse(StunMessage* res) {
    response_ = res;
    success_ = true;
  }
  void OnErrorResponse(StunMessage* res) {
    response_ = res;
    failure_ = true;
  }
  void OnTimeout() {
    timeout_ = true;
  }

 protected:
  static StunMessage* CreateStunMessage(StunMessageType type,
                                        StunMessage* req) {
    StunMessage* msg = new StunMessage();
    msg->SetType(type);
    if (req) {
      msg->SetTransactionID(req->transaction_id());
    }
    return msg;
  }
  static int TotalDelay(int sends) {
    int total = 0;
    for (int i = 0; i < sends; i++) {
      if (i < 4)
        total += 100 << i;
      else
        total += 1600;
    }
    return total;
  }

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
  StunRequestThunker(StunMessage* msg, StunRequestTest* test)
      : StunRequest(msg), test_(test) {}
  explicit StunRequestThunker(StunRequestTest* test) : test_(test) {}
 private:
  virtual void OnResponse(StunMessage* res) {
    test_->OnResponse(res);
  }
  virtual void OnErrorResponse(StunMessage* res) {
    test_->OnErrorResponse(res);
  }
  virtual void OnTimeout() {
    test_->OnTimeout();
  }

  virtual void Prepare(StunMessage* request) {
    request->SetType(STUN_BINDING_REQUEST);
  }

  StunRequestTest* test_;
};

// Test handling of a normal binding response.
TEST_F(StunRequestTest, TestSuccess) {
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);

  manager_.Send(new StunRequestThunker(req, this));
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, req);
  EXPECT_TRUE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == res);
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}

// Test handling of an error binding response.
TEST_F(StunRequestTest, TestError) {
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);

  manager_.Send(new StunRequestThunker(req, this));
  StunMessage* res = CreateStunMessage(STUN_BINDING_ERROR_RESPONSE, req);
  EXPECT_TRUE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == res);
  EXPECT_FALSE(success_);
  EXPECT_TRUE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}

// Test handling of a binding response with the wrong transaction id.
TEST_F(StunRequestTest, TestUnexpected) {
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);

  manager_.Send(new StunRequestThunker(req, this));
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, NULL);
  EXPECT_FALSE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == NULL);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}

// Test that requests are sent at the right times, and that the 9th request
// (sent at 7900 ms) can be properly replied to.
TEST_F(StunRequestTest, TestBackoff) {
  const int MAX_TIMEOUT_MS = 10000;
  rtc::ScopedFakeClock fake_clock;
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);

  int64_t start = rtc::TimeMillis();
  manager_.Send(new StunRequestThunker(req, this));
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, req);
  for (int i = 0; i < 9; ++i) {
    EXPECT_TRUE_SIMULATED_WAIT(request_count_ != i, MAX_TIMEOUT_MS, fake_clock);
    int64_t elapsed = rtc::TimeMillis() - start;
    LOG(LS_INFO) << "STUN request #" << (i + 1)
                 << " sent at " << elapsed << " ms";
    EXPECT_EQ(TotalDelay(i), elapsed);
  }
  EXPECT_TRUE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == res);
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}

// Test that we timeout properly if no response is received in 9500 ms.
TEST_F(StunRequestTest, TestTimeout) {
  rtc::ScopedFakeClock fake_clock;
  StunMessage* req = CreateStunMessage(STUN_BINDING_REQUEST, NULL);
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, req);

  manager_.Send(new StunRequestThunker(req, this));
  // Simulate the 9500 ms STUN timeout
  SIMULATED_WAIT(false, 9500, fake_clock);

  EXPECT_FALSE(manager_.CheckResponse(res));
  EXPECT_TRUE(response_ == NULL);
  EXPECT_FALSE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_TRUE(timeout_);
  delete res;
}

// Regression test for specific crash where we receive a response with the
// same id as a request that doesn't have an underlying StunMessage yet.
TEST_F(StunRequestTest, TestNoEmptyRequest) {
  StunRequestThunker* request = new StunRequestThunker(this);

  manager_.SendDelayed(request, 100);

  StunMessage dummy_req;
  dummy_req.SetTransactionID(request->id());
  StunMessage* res = CreateStunMessage(STUN_BINDING_RESPONSE, &dummy_req);

  EXPECT_TRUE(manager_.CheckResponse(res));

  EXPECT_TRUE(response_ == res);
  EXPECT_TRUE(success_);
  EXPECT_FALSE(failure_);
  EXPECT_FALSE(timeout_);
  delete res;
}
