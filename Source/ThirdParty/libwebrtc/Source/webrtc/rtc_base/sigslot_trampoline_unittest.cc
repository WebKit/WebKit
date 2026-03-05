/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/sigslot_trampoline.h"

#include <utility>

#include "absl/functional/any_invocable.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::StrictMock;

class ClassWithSlots {
 public:
  ClassWithSlots() : signal_0_trampoline_(this), signal_1_trampoline_(this) {}

  sigslot::signal0<> Signal0;
  void NotifySignal0() { Signal0(); }
  void SubscribeSignal0(absl::AnyInvocable<void()> callback) {
    signal_0_trampoline_.Subscribe(std::move(callback));
  }
  sigslot::signal1<int> Signal1;
  void NotifySignal1(int arg) { Signal1(arg); }
  void SubscribeSignal1(absl::AnyInvocable<void(int)> callback) {
    signal_1_trampoline_.Subscribe(std::move(callback));
  }

 private:
  SignalTrampoline<ClassWithSlots, &ClassWithSlots::Signal0>
      signal_0_trampoline_;
  SignalTrampoline<ClassWithSlots, &ClassWithSlots::Signal1>
      signal_1_trampoline_;
};

TEST(SigslotTrampolineTest, FireSignal0) {
  ClassWithSlots item;
  StrictMock<MockFunction<void()>> mock_slot;
  item.SubscribeSignal0(mock_slot.AsStdFunction());
  Mock::VerifyAndClearExpectations(&mock_slot);  // No call before Notify
  EXPECT_CALL(mock_slot, Call());
  item.NotifySignal0();
}

TEST(SigslotTrampolineTest, FireSignal1) {
  ClassWithSlots item;
  StrictMock<MockFunction<void(int)>> mock_slot;
  item.SubscribeSignal1(mock_slot.AsStdFunction());
  Mock::VerifyAndClearExpectations(&mock_slot);  // No call before Notify
  EXPECT_CALL(mock_slot, Call(7));
  item.NotifySignal1(7);
}

}  // namespace

}  // namespace webrtc
