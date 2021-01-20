/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/traffic_route.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace test {
namespace {

class NullReceiver : public EmulatedNetworkReceiverInterface {
 public:
  void OnPacketReceived(EmulatedIpPacket packet) override {}
};

class ActionReceiver : public EmulatedNetworkReceiverInterface {
 public:
  ActionReceiver(std::function<void()> action, EmulatedEndpoint* endpoint)
      : action_(action), endpoint_(endpoint) {}
  ~ActionReceiver() override = default;

  void OnPacketReceived(EmulatedIpPacket packet) override {
    RTC_DCHECK(port_);
    action_();
    endpoint_->UnbindReceiver(port_.value());
  }

  // We can't set port in constructor, because port will be provided by
  // endpoint, when this receiver will be binded to that endpoint.
  void SetPort(uint16_t port) { port_ = port; }

 private:
  std::function<void()> action_;
  // Endpoint and port will be used to free port in the endpoint after action
  // will be done.
  EmulatedEndpoint* endpoint_;
  absl::optional<uint16_t> port_ = absl::nullopt;
};

}  // namespace

TrafficRoute::TrafficRoute(Clock* clock,
                           EmulatedNetworkReceiverInterface* receiver,
                           EmulatedEndpoint* endpoint)
    : clock_(clock), receiver_(receiver), endpoint_(endpoint) {
  null_receiver_ = std::make_unique<NullReceiver>();
  absl::optional<uint16_t> port =
      endpoint_->BindReceiver(0, null_receiver_.get());
  RTC_DCHECK(port);
  null_receiver_port_ = port.value();
}
TrafficRoute::~TrafficRoute() = default;

void TrafficRoute::TriggerPacketBurst(size_t num_packets, size_t packet_size) {
  for (size_t i = 0; i < num_packets; ++i) {
    SendPacket(packet_size);
  }
}

void TrafficRoute::NetworkDelayedAction(size_t packet_size,
                                        std::function<void()> action) {
  auto action_receiver = std::make_unique<ActionReceiver>(action, endpoint_);
  absl::optional<uint16_t> port =
      endpoint_->BindReceiver(0, action_receiver.get());
  RTC_DCHECK(port);
  action_receiver->SetPort(port.value());
  actions_.push_back(std::move(action_receiver));
  SendPacket(packet_size, port.value());
}

void TrafficRoute::SendPacket(size_t packet_size) {
  SendPacket(packet_size, null_receiver_port_);
}

void TrafficRoute::SendPacket(size_t packet_size, uint16_t dest_port) {
  rtc::CopyOnWriteBuffer data(packet_size);
  std::fill_n(data.data<uint8_t>(), data.size(), 0);
  receiver_->OnPacketReceived(EmulatedIpPacket(
      /*from=*/rtc::SocketAddress(),
      rtc::SocketAddress(endpoint_->GetPeerLocalAddress(), dest_port), data,
      clock_->CurrentTime()));
}

}  // namespace test
}  // namespace webrtc
