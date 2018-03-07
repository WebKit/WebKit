/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/overuse_frame_detector.h"

#include <assert.h>
#include <math.h>

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <utility>

#include "api/video/video_frame.h"
#include "common_video/include/frame_callback.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <mach/mach.h>
#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

namespace webrtc {

namespace {
const int64_t kCheckForOveruseIntervalMs = 5000;
const int64_t kTimeToFirstCheckForOveruseMs = 100;

// Delay between consecutive rampups. (Used for quick recovery.)
const int kQuickRampUpDelayMs = 10 * 1000;
// Delay between rampup attempts. Initially uses standard, scales up to max.
const int kStandardRampUpDelayMs = 40 * 1000;
const int kMaxRampUpDelayMs = 240 * 1000;
// Expontential back-off factor, to prevent annoying up-down behaviour.
const double kRampUpBackoffFactor = 2.0;

// Max number of overuses detected before always applying the rampup delay.
const int kMaxOverusesBeforeApplyRampupDelay = 4;

// The maximum exponent to use in VCMExpFilter.
const float kMaxExp = 7.0f;
// Default value used before first reconfiguration.
const int kDefaultFrameRate = 30;
// Default sample diff, default frame rate.
const float kDefaultSampleDiffMs = 1000.0f / kDefaultFrameRate;
// A factor applied to the sample diff on OnTargetFramerateUpdated to determine
// a max limit for the sample diff. For instance, with a framerate of 30fps,
// the sample diff is capped to (1000 / 30) * 1.35 = 45ms. This prevents
// triggering too soon if there are individual very large outliers.
const float kMaxSampleDiffMarginFactor = 1.35f;
// Minimum framerate allowed for usage calculation. This prevents crazy long
// encode times from being accepted if the frame rate happens to be low.
const int kMinFramerate = 7;
const int kMaxFramerate = 30;

const auto kScaleReasonCpu = AdaptationObserverInterface::AdaptReason::kCpu;
}  // namespace

CpuOveruseOptions::CpuOveruseOptions()
    : high_encode_usage_threshold_percent(85),
      frame_timeout_interval_ms(1500),
      min_frame_samples(120),
      min_process_count(3),
      high_threshold_consecutive_count(2) {
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  // This is proof-of-concept code for letting the physical core count affect
  // the interval into which we attempt to scale. For now, the code is Mac OS
  // specific, since that's the platform were we saw most problems.
  // TODO(torbjorng): Enhance SystemInfo to return this metric.

  mach_port_t mach_host = mach_host_self();
  host_basic_info hbi = {};
  mach_msg_type_number_t info_count = HOST_BASIC_INFO_COUNT;
  kern_return_t kr =
      host_info(mach_host, HOST_BASIC_INFO, reinterpret_cast<host_info_t>(&hbi),
                &info_count);
  mach_port_deallocate(mach_task_self(), mach_host);

  int n_physical_cores;
  if (kr != KERN_SUCCESS) {
    // If we couldn't get # of physical CPUs, don't panic. Assume we have 1.
    n_physical_cores = 1;
    RTC_LOG(LS_ERROR)
        << "Failed to determine number of physical cores, assuming 1";
  } else {
    n_physical_cores = hbi.physical_cpu;
    RTC_LOG(LS_INFO) << "Number of physical cores:" << n_physical_cores;
  }

  // Change init list default for few core systems. The assumption here is that
  // encoding, which we measure here, takes about 1/4 of the processing of a
  // two-way call. This is roughly true for x86 using both vp8 and vp9 without
  // hardware encoding. Since we don't affect the incoming stream here, we only
  // control about 1/2 of the total processing needs, but this is not taken into
  // account.
  if (n_physical_cores == 1)
    high_encode_usage_threshold_percent = 20;  // Roughly 1/4 of 100%.
  else if (n_physical_cores == 2)
    high_encode_usage_threshold_percent = 40;  // Roughly 1/4 of 200%.
#endif  // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)

  // Note that we make the interval 2x+epsilon wide, since libyuv scaling steps
  // are close to that (when squared). This wide interval makes sure that
  // scaling up or down does not jump all the way across the interval.
  low_encode_usage_threshold_percent =
      (high_encode_usage_threshold_percent - 1) / 2;
}

// Class for calculating the processing usage on the send-side (the average
// processing time of a frame divided by the average time difference between
// captured frames).
class OveruseFrameDetector::SendProcessingUsage {
 public:
  explicit SendProcessingUsage(const CpuOveruseOptions& options)
      : kWeightFactorFrameDiff(0.998f),
        kWeightFactorProcessing(0.995f),
        kInitialSampleDiffMs(40.0f),
        count_(0),
        options_(options),
        max_sample_diff_ms_(kDefaultSampleDiffMs * kMaxSampleDiffMarginFactor),
        filtered_processing_ms_(new rtc::ExpFilter(kWeightFactorProcessing)),
        filtered_frame_diff_ms_(new rtc::ExpFilter(kWeightFactorFrameDiff)) {
    Reset();
  }
  virtual ~SendProcessingUsage() {}

