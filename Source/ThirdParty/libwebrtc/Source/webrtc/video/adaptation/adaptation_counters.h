/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_ADAPTATION_COUNTERS_H_
#define VIDEO_ADAPTATION_ADAPTATION_COUNTERS_H_

namespace webrtc {

// Counts the number of adaptations have resulted due to resource overuse.
// Today we can adapt resolution and fps.
struct AdaptationCounters {
  AdaptationCounters() : resolution_adaptations(0), fps_adaptations(0) {}
  AdaptationCounters(int resolution_adaptations, int fps_adaptations)
      : resolution_adaptations(resolution_adaptations),
        fps_adaptations(fps_adaptations) {}

  int Total() const { return fps_adaptations + resolution_adaptations; }

  bool operator==(const AdaptationCounters& rhs) const;
  bool operator!=(const AdaptationCounters& rhs) const;

  AdaptationCounters operator+(const AdaptationCounters& other) const;
  AdaptationCounters operator-(const AdaptationCounters& other) const;

  int resolution_adaptations;
  int fps_adaptations;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_ADAPTATION_COUNTERS_H_
