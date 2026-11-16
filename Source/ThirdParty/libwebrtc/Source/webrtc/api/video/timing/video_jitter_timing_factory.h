/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_TIMING_VIDEO_JITTER_TIMING_FACTORY_H_
#define API_VIDEO_TIMING_VIDEO_JITTER_TIMING_FACTORY_H_

#include <memory>

#include "api/environment/environment.h"
#include "api/video/timing/video_jitter_timing_interface.h"

namespace webrtc {

// Creates VideoJitterTimingInterface instances.
class VideoJitterTimingFactory {
 public:
  virtual ~VideoJitterTimingFactory() = default;

  virtual std::unique_ptr<VideoJitterTimingInterface> Create(
      const Environment& env) const = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_TIMING_VIDEO_JITTER_TIMING_FACTORY_H_