  void Reset() {
    count_ = 0;
    max_sample_diff_ms_ = kDefaultSampleDiffMs * kMaxSampleDiffMarginFactor;
    filtered_frame_diff_ms_->Reset(kWeightFactorFrameDiff);
    filtered_frame_diff_ms_->Apply(1.0f, kInitialSampleDiffMs);
    filtered_processing_ms_->Reset(kWeightFactorProcessing);
    filtered_processing_ms_->Apply(1.0f, InitialProcessingMs());
  }

  void SetMaxSampleDiffMs(float diff_ms) { max_sample_diff_ms_ = diff_ms; }

  void AddCaptureSample(float sample_ms) {
    float exp = sample_ms / kDefaultSampleDiffMs;
    exp = std::min(exp, kMaxExp);
    filtered_frame_diff_ms_->Apply(exp, sample_ms);
  }

  void AddSample(float processing_ms, int64_t diff_last_sample_ms) {
    ++count_;
    float exp = diff_last_sample_ms / kDefaultSampleDiffMs;
    exp = std::min(exp, kMaxExp);
    filtered_processing_ms_->Apply(exp, processing_ms);
  }

  virtual int Value() {
    if (count_ < static_cast<uint32_t>(options_.min_frame_samples)) {
      return static_cast<int>(InitialUsageInPercent() + 0.5f);
    }
    float frame_diff_ms = std::max(filtered_frame_diff_ms_->filtered(), 1.0f);
    frame_diff_ms = std::min(frame_diff_ms, max_sample_diff_ms_);
    float encode_usage_percent =
        100.0f * filtered_processing_ms_->filtered() / frame_diff_ms;
    return static_cast<int>(encode_usage_percent + 0.5);
  }

 private:
  float InitialUsageInPercent() const {
    // Start in between the underuse and overuse threshold.
    return (options_.low_encode_usage_threshold_percent +
            options_.high_encode_usage_threshold_percent) / 2.0f;
  }

  float InitialProcessingMs() const {
    return InitialUsageInPercent() * kInitialSampleDiffMs / 100;
  }

