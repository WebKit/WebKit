/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/performance_stats.h"

#include <algorithm>

namespace webrtc {
namespace test {
void VideoFramesStats::AddFrameInfo(const VideoFrameBuffer& frame,
                                    Timestamp at_time) {
  ++count;
  RTC_DCHECK(at_time.IsFinite());
  pixels.AddSample(frame.width() * frame.height());
  resolution.AddSample(std::max(frame.width(), frame.height()));
  frames.AddEvent(at_time);
}

void VideoFramesStats::AddStats(const VideoFramesStats& other) {
  count += other.count;
  pixels.AddSamples(other.pixels);
  resolution.AddSamples(other.resolution);
  frames.AddEvents(other.frames);
}

void VideoQualityStats::AddStats(const VideoQualityStats& other) {
  capture.AddStats(other.capture);
  render.AddStats(other.render);
  lost_count += other.lost_count;
  freeze_count += other.freeze_count;
  capture_to_decoded_delay.AddSamples(other.capture_to_decoded_delay);
  end_to_end_delay.AddSamples(other.end_to_end_delay);
  psnr.AddSamples(other.psnr);
  psnr_with_freeze.AddSamples(other.psnr_with_freeze);
  skipped_between_rendered.AddSamples(other.skipped_between_rendered);
  freeze_duration.AddSamples(other.freeze_duration);
  time_between_freezes.AddSamples(other.time_between_freezes);
}

}  // namespace test
}  // namespace webrtc
