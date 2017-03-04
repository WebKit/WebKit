/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_MONITOR_MODULE_H
#define WEBRTC_VOICE_ENGINE_MONITOR_MODULE_H

#include "webrtc/modules/include/module.h"

namespace webrtc {
namespace voe {

// When associated with a ProcessThread, calls a callback method
// |OnPeriodicProcess()| implemented by the |Observer|.
// TODO(tommi): This could be replaced with PostDelayedTask().
// Better yet, delete it and delete code related to |_saturationWarning|
// in TransmitMixer (and the OnPeriodicProcess callback).
template <typename Observer>
class MonitorModule : public Module {
 public:
  explicit MonitorModule(Observer* observer) : observer_(observer) {}
  ~MonitorModule() override {}

 private:
  int64_t TimeUntilNextProcess() override { return 1000; }
  void Process() override { observer_->OnPeriodicProcess(); }

  Observer* const observer_;
};

}  // namespace voe
}  // namespace webrtc

#endif  // VOICE_ENGINE_MONITOR_MODULE
