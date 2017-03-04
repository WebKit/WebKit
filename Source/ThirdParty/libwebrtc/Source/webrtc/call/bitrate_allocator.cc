/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/call/bitrate_allocator.h"

#include <algorithm>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/metrics.h"

namespace webrtc {

// Allow packets to be transmitted in up to 2 times max video bitrate if the
// bandwidth estimate allows it.
const int kTransmissionMaxBitrateMultiplier = 2;
const int kDefaultBitrateBps = 300000;

// Require a bitrate increase of max(10%, 20kbps) to resume paused streams.
const double kToggleFactor = 0.1;
const uint32_t kMinToggleBitrateBps = 20000;

const int64_t kBweLogIntervalMs = 5000;

namespace {

double MediaRatio(uint32_t allocated_bitrate, uint32_t protection_bitrate) {
  RTC_DCHECK_GT(allocated_bitrate, 0);
  if (protection_bitrate == 0)
    return 1.0;

  uint32_t media_bitrate = allocated_bitrate - protection_bitrate;
  return media_bitrate / static_cast<double>(allocated_bitrate);
}
}  // namespace

BitrateAllocator::BitrateAllocator(LimitObserver* limit_observer)
    : limit_observer_(limit_observer),
      bitrate_observer_configs_(),
      last_bitrate_bps_(0),
      last_non_zero_bitrate_bps_(kDefaultBitrateBps),
      last_fraction_loss_(0),
      last_rtt_(0),
      num_pause_events_(0),
      clock_(Clock::GetRealTimeClock()),
      last_bwe_log_time_(0),
      total_requested_padding_bitrate_(0),
      total_requested_min_bitrate_(0) {
  sequenced_checker_.Detach();
}

BitrateAllocator::~BitrateAllocator() {
  RTC_HISTOGRAM_COUNTS_100("WebRTC.Call.NumberOfPauseEvents",
                           num_pause_events_);
}

void BitrateAllocator::OnNetworkChanged(uint32_t target_bitrate_bps,
                                        uint8_t fraction_loss,
                                        int64_t rtt,
                                        int64_t probing_interval_ms) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  last_bitrate_bps_ = target_bitrate_bps;
  last_non_zero_bitrate_bps_ =
      target_bitrate_bps > 0 ? target_bitrate_bps : last_non_zero_bitrate_bps_;
  last_fraction_loss_ = fraction_loss;
  last_rtt_ = rtt;
  last_probing_interval_ms_ = probing_interval_ms;

  // Periodically log the incoming BWE.
  int64_t now = clock_->TimeInMilliseconds();
  if (now > last_bwe_log_time_ + kBweLogIntervalMs) {
    LOG(LS_INFO) << "Current BWE " << target_bitrate_bps;
    last_bwe_log_time_ = now;
  }

  ObserverAllocation allocation = AllocateBitrates(target_bitrate_bps);

  for (auto& config : bitrate_observer_configs_) {
    uint32_t allocated_bitrate = allocation[config.observer];
    uint32_t protection_bitrate = config.observer->OnBitrateUpdated(
        allocated_bitrate, last_fraction_loss_, last_rtt_,
        last_probing_interval_ms_);

    if (allocated_bitrate == 0 && config.allocated_bitrate_bps > 0) {
      if (target_bitrate_bps > 0)
        ++num_pause_events_;
      // The protection bitrate is an estimate based on the ratio between media
      // and protection used before this observer was muted.
      uint32_t predicted_protection_bps =
          (1.0 - config.media_ratio) * config.min_bitrate_bps;
      LOG(LS_INFO) << "Pausing observer " << config.observer
                   << " with configured min bitrate " << config.min_bitrate_bps
                   << " and current estimate of " << target_bitrate_bps
                   << " and protection bitrate " << predicted_protection_bps;
    } else if (allocated_bitrate > 0 && config.allocated_bitrate_bps == 0) {
      if (target_bitrate_bps > 0)
        ++num_pause_events_;
      LOG(LS_INFO) << "Resuming observer " << config.observer
                   << ", configured min bitrate " << config.min_bitrate_bps
                   << ", current allocation " << allocated_bitrate
                   << " and protection bitrate " << protection_bitrate;
    }

    // Only update the media ratio if the observer got an allocation.
    if (allocated_bitrate > 0)
      config.media_ratio = MediaRatio(allocated_bitrate, protection_bitrate);
    config.allocated_bitrate_bps = allocated_bitrate;
  }
  UpdateAllocationLimits();
}

