/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_OVERUSE_FRAME_DETECTOR_H_
#define WEBRTC_VIDEO_OVERUSE_FRAME_DETECTOR_H_

#include <list>
#include <memory>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/numerics/exp_filter.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/sequenced_task_checker.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/video_coding/utility/quality_scaler.h"

namespace webrtc {

class EncodedFrameObserver;
class VideoFrame;

struct CpuOveruseOptions {
  CpuOveruseOptions();

  int low_encode_usage_threshold_percent;  // Threshold for triggering underuse.
  int high_encode_usage_threshold_percent;  // Threshold for triggering overuse.
  // General settings.
  int frame_timeout_interval_ms;  // The maximum allowed interval between two
                                  // frames before resetting estimations.
  int min_frame_samples;  // The minimum number of frames required.
  int min_process_count;  // The number of initial process times required before
                          // triggering an overuse/underuse.
  int high_threshold_consecutive_count;  // The number of consecutive checks
                                         // above the high threshold before
                                         // triggering an overuse.
};

struct CpuOveruseMetrics {
  CpuOveruseMetrics() : encode_usage_percent(-1) {}

  int encode_usage_percent;  // Average encode time divided by the average time
                             // difference between incoming captured frames.
};

class CpuOveruseMetricsObserver {
 public:
  virtual ~CpuOveruseMetricsObserver() {}
  virtual void OnEncodedFrameTimeMeasured(int encode_duration_ms,
                                          const CpuOveruseMetrics& metrics) = 0;
};

// Use to detect system overuse based on the send-side processing time of
// incoming frames. All methods must be called on a single task queue but it can
// be created and destroyed on an arbitrary thread.
// OveruseFrameDetector::StartCheckForOveruse  must be called to periodically
// check for overuse.
class OveruseFrameDetector {
 public:
  OveruseFrameDetector(const CpuOveruseOptions& options,
                       AdaptationObserverInterface* overuse_observer,
                       EncodedFrameObserver* encoder_timing_,
                       CpuOveruseMetricsObserver* metrics_observer);
  ~OveruseFrameDetector();

  // Start to periodically check for overuse.
  void StartCheckForOveruse();

  // StopCheckForOveruse must be called before destruction if
  // StartCheckForOveruse has been called.
  void StopCheckForOveruse();

  // Called for each captured frame.
  void FrameCaptured(const VideoFrame& frame, int64_t time_when_first_seen_us);

  // Called for each sent frame.
  void FrameSent(uint32_t timestamp, int64_t time_sent_in_us);

 protected:
  void CheckForOveruse();  // Protected for test purposes.

 private:
  class SendProcessingUsage;
  class CheckOveruseTask;
  struct FrameTiming {
    FrameTiming(int64_t capture_time_us, uint32_t timestamp, int64_t now)
        : capture_time_us(capture_time_us),
          timestamp(timestamp),
          capture_us(now),
          last_send_us(-1) {}
    int64_t capture_time_us;
    uint32_t timestamp;
    int64_t capture_us;
    int64_t last_send_us;
  };

  void EncodedFrameTimeMeasured(int encode_duration_ms);
  bool IsOverusing(const CpuOveruseMetrics& metrics);
  bool IsUnderusing(const CpuOveruseMetrics& metrics, int64_t time_now);

  bool FrameTimeoutDetected(int64_t now) const;
  bool FrameSizeChanged(int num_pixels) const;

  void ResetAll(int num_pixels);

  rtc::SequencedTaskChecker task_checker_;
  // Owned by the task queue from where StartCheckForOveruse is called.
  CheckOveruseTask* check_overuse_task_;

  const CpuOveruseOptions options_;

  // Observer getting overuse reports.
  AdaptationObserverInterface* const observer_;
  EncodedFrameObserver* const encoder_timing_;

  // Stats metrics.
  CpuOveruseMetricsObserver* const metrics_observer_;
  rtc::Optional<CpuOveruseMetrics> metrics_ GUARDED_BY(task_checker_);

  int64_t num_process_times_ GUARDED_BY(task_checker_);

  int64_t last_capture_time_us_ GUARDED_BY(task_checker_);
  int64_t last_processed_capture_time_us_ GUARDED_BY(task_checker_);

  // Number of pixels of last captured frame.
  int num_pixels_ GUARDED_BY(task_checker_);
  int64_t last_overuse_time_ms_ GUARDED_BY(task_checker_);
  int checks_above_threshold_ GUARDED_BY(task_checker_);
  int num_overuse_detections_ GUARDED_BY(task_checker_);
  int64_t last_rampup_time_ms_ GUARDED_BY(task_checker_);
  bool in_quick_rampup_ GUARDED_BY(task_checker_);
  int current_rampup_delay_ms_ GUARDED_BY(task_checker_);

  // TODO(asapersson): Can these be regular members (avoid separate heap
  // allocs)?
  const std::unique_ptr<SendProcessingUsage> usage_ GUARDED_BY(task_checker_);
  std::list<FrameTiming> frame_timing_ GUARDED_BY(task_checker_);

  RTC_DISALLOW_COPY_AND_ASSIGN(OveruseFrameDetector);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_OVERUSE_FRAME_DETECTOR_H_
