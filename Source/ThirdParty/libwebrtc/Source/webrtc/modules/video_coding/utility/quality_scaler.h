/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
#define MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "absl/types/optional.h"
#include "api/video_codecs/video_encoder.h"
#include "rtc_base/experiments/quality_scaling_experiment.h"
#include "rtc_base/numerics/moving_average.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"

namespace webrtc {

// An interface for signaling requests to limit or increase the resolution or
// framerate of the captured video stream.
// TODO(hbos): Can we remove AdaptationObserverInterface in favor of
// ResourceUsageListener? If we need to adapt that is because of resource usage.
// A multi-stream and multi-resource aware solution needs to sparate the notion
// of being resource constrained from the decision to downgrade a specific
// stream.
class AdaptationObserverInterface {
 public:
  // Indicates if the adaptation is due to overuse of the CPU resources, or if
  // the quality of the encoded frames have dropped too low.
  enum AdaptReason : size_t { kQuality = 0, kCpu = 1 };
  static const size_t kScaleReasonSize = 2;
  // Called to signal that we can handle larger or more frequent frames.
  virtual void AdaptUp(AdaptReason reason) = 0;
  // Called to signal that the source should reduce the resolution or framerate.
  // Returns false if a downgrade was requested but the request did not result
  // in a new limiting resolution or fps.
  virtual bool AdaptDown(AdaptReason reason) = 0;

 protected:
  virtual ~AdaptationObserverInterface() {}
};

// QualityScaler runs asynchronously and monitors QP values of encoded frames.
// It holds a reference to an AdaptationObserverInterface implementation to
// signal an intent to scale up or down.
class QualityScaler {
 public:
  // Construct a QualityScaler with given |thresholds| and |observer|.
  // This starts the quality scaler periodically checking what the average QP
  // has been recently.
  QualityScaler(AdaptationObserverInterface* observer,
                VideoEncoder::QpThresholds thresholds);
  virtual ~QualityScaler();
  // Should be called each time a frame is dropped at encoding.
  void ReportDroppedFrameByMediaOpt();
  void ReportDroppedFrameByEncoder();
  // Inform the QualityScaler of the last seen QP.
  void ReportQp(int qp, int64_t time_sent_us);

  void SetQpThresholds(VideoEncoder::QpThresholds thresholds);
  bool QpFastFilterLow() const;

  // The following members declared protected for testing purposes.
 protected:
  QualityScaler(AdaptationObserverInterface* observer,
                VideoEncoder::QpThresholds thresholds,
                int64_t sampling_period_ms);

 private:
  class QpSmoother;

  void CheckQp();
  void ClearSamples();
  void ReportQpLow();
  void ReportQpHigh();
  int64_t GetSamplingPeriodMs() const;

  RepeatingTaskHandle check_qp_task_ RTC_GUARDED_BY(&task_checker_);
  AdaptationObserverInterface* const observer_ RTC_GUARDED_BY(&task_checker_);
  SequenceChecker task_checker_;

  VideoEncoder::QpThresholds thresholds_ RTC_GUARDED_BY(&task_checker_);
  const int64_t sampling_period_ms_;
  bool fast_rampup_ RTC_GUARDED_BY(&task_checker_);
  rtc::MovingAverage average_qp_ RTC_GUARDED_BY(&task_checker_);
  rtc::MovingAverage framedrop_percent_media_opt_
      RTC_GUARDED_BY(&task_checker_);
  rtc::MovingAverage framedrop_percent_all_ RTC_GUARDED_BY(&task_checker_);

  // Used by QualityScalingExperiment.
  const bool experiment_enabled_;
  QualityScalingExperiment::Config config_ RTC_GUARDED_BY(&task_checker_);
  std::unique_ptr<QpSmoother> qp_smoother_high_ RTC_GUARDED_BY(&task_checker_);
  std::unique_ptr<QpSmoother> qp_smoother_low_ RTC_GUARDED_BY(&task_checker_);
  bool observed_enough_frames_ RTC_GUARDED_BY(&task_checker_);

  const size_t min_frames_needed_;
  const double initial_scale_factor_;
  const absl::optional<double> scale_factor_;
  bool adapt_called_ RTC_GUARDED_BY(&task_checker_);
  bool adapt_failed_ RTC_GUARDED_BY(&task_checker_);
};
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
