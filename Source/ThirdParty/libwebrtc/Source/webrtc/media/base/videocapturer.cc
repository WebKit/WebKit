/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Implementation file of class VideoCapturer.

#include "webrtc/media/base/videocapturer.h"

#include <algorithm>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/base/logging.h"

namespace cricket {

namespace {

static const int64_t kMaxDistance = ~(static_cast<int64_t>(1) << 63);
#ifdef WEBRTC_LINUX
static const int kYU12Penalty = 16;  // Needs to be higher than MJPG index.
#endif

}  // namespace

/////////////////////////////////////////////////////////////////////
// Implementation of class VideoCapturer
/////////////////////////////////////////////////////////////////////
VideoCapturer::VideoCapturer() : apply_rotation_(false) {
  thread_checker_.DetachFromThread();
  Construct();
}

void VideoCapturer::Construct() {
  enable_camera_list_ = false;
  capture_state_ = CS_STOPPED;
  scaled_width_ = 0;
  scaled_height_ = 0;
  enable_video_adapter_ = true;
}

const std::vector<VideoFormat>* VideoCapturer::GetSupportedFormats() const {
  return &filtered_supported_formats_;
}

bool VideoCapturer::StartCapturing(const VideoFormat& capture_format) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  CaptureState result = Start(capture_format);
  const bool success = (result == CS_RUNNING) || (result == CS_STARTING);
  if (!success) {
    return false;
  }
  if (result == CS_RUNNING) {
    SetCaptureState(result);
  }
  return true;
}

void VideoCapturer::SetSupportedFormats(
    const std::vector<VideoFormat>& formats) {
  // This method is OK to call during initialization on a separate thread.
  RTC_DCHECK(capture_state_ == CS_STOPPED ||
             thread_checker_.CalledOnValidThread());
  supported_formats_ = formats;
  UpdateFilteredSupportedFormats();
}

bool VideoCapturer::GetBestCaptureFormat(const VideoFormat& format,
                                         VideoFormat* best_format) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(fbarchard): Directly support max_format.
  UpdateFilteredSupportedFormats();
  const std::vector<VideoFormat>* supported_formats = GetSupportedFormats();

  if (supported_formats->empty()) {
    return false;
  }
  LOG(LS_INFO) << " Capture Requested " << format.ToString();
  int64_t best_distance = kMaxDistance;
  std::vector<VideoFormat>::const_iterator best = supported_formats->end();
  std::vector<VideoFormat>::const_iterator i;
  for (i = supported_formats->begin(); i != supported_formats->end(); ++i) {
    int64_t distance = GetFormatDistance(format, *i);
    // TODO(fbarchard): Reduce to LS_VERBOSE if/when camera capture is
    // relatively bug free.
    LOG(LS_INFO) << " Supported " << i->ToString() << " distance " << distance;
    if (distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }
  if (supported_formats->end() == best) {
    LOG(LS_ERROR) << " No acceptable camera format found";
    return false;
  }

  if (best_format) {
    best_format->width = best->width;
    best_format->height = best->height;
    best_format->fourcc = best->fourcc;
    best_format->interval = best->interval;
    LOG(LS_INFO) << " Best " << best_format->ToString() << " Interval "
                 << best_format->interval << " distance " << best_distance;
  }
  return true;
}

void VideoCapturer::ConstrainSupportedFormats(const VideoFormat& max_format) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  max_format_.reset(new VideoFormat(max_format));
  LOG(LS_VERBOSE) << " ConstrainSupportedFormats " << max_format.ToString();
  UpdateFilteredSupportedFormats();
}

bool VideoCapturer::GetInputSize(int* width, int* height) {
  rtc::CritScope cs(&frame_stats_crit_);
  if (!input_size_valid_) {
    return false;
  }
  *width = input_width_;
  *height = input_height_;

  return true;
}

void VideoCapturer::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  broadcaster_.RemoveSink(sink);
  OnSinkWantsChanged(broadcaster_.wants());
}

void VideoCapturer::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  broadcaster_.AddOrUpdateSink(sink, wants);
  OnSinkWantsChanged(broadcaster_.wants());
}

