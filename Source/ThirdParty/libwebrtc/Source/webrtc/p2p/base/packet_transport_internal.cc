/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/packet_transport_internal.h"

#include <optional>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/sequence_checker.h"
#include "rtc_base/checks.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"

namespace webrtc {

PacketTransportInternal::PacketTransportInternal() = default;

PacketTransportInternal::~PacketTransportInternal() = default;

bool PacketTransportInternal::GetOption(Socket::Option /* opt */,
                                        int* /* value */) {
  return false;
}

std::optional<NetworkRoute> PacketTransportInternal::network_route() const {
  return std::optional<NetworkRoute>();
}

void PacketTransportInternal::RegisterReceivedPacketCallback(
    void* id,
    absl::AnyInvocable<void(PacketTransportInternal*, const ReceivedIpPacket&)>
        callback) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  received_packet_callback_list_.AddReceiver(id, std::move(callback));
}

void PacketTransportInternal::DeregisterReceivedPacketCallback(void* id) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  received_packet_callback_list_.RemoveReceivers(id);
}

void PacketTransportInternal::SetOnCloseCallback(
    absl::AnyInvocable<void() &&> callback) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  RTC_DCHECK(!on_close_ || !callback);
  on_close_ = std::move(callback);
}

void PacketTransportInternal::NotifyPacketReceived(
    const ReceivedIpPacket& packet) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  received_packet_callback_list_.Send(this, packet);
}

void PacketTransportInternal::NotifyOnClose() {
  RTC_DCHECK_RUN_ON(&network_checker_);
  if (on_close_) {
    std::move(on_close_)();
    on_close_ = nullptr;
  }
}

void PacketTransportInternal::SubscribeWritableState(
    void* tag,
    absl::AnyInvocable<void(PacketTransportInternal*)> callback) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  writable_state_callbacks_.AddReceiver(tag, std::move(callback));
}
void PacketTransportInternal::UnsubscribeWritableState(void* tag) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  writable_state_callbacks_.RemoveReceivers(tag);
}
void PacketTransportInternal::NotifyWritableState(
    PacketTransportInternal* packet_transport) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  writable_state_callbacks_.Send(packet_transport);
}

void PacketTransportInternal::SubscribeReadyToSend(
    void* tag,
    absl::AnyInvocable<void(PacketTransportInternal*)> callback) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  ready_to_send_callbacks_.AddReceiver(tag, std::move(callback));
}
void PacketTransportInternal::UnsubscribeReadyToSend(void* tag) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  ready_to_send_callbacks_.RemoveReceivers(tag);
}
void PacketTransportInternal::NotifyReadyToSend(
    PacketTransportInternal* packet_transport) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  ready_to_send_callbacks_.Send(packet_transport);
}

void PacketTransportInternal::SubscribeReceivingState(
    absl::AnyInvocable<void(PacketTransportInternal*)> callback) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  receiving_state_callbacks_.AddReceiver(std::move(callback));
}
void PacketTransportInternal::SubscribeReceivingState(
    void* tag,
    absl::AnyInvocable<void(PacketTransportInternal*)> callback) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  receiving_state_callbacks_.AddReceiver(tag, std::move(callback));
}
void PacketTransportInternal::NotifyReceivingState(
    PacketTransportInternal* packet_transport) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  receiving_state_callbacks_.Send(packet_transport);
}

void PacketTransportInternal::SubscribeNetworkRouteChanged(
    void* tag,
    absl::AnyInvocable<void(std::optional<NetworkRoute>)> callback) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  network_route_changed_callbacks_.AddReceiver(tag, std::move(callback));
}
void PacketTransportInternal::UnsubscribeNetworkRouteChanged(void* tag) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  network_route_changed_callbacks_.RemoveReceivers(tag);
}
void PacketTransportInternal::NotifyNetworkRouteChanged(
    std::optional<webrtc::NetworkRoute> network_route) {
  RTC_DCHECK_RUN_ON(&network_checker_);
  network_route_changed_callbacks_.Send(network_route);
}

}  // namespace webrtc
