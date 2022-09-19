/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/metronome/test/fake_metronome.h"

#include "api/priority.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "rtc_base/event.h"
#include "rtc_base/task_utils/repeating_task.h"

namespace webrtc::test {

ForcedTickMetronome::ForcedTickMetronome(TimeDelta tick_period)
    : tick_period_(tick_period) {}

void ForcedTickMetronome::AddListener(TickListener* listener) {
  listeners_.insert(listener);
}

void ForcedTickMetronome::RemoveListener(TickListener* listener) {
  listeners_.erase(listener);
}

TimeDelta ForcedTickMetronome::TickPeriod() const {
  return tick_period_;
}

size_t ForcedTickMetronome::NumListeners() {
  return listeners_.size();
}

void ForcedTickMetronome::Tick() {
  for (auto* listener : listeners_) {
    listener->OnTickTaskQueue()->PostTask([listener] { listener->OnTick(); });
  }
}

FakeMetronome::FakeMetronome(TaskQueueFactory* factory, TimeDelta tick_period)
    : tick_period_(tick_period),
      queue_(factory->CreateTaskQueue("MetronomeQueue",
                                      TaskQueueFactory::Priority::HIGH)) {}

FakeMetronome::~FakeMetronome() {
  RTC_DCHECK(listeners_.empty());
}

void FakeMetronome::AddListener(TickListener* listener) {
  MutexLock lock(&mutex_);
  listeners_.insert(listener);
  if (!started_) {
    tick_task_ = RepeatingTaskHandle::Start(queue_.Get(), [this] {
      MutexLock lock(&mutex_);
      // Stop if empty.
      if (listeners_.empty())
        return TimeDelta::PlusInfinity();
      for (auto* listener : listeners_) {
        listener->OnTickTaskQueue()->PostTask(
            [listener] { listener->OnTick(); });
      }
      return tick_period_;
    });
    started_ = true;
  }
}

void FakeMetronome::RemoveListener(TickListener* listener) {
  MutexLock lock(&mutex_);
  listeners_.erase(listener);
}

void FakeMetronome::Stop() {
  MutexLock lock(&mutex_);
  RTC_DCHECK(listeners_.empty());
  if (started_)
    queue_.PostTask([this] { tick_task_.Stop(); });
}

TimeDelta FakeMetronome::TickPeriod() const {
  return tick_period_;
}

}  // namespace webrtc::test