void VideoCapturer::OnSinkWantsChanged(const rtc::VideoSinkWants& wants) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  apply_rotation_ = wants.rotation_applied;

  if (video_adapter()) {
    video_adapter()->OnResolutionFramerateRequest(wants.target_pixel_count,
                                                  wants.max_pixel_count,
                                                  wants.max_framerate_fps);
  }
}

bool VideoCapturer::AdaptFrame(int width,
                               int height,
                               int64_t camera_time_us,
                               int64_t system_time_us,
                               int* out_width,
                               int* out_height,
                               int* crop_width,
                               int* crop_height,
                               int* crop_x,
                               int* crop_y,
                               int64_t* translated_camera_time_us) {
  if (translated_camera_time_us) {
    *translated_camera_time_us =
        timestamp_aligner_.TranslateTimestamp(camera_time_us, system_time_us);
  }
  if (!broadcaster_.frame_wanted()) {
    return false;
  }

  if (enable_video_adapter_ && !IsScreencast()) {
    if (!video_adapter_.AdaptFrameResolution(
            width, height, camera_time_us * rtc::kNumNanosecsPerMicrosec,
            crop_width, crop_height, out_width, out_height)) {
      // VideoAdapter dropped the frame.
      return false;
    }
    *crop_x = (width - *crop_width) / 2;
    *crop_y = (height - *crop_height) / 2;
  } else {
    *out_width = width;
    *out_height = height;
    *crop_width = width;
    *crop_height = height;
    *crop_x = 0;
    *crop_y = 0;
  }

  return true;
}

void VideoCapturer::OnFrame(const webrtc::VideoFrame& frame,
                            int orig_width,
                            int orig_height) {
  // For a child class which implements rotation itself, we should
  // always have apply_rotation_ == false or frame.rotation() == 0.
  // Except possibly during races where apply_rotation_ is changed
  // mid-stream.
  if (apply_rotation_ && frame.rotation() != webrtc::kVideoRotation_0) {
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(
        frame.video_frame_buffer());
    if (buffer->type() != webrtc::VideoFrameBuffer::Type::kI420) {
      // Sources producing non-I420 frames must handle apply_rotation
      // themselves. But even if they do, we may occasionally end up
      // in this case, for frames in flight at the time
      // applied_rotation is set to true. In that case, we just drop
      // the frame.
      LOG(LS_WARNING) << "Non-I420 frame requiring rotation. Discarding.";
      return;
    }
    broadcaster_.OnFrame(webrtc::VideoFrame(
        webrtc::I420Buffer::Rotate(*buffer->GetI420(), frame.rotation()),
        webrtc::kVideoRotation_0, frame.timestamp_us()));
  } else {
    broadcaster_.OnFrame(frame);
  }
  UpdateInputSize(orig_width, orig_height);
}

void VideoCapturer::SetCaptureState(CaptureState state) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (state == capture_state_) {
    // Don't trigger a state changed callback if the state hasn't changed.
    return;
  }
  capture_state_ = state;
  SignalStateChange(this, capture_state_);
}