  const float kWeightFactorFrameDiff;
  const float kWeightFactorProcessing;
  const float kInitialSampleDiffMs;
  uint64_t count_;
  const CpuOveruseOptions options_;
  float max_sample_diff_ms_;
  std::unique_ptr<rtc::ExpFilter> filtered_processing_ms_;
  std::unique_ptr<rtc::ExpFilter> filtered_frame_diff_ms_;
};

// Class used for manual testing of overuse, enabled via field trial flag.
class OveruseFrameDetector::OverdoseInjector
    : public OveruseFrameDetector::SendProcessingUsage {
 public:
  OverdoseInjector(const CpuOveruseOptions& options,
                   int64_t normal_period_ms,
                   int64_t overuse_period_ms,
                   int64_t underuse_period_ms)
      : OveruseFrameDetector::SendProcessingUsage(options),
        normal_period_ms_(normal_period_ms),
        overuse_period_ms_(overuse_period_ms),
        underuse_period_ms_(underuse_period_ms),
        state_(State::kNormal),
        last_toggling_ms_(-1) {
    RTC_DCHECK_GT(overuse_period_ms, 0);
    RTC_DCHECK_GT(normal_period_ms, 0);
    RTC_LOG(LS_INFO) << "Simulating overuse with intervals " << normal_period_ms
                     << "ms normal mode, " << overuse_period_ms
                     << "ms overuse mode.";
  }

  ~OverdoseInjector() override {}

  int Value() override {
    int64_t now_ms = rtc::TimeMillis();
    if (last_toggling_ms_ == -1) {
      last_toggling_ms_ = now_ms;
    } else {
      switch (state_) {
        case State::kNormal:
          if (now_ms > last_toggling_ms_ + normal_period_ms_) {
            state_ = State::kOveruse;
            last_toggling_ms_ = now_ms;
            RTC_LOG(LS_INFO) << "Simulating CPU overuse.";
          }
          break;
        case State::kOveruse:
          if (now_ms > last_toggling_ms_ + overuse_period_ms_) {
            state_ = State::kUnderuse;
            last_toggling_ms_ = now_ms;
            RTC_LOG(LS_INFO) << "Simulating CPU underuse.";
          }
          break;
        case State::kUnderuse:
          if (now_ms > last_toggling_ms_ + underuse_period_ms_) {
            state_ = State::kNormal;
            last_toggling_ms_ = now_ms;
            RTC_LOG(LS_INFO) << "Actual CPU overuse measurements in effect.";
          }
          break;
      }
    }

    rtc::Optional<int> overried_usage_value;
    switch (state_) {
      case State::kNormal:
        break;
      case State::kOveruse:
        overried_usage_value.emplace(250);
        break;
      case State::kUnderuse:
        overried_usage_value.emplace(5);
        break;
    }

    return overried_usage_value.value_or(SendProcessingUsage::Value());
  }

 private:
  const int64_t normal_period_ms_;
  const int64_t overuse_period_ms_;
  const int64_t underuse_period_ms_;
  enum class State { kNormal, kOveruse, kUnderuse } state_;
  int64_t last_toggling_ms_;
};

std::unique_ptr<OveruseFrameDetector::SendProcessingUsage>
OveruseFrameDetector::CreateSendProcessingUsage(
    const CpuOveruseOptions& options) {
  std::unique_ptr<SendProcessingUsage> instance;
  std::string toggling_interval =
      field_trial::FindFullName("WebRTC-ForceSimulatedOveruseIntervalMs");
  if (!toggling_interval.empty()) {
    int normal_period_ms = 0;
    int overuse_period_ms = 0;
    int underuse_period_ms = 0;
    if (sscanf(toggling_interval.c_str(), "%d-%d-%d", &normal_period_ms,
               &overuse_period_ms, &underuse_period_ms) == 3) {
      if (normal_period_ms > 0 && overuse_period_ms > 0 &&
          underuse_period_ms > 0) {
        instance.reset(new OverdoseInjector(
            options, normal_period_ms, overuse_period_ms, underuse_period_ms));
      } else {
        RTC_LOG(LS_WARNING)
            << "Invalid (non-positive) normal/overuse/underuse periods: "
            << normal_period_ms << " / " << overuse_period_ms << " / "
            << underuse_period_ms;
      }
    } else {
      RTC_LOG(LS_WARNING) << "Malformed toggling interval: "
                          << toggling_interval;
    }
  }

  if (!instance) {
    // No valid overuse simulation parameters set, use normal usage class.
    instance.reset(new SendProcessingUsage(options));
  }

  return instance;
}

class OveruseFrameDetector::CheckOveruseTask : public rtc::QueuedTask {
 public:
  explicit CheckOveruseTask(OveruseFrameDetector* overuse_detector)
      : overuse_detector_(overuse_detector) {
    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), kTimeToFirstCheckForOveruseMs);
  }

  void Stop() {
    RTC_CHECK(task_checker_.CalledSequentially());
    overuse_detector_ = nullptr;
  }

 private:
  bool Run() override {
    RTC_CHECK(task_checker_.CalledSequentially());
    if (!overuse_detector_)
      return true;  // This will make the task queue delete this task.
    overuse_detector_->CheckForOveruse();

    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), kCheckForOveruseIntervalMs);
    // Return false to prevent this task from being deleted. Ownership has been
    // transferred to the task queue when PostDelayedTask was called.
    return false;
  }
  rtc::SequencedTaskChecker task_checker_;
  OveruseFrameDetector* overuse_detector_;
};