void BitrateAllocator::AddObserver(BitrateAllocatorObserver* observer,
                                   uint32_t min_bitrate_bps,
                                   uint32_t max_bitrate_bps,
                                   uint32_t pad_up_bitrate_bps,
                                   bool enforce_min_bitrate) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  auto it = FindObserverConfig(observer);

  // Update settings if the observer already exists, create a new one otherwise.
  if (it != bitrate_observer_configs_.end()) {
    it->min_bitrate_bps = min_bitrate_bps;
    it->max_bitrate_bps = max_bitrate_bps;
    it->pad_up_bitrate_bps = pad_up_bitrate_bps;
    it->enforce_min_bitrate = enforce_min_bitrate;
  } else {
    bitrate_observer_configs_.push_back(
        ObserverConfig(observer, min_bitrate_bps, max_bitrate_bps,
                       pad_up_bitrate_bps, enforce_min_bitrate));
  }

  ObserverAllocation allocation;
  if (last_bitrate_bps_ > 0) {
    // Calculate a new allocation and update all observers.
    allocation = AllocateBitrates(last_bitrate_bps_);
    for (auto& config : bitrate_observer_configs_) {
      uint32_t allocated_bitrate = allocation[config.observer];
      uint32_t protection_bitrate = config.observer->OnBitrateUpdated(
          allocated_bitrate, last_fraction_loss_, last_rtt_,
          last_probing_interval_ms_);
      config.allocated_bitrate_bps = allocated_bitrate;
      if (allocated_bitrate > 0)
        config.media_ratio = MediaRatio(allocated_bitrate, protection_bitrate);
    }
  } else {
    // Currently, an encoder is not allowed to produce frames.
    // But we still have to return the initial config bitrate + let the
    // observer know that it can not produce frames.
    allocation = AllocateBitrates(last_non_zero_bitrate_bps_);
    observer->OnBitrateUpdated(0, last_fraction_loss_, last_rtt_,
                               last_probing_interval_ms_);
  }
  UpdateAllocationLimits();
}

void BitrateAllocator::UpdateAllocationLimits() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  uint32_t total_requested_padding_bitrate = 0;
  uint32_t total_requested_min_bitrate = 0;

  for (const auto& config : bitrate_observer_configs_) {
    uint32_t stream_padding = config.pad_up_bitrate_bps;
    if (config.enforce_min_bitrate) {
      total_requested_min_bitrate += config.min_bitrate_bps;
    } else if (config.allocated_bitrate_bps == 0) {
      stream_padding =
          std::max(MinBitrateWithHysteresis(config), stream_padding);
    }
    total_requested_padding_bitrate += stream_padding;
  }

  if (total_requested_padding_bitrate == total_requested_padding_bitrate_ &&
      total_requested_min_bitrate == total_requested_min_bitrate_) {
    return;
  }

  total_requested_min_bitrate_ = total_requested_min_bitrate;
  total_requested_padding_bitrate_ = total_requested_padding_bitrate;

  LOG(LS_INFO) << "UpdateAllocationLimits : total_requested_min_bitrate: "
               << total_requested_min_bitrate
               << "bps, total_requested_padding_bitrate: "
               << total_requested_padding_bitrate << "bps";
  limit_observer_->OnAllocationLimitsChanged(total_requested_min_bitrate,
                                             total_requested_padding_bitrate);
}

void BitrateAllocator::RemoveObserver(BitrateAllocatorObserver* observer) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  auto it = FindObserverConfig(observer);
  if (it != bitrate_observer_configs_.end()) {
    bitrate_observer_configs_.erase(it);
  }

  UpdateAllocationLimits();
}

int BitrateAllocator::GetStartBitrate(BitrateAllocatorObserver* observer) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  const auto& it = FindObserverConfig(observer);
  if (it == bitrate_observer_configs_.end()) {
    // This observer hasn't been added yet, just give it its fair share.
    return last_non_zero_bitrate_bps_ /
           static_cast<int>((bitrate_observer_configs_.size() + 1));
  } else if (it->allocated_bitrate_bps == -1) {
    // This observer hasn't received an allocation yet, so do the same.
    return last_non_zero_bitrate_bps_ /
           static_cast<int>(bitrate_observer_configs_.size());
  } else {
    // This observer already has an allocation.
    return it->allocated_bitrate_bps;
  }
}

