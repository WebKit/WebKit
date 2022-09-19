/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_METRONOME_TEST_FAKE_METRONOME_H_
#define API_METRONOME_TEST_FAKE_METRONOME_H_

#include <memory>
#include <set>

#include "api/metronome/metronome.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc::test {

// ForcedTickMetronome is a Metronome that ticks when `Tick()` is invoked.
// The constructor argument `tick_period` returned in `TickPeriod()`.
class ForcedTickMetronome : public Metronome {
 public:
  explicit ForcedTickMetronome(TimeDelta tick_period);

  // Forces all TickListeners to run `OnTick`.
  void Tick();
  size_t NumListeners();

  // Metronome implementation.
  void AddListener(TickListener* listener) override;
  void RemoveListener(TickListener* listener) override;
  TimeDelta TickPeriod() const override;

 private:
  const TimeDelta tick_period_;
  std::set<TickListener*> listeners_;
};

// FakeMetronome is a metronome that ticks based on a repeating task at the
// `tick_period` provided in the constructor. It is designed for use with
// simulated task queues for unit tests.
//
// `Stop()` must be called before destruction, as it cancels the metronome tick
// on the proper task queue.
class FakeMetronome : public Metronome {
 public:
  FakeMetronome(TaskQueueFactory* factory, TimeDelta tick_period);
  ~FakeMetronome() override;

  // Metronome implementation.
  void AddListener(TickListener* listener) override;
  void RemoveListener(TickListener* listener) override;
  TimeDelta TickPeriod() const override;

  void Stop();

 private:
  const TimeDelta tick_period_;
  RepeatingTaskHandle tick_task_;
  bool started_ RTC_GUARDED_BY(mutex_) = false;
  std::set<TickListener*> listeners_ RTC_GUARDED_BY(mutex_);
  Mutex mutex_;
  rtc::TaskQueue queue_;
};

}  // namespace webrtc::test

#endif  // API_METRONOME_TEST_FAKE_METRONOME_H_
