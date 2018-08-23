/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SYSTEM_WRAPPERS_SOURCE_EVENT_WIN_H_
#define SYSTEM_WRAPPERS_SOURCE_EVENT_WIN_H_

#include <stdint.h>

#include <windows.h>

#include "system_wrappers/include/event_wrapper.h"

namespace webrtc {

class EventTimerWin : public EventTimerWrapper {
 public:
  EventTimerWin();
  ~EventTimerWin() override;

  EventTypeWrapper Wait(unsigned long max_time) override;
  bool Set() override;

  bool StartTimer(bool periodic, unsigned long time) override;
  bool StopTimer() override;

 private:
  HANDLE event_;
  uint32_t timerID_;
};

}  // namespace webrtc

#endif  // SYSTEM_WRAPPERS_SOURCE_EVENT_WIN_H_
