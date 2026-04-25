/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/adaptation/pixel_limit_resource.h"

#include <optional>
#include <string>

#include "api/adaptation/resource.h"
#include "api/field_trials_view.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/video/video_adaptation_reason.h"
#include "call/adaptation/video_stream_adapter.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/task_utils/repeating_task.h"

namespace webrtc {

namespace {

// How to enable the PixelLimitResource, example:
// --force-fieldtrials=WebRTC-PixelLimitResource/target_pixels:230400,
// interval:5s,reason:quality,toggle:5s/
//
// "target_pixels" is the encoder input video size (e.g. 640x360 = 230400) that
// the PixelLimitResource will try to achieve by signaling kOveruse or kUnderuse
// whenever the current input pixel count is too high or too low relative to
// this. Defaults to 0.
//
// "interval" is the interval at which PixelLimitResource checks whether it
// it should report kOveruse or kUnderuse, impacting how quickly adaptation
// converges on the target. Defaults to 5s.
//
// "reason" is the VideoAdaptationReason that is associated with this resource,
// valid values are "cpu" or "quality". Defaults to "cpu".
//
// If "toggle" is specified, then PixelLimitResource will turn "on" and "off"
// every specified amount of time. By turning "off" we mean that it will
// repeatedly signal kUnderuse as to remove any existing adaptation pressure.
// If not specified the PixelLimitResource is always "on".
struct PixelLimitResourceParams {
  static std::optional<PixelLimitResourceParams> Parse(
      const FieldTrialsView& field_trials) {
    std::string params_str = field_trials.Lookup("WebRTC-PixelLimitResource");
    if (params_str.empty()) {
      return std::nullopt;
    }
    PixelLimitResourceParams params;
    ParseFieldTrial({&params.target_pixels, &params.interval, &params.reason,
                     &params.toggle},
                    params_str);
    return params;
  }

  PixelLimitResourceParams()
      : target_pixels("target_pixels", 0),
        interval("interval", TimeDelta::Seconds(5)),
        reason("reason",
               VideoAdaptationReason::kCpu,
               {{"cpu", VideoAdaptationReason::kCpu},
                {"quality", VideoAdaptationReason::kQuality}}),
        toggle("toggle") {}

  FieldTrialParameter<int> target_pixels;
  FieldTrialParameter<TimeDelta> interval;
  FieldTrialEnum<VideoAdaptationReason> reason;
  FieldTrialOptional<TimeDelta> toggle;
};

const char* ToString(VideoAdaptationReason reason) {
  switch (reason) {
    case VideoAdaptationReason::kQuality:
      return "quality";
    case VideoAdaptationReason::kCpu:
      return "cpu";
  }
}

}  // namespace

// static
scoped_refptr<PixelLimitResource> PixelLimitResource::CreateIfFieldTrialEnabled(
    const FieldTrialsView& field_trials,
    TaskQueueBase* task_queue,
    VideoStreamInputStateProvider* input_state_provider) {
  std::optional<PixelLimitResourceParams> params =
      PixelLimitResourceParams::Parse(field_trials);
  if (!params.has_value()) {
    return nullptr;
  }
  auto pixel_limit_resource = make_ref_counted<PixelLimitResource>(
      task_queue, input_state_provider, params->target_pixels.Get(),
      params->interval.Get(), params->reason.Get(),
      params->toggle.GetOptional());
  RTC_LOG(LS_INFO) << "Running with PixelLimitResource {target_pixels:"
                   << params->target_pixels.Get()
                   << ", interval: " << params->interval.Get()
                   << ", reason:" << ToString(params->reason.Get())
                   << ", toggle:"
                   << (params->toggle ? ToString(*params->toggle)
                                      : std::string("N/A"))
                   << "}";
  return pixel_limit_resource;
}

PixelLimitResource::PixelLimitResource(
    TaskQueueBase* task_queue,
    VideoStreamInputStateProvider* input_state_provider,
    int target_pixels,
    TimeDelta interval,
    VideoAdaptationReason reason,
    std::optional<TimeDelta> toggle_interval)
    : task_queue_(task_queue),
      input_state_provider_(input_state_provider),
      target_pixels_(target_pixels),
      interval_(interval),
      reason_(reason),
      toggle_interval_(toggle_interval),
      is_enabled_(true),
      time_since_last_toggle_(TimeDelta::Zero()),
      listener_(nullptr) {
  RTC_DCHECK(task_queue_);
  RTC_DCHECK(input_state_provider_);
}

PixelLimitResource::~PixelLimitResource() {
  RTC_DCHECK(!listener_);
  RTC_DCHECK(!repeating_task_.Running());
}

void PixelLimitResource::SetResourceListener(ResourceListener* listener) {
  RTC_DCHECK_RUN_ON(task_queue_);
  listener_ = listener;
  if (listener_) {
    repeating_task_.Stop();
    repeating_task_ =
        RepeatingTaskHandle::DelayedStart(task_queue_, interval_, [&] {
          RTC_DCHECK_RUN_ON(task_queue_);
          if (!listener_) {
            // We don't have a listener so resource adaptation must not be
            // running, try again later.
            return interval_;
          }
          if (is_enabled_) {
            // When "enabled", we try to influence the input pixels to gravitate
            // towards our `target_pixels_`. NO-OP if we don't know current
            // pixels.
            std::optional<int> current_pixels =
                input_state_provider_->InputState().frame_size_pixels();
            if (current_pixels.has_value()) {
              // Use lower bounds that is one step lower than `target_pixels_`
              // to avoid risk of flip-flopping up and down.
              int target_pixels_lower_bounds =
                  GetLowerResolutionThan(target_pixels_);
              if (*current_pixels > target_pixels_) {
                listener_->OnResourceUsageStateMeasured(
                    scoped_refptr<Resource>(this),
                    ResourceUsageState::kOveruse);
              } else if (*current_pixels < target_pixels_lower_bounds) {
                listener_->OnResourceUsageStateMeasured(
                    scoped_refptr<Resource>(this),
                    ResourceUsageState::kUnderuse);
              }
            }
          } else {
            // When "disabled", we always signal kUnderuse.
            listener_->OnResourceUsageStateMeasured(
                scoped_refptr<Resource>(this), ResourceUsageState::kUnderuse);
          }
          // Maybe toggle "on" or "off".
          if (toggle_interval_.has_value()) {
            time_since_last_toggle_ += interval_;
            if (time_since_last_toggle_ >= toggle_interval_.value()) {
              is_enabled_ = !is_enabled_;
              RTC_LOG(LS_INFO) << "PixelLimitResource toggled "
                               << (is_enabled_ ? "on" : "off");
              time_since_last_toggle_ = TimeDelta::Zero();
            }
          }
          return interval_;
        });
  } else {
    repeating_task_.Stop();
  }
  // The task must be running if we have a listener.
  RTC_DCHECK(repeating_task_.Running() || !listener_);
}

}  // namespace webrtc
