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
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/call/bitrate_allocation.h"
#include "api/field_trials_view.h"
#include "api/sequence_checker.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {
using bitrate_allocator_impl::AllocatableTrack;

// Allow packets to be transmitted in up to 2 times max video bitrate if the
// bandwidth estimate allows it.
const uint8_t kTransmissionMaxBitrateMultiplier = 2;
const int kDefaultBitrateBps = 300000;

// Require a bitrate increase of max(10%, 20kbps) to resume paused streams.
const double kToggleFactor = 0.1;
const uint32_t kMinToggleBitrateBps = 20000;

const int64_t kBweLogIntervalMs = 5000;

double MediaRatio(uint32_t allocated_bitrate, uint32_t protection_bitrate) {
  RTC_DCHECK_GT(allocated_bitrate, 0);
  if (protection_bitrate == 0)
    return 1.0;

  uint32_t media_bitrate = allocated_bitrate - protection_bitrate;
  return media_bitrate / static_cast<double>(allocated_bitrate);
}

bool EnoughBitrateForAllObservers(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate,
    uint32_t sum_min_bitrates) {
  if (bitrate < sum_min_bitrates)
    return false;

  uint32_t extra_bitrate_per_observer =
      (bitrate - sum_min_bitrates) /
      static_cast<uint32_t>(allocatable_tracks.size());
  for (const auto& observer_config : allocatable_tracks) {
    if (observer_config.config.min_bitrate_bps + extra_bitrate_per_observer <
        observer_config.MinBitrateWithHysteresis()) {
      return false;
    }
  }
  return true;
}

