/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_BITRATE_ALLOCATOR_H_
#define WEBRTC_CALL_BITRATE_ALLOCATOR_H_

#include <stdint.h>

#include <map>
#include <utility>
#include <vector>

#include "webrtc/base/sequenced_task_checker.h"

namespace webrtc {

class Clock;

// Used by all send streams with adaptive bitrate, to get the currently
// allocated bitrate for the send stream. The current network properties are
// given at the same time, to let the send stream decide about possible loss
// protection.
class BitrateAllocatorObserver {
 public:
  // Returns the amount of protection used by the BitrateAllocatorObserver
  // implementation, as bitrate in bps.
  virtual uint32_t OnBitrateUpdated(uint32_t bitrate_bps,
                                    uint8_t fraction_loss,
                                    int64_t rtt,
                                    int64_t bwe_period_ms) = 0;

 protected:
  virtual ~BitrateAllocatorObserver() {}
};

// Usage: this class will register multiple RtcpBitrateObserver's one at each
// RTCP module. It will aggregate the results and run one bandwidth estimation
// and push the result to the encoders via BitrateAllocatorObserver(s).
class BitrateAllocator {
 public:
  // Used to get notified when send stream limits such as the minimum send
  // bitrate and max padding bitrate is changed.
  class LimitObserver {
   public:
    virtual void OnAllocationLimitsChanged(
        uint32_t min_send_bitrate_bps,
        uint32_t max_padding_bitrate_bps) = 0;

   protected:
    virtual ~LimitObserver() {}
  };

  explicit BitrateAllocator(LimitObserver* limit_observer);
  ~BitrateAllocator();

  // Allocate target_bitrate across the registered BitrateAllocatorObservers.
  void OnNetworkChanged(uint32_t target_bitrate_bps,
                        uint8_t fraction_loss,
                        int64_t rtt,
                        int64_t bwe_period_ms);

  // Set the start and max send bitrate used by the bandwidth management.
  //
  // |observer| updates bitrates if already in use.
  // |min_bitrate_bps| = 0 equals no min bitrate.
  // |max_bitrate_bps| = 0 equals no max bitrate.
  // |enforce_min_bitrate| = 'true' will allocate at least |min_bitrate_bps| for
  //    this observer, even if the BWE is too low, 'false' will allocate 0 to
  //    the observer if BWE doesn't allow |min_bitrate_bps|.
  // Note that |observer|->OnBitrateUpdated() will be called within the scope of
  // this method with the current rtt, fraction_loss and available bitrate and
  // that the bitrate in OnBitrateUpdated will be zero if the |observer| is
  // currently not allowed to send data.
  void AddObserver(BitrateAllocatorObserver* observer,
                   uint32_t min_bitrate_bps,
                   uint32_t max_bitrate_bps,
                   uint32_t pad_up_bitrate_bps,
                   bool enforce_min_bitrate);

  // Removes a previously added observer, but will not trigger a new bitrate
  // allocation.
  void RemoveObserver(BitrateAllocatorObserver* observer);

  // Returns initial bitrate allocated for |observer|. If |observer| is not in
  // the list of added observers, a best guess is returned.
  int GetStartBitrate(BitrateAllocatorObserver* observer);

 private:
  // Note: All bitrates for member variables and methods are in bps.
  struct ObserverConfig {
    ObserverConfig(BitrateAllocatorObserver* observer,
                   uint32_t min_bitrate_bps,
                   uint32_t max_bitrate_bps,
                   uint32_t pad_up_bitrate_bps,
                   bool enforce_min_bitrate)
        : observer(observer),
          min_bitrate_bps(min_bitrate_bps),
          max_bitrate_bps(max_bitrate_bps),
          pad_up_bitrate_bps(pad_up_bitrate_bps),
          enforce_min_bitrate(enforce_min_bitrate),
          allocated_bitrate_bps(-1),
          media_ratio(1.0) {}

    BitrateAllocatorObserver* observer;
    uint32_t min_bitrate_bps;
    uint32_t max_bitrate_bps;
    uint32_t pad_up_bitrate_bps;
    bool enforce_min_bitrate;
    int64_t allocated_bitrate_bps;
    double media_ratio;  // Part of the total bitrate used for media [0.0, 1.0].
  };

  // Calculates the minimum requested send bitrate and max padding bitrate and
  // calls LimitObserver::OnAllocationLimitsChanged.
  void UpdateAllocationLimits();

  typedef std::vector<ObserverConfig> ObserverConfigs;
  ObserverConfigs::iterator FindObserverConfig(
      const BitrateAllocatorObserver* observer);

  typedef std::multimap<uint32_t, const ObserverConfig*> ObserverSortingMap;
  typedef std::map<BitrateAllocatorObserver*, int> ObserverAllocation;

  ObserverAllocation AllocateBitrates(uint32_t bitrate);

  ObserverAllocation ZeroRateAllocation();
  ObserverAllocation LowRateAllocation(uint32_t bitrate);
  ObserverAllocation NormalRateAllocation(uint32_t bitrate,
                                          uint32_t sum_min_bitrates);
  ObserverAllocation MaxRateAllocation(uint32_t bitrate,
                                       uint32_t sum_max_bitrates);

  uint32_t LastAllocatedBitrate(const ObserverConfig& observer_config);
  // The minimum bitrate required by this observer, including enable-hysteresis
  // if the observer is in a paused state.
  uint32_t MinBitrateWithHysteresis(const ObserverConfig& observer_config);
  // Splits |bitrate| evenly to observers already in |allocation|.
  // |include_zero_allocations| decides if zero allocations should be part of
  // the distribution or not. The allowed max bitrate is |max_multiplier| x
  // observer max bitrate.
  void DistributeBitrateEvenly(uint32_t bitrate,
                               bool include_zero_allocations,
                               int max_multiplier,
                               ObserverAllocation* allocation);
  bool EnoughBitrateForAllObservers(uint32_t bitrate,
                                    uint32_t sum_min_bitrates);

  rtc::SequencedTaskChecker sequenced_checker_;
  LimitObserver* const limit_observer_ GUARDED_BY(&sequenced_checker_);
  // Stored in a list to keep track of the insertion order.
  ObserverConfigs bitrate_observer_configs_ GUARDED_BY(&sequenced_checker_);
  uint32_t last_bitrate_bps_ GUARDED_BY(&sequenced_checker_);
  uint32_t last_non_zero_bitrate_bps_ GUARDED_BY(&sequenced_checker_);
  uint8_t last_fraction_loss_ GUARDED_BY(&sequenced_checker_);
  int64_t last_rtt_ GUARDED_BY(&sequenced_checker_);
  int64_t last_bwe_period_ms_ GUARDED_BY(&sequenced_checker_);
  // Number of mute events based on too low BWE, not network up/down.
  int num_pause_events_ GUARDED_BY(&sequenced_checker_);
  Clock* const clock_ GUARDED_BY(&sequenced_checker_);
  int64_t last_bwe_log_time_ GUARDED_BY(&sequenced_checker_);
  uint32_t total_requested_padding_bitrate_ GUARDED_BY(&sequenced_checker_);
  uint32_t total_requested_min_bitrate_ GUARDED_BY(&sequenced_checker_);
};
}  // namespace webrtc
#endif  // WEBRTC_CALL_BITRATE_ALLOCATOR_H_
