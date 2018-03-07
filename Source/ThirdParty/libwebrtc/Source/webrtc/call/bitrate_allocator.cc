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

#include "call/bitrate_allocator.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "modules/bitrate_controller/include/bitrate_controller.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

// Allow packets to be transmitted in up to 2 times max video bitrate if the
// bandwidth estimate allows it.
// TODO(bugs.webrtc.org/8541): May be worth to refactor to keep this logic in
// video send stream. Similar logic is implemented in
// AudioPriorityBitrateAllocationStrategy.
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
      last_bitrate_bps_(0),
      last_non_zero_bitrate_bps_(kDefaultBitrateBps),
      last_fraction_loss_(0),
      last_rtt_(0),
      num_pause_events_(0),
      clock_(Clock::GetRealTimeClock()),
      last_bwe_log_time_(0),
      total_requested_padding_bitrate_(0),
      total_requested_min_bitrate_(0),
      bitrate_allocation_strategy_(nullptr) {
  sequenced_checker_.Detach();
}

BitrateAllocator::~BitrateAllocator() {
  RTC_HISTOGRAM_COUNTS_100("WebRTC.Call.NumberOfPauseEvents",
                           num_pause_events_);
}

void BitrateAllocator::OnNetworkChanged(uint32_t target_bitrate_bps,
                                        uint8_t fraction_loss,
                                        int64_t rtt,
                                        int64_t bwe_period_ms) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  last_bitrate_bps_ = target_bitrate_bps;
  last_non_zero_bitrate_bps_ =
      target_bitrate_bps > 0 ? target_bitrate_bps : last_non_zero_bitrate_bps_;
  last_fraction_loss_ = fraction_loss;
  last_rtt_ = rtt;
  last_bwe_period_ms_ = bwe_period_ms;

  // Periodically log the incoming BWE.
  int64_t now = clock_->TimeInMilliseconds();
  if (now > last_bwe_log_time_ + kBweLogIntervalMs) {
    RTC_LOG(LS_INFO) << "Current BWE " << target_bitrate_bps;
    last_bwe_log_time_ = now;
  }

  ObserverAllocation allocation = AllocateBitrates(target_bitrate_bps);

  for (auto& config : bitrate_observer_configs_) {
    uint32_t allocated_bitrate = allocation[config.observer];
    uint32_t protection_bitrate = config.observer->OnBitrateUpdated(
        allocated_bitrate, last_fraction_loss_, last_rtt_,
        last_bwe_period_ms_);

    if (allocated_bitrate == 0 && config.allocated_bitrate_bps > 0) {
      if (target_bitrate_bps > 0)
        ++num_pause_events_;
      // The protection bitrate is an estimate based on the ratio between media
      // and protection used before this observer was muted.
      uint32_t predicted_protection_bps =
          (1.0 - config.media_ratio) * config.min_bitrate_bps;
      RTC_LOG(LS_INFO) << "Pausing observer " << config.observer
                       << " with configured min bitrate "
                       << config.min_bitrate_bps << " and current estimate of "
                       << target_bitrate_bps << " and protection bitrate "
                       << predicted_protection_bps;
    } else if (allocated_bitrate > 0 && config.allocated_bitrate_bps == 0) {
      if (target_bitrate_bps > 0)
        ++num_pause_events_;
      RTC_LOG(LS_INFO) << "Resuming observer " << config.observer
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
                                   bool enforce_min_bitrate,
                                   std::string track_id,
                                   double bitrate_priority) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  RTC_DCHECK_GT(bitrate_priority, 0);
  RTC_DCHECK(std::isnormal(bitrate_priority));
  auto it = FindObserverConfig(observer);

  // Update settings if the observer already exists, create a new one otherwise.
  if (it != bitrate_observer_configs_.end()) {
    it->min_bitrate_bps = min_bitrate_bps;
    it->max_bitrate_bps = max_bitrate_bps;
    it->pad_up_bitrate_bps = pad_up_bitrate_bps;
    it->enforce_min_bitrate = enforce_min_bitrate;
    it->bitrate_priority = bitrate_priority;
  } else {
    bitrate_observer_configs_.push_back(ObserverConfig(
        observer, min_bitrate_bps, max_bitrate_bps, pad_up_bitrate_bps,
        enforce_min_bitrate, track_id, bitrate_priority));
  }

  ObserverAllocation allocation;
  if (last_bitrate_bps_ > 0) {
    // Calculate a new allocation and update all observers.
    allocation = AllocateBitrates(last_bitrate_bps_);
    for (auto& config : bitrate_observer_configs_) {
      uint32_t allocated_bitrate = allocation[config.observer];
      uint32_t protection_bitrate = config.observer->OnBitrateUpdated(
          allocated_bitrate, last_fraction_loss_, last_rtt_,
          last_bwe_period_ms_);
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
                               last_bwe_period_ms_);
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

  RTC_LOG(LS_INFO) << "UpdateAllocationLimits : total_requested_min_bitrate: "
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

void BitrateAllocator::SetBitrateAllocationStrategy(
    std::unique_ptr<rtc::BitrateAllocationStrategy>
        bitrate_allocation_strategy) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  bitrate_allocation_strategy_ = std::move(bitrate_allocation_strategy);
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

  if (bitrate_allocation_strategy_ != nullptr) {
    std::vector<const rtc::BitrateAllocationStrategy::TrackConfig*>
        track_configs(bitrate_observer_configs_.size());
    int i = 0;
    for (const auto& c : bitrate_observer_configs_) {
      track_configs[i++] = &c;
    }
    std::vector<uint32_t> track_allocations =
        bitrate_allocation_strategy_->AllocateBitrates(bitrate, track_configs);
    // The strategy should return allocation for all tracks.
    RTC_CHECK(track_allocations.size() == bitrate_observer_configs_.size());
    ObserverAllocation allocation;
    auto track_allocations_it = track_allocations.begin();
    for (const auto& observer_config : bitrate_observer_configs_) {
      allocation[observer_config.observer] = *track_allocations_it++;
    }
    return allocation;
  }

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

  // All observers will get their min bitrate plus a share of the rest. This
  // share is allocated to each observer based on its bitrate_priority.
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

// Allocates the bitrate based on the bitrate priority of each observer. This
// bitrate priority defines the priority for bitrate to be allocated to that
// observer in relation to other observers. For example with two observers, if
// observer 1 had a bitrate_priority = 1.0, and observer 2 has a
// bitrate_priority = 2.0, the expected behavior is that observer 2 will be
// allocated twice the bitrate as observer 1 above the each observer's
// min_bitrate_bps values, until one of the observers hits its max_bitrate_bps.
BitrateAllocator::ObserverAllocation BitrateAllocator::NormalRateAllocation(
    uint32_t bitrate,
    uint32_t sum_min_bitrates) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  ObserverAllocation allocation;
  ObserverAllocation observers_capacities;
  for (const auto& observer_config : bitrate_observer_configs_) {
    allocation[observer_config.observer] = observer_config.min_bitrate_bps;
    observers_capacities[observer_config.observer] =
        observer_config.max_bitrate_bps - observer_config.min_bitrate_bps;
  }

  bitrate -= sum_min_bitrates;
  // From the remaining bitrate, allocate a proportional amount to each observer
  // above the min bitrate already allocated.
  if (bitrate > 0)
    DistributeBitrateRelatively(bitrate, observers_capacities, &allocation);

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

void BitrateAllocator::DistributeBitrateRelatively(
    uint32_t remaining_bitrate,
    const ObserverAllocation& observers_capacities,
    ObserverAllocation* allocation) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequenced_checker_);
  RTC_DCHECK_EQ(allocation->size(), bitrate_observer_configs_.size());
  RTC_DCHECK_EQ(observers_capacities.size(), bitrate_observer_configs_.size());

  struct PriorityRateObserverConfig {
    PriorityRateObserverConfig(BitrateAllocatorObserver* allocation_key,
                               uint32_t capacity_bps,
                               double bitrate_priority)
        : allocation_key(allocation_key),
          capacity_bps(capacity_bps),
          bitrate_priority(bitrate_priority) {}

    BitrateAllocatorObserver* allocation_key;
    // The amount of bitrate bps that can be allocated to this observer.
    uint32_t capacity_bps;
    double bitrate_priority;

    // We want to sort by which observers will be allocated their full capacity
    // first. By dividing each observer's capacity by its bitrate priority we
    // are "normalizing" the capacity of an observer by the rate it will be
    // filled. This is because the amount allocated is based upon bitrate
    // priority. We allocate twice as much bitrate to an observer with twice the
    // bitrate priority of another.
    bool operator<(const PriorityRateObserverConfig& other) const {
      return capacity_bps / bitrate_priority <
             other.capacity_bps / other.bitrate_priority;
    }
  };

  double bitrate_priority_sum = 0;
  std::vector<PriorityRateObserverConfig> priority_rate_observers;
  for (const auto& observer_config : bitrate_observer_configs_) {
    uint32_t capacity_bps = observers_capacities.at(observer_config.observer);
    priority_rate_observers.emplace_back(observer_config.observer, capacity_bps,
                                         observer_config.bitrate_priority);
    bitrate_priority_sum += observer_config.bitrate_priority;
  }

  // Iterate in the order observers can be allocated their full capacity.
  std::sort(priority_rate_observers.begin(), priority_rate_observers.end());
  size_t i;
  for (i = 0; i < priority_rate_observers.size(); ++i) {
    const auto& priority_rate_observer = priority_rate_observers[i];
    // We allocate the full capacity to an observer only if its relative
    // portion from the remaining bitrate is sufficient to allocate its full
    // capacity. This means we aren't greedily allocating the full capacity, but
    // that it is only done when there is also enough bitrate to allocate the
    // proportional amounts to all other observers.
    double observer_share =
        priority_rate_observer.bitrate_priority / bitrate_priority_sum;
    double allocation_bps = observer_share * remaining_bitrate;
    bool enough_bitrate = allocation_bps >= priority_rate_observer.capacity_bps;
    if (!enough_bitrate)
      break;
    allocation->at(priority_rate_observer.allocation_key) +=
        priority_rate_observer.capacity_bps;
    remaining_bitrate -= priority_rate_observer.capacity_bps;
    bitrate_priority_sum -= priority_rate_observer.bitrate_priority;
  }

  // From the remaining bitrate, allocate the proportional amounts to the
  // observers that aren't allocated their max capacity.
  for (; i < priority_rate_observers.size(); ++i) {
    const auto& priority_rate_observer = priority_rate_observers[i];
    double fraction_allocated =
        priority_rate_observer.bitrate_priority / bitrate_priority_sum;
    allocation->at(priority_rate_observer.allocation_key) +=
        fraction_allocated * remaining_bitrate;
  }
}

}  // namespace webrtc
