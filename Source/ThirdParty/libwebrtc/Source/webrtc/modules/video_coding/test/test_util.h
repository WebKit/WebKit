/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TEST_TEST_UTIL_H_
#define MODULES_VIDEO_CODING_TEST_TEST_UTIL_H_

#include "system_wrappers/include/event_wrapper.h"

class NullEventFactory : public webrtc::EventFactory {
 public:
  virtual ~NullEventFactory() {}

  webrtc::EventWrapper* CreateEvent() override { return new NullEvent; }

 private:
  // Private class to avoid more dependencies on it in tests.
  class NullEvent : public webrtc::EventWrapper {
   public:
    ~NullEvent() override {}
    bool Set() override { return true; }
    webrtc::EventTypeWrapper Wait(unsigned long max_time) override {  // NOLINT
      return webrtc::kEventTimeout;
    }
  };
};

#endif  // MODULES_VIDEO_CODING_TEST_TEST_UTIL_H_