OveruseFrameDetector::OveruseFrameDetector(
    const CpuOveruseOptions& options,
    AdaptationObserverInterface* observer,
    EncodedFrameObserver* encoder_timing,
    CpuOveruseMetricsObserver* metrics_observer)
    : check_overuse_task_(nullptr),
      options_(options),
      observer_(observer),
      encoder_timing_(encoder_timing),
      metrics_observer_(metrics_observer),
      num_process_times_(0),
      // TODO(nisse): Use rtc::Optional
      last_capture_time_us_(-1),
      last_processed_capture_time_us_(-1),
      num_pixels_(0),
      max_framerate_(kDefaultFrameRate),
      last_overuse_time_ms_(-1),
      checks_above_threshold_(0),
      num_overuse_detections_(0),
      last_rampup_time_ms_(-1),
      in_quick_rampup_(false),
      current_rampup_delay_ms_(kStandardRampUpDelayMs),
      usage_(CreateSendProcessingUsage(options)) {
  task_checker_.Detach();
}

OveruseFrameDetector::~OveruseFrameDetector() {
  RTC_DCHECK(!check_overuse_task_) << "StopCheckForOverUse must be called.";
}

void OveruseFrameDetector::StartCheckForOveruse() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  RTC_DCHECK(!check_overuse_task_);
  check_overuse_task_ = new CheckOveruseTask(this);
}
void OveruseFrameDetector::StopCheckForOveruse() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  check_overuse_task_->Stop();
  check_overuse_task_ = nullptr;
}

void OveruseFrameDetector::EncodedFrameTimeMeasured(int encode_duration_ms) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  if (!metrics_)
    metrics_ = rtc::Optional<CpuOveruseMetrics>(CpuOveruseMetrics());
  metrics_->encode_usage_percent = usage_->Value();

  metrics_observer_->OnEncodedFrameTimeMeasured(encode_duration_ms, *metrics_);
}

bool OveruseFrameDetector::FrameSizeChanged(int num_pixels) const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  if (num_pixels != num_pixels_) {
    return true;
  }
  return false;
}

bool OveruseFrameDetector::FrameTimeoutDetected(int64_t now_us) const {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  if (last_capture_time_us_ == -1)
    return false;
  return (now_us - last_capture_time_us_) >
      options_.frame_timeout_interval_ms * rtc::kNumMicrosecsPerMillisec;
}

void OveruseFrameDetector::ResetAll(int num_pixels) {
  // Reset state, as a result resolution being changed. Do not however change
  // the current frame rate back to the default.
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  num_pixels_ = num_pixels;
  usage_->Reset();
  frame_timing_.clear();
  last_capture_time_us_ = -1;
  last_processed_capture_time_us_ = -1;
  num_process_times_ = 0;
  metrics_ = rtc::Optional<CpuOveruseMetrics>();
  OnTargetFramerateUpdated(max_framerate_);
}

void OveruseFrameDetector::OnTargetFramerateUpdated(int framerate_fps) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  RTC_DCHECK_GE(framerate_fps, 0);
  max_framerate_ = std::min(kMaxFramerate, framerate_fps);
  usage_->SetMaxSampleDiffMs((1000 / std::max(kMinFramerate, max_framerate_)) *
                             kMaxSampleDiffMarginFactor);
}

void OveruseFrameDetector::FrameCaptured(const VideoFrame& frame,
                                         int64_t time_when_first_seen_us) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);

  if (FrameSizeChanged(frame.width() * frame.height()) ||
      FrameTimeoutDetected(time_when_first_seen_us)) {
    ResetAll(frame.width() * frame.height());
  }

  if (last_capture_time_us_ != -1)
    usage_->AddCaptureSample(
        1e-3 * (time_when_first_seen_us - last_capture_time_us_));

  last_capture_time_us_ = time_when_first_seen_us;

  frame_timing_.push_back(FrameTiming(frame.timestamp_us(), frame.timestamp(),
                                      time_when_first_seen_us));
}

