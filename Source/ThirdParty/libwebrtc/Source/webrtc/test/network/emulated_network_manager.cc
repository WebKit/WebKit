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

#include "absl/base/nullability.h"
#include "absl/memory/memory.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/time_controller.h"
#include "rtc_base/checks.h"
#include "rtc_base/network.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "test/network/fake_network_socket_server.h"
#include "test/network/network_emulation.h"

namespace webrtc {
namespace test {

// Framework assumes that webrtc::NetworkManager is called from network thread.
class EmulatedNetworkManager::NetworkManagerImpl : public NetworkManagerBase {
 public:
  explicit NetworkManagerImpl(Thread* absl_nonnull network_thread,
                              EndpointsContainer* absl_nonnull
                                  endpoints_container)
      : network_thread_(network_thread),
        endpoints_container_(endpoints_container) {}

  void StartUpdating() override;
  void StopUpdating() override;

  void UpdateNetworksOnce();
  void MaybeSignalNetworksChanged();

  // We don't support any address interfaces in the network emulation framework.
  std::vector<const Network*> GetAnyAddressNetworks() override { return {}; }

 private:
  Thread* absl_nonnull const network_thread_;
  const EndpointsContainer* absl_nonnull const endpoints_container_;
  bool sent_first_update_ RTC_GUARDED_BY(network_thread_) = false;
  int start_count_ RTC_GUARDED_BY(network_thread_) = 0;
};

EmulatedNetworkManager::EmulatedNetworkManager(
    TimeController* time_controller,
    TaskQueueBase* task_queue,
    EndpointsContainer* endpoints_container)
    : task_queue_(task_queue),
      endpoints_container_(endpoints_container),
      socket_server_(new FakeNetworkSocketServer(endpoints_container)),
      network_thread_(
          time_controller->CreateThread("net_thread",
                                        absl::WrapUnique(socket_server_))),
      network_manager_(
          std::make_unique<NetworkManagerImpl>(network_thread_.get(),
                                               endpoints_container)),
      network_manager_ptr_(network_manager_.get()) {}

EmulatedNetworkManager::~EmulatedNetworkManager() = default;

absl_nonnull std::unique_ptr<NetworkManager>
EmulatedNetworkManager::ReleaseNetworkManager() {
  RTC_CHECK(network_manager_ != nullptr)
      << "ReleaseNetworkManager can be called at most once.";
  return std::move(network_manager_);
}

void EmulatedNetworkManager::UpdateNetworks() {
  NetworkManagerImpl* absl_nonnull network_manager = network_manager_ptr_;
  network_thread_->PostTask(
      [network_manager] { network_manager->UpdateNetworksOnce(); });
}

void EmulatedNetworkManager::NetworkManagerImpl::StartUpdating() {
  RTC_DCHECK_RUN_ON(network_thread_);

  if (start_count_ > 0) {
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

void EmulatedNetworkManager::NetworkManagerImpl::StopUpdating() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (start_count_ == 0)
    return;

  --start_count_;
  if (start_count_ == 0) {
    sent_first_update_ = false;
  }
}

void EmulatedNetworkManager::GetStats(
    std::function<void(EmulatedNetworkStats)> stats_callback) const {
  task_queue_->PostTask([stats_callback, this]() {
    stats_callback(endpoints_container_->GetStats());
  });
}

void EmulatedNetworkManager::NetworkManagerImpl::UpdateNetworksOnce() {
  RTC_DCHECK_RUN_ON(network_thread_);

  std::vector<std::unique_ptr<Network>> networks;
  for (std::unique_ptr<Network>& net :
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

void EmulatedNetworkManager::NetworkManagerImpl::MaybeSignalNetworksChanged() {
  RTC_DCHECK_RUN_ON(network_thread_);
  // If manager is stopped we don't need to signal anything.
  if (start_count_ == 0) {
    return;
  }
  NotifyNetworksChanged();
}

}  // namespace test
}  // namespace webrtc