BitrateAllocator::ObserverConfigs::iterator
BitrateAllocator::FindObserverConfig(const BitrateAllocatorObserver* observer) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  for (auto it = bitrate_observer_configs_.begin();
       it != bitrate_observer_configs_.end(); ++it) {
    if (it->observer == observer)
      return it;
  }
  return bitrate_observer_configs_.end();
}

BitrateAllocator::ObserverAllocation BitrateAllocator::AllocateBitrates(
    uint32_t bitrate) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  if (bitrate_observer_configs_.empty())
    return ObserverAllocation();

  if (bitrate == 0)
    return ZeroRateAllocation();

  uint32_t sum_min_bitrates = 0;
  uint32_t sum_max_bitrates = 0;
  for (const auto& observer_config : bitrate_observer_configs_) {
    sum_min_bitrates += observer_config.min_bitrate_bps;
    sum_max_bitrates += observer_config.max_bitrate_bps;
  }

  // Not enough for all observers to get an allocation, allocate according to:
  // enforced min bitrate -> allocated bitrate previous round -> restart paused
  // streams.
  if (!EnoughBitrateForAllObservers(bitrate, sum_min_bitrates))
    return LowRateAllocation(bitrate);

  // All observers will get their min bitrate plus an even share of the rest.
  if (bitrate <= sum_max_bitrates)
    return NormalRateAllocation(bitrate, sum_min_bitrates);

  // All observers will get up to kTransmissionMaxBitrateMultiplier x max.
  return MaxRateAllocation(bitrate, sum_max_bitrates);
}

BitrateAllocator::ObserverAllocation BitrateAllocator::ZeroRateAllocation() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  ObserverAllocation allocation;
  for (const auto& observer_config : bitrate_observer_configs_)
    allocation[observer_config.observer] = 0;
  return allocation;
}

BitrateAllocator::ObserverAllocation BitrateAllocator::LowRateAllocation(
    uint32_t bitrate) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  ObserverAllocation allocation;
  // Start by allocating bitrate to observers enforcing a min bitrate, hence
  // remaining_bitrate might turn negative.
  int64_t remaining_bitrate = bitrate;
  for (const auto& observer_config : bitrate_observer_configs_) {
    int32_t allocated_bitrate = 0;
    if (observer_config.enforce_min_bitrate)
      allocated_bitrate = observer_config.min_bitrate_bps;

    allocation[observer_config.observer] = allocated_bitrate;
    remaining_bitrate -= allocated_bitrate;
  }

  // Allocate bitrate to all previously active streams.
  if (remaining_bitrate > 0) {
    for (const auto& observer_config : bitrate_observer_configs_) {
      if (observer_config.enforce_min_bitrate ||
          LastAllocatedBitrate(observer_config) == 0)
        continue;

      uint32_t required_bitrate = MinBitrateWithHysteresis(observer_config);
      if (remaining_bitrate >= required_bitrate) {
        allocation[observer_config.observer] = required_bitrate;
        remaining_bitrate -= required_bitrate;
      }
    }
  }

  // Allocate bitrate to previously paused streams.
  if (remaining_bitrate > 0) {
    for (const auto& observer_config : bitrate_observer_configs_) {
      if (LastAllocatedBitrate(observer_config) != 0)
        continue;

      // Add a hysteresis to avoid toggling.
      uint32_t required_bitrate = MinBitrateWithHysteresis(observer_config);
      if (remaining_bitrate >= required_bitrate) {
        allocation[observer_config.observer] = required_bitrate;
        remaining_bitrate -= required_bitrate;
      }
    }
  }

  // Split a possible remainder evenly on all streams with an allocation.
  if (remaining_bitrate > 0)
    DistributeBitrateEvenly(remaining_bitrate, false, 1, &allocation);

  RTC_DCHECK_EQ(allocation.size(), bitrate_observer_configs_.size());
  return allocation;
}

