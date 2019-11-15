/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/jitter_buffer_delay.h"

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "pc/test/mock_delayable.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Return;

namespace {
constexpr int kSsrc = 1234;
}  // namespace

namespace webrtc {

class JitterBufferDelayTest : public ::testing::Test {
 public:
  JitterBufferDelayTest()
      : delay_(new rtc::RefCountedObject<JitterBufferDelay>(
            rtc::Thread::Current())) {}

 protected:
  rtc::scoped_refptr<JitterBufferDelayInterface> delay_;
  MockDelayable delayable_;
};

TEST_F(JitterBufferDelayTest, Set) {
  delay_->OnStart(&delayable_, kSsrc);

  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 3000))
      .WillOnce(Return(true));

  // Delay in seconds.
  delay_->Set(3.0);
}

TEST_F(JitterBufferDelayTest, Caching) {
  // Check that value is cached before start.
  delay_->Set(4.0);

  // Check that cached value applied on the start.
  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 4000))
      .WillOnce(Return(true));
  delay_->OnStart(&delayable_, kSsrc);
}

TEST_F(JitterBufferDelayTest, Clamping) {
  delay_->OnStart(&delayable_, kSsrc);

  // In current Jitter Buffer implementation (Audio or Video) maximum supported
  // value is 10000 milliseconds.
  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 10000))
      .WillOnce(Return(true));
  delay_->Set(10.5);

  // Test int overflow.
  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 10000))
      .WillOnce(Return(true));
  delay_->Set(21474836470.0);

  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 0))
      .WillOnce(Return(true));
  delay_->Set(-21474836470.0);

  // Boundary value in seconds to milliseconds conversion.
  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 0))
      .WillOnce(Return(true));
  delay_->Set(0.0009);

  EXPECT_CALL(delayable_, SetBaseMinimumPlayoutDelayMs(kSsrc, 0))
      .WillOnce(Return(true));

  delay_->Set(-2.0);
}

}  // namespace webrtc