// Splits `bitrate` evenly to observers already in `allocation`.
// `include_zero_allocations` decides if zero allocations should be part of
// the distribution or not. The allowed max bitrate is `max_multiplier` x
// observer max bitrate.
void DistributeBitrateEvenly(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate,
    bool include_zero_allocations,
    int max_multiplier,
    std::map<BitrateAllocatorObserver*, int>* allocation) {
  RTC_DCHECK_EQ(allocation->size(), allocatable_tracks.size());

  std::multimap<uint32_t, const AllocatableTrack*> list_max_bitrates;
  for (const auto& observer_config : allocatable_tracks) {
    if (include_zero_allocations ||
        allocation->at(observer_config.observer) != 0) {
      list_max_bitrates.insert(
          {observer_config.config.max_bitrate_bps, &observer_config});
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

// From the available `bitrate`, each observer will be allocated a
// proportional amount based upon its bitrate priority. If that amount is
// more than the observer's capacity, it will be allocated its capacity, and
// the excess bitrate is still allocated proportionally to other observers.
// Allocating the proportional amount means an observer with twice the
// bitrate_priority of another will be allocated twice the bitrate.
void DistributeBitrateRelatively(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t remaining_bitrate,
    const std::map<BitrateAllocatorObserver*, int>& observers_capacities,
    std::map<BitrateAllocatorObserver*, int>* allocation) {
  RTC_DCHECK_EQ(allocation->size(), allocatable_tracks.size());
  RTC_DCHECK_EQ(observers_capacities.size(), allocatable_tracks.size());

  struct PriorityRateObserverConfig {
    BitrateAllocatorObserver* allocation_key;
    // The amount of bitrate bps that can be allocated to this observer.
    int capacity_bps;
    double bitrate_priority;
  };

  double bitrate_priority_sum = 0;
  std::vector<PriorityRateObserverConfig> priority_rate_observers;
  for (const auto& observer_config : allocatable_tracks) {
    priority_rate_observers.push_back(PriorityRateObserverConfig{
        observer_config.observer,
        observers_capacities.at(observer_config.observer),
        observer_config.config.bitrate_priority});
    bitrate_priority_sum += observer_config.config.bitrate_priority;
  }

  // Iterate in the order observers can be allocated their full capacity.

  // We want to sort by which observers will be allocated their full capacity
  // first. By dividing each observer's capacity by its bitrate priority we
  // are "normalizing" the capacity of an observer by the rate it will be
  // filled. This is because the amount allocated is based upon bitrate
  // priority. We allocate twice as much bitrate to an observer with twice the
  // bitrate priority of another.
  absl::c_sort(priority_rate_observers, [](const auto& a, const auto& b) {
    return a.capacity_bps / a.bitrate_priority <
           b.capacity_bps / b.bitrate_priority;
  });
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

// Allocates bitrate to observers when there isn't enough to allocate the
// minimum to all observers.
std::map<BitrateAllocatorObserver*, int> LowRateAllocation(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate) {
  std::map<BitrateAllocatorObserver*, int> allocation;
  // Start by allocating bitrate to observers enforcing a min bitrate, hence
  // remaining_bitrate might turn negative.
  int64_t remaining_bitrate = bitrate;
  for (const auto& observer_config : allocatable_tracks) {
    int32_t allocated_bitrate = 0;
    if (observer_config.config.enforce_min_bitrate)
      allocated_bitrate = observer_config.config.min_bitrate_bps;

    allocation[observer_config.observer] = allocated_bitrate;
    remaining_bitrate -= allocated_bitrate;
  }

  // Allocate bitrate to all previously active streams.
  if (remaining_bitrate > 0) {
    for (const auto& observer_config : allocatable_tracks) {
      if (observer_config.config.enforce_min_bitrate ||
          observer_config.LastAllocatedBitrate() == 0)
        continue;

      uint32_t required_bitrate = observer_config.MinBitrateWithHysteresis();
      if (remaining_bitrate >= required_bitrate) {
        allocation[observer_config.observer] = required_bitrate;
        remaining_bitrate -= required_bitrate;
      }
    }
  }

  // Allocate bitrate to previously paused streams.
  if (remaining_bitrate > 0) {
    for (const auto& observer_config : allocatable_tracks) {
      if (observer_config.LastAllocatedBitrate() != 0)
        continue;

      // Add a hysteresis to avoid toggling.
      uint32_t required_bitrate = observer_config.MinBitrateWithHysteresis();
      if (remaining_bitrate >= required_bitrate) {
        allocation[observer_config.observer] = required_bitrate;
        remaining_bitrate -= required_bitrate;
      }
    }
  }

  // Split a possible remainder evenly on all streams with an allocation.
  if (remaining_bitrate > 0)
    DistributeBitrateEvenly(allocatable_tracks, remaining_bitrate, false, 1,
                            &allocation);

  RTC_DCHECK_EQ(allocation.size(), allocatable_tracks.size());
  return allocation;
}

// Allocates bitrate to all observers when the available bandwidth is enough
// to allocate the minimum to all observers but not enough to allocate the
// max bitrate of each observer.

// Allocates the bitrate based on the bitrate priority of each observer. This
// bitrate priority defines the priority for bitrate to be allocated to that
// observer in relation to other observers. For example with two observers, if
// observer 1 had a bitrate_priority = 1.0, and observer 2 has a
// bitrate_priority = 2.0, the expected behavior is that observer 2 will be
// allocated twice the bitrate as observer 1 above the each observer's
// min_bitrate_bps values, until one of the observers hits its max_bitrate_bps.
std::map<BitrateAllocatorObserver*, int> NormalRateAllocation(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate,
    uint32_t sum_min_bitrates) {
  std::map<BitrateAllocatorObserver*, int> allocation;
  std::map<BitrateAllocatorObserver*, int> observers_capacities;
  for (const auto& observer_config : allocatable_tracks) {
    allocation[observer_config.observer] =
        observer_config.config.min_bitrate_bps;
    observers_capacities[observer_config.observer] =
        observer_config.config.max_bitrate_bps -
        observer_config.config.min_bitrate_bps;
  }

  bitrate -= sum_min_bitrates;

  // TODO(srte): Implement fair sharing between prioritized streams, currently
  // they are treated on a first come first serve basis.
  for (const auto& observer_config : allocatable_tracks) {
    int64_t priority_margin = observer_config.config.priority_bitrate_bps -
                              allocation[observer_config.observer];
    if (priority_margin > 0 && bitrate > 0) {
      int64_t extra_bitrate = std::min<int64_t>(priority_margin, bitrate);
      allocation[observer_config.observer] +=
          rtc::dchecked_cast<int>(extra_bitrate);
      observers_capacities[observer_config.observer] -= extra_bitrate;
      bitrate -= extra_bitrate;
    }
  }

  // From the remaining bitrate, allocate a proportional amount to each observer
  // above the min bitrate already allocated.
  if (bitrate > 0)
    DistributeBitrateRelatively(allocatable_tracks, bitrate,
                                observers_capacities, &allocation);

  return allocation;
}

// Allocates bitrate to observers when there is enough available bandwidth
// for all observers to be allocated their max bitrate.
std::map<BitrateAllocatorObserver*, int> MaxRateAllocation(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate,
    uint32_t /* sum_max_bitrates */) {
  std::map<BitrateAllocatorObserver*, int> allocation;

  for (const auto& observer_config : allocatable_tracks) {
    allocation[observer_config.observer] =
        observer_config.config.max_bitrate_bps;
    bitrate -= observer_config.config.max_bitrate_bps;
  }
  DistributeBitrateEvenly(allocatable_tracks, bitrate, true,
                          kTransmissionMaxBitrateMultiplier, &allocation);
  return allocation;
}

// Allocates zero bitrate to all observers.
std::map<BitrateAllocatorObserver*, int> ZeroRateAllocation(
    const std::vector<AllocatableTrack>& allocatable_tracks) {
  std::map<BitrateAllocatorObserver*, int> allocation;
  for (const auto& observer_config : allocatable_tracks)
    allocation[observer_config.observer] = 0;
  return allocation;
}

// Returns new allocation if modified, std::nullopt otherwise.
std::optional<std::map<BitrateAllocatorObserver*, int>> MaybeApplySurplus(
    const std::map<BitrateAllocatorObserver*, int>& allocation,
    const std::vector<AllocatableTrack>& allocatable_tracks,
    DataRate bitrate,
    DataRate upper_elastic_limit) {
  if (upper_elastic_limit.IsZero())
    return std::nullopt;

  // In this first pass looping over all `allocatable_tracks`, we aggregates
  // - `surplus`: sum of unused rates for all kCanContribute* tracks,
  // - `sum_demand`: sum of `bitrate_priority` for all tracks that can consume
  //    more bitrate to allow proportional sharing of surplus later,
  // - `sum_allocated`: sum of allocated bitrates for all tracks, which might
  //    be larger than `bitrate` e.g. when min_bitrate_bps are enforced.
  DataRate surplus = DataRate::Zero();
  double sum_demand = 0.0;
  DataRate sum_allocated = DataRate::Zero();

  for (const auto& observer_config : allocatable_tracks) {
    const auto it = allocation.find(observer_config.observer);
    if (it == allocation.end()) {
      // No allocation for this track.
      continue;
    }
    const DataRate allocated = DataRate::BitsPerSec(it->second);
    sum_allocated += allocated;
    if (const std::optional<TrackRateElasticity> elasticity =
            observer_config.config.rate_elasticity) {
      bool inactive_can_contribute_and_consume = false;
      if (elasticity == TrackRateElasticity::kCanContributeUnusedRate ||
          elasticity == TrackRateElasticity::kCanContributeAndConsume) {
        if (const std::optional<DataRate> used =
                observer_config.observer->GetUsedRate()) {
          if (*used < allocated) {
            surplus += allocated - *used;
            if (elasticity == TrackRateElasticity::kCanContributeAndConsume &&
                *used < allocated / 2) {
              inactive_can_contribute_and_consume = true;
            }
          }
        }
      }
      if (!inactive_can_contribute_and_consume &&
          (elasticity == TrackRateElasticity::kCanConsumeExtraRate ||
           elasticity == TrackRateElasticity::kCanContributeAndConsume)) {
        sum_demand += observer_config.config.bitrate_priority;
      }
    }
  }

  // `sum_allocated` can exceed `bitrate` if sum minBitrates exceeds
  // estimated rate. The real `surplus` should cover the difference.
  DataRate overshoot =
      (sum_allocated >= bitrate) ? (sum_allocated - bitrate) : DataRate::Zero();
  if (sum_demand < 0.0001 || overshoot > surplus) {
    // No demand for extra bitrate or no available surplus.
    return std::nullopt;
  }
  surplus -= overshoot;

  auto new_allocation = allocation;
  // We loop over all allocatable_tracks again, and proportionally assign
  // `surplus` to each track according to `bitrate_priority`.
  for (const auto& observer_config : allocatable_tracks) {
    auto it = new_allocation.find(observer_config.observer);
    if (it == new_allocation.end()) {
      // No allocation for this track.
      continue;
    }
    std::optional<TrackRateElasticity> elasticity =
        observer_config.config.rate_elasticity;
    if (elasticity == TrackRateElasticity::kCanConsumeExtraRate ||
        elasticity == TrackRateElasticity::kCanContributeAndConsume) {
      DataRate allocated = DataRate::BitsPerSec(it->second);
      if (allocated < upper_elastic_limit) {
        allocated +=
            surplus * (observer_config.config.bitrate_priority / sum_demand);
        if (allocated > upper_elastic_limit)
          allocated = upper_elastic_limit;
      }
      DataRate max_bitrate =
          DataRate::BitsPerSec(observer_config.config.max_bitrate_bps);
      if (allocated > max_bitrate) {
        allocated = max_bitrate;
      }
      // Save new allocated rate back to `new_allocation`.
      it->second = allocated.bps();
    }
  }
  return new_allocation;
}

std::map<BitrateAllocatorObserver*, int> AllocateBitrates(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate,
    DataRate upper_elastic_limit) {
  if (allocatable_tracks.empty())
    return std::map<BitrateAllocatorObserver*, int>();

  if (bitrate == 0)
    return ZeroRateAllocation(allocatable_tracks);

  uint32_t sum_min_bitrates = 0;
  uint32_t sum_max_bitrates = 0;
  for (const auto& observer_config : allocatable_tracks) {
    sum_min_bitrates += observer_config.config.min_bitrate_bps;
    sum_max_bitrates += observer_config.config.max_bitrate_bps;
  }

  // Not enough for all observers to get an allocation, allocate according to:
  // enforced min bitrate -> allocated bitrate previous round -> restart paused
  // streams.
  if (!EnoughBitrateForAllObservers(allocatable_tracks, bitrate,
                                    sum_min_bitrates))
    return LowRateAllocation(allocatable_tracks, bitrate);

  // All observers will get their min bitrate plus a share of the rest. This
  // share is allocated to each observer based on its bitrate_priority.
  if (bitrate <= sum_max_bitrates) {
    auto allocation =
        NormalRateAllocation(allocatable_tracks, bitrate, sum_min_bitrates);
    return MaybeApplySurplus(allocation, allocatable_tracks,
                             DataRate::BitsPerSec(bitrate), upper_elastic_limit)
        .value_or(allocation);
  }

  // All observers will get up to transmission_max_bitrate_multiplier_ x max.
  return MaxRateAllocation(allocatable_tracks, bitrate, sum_max_bitrates);
}

}  // namespace

BitrateAllocator::BitrateAllocator(LimitObserver* limit_observer,
                                   DataRate upper_elastic_rate_limit)
    : limit_observer_(limit_observer),
      last_target_bps_(0),
      last_stable_target_bps_(0),
      last_non_zero_bitrate_bps_(kDefaultBitrateBps),
      last_fraction_loss_(0),
      last_rtt_(0),
      last_bwe_period_ms_(1000),
      num_pause_events_(0),
      last_bwe_log_time_(0),
      upper_elastic_rate_limit_(upper_elastic_rate_limit) {
  sequenced_checker_.Detach();
}

BitrateAllocator::~BitrateAllocator() {
  RTC_HISTOGRAM_COUNTS_100("WebRTC.Call.NumberOfPauseEvents",
                           num_pause_events_);
}

void BitrateAllocator::UpdateStartRate(uint32_t start_rate_bps) {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  last_non_zero_bitrate_bps_ = start_rate_bps;
}

void BitrateAllocator::OnNetworkEstimateChanged(TargetTransferRate msg) {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  last_target_bps_ = msg.target_rate.bps();
  last_stable_target_bps_ = msg.stable_target_rate.bps();
  last_non_zero_bitrate_bps_ =
      last_target_bps_ > 0 ? last_target_bps_ : last_non_zero_bitrate_bps_;

  int loss_ratio_255 = msg.network_estimate.loss_rate_ratio * 255;
  last_fraction_loss_ =
      rtc::dchecked_cast<uint8_t>(rtc::SafeClamp(loss_ratio_255, 0, 255));
  last_rtt_ = msg.network_estimate.round_trip_time.ms();
  last_bwe_period_ms_ = msg.network_estimate.bwe_period.ms();

  // Periodically log the incoming BWE.
  int64_t now = msg.at_time.ms();
  if (now > last_bwe_log_time_ + kBweLogIntervalMs) {
    RTC_LOG(LS_INFO) << "Current BWE " << last_target_bps_;
    last_bwe_log_time_ = now;
  }

  auto allocation = AllocateBitrates(allocatable_tracks_, last_target_bps_,
                                     upper_elastic_rate_limit_);
  auto stable_bitrate_allocation = AllocateBitrates(
      allocatable_tracks_, last_stable_target_bps_, DataRate::Zero());

  for (auto& config : allocatable_tracks_) {
    uint32_t allocated_bitrate = allocation[config.observer];
    uint32_t allocated_stable_target_rate =
        stable_bitrate_allocation[config.observer];
    BitrateAllocationUpdate update;
    update.target_bitrate = DataRate::BitsPerSec(allocated_bitrate);
    update.stable_target_bitrate =
        DataRate::BitsPerSec(allocated_stable_target_rate);
    update.packet_loss_ratio = last_fraction_loss_ / 256.0;
    update.round_trip_time = TimeDelta::Millis(last_rtt_);
    update.bwe_period = TimeDelta::Millis(last_bwe_period_ms_);
    update.cwnd_reduce_ratio = msg.cwnd_reduce_ratio;
    uint32_t protection_bitrate = config.observer->OnBitrateUpdated(update);

    if (allocated_bitrate == 0 && config.allocated_bitrate_bps > 0) {
      if (last_target_bps_ > 0)
        ++num_pause_events_;
      // The protection bitrate is an estimate based on the ratio between media
      // and protection used before this observer was muted.
      uint32_t predicted_protection_bps =
          (1.0 - config.media_ratio) * config.config.min_bitrate_bps;
      RTC_LOG(LS_INFO) << "Pausing observer " << config.observer
                       << " with configured min bitrate "
                       << config.config.min_bitrate_bps
                       << " and current estimate of " << last_target_bps_
                       << " and protection bitrate "
                       << predicted_protection_bps;
    } else if (allocated_bitrate > 0 && config.allocated_bitrate_bps == 0) {
      if (last_target_bps_ > 0)
        ++num_pause_events_;
      RTC_LOG(LS_INFO) << "Resuming observer " << config.observer
                       << ", configured min bitrate "
                       << config.config.min_bitrate_bps
                       << ", current allocation " << allocated_bitrate
                       << " and protection bitrate " << protection_bitrate;
    }

    // Only update the media ratio if the observer got an allocation.
    if (allocated_bitrate > 0)
      config.media_ratio = MediaRatio(allocated_bitrate, protection_bitrate);
    config.allocated_bitrate_bps = allocated_bitrate;
    config.last_used_bitrate = config.observer->GetUsedRate();
  }
  UpdateAllocationLimits();
}

void BitrateAllocator::AddObserver(BitrateAllocatorObserver* observer,
                                   MediaStreamAllocationConfig config) {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  RTC_DCHECK_GT(config.bitrate_priority, 0);
  RTC_DCHECK(std::isnormal(config.bitrate_priority));
  auto it = absl::c_find_if(
      allocatable_tracks_,
      [observer](const auto& config) { return config.observer == observer; });
  // Update settings if the observer already exists, create a new one otherwise.
  if (it != allocatable_tracks_.end()) {
    it->config = config;
  } else {
    allocatable_tracks_.push_back(AllocatableTrack(observer, config));
  }

  if (last_target_bps_ > 0) {
    // Calculate a new allocation and update all observers.

    auto allocation = AllocateBitrates(allocatable_tracks_, last_target_bps_,
                                       upper_elastic_rate_limit_);
    auto stable_bitrate_allocation = AllocateBitrates(
        allocatable_tracks_, last_stable_target_bps_, DataRate::Zero());
    for (auto& config : allocatable_tracks_) {
      uint32_t allocated_bitrate = allocation[config.observer];
      uint32_t allocated_stable_bitrate =
          stable_bitrate_allocation[config.observer];
      BitrateAllocationUpdate update;
      update.target_bitrate = DataRate::BitsPerSec(allocated_bitrate);
      update.stable_target_bitrate =
          DataRate::BitsPerSec(allocated_stable_bitrate);
      update.packet_loss_ratio = last_fraction_loss_ / 256.0;
      update.round_trip_time = TimeDelta::Millis(last_rtt_);
      update.bwe_period = TimeDelta::Millis(last_bwe_period_ms_);
      uint32_t protection_bitrate = config.observer->OnBitrateUpdated(update);
      config.allocated_bitrate_bps = allocated_bitrate;
      config.last_used_bitrate = config.observer->GetUsedRate();
      if (allocated_bitrate > 0)
        config.media_ratio = MediaRatio(allocated_bitrate, protection_bitrate);
    }
  } else {
    // Currently, an encoder is not allowed to produce frames.
    // But we still have to return the initial config bitrate + let the
    // observer know that it can not produce frames.

    BitrateAllocationUpdate update;
    update.target_bitrate = DataRate::Zero();
    update.stable_target_bitrate = DataRate::Zero();
    update.packet_loss_ratio = last_fraction_loss_ / 256.0;
    update.round_trip_time = TimeDelta::Millis(last_rtt_);
    update.bwe_period = TimeDelta::Millis(last_bwe_period_ms_);
    observer->OnBitrateUpdated(update);
  }
  UpdateAllocationLimits();
}

bool BitrateAllocator::RecomputeAllocationIfNeeded() {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);

  if (upper_elastic_rate_limit_.IsZero()) {
    return false;
  }

  bool need_recompute = false;
  bool has_contributor = false;
  bool has_consumer = false;

  // Recomputes if there is a kCanContribute* track whose current bitrate usage
  // has a jump (i.e., increase only) larger than 20% of allocated_bitrate.
  constexpr double kUsageJumpRatioThreshold = 0.2;
  for (auto& config : allocatable_tracks_) {
    if (config.config.rate_elasticity.has_value()) {
      const TrackRateElasticity elasticity = *config.config.rate_elasticity;
      if (elasticity == TrackRateElasticity::kCanContributeUnusedRate ||
          elasticity == TrackRateElasticity::kCanContributeAndConsume) {
        DataRate current_usage =
            config.observer->GetUsedRate().value_or(DataRate::Zero());
        DataRate last_usage =
            config.last_used_bitrate.value_or(DataRate::Zero());
        if (!last_usage.IsZero()) {
          has_contributor = true;
          DataRate recompute_threshold =
              DataRate::BitsPerSec(config.LastAllocatedBitrate()) *
              kUsageJumpRatioThreshold;
          if (current_usage > last_usage + recompute_threshold) {
            need_recompute = true;
          }
        }
      }
      if (elasticity == TrackRateElasticity::kCanConsumeExtraRate ||
          elasticity == TrackRateElasticity::kCanContributeAndConsume) {
        has_consumer = true;
      }
    }
  }
  if (has_contributor == false || has_consumer == false)
    return false;

  if (need_recompute && last_target_bps_ > 0) {
    // Calculate a new allocation and update all observers.
    auto allocation = AllocateBitrates(allocatable_tracks_, last_target_bps_,
                                       upper_elastic_rate_limit_);
    auto stable_bitrate_allocation = AllocateBitrates(
        allocatable_tracks_, last_stable_target_bps_, DataRate::Zero());
    for (auto& config : allocatable_tracks_) {
      DataRate allocated_bitrate =
          DataRate::BitsPerSec(allocation[config.observer]);
      DataRate allocated_stable_bitrate =
          DataRate::BitsPerSec(stable_bitrate_allocation[config.observer]);
      BitrateAllocationUpdate update;
      update.target_bitrate = allocated_bitrate;
      update.stable_target_bitrate = allocated_stable_bitrate;
      update.packet_loss_ratio = last_fraction_loss_ / 256.0;
      update.round_trip_time = TimeDelta::Millis(last_rtt_);
      update.bwe_period = TimeDelta::Millis(last_bwe_period_ms_);
      DataRate protection_bitrate =
          DataRate::BitsPerSec(config.observer->OnBitrateUpdated(update));
      config.allocated_bitrate_bps = allocated_bitrate.bps();
      config.last_used_bitrate = config.observer->GetUsedRate();
      if (allocated_bitrate.bps() > 0)
        config.media_ratio =
            MediaRatio(allocated_bitrate.bps(), protection_bitrate.bps());
    }
    UpdateAllocationLimits();
  }
  return true;
}

void BitrateAllocator::UpdateAllocationLimits() {
  BitrateAllocationLimits limits;
  for (const auto& config : allocatable_tracks_) {
    uint32_t stream_padding = config.config.pad_up_bitrate_bps;
    if (config.config.enforce_min_bitrate) {
      limits.min_allocatable_rate +=
          DataRate::BitsPerSec(config.config.min_bitrate_bps);
    } else if (config.allocated_bitrate_bps == 0) {
      stream_padding =
          std::max(config.MinBitrateWithHysteresis(), stream_padding);
    }
    limits.max_padding_rate += DataRate::BitsPerSec(stream_padding);
    limits.max_allocatable_rate +=
        DataRate::BitsPerSec(config.config.max_bitrate_bps);
  }

  if (limits.min_allocatable_rate == current_limits_.min_allocatable_rate &&
      limits.max_allocatable_rate == current_limits_.max_allocatable_rate &&
      limits.max_padding_rate == current_limits_.max_padding_rate) {
    return;
  }
  current_limits_ = limits;

  RTC_LOG(LS_INFO) << "UpdateAllocationLimits : total_requested_min_bitrate: "
                   << ToString(limits.min_allocatable_rate)
                   << ", total_requested_padding_bitrate: "
                   << ToString(limits.max_padding_rate)
                   << ", total_requested_max_bitrate: "
                   << ToString(limits.max_allocatable_rate);

  limit_observer_->OnAllocationLimitsChanged(limits);
}

void BitrateAllocator::RemoveObserver(BitrateAllocatorObserver* observer) {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  for (auto it = allocatable_tracks_.begin(); it != allocatable_tracks_.end();
       ++it) {
    if (it->observer == observer) {
      allocatable_tracks_.erase(it);
      break;
    }
  }

  UpdateAllocationLimits();
}

int BitrateAllocator::GetStartBitrate(
    BitrateAllocatorObserver* observer) const {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  auto it = absl::c_find_if(
      allocatable_tracks_,
      [observer](const auto& config) { return config.observer == observer; });
  if (it == allocatable_tracks_.end()) {
    // This observer hasn't been added yet, just give it its fair share.
    return last_non_zero_bitrate_bps_ /
           static_cast<int>((allocatable_tracks_.size() + 1));
  } else if (it->allocated_bitrate_bps == -1) {
    // This observer hasn't received an allocation yet, so do the same.
    return last_non_zero_bitrate_bps_ /
           static_cast<int>(allocatable_tracks_.size());
  } else {
    // This observer already has an allocation.
    return it->allocated_bitrate_bps;
  }
}

uint32_t bitrate_allocator_impl::AllocatableTrack::LastAllocatedBitrate()
    const {
  // Return the configured minimum bitrate for newly added observers, to avoid
  // requiring an extra high bitrate for the observer to get an allocated
  // bitrate.
  return allocated_bitrate_bps == -1 ? config.min_bitrate_bps
                                     : allocated_bitrate_bps;
}

uint32_t bitrate_allocator_impl::AllocatableTrack::MinBitrateWithHysteresis()
    const {
  uint32_t min_bitrate = config.min_bitrate_bps;
  if (LastAllocatedBitrate() == 0) {
    min_bitrate += std::max(static_cast<uint32_t>(kToggleFactor * min_bitrate),
                            kMinToggleBitrateBps);
  }
  // Account for protection bitrate used by this observer in the previous
  // allocation.
  // Note: the ratio will only be updated when the stream is active, meaning a
  // paused stream won't get any ratio updates. This might lead to waiting a bit
  // longer than necessary if the network condition improves, but this is to
  // avoid too much toggling.
  if (media_ratio > 0.0 && media_ratio < 1.0)
    min_bitrate += min_bitrate * (1.0 - media_ratio);

  return min_bitrate;
}

// TODO(b/350555527): Remove after experiment
const char kElasticBitrateAllocator[] = "WebRTC-ElasticBitrateAllocation";
DataRate GetElasticRateAllocationFieldTrialParameter(
    const FieldTrialsView& field_trials) {
  FieldTrialParameter<DataRate> elastic_rate_limit("upper_limit",
                                                   DataRate::Zero());
  std::string trial_string = field_trials.Lookup(kElasticBitrateAllocator);
  ParseFieldTrial({&elastic_rate_limit}, trial_string);
  return elastic_rate_limit.Get();
}

}  // namespace webrtc