BitrateAllocator::ObserverAllocation BitrateAllocator::NormalRateAllocation(
    uint32_t bitrate,
    uint32_t sum_min_bitrates) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  ObserverAllocation allocation;
  for (const auto& observer_config : bitrate_observer_configs_)
    allocation[observer_config.observer] = observer_config.min_bitrate_bps;

  bitrate -= sum_min_bitrates;
  if (bitrate > 0)
    DistributeBitrateEvenly(bitrate, true, 1, &allocation);

  return allocation;
}

BitrateAllocator::ObserverAllocation BitrateAllocator::MaxRateAllocation(
    uint32_t bitrate,
    uint32_t sum_max_bitrates) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  ObserverAllocation allocation;

  for (const auto& observer_config : bitrate_observer_configs_) {
    allocation[observer_config.observer] = observer_config.max_bitrate_bps;
    bitrate -= observer_config.max_bitrate_bps;
  }
  DistributeBitrateEvenly(bitrate, true, kTransmissionMaxBitrateMultiplier,
                          &allocation);
  return allocation;
}

uint32_t BitrateAllocator::LastAllocatedBitrate(
    const ObserverConfig& observer_config) {
  // Return the configured minimum bitrate for newly added observers, to avoid
  // requiring an extra high bitrate for the observer to get an allocated
  // bitrate.
  return observer_config.allocated_bitrate_bps == -1
             ? observer_config.min_bitrate_bps
             : observer_config.allocated_bitrate_bps;
}

uint32_t BitrateAllocator::MinBitrateWithHysteresis(
    const ObserverConfig& observer_config) {
  uint32_t min_bitrate = observer_config.min_bitrate_bps;
  if (LastAllocatedBitrate(observer_config) == 0) {
    min_bitrate += std::max(static_cast<uint32_t>(kToggleFactor * min_bitrate),
                            kMinToggleBitrateBps);
  }
  // Account for protection bitrate used by this observer in the previous
  // allocation.
  // Note: the ratio will only be updated when the stream is active, meaning a
  // paused stream won't get any ratio updates. This might lead to waiting a bit
  // longer than necessary if the network condition improves, but this is to
  // avoid too much toggling.
  if (observer_config.media_ratio > 0.0 && observer_config.media_ratio < 1.0)
    min_bitrate += min_bitrate * (1.0 - observer_config.media_ratio);

  return min_bitrate;
}

void BitrateAllocator::DistributeBitrateEvenly(uint32_t bitrate,
                                               bool include_zero_allocations,
                                               int max_multiplier,
                                               ObserverAllocation* allocation) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  RTC_DCHECK_EQ(allocation->size(), bitrate_observer_configs_.size());

  ObserverSortingMap list_max_bitrates;
  for (const auto& observer_config : bitrate_observer_configs_) {
    if (include_zero_allocations ||
        allocation->at(observer_config.observer) != 0) {
      list_max_bitrates.insert(std::pair<uint32_t, const ObserverConfig*>(
          observer_config.max_bitrate_bps, &observer_config));
    }
  }
  auto it = list_max_bitrates.begin();
  while (it != list_max_bitrates.end()) {
    RTC_DCHECK_GT(bitrate, 0);
    uint32_t extra_allocation =
        bitrate / static_cast<uint32_t>(list_max_bitrates.size());
    uint32_t total_allocation =
        extra_allocation + allocation->at(it->second->observer);
    bitrate -= extra_allocation;
    if (total_allocation > max_multiplier * it->first) {
      // There is more than we can fit for this observer, carry over to the
      // remaining observers.
      bitrate += total_allocation - max_multiplier * it->first;
      total_allocation = max_multiplier * it->first;
    }
    // Finally, update the allocation for this observer.
    allocation->at(it->second->observer) = total_allocation;
    it = list_max_bitrates.erase(it);
  }
}

bool BitrateAllocator::EnoughBitrateForAllObservers(uint32_t bitrate,
                                                    uint32_t sum_min_bitrates) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  if (bitrate < sum_min_bitrates)
    return false;

  uint32_t extra_bitrate_per_observer =
      (bitrate - sum_min_bitrates) /
      static_cast<uint32_t>(bitrate_observer_configs_.size());
  for (const auto& observer_config : bitrate_observer_configs_) {
    if (observer_config.min_bitrate_bps + extra_bitrate_per_observer <
        MinBitrateWithHysteresis(observer_config)) {
      return false;
    }
  }
  return true;
}
}  // namespace webrtc
