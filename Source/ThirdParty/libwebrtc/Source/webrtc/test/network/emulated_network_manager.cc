/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/emulated_network_manager.h"

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "test/network/fake_network_socket_server.h"

namespace webrtc {
namespace test {

EmulatedNetworkManager::EmulatedNetworkManager(
    TimeController* time_controller,
    TaskQueueForTest* task_queue,
    EndpointsContainer* endpoints_container)
    : task_queue_(task_queue),
      endpoints_container_(endpoints_container),
      sent_first_update_(false),
      start_count_(0) {
  auto socket_server =
      std::make_unique<FakeNetworkSocketServer>(endpoints_container);
  packet_socket_factory_ =
      std::make_unique<rtc::BasicPacketSocketFactory>(socket_server.get());
  // Since we pass ownership of the socket server to `network_thread_`, we must
  // arrange that it outlives `packet_socket_factory_` which refers to it.
  network_thread_ =
      time_controller->CreateThread("net_thread", std::move(socket_server));
}

void EmulatedNetworkManager::EnableEndpoint(EmulatedEndpointImpl* endpoint) {
  RTC_CHECK(endpoints_container_->HasEndpoint(endpoint))
      << "No such interface: " << endpoint->GetPeerLocalAddress().ToString();
  network_thread_->PostTask([this, endpoint]() {
    endpoint->Enable();
    UpdateNetworksOnce();
  });
}

void EmulatedNetworkManager::DisableEndpoint(EmulatedEndpointImpl* endpoint) {
  RTC_CHECK(endpoints_container_->HasEndpoint(endpoint))
      << "No such interface: " << endpoint->GetPeerLocalAddress().ToString();
  network_thread_->PostTask([this, endpoint]() {
    endpoint->Disable();
    UpdateNetworksOnce();
  });
}

// Network manager interface. All these methods are supposed to be called from
// the same thread.
void EmulatedNetworkManager::StartUpdating() {
  RTC_DCHECK_RUN_ON(network_thread_.get());

  if (start_count_) {
    // If network interfaces are already discovered and signal is sent,
    // we should trigger network signal immediately for the new clients
    // to start allocating ports.
    if (sent_first_update_)
      network_thread_->PostTask([this]() { MaybeSignalNetworksChanged(); });
  } else {
    network_thread_->PostTask([this]() { UpdateNetworksOnce(); });
  }
  ++start_count_;
}

void EmulatedNetworkManager::StopUpdating() {
  RTC_DCHECK_RUN_ON(network_thread_.get());
  if (!start_count_)
    return;

  --start_count_;
  if (!start_count_) {
    sent_first_update_ = false;
  }
}

void EmulatedNetworkManager::GetStats(
    std::function<void(EmulatedNetworkStats)> stats_callback) const {
  task_queue_->PostTask([stats_callback, this]() {
    stats_callback(endpoints_container_->GetStats());
  });
}

void EmulatedNetworkManager::UpdateNetworksOnce() {
  RTC_DCHECK_RUN_ON(network_thread_.get());

  std::vector<std::unique_ptr<rtc::Network>> networks;
  for (std::unique_ptr<rtc::Network>& net :
       endpoints_container_->GetEnabledNetworks()) {
    net->set_default_local_address_provider(this);
    networks.push_back(std::move(net));
  }

  bool changed;
  MergeNetworkList(std::move(networks), &changed);
  if (changed || !sent_first_update_) {
    MaybeSignalNetworksChanged();
    sent_first_update_ = true;
  }
}

void EmulatedNetworkManager::MaybeSignalNetworksChanged() {
  RTC_DCHECK_RUN_ON(network_thread_.get());
  // If manager is stopped we don't need to signal anything.
  if (start_count_ == 0) {
    return;
  }
  SignalNetworksChanged();
}

}  // namespace test
}  // namespace webrtc