void OveruseFrameDetector::FrameSent(uint32_t timestamp,
                                     int64_t time_sent_in_us) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  // Delay before reporting actual encoding time, used to have the ability to
  // detect total encoding time when encoding more than one layer. Encoding is
  // here assumed to finish within a second (or that we get enough long-time
  // samples before one second to trigger an overuse even when this is not the
  // case).
  static const int64_t kEncodingTimeMeasureWindowMs = 1000;
  for (auto& it : frame_timing_) {
    if (it.timestamp == timestamp) {
      it.last_send_us = time_sent_in_us;
      break;
    }
  }
  // TODO(pbos): Handle the case/log errors when not finding the corresponding
  // frame (either very slow encoding or incorrect wrong timestamps returned
  // from the encoder).
  // This is currently the case for all frames on ChromeOS, so logging them
  // would be spammy, and triggering overuse would be wrong.
  // https://crbug.com/350106
  while (!frame_timing_.empty()) {
    FrameTiming timing = frame_timing_.front();
    if (time_sent_in_us - timing.capture_us <
        kEncodingTimeMeasureWindowMs * rtc::kNumMicrosecsPerMillisec) {
      break;
    }
    if (timing.last_send_us != -1) {
      int encode_duration_us =
          static_cast<int>(timing.last_send_us - timing.capture_us);
      if (encoder_timing_) {
        // TODO(nisse): Update encoder_timing_ to also use us units.
        encoder_timing_->OnEncodeTiming(timing.capture_time_us /
                                        rtc::kNumMicrosecsPerMillisec,
                                        encode_duration_us /
                                        rtc::kNumMicrosecsPerMillisec);
      }
      if (last_processed_capture_time_us_ != -1) {
        int64_t diff_us = timing.capture_us - last_processed_capture_time_us_;
        usage_->AddSample(1e-3 * encode_duration_us, 1e-3 * diff_us);
      }
      last_processed_capture_time_us_ = timing.capture_us;
      EncodedFrameTimeMeasured(encode_duration_us /
                               rtc::kNumMicrosecsPerMillisec);
    }
    frame_timing_.pop_front();
  }
}

void OveruseFrameDetector::CheckForOveruse() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  ++num_process_times_;
  if (num_process_times_ <= options_.min_process_count || !metrics_)
    return;

  int64_t now_ms = rtc::TimeMillis();

  if (IsOverusing(*metrics_)) {
    // If the last thing we did was going up, and now have to back down, we need
    // to check if this peak was short. If so we should back off to avoid going
    // back and forth between this load, the system doesn't seem to handle it.
    bool check_for_backoff = last_rampup_time_ms_ > last_overuse_time_ms_;
    if (check_for_backoff) {
      if (now_ms - last_rampup_time_ms_ < kStandardRampUpDelayMs ||
          num_overuse_detections_ > kMaxOverusesBeforeApplyRampupDelay) {
        // Going up was not ok for very long, back off.
        current_rampup_delay_ms_ *= kRampUpBackoffFactor;
        if (current_rampup_delay_ms_ > kMaxRampUpDelayMs)
          current_rampup_delay_ms_ = kMaxRampUpDelayMs;
      } else {
        // Not currently backing off, reset rampup delay.
        current_rampup_delay_ms_ = kStandardRampUpDelayMs;
      }
    }

    last_overuse_time_ms_ = now_ms;
    in_quick_rampup_ = false;
    checks_above_threshold_ = 0;
    ++num_overuse_detections_;

    if (observer_)
      observer_->AdaptDown(kScaleReasonCpu);
  } else if (IsUnderusing(*metrics_, now_ms)) {
    last_rampup_time_ms_ = now_ms;
    in_quick_rampup_ = true;

    if (observer_)
      observer_->AdaptUp(kScaleReasonCpu);
  }

  int rampup_delay =
      in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;

  RTC_LOG(LS_VERBOSE) << " Frame stats: "
                      << " encode usage " << metrics_->encode_usage_percent
                      << " overuse detections " << num_overuse_detections_
                      << " rampup delay " << rampup_delay;
}

bool OveruseFrameDetector::IsOverusing(const CpuOveruseMetrics& metrics) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);

  if (metrics.encode_usage_percent >=
      options_.high_encode_usage_threshold_percent) {
    ++checks_above_threshold_;
  } else {
    checks_above_threshold_ = 0;
  }
  return checks_above_threshold_ >= options_.high_threshold_consecutive_count;
}

bool OveruseFrameDetector::IsUnderusing(const CpuOveruseMetrics& metrics,
                                        int64_t time_now) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&task_checker_);
  int delay = in_quick_rampup_ ? kQuickRampUpDelayMs : current_rampup_delay_ms_;
  if (time_now < last_rampup_time_ms_ + delay)
    return false;

  return metrics.encode_usage_percent <
         options_.low_encode_usage_threshold_percent;
}
}  // namespace webrtc
