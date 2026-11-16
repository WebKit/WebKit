/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/rtp_stream_receiver_controller.h"

#include <cstdint>
#include <memory>
#include <set>

#include "call/rtp_packet_sink_interface.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class FakeRtpPacketSink : public RtpPacketSinkInterface {
 public:
  void OnRtpPacket(const RtpPacketReceived& packet) override {}
};

class TestRtpSinkValidator : public RtpSinkValidator {
 public:
  void OnSinkAdded(RtpPacketSinkInterface* sink) override {
    registered_sinks_.insert(sink);
  }
  void OnSinkRemoved(RtpPacketSinkInterface* sink) override {
    registered_sinks_.erase(sink);
  }
  bool IsValidSink(RtpPacketSinkInterface* sink) const override {
    return registered_sinks_.count(sink) > 0;
  }

 private:
  std::set<RtpPacketSinkInterface*> registered_sinks_;
};

TEST(RtpStreamReceiverControllerTest, PreventsStalePointerReuse) {
  auto network_thread = Thread::Create();
  network_thread->Start();
  auto worker_thread = Thread::Create();
  worker_thread->Start();

  TestRtpSinkValidator validator;
  std::unique_ptr<RtpStreamReceiverController> controller;
  worker_thread->BlockingCall([&] {
    controller = std::make_unique<RtpStreamReceiverController>(
        network_thread.get(), worker_thread.get(), &validator);
  });

  // Allocate a sink.
  FakeRtpPacketSink sink1;
  uint32_t ssrc1 = 111;
  RtpPacketReceived packet1;
  packet1.SetSsrc(ssrc1);

  RtpPacketSinkInterface* resolved_sink1 = nullptr;

  worker_thread->BlockingCall([&] {
    // 1. Create the first receiver.
    auto receiver1 = controller->CreateReceiver(ssrc1, &sink1);

    // 2. On the network thread, a packet arrives and resolves to a sink.
    network_thread->BlockingCall(
        [&] { resolved_sink1 = controller->ResolveSink(packet1); });

    EXPECT_NE(resolved_sink1, nullptr);
    // On the worker thread, it is currently valid.
    EXPECT_TRUE(validator.IsValidSink(resolved_sink1));

    // 3. Destroy the receiver. The proxy sink is removed from the validator,
    // but its destruction is deferred to the network thread and then bounced
    // back to the worker thread queue.
    receiver1.reset();

    // After destruction, it should immediately be invalid on the worker thread.
    EXPECT_FALSE(validator.IsValidSink(resolved_sink1));

    // 4. Create a new receiver immediately, reusing the same raw sink address,
    // which simulates the reallocation scenario during rapid teardown.
    uint32_t ssrc2 = 222;
    RtpPacketReceived packet2;
    packet2.SetSsrc(ssrc2);

    RtpPacketSinkInterface* resolved_sink2 = nullptr;
    auto receiver2 = controller->CreateReceiver(ssrc2, &sink1);

    network_thread->BlockingCall(
        [&] { resolved_sink2 = controller->ResolveSink(packet2); });

    EXPECT_NE(resolved_sink2, nullptr);
    EXPECT_TRUE(validator.IsValidSink(resolved_sink2));

    // The newly resolved sink pointer must not be equal to the old resolved
    // sink pointer, even though they wrap the same raw sink address. Because
    // the old proxy sink is kept alive in the task queues, the allocator
    // cannot reuse its address for the new proxy sink.
    EXPECT_NE(resolved_sink1, resolved_sink2);

    // resolved_sink1 is still invalid, while resolved_sink2 is valid.
    EXPECT_FALSE(validator.IsValidSink(resolved_sink1));
    EXPECT_TRUE(validator.IsValidSink(resolved_sink2));
  });

  network_thread->BlockingCall(
      [&] { controller->DisconnectFromNetworkThread(); });
  worker_thread->BlockingCall([&] { controller.reset(); });
}

}  // namespace
}  // namespace webrtc
