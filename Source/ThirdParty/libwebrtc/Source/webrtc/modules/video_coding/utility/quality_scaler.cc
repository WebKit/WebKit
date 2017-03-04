/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/quality_scaler.h"

#include <math.h>

#include <algorithm>
#include <memory>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/task_queue.h"

// TODO(kthelgason): Some versions of Android have issues with log2.
// See https://code.google.com/p/android/issues/detail?id=212634 for details
#if defined(WEBRTC_ANDROID)
#define log2(x) (log(x) / log(2))
#endif

namespace webrtc {

namespace {
// Threshold constant used until first downscale (to permit fast rampup).
static const int kMeasureMs = 2000;
static const float kSamplePeriodScaleFactor = 2.5;
static const int kFramedropPercentThreshold = 60;
// QP scaling threshold defaults:
static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 37;
// QP is obtained from VP8-bitstream for HW, so the QP corresponds to the
// bitstream range of [0, 127] and not the user-level range of [0,63].
static const int kLowVp8QpThreshold = 29;
static const int kHighVp8QpThreshold = 95;

static VideoEncoder::QpThresholds CodecTypeToDefaultThresholds(
    VideoCodecType codec_type) {
  int low = -1;
  int high = -1;
  switch (codec_type) {
    case kVideoCodecH264:
      low = kLowH264QpThreshold;
      high = kHighH264QpThreshold;
      break;
    case kVideoCodecVP8:
      low = kLowVp8QpThreshold;
      high = kHighVp8QpThreshold;
      break;
    default:
      RTC_NOTREACHED() << "Invalid codec type for QualityScaler.";
  }
  return VideoEncoder::QpThresholds(low, high);
}
}  // namespace

class QualityScaler::CheckQPTask : public rtc::QueuedTask {
 public:
  explicit CheckQPTask(QualityScaler* scaler) : scaler_(scaler) {
    LOG(LS_INFO) << "Created CheckQPTask. Scheduling on queue...";
    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), scaler_->GetSamplingPeriodMs());
  }
  void Stop() {
    RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
    LOG(LS_INFO) << "Stopping QP Check task.";
    stop_ = true;
  }

 private:
  bool Run() override {
    RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
    if (stop_)
      return true;  // TaskQueue will free this task.
    scaler_->CheckQP();
    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), scaler_->GetSamplingPeriodMs());
    return false;  // Retain the task in order to reuse it.
  }

  QualityScaler* const scaler_;
  bool stop_ = false;
  rtc::SequencedTaskChecker task_checker_;
};

QualityScaler::QualityScaler(AdaptationObserverInterface* observer,
                             VideoCodecType codec_type)
    : QualityScaler(observer, CodecTypeToDefaultThresholds(codec_type)) {}

QualityScaler::QualityScaler(AdaptationObserverInterface* observer,
                             VideoEncoder::QpThresholds thresholds)
    : QualityScaler(observer, thresholds, kMeasureMs) {}

// Protected ctor, should not be called directly.
QualityScaler::QualityScaler(AdaptationObserverInterface* observer,
                             VideoEncoder::QpThresholds thresholds,
                             int64_t sampling_period)
    : check_qp_task_(nullptr),
      observer_(observer),
      sampling_period_ms_(sampling_period),
      fast_rampup_(true),
      // Arbitrarily choose size based on 30 fps for 5 seconds.
      average_qp_(5 * 30),
      framedrop_percent_(5 * 30),
      thresholds_(thresholds) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  RTC_DCHECK(observer_ != nullptr);
  check_qp_task_ = new CheckQPTask(this);
}

QualityScaler::~QualityScaler() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  check_qp_task_->Stop();
}

int64_t QualityScaler::GetSamplingPeriodMs() const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  return fast_rampup_ ? sampling_period_ms_
                      : (sampling_period_ms_ * kSamplePeriodScaleFactor);
}

void QualityScaler::ReportDroppedFrame() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  framedrop_percent_.AddSample(100);
}

void QualityScaler::ReportQP(int qp) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  framedrop_percent_.AddSample(0);
  average_qp_.AddSample(qp);
}

void QualityScaler::CheckQP() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  // Should be set through InitEncode -> Should be set by now.
  RTC_DCHECK_GE(thresholds_.low, 0);
  LOG(LS_INFO) << "Checking if average QP exceeds threshold";
  // Check if we should scale down due to high frame drop.
  const rtc::Optional<int> drop_rate = framedrop_percent_.GetAverage();
  if (drop_rate && *drop_rate >= kFramedropPercentThreshold) {
    ReportQPHigh();
    return;
  }

  // Check if we should scale up or down based on QP.
  const rtc::Optional<int> avg_qp = average_qp_.GetAverage();
  if (avg_qp && *avg_qp > thresholds_.high) {
    ReportQPHigh();
    return;
  }
  if (avg_qp && *avg_qp <= thresholds_.low) {
    // QP has been low. We want to try a higher resolution.
    ReportQPLow();
    return;
  }
}

void QualityScaler::ReportQPLow() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  LOG(LS_INFO) << "QP has been low, asking for higher resolution.";
  ClearSamples();
  observer_->AdaptUp(AdaptationObserverInterface::AdaptReason::kQuality);
}

void QualityScaler::ReportQPHigh() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  LOG(LS_INFO) << "QP has been high , asking for lower resolution.";
  ClearSamples();
  observer_->AdaptDown(AdaptationObserverInterface::AdaptReason::kQuality);
  // If we've scaled down, wait longer before scaling up again.
  if (fast_rampup_) {
    fast_rampup_ = false;
  }
}

void QualityScaler::ClearSamples() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  framedrop_percent_.Reset();
  average_qp_.Reset();
}
}  // namespace webrtc