// Get the distance between the supported and desired formats.
// Prioritization is done according to this algorithm:
// 1) Width closeness. If not same, we prefer wider.
// 2) Height closeness. If not same, we prefer higher.
// 3) Framerate closeness. If not same, we prefer faster.
// 4) Compression. If desired format has a specific fourcc, we need exact match;
//                otherwise, we use preference.
int64_t VideoCapturer::GetFormatDistance(const VideoFormat& desired,
                                         const VideoFormat& supported) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int64_t distance = kMaxDistance;

  // Check fourcc.
  uint32_t supported_fourcc = CanonicalFourCC(supported.fourcc);
  int64_t delta_fourcc = kMaxDistance;
  if (FOURCC_ANY == desired.fourcc) {
    // Any fourcc is OK for the desired. Use preference to find best fourcc.
    std::vector<uint32_t> preferred_fourccs;
    if (!GetPreferredFourccs(&preferred_fourccs)) {
      return distance;
    }

    for (size_t i = 0; i < preferred_fourccs.size(); ++i) {
      if (supported_fourcc == CanonicalFourCC(preferred_fourccs[i])) {
        delta_fourcc = i;
#ifdef WEBRTC_LINUX
        // For HD avoid YU12 which is a software conversion and has 2 bugs
        // b/7326348 b/6960899.  Reenable when fixed.
        if (supported.height >= 720 && (supported_fourcc == FOURCC_YU12 ||
                                        supported_fourcc == FOURCC_YV12)) {
          delta_fourcc += kYU12Penalty;
        }
#endif
        break;
      }
    }
  } else if (supported_fourcc == CanonicalFourCC(desired.fourcc)) {
    delta_fourcc = 0;  // Need exact match.
  }

  if (kMaxDistance == delta_fourcc) {
    // Failed to match fourcc.
    return distance;
  }

  // Check resolution and fps.
  int desired_width = desired.width;
  int desired_height = desired.height;
  int64_t delta_w = supported.width - desired_width;
  float supported_fps = VideoFormat::IntervalToFpsFloat(supported.interval);
  float delta_fps =
      supported_fps - VideoFormat::IntervalToFpsFloat(desired.interval);
  // Check height of supported height compared to height we would like it to be.
  int64_t aspect_h = desired_width
                         ? supported.width * desired_height / desired_width
                         : desired_height;
  int64_t delta_h = supported.height - aspect_h;

  distance = 0;
  // Set high penalty if the supported format is lower than the desired format.
  // 3x means we would prefer down to down to 3/4, than up to double.
  // But we'd prefer up to double than down to 1/2.  This is conservative,
  // strongly avoiding going down in resolution, similar to
  // the old method, but not completely ruling it out in extreme situations.
  // It also ignores framerate, which is often very low at high resolutions.
  // TODO(fbarchard): Improve logic to use weighted factors.
  static const int kDownPenalty = -3;
  if (delta_w < 0) {
    delta_w = delta_w * kDownPenalty;
  }
  if (delta_h < 0) {
    delta_h = delta_h * kDownPenalty;
  }
  // Require camera fps to be at least 80% of what is requested if resolution
  // matches.
  // Require camera fps to be at least 96% of what is requested, or higher,
  // if resolution differs. 96% allows for slight variations in fps. e.g. 29.97
  if (delta_fps < 0) {
    float min_desirable_fps = delta_w ?
    VideoFormat::IntervalToFpsFloat(desired.interval) * 28.f / 30.f :
    VideoFormat::IntervalToFpsFloat(desired.interval) * 23.f / 30.f;
    delta_fps = -delta_fps;
    if (supported_fps < min_desirable_fps) {
      distance |= static_cast<int64_t>(1) << 62;
    } else {
      distance |= static_cast<int64_t>(1) << 15;
    }
  }
  int64_t idelta_fps = static_cast<int>(delta_fps);

  // 12 bits for width and height and 8 bits for fps and fourcc.
  distance |=
      (delta_w << 28) | (delta_h << 16) | (idelta_fps << 8) | delta_fourcc;

  return distance;
}

void VideoCapturer::UpdateFilteredSupportedFormats() {
  filtered_supported_formats_.clear();
  filtered_supported_formats_ = supported_formats_;
  if (!max_format_) {
    return;
  }
  std::vector<VideoFormat>::iterator iter = filtered_supported_formats_.begin();
  while (iter != filtered_supported_formats_.end()) {
    if (ShouldFilterFormat(*iter)) {
      iter = filtered_supported_formats_.erase(iter);
    } else {
      ++iter;
    }
  }
  if (filtered_supported_formats_.empty()) {
    // The device only captures at resolutions higher than |max_format_| this
    // indicates that |max_format_| should be ignored as it is better to capture
    // at too high a resolution than to not capture at all.
    filtered_supported_formats_ = supported_formats_;
  }
}

bool VideoCapturer::ShouldFilterFormat(const VideoFormat& format) const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!enable_camera_list_) {
    return false;
  }
  return format.width > max_format_->width ||
         format.height > max_format_->height;
}

void VideoCapturer::UpdateInputSize(int width, int height) {
  // Update stats protected from fetches from different thread.
  rtc::CritScope cs(&frame_stats_crit_);

  input_size_valid_ = true;
  input_width_ = width;
  input_height_ = height;
}

}  // namespace cricket
