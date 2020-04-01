/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/adaptation_counters.h"

namespace webrtc {

bool AdaptationCounters::operator==(const AdaptationCounters& rhs) const {
  return fps_adaptations == rhs.fps_adaptations &&
         resolution_adaptations == rhs.resolution_adaptations;
}

bool AdaptationCounters::operator!=(const AdaptationCounters& rhs) const {
  return !(rhs == *this);
}

AdaptationCounters AdaptationCounters::operator+(
    const AdaptationCounters& other) const {
  return AdaptationCounters(
      resolution_adaptations + other.resolution_adaptations,
      fps_adaptations + other.fps_adaptations);
}

AdaptationCounters AdaptationCounters::operator-(
    const AdaptationCounters& other) const {
  return AdaptationCounters(
      resolution_adaptations - other.resolution_adaptations,
      fps_adaptations - other.fps_adaptations);
}

}  // namespace webrtc
