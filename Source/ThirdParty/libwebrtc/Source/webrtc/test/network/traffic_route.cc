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
  explicit ActionReceiver(std::function<void()> action) : action_(action) {}
  ~ActionReceiver() override = default;

  void OnPacketReceived(EmulatedIpPacket packet) override { action_(); }

 private:
  std::function<void()> action_;
};

}  // namespace

CrossTrafficRouteImpl::CrossTrafficRouteImpl(
    Clock* clock,
    EmulatedNetworkReceiverInterface* receiver,
    EmulatedEndpointImpl* endpoint)
    : clock_(clock), receiver_(receiver), endpoint_(endpoint) {
  null_receiver_ = std::make_unique<NullReceiver>();
  absl::optional<uint16_t> port =
      endpoint_->BindReceiver(0, null_receiver_.get());
  RTC_DCHECK(port);
  null_receiver_port_ = port.value();
}
CrossTrafficRouteImpl::~CrossTrafficRouteImpl() = default;

void CrossTrafficRouteImpl::TriggerPacketBurst(size_t num_packets,
                                               size_t packet_size) {
  for (size_t i = 0; i < num_packets; ++i) {
    SendPacket(packet_size);
  }
}

void CrossTrafficRouteImpl::NetworkDelayedAction(size_t packet_size,
                                                 std::function<void()> action) {
  auto action_receiver = std::make_unique<ActionReceiver>(action);
  // BindOneShotReceiver arranges to free the port in the endpoint after the
  // action is done.
  absl::optional<uint16_t> port =
      endpoint_->BindOneShotReceiver(0, action_receiver.get());
  RTC_DCHECK(port);
  actions_.push_back(std::move(action_receiver));
  SendPacket(packet_size, port.value());
}

void CrossTrafficRouteImpl::SendPacket(size_t packet_size) {
  SendPacket(packet_size, null_receiver_port_);
}

void CrossTrafficRouteImpl::SendPacket(size_t packet_size, uint16_t dest_port) {
  rtc::CopyOnWriteBuffer data(packet_size);
  std::fill_n(data.MutableData(), data.size(), 0);
  receiver_->OnPacketReceived(EmulatedIpPacket(
      /*from=*/rtc::SocketAddress(),
      rtc::SocketAddress(endpoint_->GetPeerLocalAddress(), dest_port), data,
      clock_->CurrentTime()));
}

}  // namespace test
}  // namespace webrtc
