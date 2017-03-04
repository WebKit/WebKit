/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/videocapturertracksource.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"

using cricket::CaptureState;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;

namespace {

const double kRoundingTruncation = 0.0005;

// Default resolution. If no constraint is specified, this is the resolution we
// will use.
static const cricket::VideoFormatPod kDefaultFormat = {
    640, 480, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY};

// List of formats used if the camera doesn't support capability enumeration.
static const cricket::VideoFormatPod kVideoFormats[] = {
    {1920, 1080, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
    {1280, 720, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
    {960, 720, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
    {640, 360, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
    {640, 480, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
    {320, 240, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY},
    {320, 180, FPS_TO_INTERVAL(30), cricket::FOURCC_ANY}};

MediaSourceInterface::SourceState GetReadyState(cricket::CaptureState state) {
  switch (state) {
    case cricket::CS_STARTING:
      return MediaSourceInterface::kInitializing;
    case cricket::CS_RUNNING:
      return MediaSourceInterface::kLive;
    case cricket::CS_FAILED:
    case cricket::CS_STOPPED:
      return MediaSourceInterface::kEnded;
    default:
      RTC_NOTREACHED() << "GetReadyState unknown state";
  }
  return MediaSourceInterface::kEnded;
}

void SetUpperLimit(int new_limit, int* original_limit) {
  if (*original_limit < 0 || new_limit < *original_limit)
    *original_limit = new_limit;
}

// Updates |format_upper_limit| from |constraint|.
// If constraint.maxFoo is smaller than format_upper_limit.foo,
// set format_upper_limit.foo to constraint.maxFoo.
void SetUpperLimitFromConstraint(
    const MediaConstraintsInterface::Constraint& constraint,
    cricket::VideoFormat* format_upper_limit) {
  if (constraint.key == MediaConstraintsInterface::kMaxWidth) {
    int value = rtc::FromString<int>(constraint.value);
    SetUpperLimit(value, &(format_upper_limit->width));
  } else if (constraint.key == MediaConstraintsInterface::kMaxHeight) {
    int value = rtc::FromString<int>(constraint.value);
    SetUpperLimit(value, &(format_upper_limit->height));
  }
}

// Fills |format_out| with the max width and height allowed by |constraints|.
void FromConstraintsForScreencast(
    const MediaConstraintsInterface::Constraints& constraints,
    cricket::VideoFormat* format_out) {
  typedef MediaConstraintsInterface::Constraints::const_iterator
      ConstraintsIterator;

  cricket::VideoFormat upper_limit(-1, -1, 0, 0);
  for (ConstraintsIterator constraints_it = constraints.begin();
       constraints_it != constraints.end(); ++constraints_it)
    SetUpperLimitFromConstraint(*constraints_it, &upper_limit);

  if (upper_limit.width >= 0)
    format_out->width = upper_limit.width;
  if (upper_limit.height >= 0)
    format_out->height = upper_limit.height;
}

// Returns true if |constraint| is fulfilled. |format_out| can differ from
// |format_in| if the format is changed by the constraint. Ie - the frame rate
// can be changed by setting maxFrameRate.
bool NewFormatWithConstraints(
    const MediaConstraintsInterface::Constraint& constraint,
    const cricket::VideoFormat& format_in,
    bool mandatory,
    cricket::VideoFormat* format_out) {
  RTC_DCHECK(format_out != NULL);
  *format_out = format_in;

  if (constraint.key == MediaConstraintsInterface::kMinWidth) {
    int value = rtc::FromString<int>(constraint.value);
    return (value <= format_in.width);
  } else if (constraint.key == MediaConstraintsInterface::kMaxWidth) {
    int value = rtc::FromString<int>(constraint.value);
    return (value >= format_in.width);
  } else if (constraint.key == MediaConstraintsInterface::kMinHeight) {
    int value = rtc::FromString<int>(constraint.value);
    return (value <= format_in.height);
  } else if (constraint.key == MediaConstraintsInterface::kMaxHeight) {
    int value = rtc::FromString<int>(constraint.value);
    return (value >= format_in.height);
  } else if (constraint.key == MediaConstraintsInterface::kMinFrameRate) {
    int value = rtc::FromString<int>(constraint.value);
    return (value <= cricket::VideoFormat::IntervalToFps(format_in.interval));
  } else if (constraint.key == MediaConstraintsInterface::kMaxFrameRate) {
    int value = rtc::FromString<int>(constraint.value);
    if (value == 0) {
      if (mandatory) {
        // TODO(ronghuawu): Convert the constraint value to float when sub-1fps
        // is supported by the capturer.
        return false;
      } else {
        value = 1;
      }
    }
    if (value <= cricket::VideoFormat::IntervalToFps(format_in.interval))
      format_out->interval = cricket::VideoFormat::FpsToInterval(value);
    return true;
  } else if (constraint.key == MediaConstraintsInterface::kMinAspectRatio) {
    double value = rtc::FromString<double>(constraint.value);
    // The aspect ratio in |constraint.value| has been converted to a string and
    // back to a double, so it may have a rounding error.
    // E.g if the value 1/3 is converted to a string, the string will not have
    // infinite length.
    // We add a margin of 0.0005 which is high enough to detect the same aspect
    // ratio but small enough to avoid matching wrong aspect ratios.
    double ratio = static_cast<double>(format_in.width) / format_in.height;
    return (value <= ratio + kRoundingTruncation);
  } else if (constraint.key == MediaConstraintsInterface::kMaxAspectRatio) {
    double value = rtc::FromString<double>(constraint.value);
    double ratio = static_cast<double>(format_in.width) / format_in.height;
    // Subtract 0.0005 to avoid rounding problems. Same as above.
    const double kRoundingTruncation = 0.0005;
    return (value >= ratio - kRoundingTruncation);
  } else if (constraint.key == MediaConstraintsInterface::kNoiseReduction) {
    // These are actually options, not constraints, so they can be satisfied
    // regardless of the format.
    return true;
  }
  LOG(LS_WARNING) << "Found unknown MediaStream constraint. Name:"
                  << constraint.key << " Value:" << constraint.value;
  return false;
}

// Removes cricket::VideoFormats from |formats| that don't meet |constraint|.
void FilterFormatsByConstraint(
    const MediaConstraintsInterface::Constraint& constraint,
    bool mandatory,
    std::vector<cricket::VideoFormat>* formats) {
  std::vector<cricket::VideoFormat>::iterator format_it = formats->begin();
  while (format_it != formats->end()) {
    // Modify the format_it to fulfill the constraint if possible.
    // Delete it otherwise.
    if (!NewFormatWithConstraints(constraint, (*format_it), mandatory,
                                  &(*format_it))) {
      format_it = formats->erase(format_it);
    } else {
      ++format_it;
    }
  }
}

// Returns a vector of cricket::VideoFormat that best match |constraints|.
std::vector<cricket::VideoFormat> FilterFormats(
    const MediaConstraintsInterface::Constraints& mandatory,
    const MediaConstraintsInterface::Constraints& optional,
    const std::vector<cricket::VideoFormat>& supported_formats) {
  typedef MediaConstraintsInterface::Constraints::const_iterator
      ConstraintsIterator;
  std::vector<cricket::VideoFormat> candidates = supported_formats;

  for (ConstraintsIterator constraints_it = mandatory.begin();
       constraints_it != mandatory.end(); ++constraints_it)
    FilterFormatsByConstraint(*constraints_it, true, &candidates);

  if (candidates.size() == 0)
    return candidates;

  // Ok - all mandatory checked and we still have a candidate.
  // Let's try filtering using the optional constraints.
  for (ConstraintsIterator constraints_it = optional.begin();
       constraints_it != optional.end(); ++constraints_it) {
    std::vector<cricket::VideoFormat> current_candidates = candidates;
    FilterFormatsByConstraint(*constraints_it, false, &current_candidates);
    if (current_candidates.size() > 0) {
      candidates = current_candidates;
    }
  }

  // We have done as good as we can to filter the supported resolutions.
  return candidates;
}

// Find the format that best matches the default video size.
// Constraints are optional and since the performance of a video call
// might be bad due to bitrate limitations, CPU, and camera performance,
// it is better to select a resolution that is as close as possible to our
// default and still meets the contraints.
const cricket::VideoFormat& GetBestCaptureFormat(
    const std::vector<cricket::VideoFormat>& formats) {
  RTC_DCHECK(formats.size() > 0);

  int default_area = kDefaultFormat.width * kDefaultFormat.height;

  std::vector<cricket::VideoFormat>::const_iterator it = formats.begin();
  std::vector<cricket::VideoFormat>::const_iterator best_it = formats.begin();
  int best_diff_area = std::abs(default_area - it->width * it->height);
  int64_t best_diff_interval = kDefaultFormat.interval;
  for (; it != formats.end(); ++it) {
    int diff_area = std::abs(default_area - it->width * it->height);
    int64_t diff_interval = std::abs(kDefaultFormat.interval - it->interval);
    if (diff_area < best_diff_area ||
        (diff_area == best_diff_area && diff_interval < best_diff_interval)) {
      best_diff_area = diff_area;
      best_diff_interval = diff_interval;
      best_it = it;
    }
  }
  return *best_it;
}

// Set |option| to the highest-priority value of |key| in the constraints.
// Return false if the key is mandatory, and the value is invalid.
bool ExtractOption(const MediaConstraintsInterface* all_constraints,
                   const std::string& key,
                   rtc::Optional<bool>* option) {
  size_t mandatory = 0;
  bool value;
  if (FindConstraint(all_constraints, key, &value, &mandatory)) {
    *option = rtc::Optional<bool>(value);
    return true;
  }

  return mandatory == 0;
}

}  // anonymous namespace

namespace webrtc {

rtc::scoped_refptr<VideoTrackSourceInterface> VideoCapturerTrackSource::Create(
    rtc::Thread* worker_thread,
    std::unique_ptr<cricket::VideoCapturer> capturer,
    const webrtc::MediaConstraintsInterface* constraints,
    bool remote) {
  RTC_DCHECK(worker_thread != NULL);
  RTC_DCHECK(capturer != NULL);
  rtc::scoped_refptr<VideoCapturerTrackSource> source(
      new rtc::RefCountedObject<VideoCapturerTrackSource>(
          worker_thread, std::move(capturer), remote));
  source->Initialize(constraints);
  return source;
}

rtc::scoped_refptr<VideoTrackSourceInterface> VideoCapturerTrackSource::Create(
    rtc::Thread* worker_thread,
    std::unique_ptr<cricket::VideoCapturer> capturer,
    bool remote) {
  RTC_DCHECK(worker_thread != NULL);
  RTC_DCHECK(capturer != NULL);
  rtc::scoped_refptr<VideoCapturerTrackSource> source(
      new rtc::RefCountedObject<VideoCapturerTrackSource>(
          worker_thread, std::move(capturer), remote));
  source->Initialize(nullptr);
  return source;
}

VideoCapturerTrackSource::VideoCapturerTrackSource(
    rtc::Thread* worker_thread,
    std::unique_ptr<cricket::VideoCapturer> capturer,
    bool remote)
    : VideoTrackSource(capturer.get(), remote),
      signaling_thread_(rtc::Thread::Current()),
      worker_thread_(worker_thread),
      video_capturer_(std::move(capturer)),
      started_(false) {
  video_capturer_->SignalStateChange.connect(
      this, &VideoCapturerTrackSource::OnStateChange);
}

VideoCapturerTrackSource::~VideoCapturerTrackSource() {
  video_capturer_->SignalStateChange.disconnect(this);
  Stop();
}

void VideoCapturerTrackSource::Initialize(
    const webrtc::MediaConstraintsInterface* constraints) {
  std::vector<cricket::VideoFormat> formats =
      *video_capturer_->GetSupportedFormats();
  if (formats.empty()) {
    if (video_capturer_->IsScreencast()) {
      // The screen capturer can accept any resolution and we will derive the
      // format from the constraints if any.
      // Note that this only affects tab capturing, not desktop capturing,
      // since the desktop capturer does not respect the VideoFormat passed in.
      formats.push_back(cricket::VideoFormat(kDefaultFormat));
    } else {
      // The VideoCapturer implementation doesn't support capability
      // enumeration. We need to guess what the camera supports.
      for (uint32_t i = 0; i < arraysize(kVideoFormats); ++i) {
        formats.push_back(cricket::VideoFormat(kVideoFormats[i]));
      }
    }
  }

  if (constraints) {
    MediaConstraintsInterface::Constraints mandatory_constraints =
        constraints->GetMandatory();
    MediaConstraintsInterface::Constraints optional_constraints;
    optional_constraints = constraints->GetOptional();

    if (video_capturer_->IsScreencast()) {
      // Use the maxWidth and maxHeight allowed by constraints for screencast.
      FromConstraintsForScreencast(mandatory_constraints, &(formats[0]));
    }

    formats =
        FilterFormats(mandatory_constraints, optional_constraints, formats);
  }

  if (formats.size() == 0) {
    LOG(LS_WARNING) << "Failed to find a suitable video format.";
    SetState(kEnded);
    return;
  }

  if (!ExtractOption(constraints, MediaConstraintsInterface::kNoiseReduction,
                     &needs_denoising_)) {
    LOG(LS_WARNING) << "Invalid mandatory value for"
                    << MediaConstraintsInterface::kNoiseReduction;
    SetState(kEnded);
    return;
  }

  format_ = GetBestCaptureFormat(formats);
  // Start the camera with our best guess.
  if (!worker_thread_->Invoke<bool>(
          RTC_FROM_HERE, rtc::Bind(&cricket::VideoCapturer::StartCapturing,
                                   video_capturer_.get(), format_))) {
    SetState(kEnded);
    return;
  }
  started_ = true;
  // Initialize hasn't succeeded until a successful state change has occurred.
}

bool VideoCapturerTrackSource::GetStats(Stats* stats) {
  return video_capturer_->GetInputSize(&stats->input_width,
                                       &stats->input_height);
}

void VideoCapturerTrackSource::Stop() {
  if (!started_) {
    return;
  }
  started_ = false;
  worker_thread_->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&cricket::VideoCapturer::Stop, video_capturer_.get()));
}

// OnStateChange listens to the cricket::VideoCapturer::SignalStateChange.
void VideoCapturerTrackSource::OnStateChange(
    cricket::VideoCapturer* capturer,
    cricket::CaptureState capture_state) {
  if (rtc::Thread::Current() != signaling_thread_) {
    // Use rtc::Unretained, because we don't want this to capture a reference
    // to ourselves. If our destructor is called while this task is executing,
    // that's fine; our AsyncInvoker destructor will wait for it to finish if
    // it isn't simply canceled.
    invoker_.AsyncInvoke<void>(
        RTC_FROM_HERE, signaling_thread_,
        rtc::Bind(&VideoCapturerTrackSource::OnStateChange,
                  rtc::Unretained(this), capturer, capture_state));
    return;
  }

  if (capturer == video_capturer_.get()) {
    SetState(GetReadyState(capture_state));
  }
}

}  // namespace webrtc
