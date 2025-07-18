/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/screen_capturer_sck.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <atomic>

#include "absl/strings/str_format.h"
#include "api/sequence_checker.h"
#include "modules/desktop_capture/mac/desktop_frame_iosurface.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "sck_picker_handle.h"
#include "sdk/objc/helpers/scoped_cftyperef.h"

using webrtc::DesktopFrameIOSurface;

namespace webrtc {
class ScreenCapturerSck;
}  // namespace webrtc

// The ScreenCaptureKit API was available in macOS 12.3, but full-screen capture
// was reported to be broken before macOS 13 - see http://crbug.com/40234870.
// Also, the `SCContentFilter` fields `contentRect` and `pointPixelScale` were
// introduced in macOS 14.
API_AVAILABLE(macos(14.0))
@interface SckHelper : NSObject <SCStreamDelegate,
                                 SCStreamOutput,
                                 SCContentSharingPickerObserver>

- (instancetype)initWithCapturer:(webrtc::ScreenCapturerSck*)capturer;

- (void)onShareableContentCreated:(SCShareableContent*)content
                            error:(NSError*)error;

// Called just before the capturer is destroyed. This avoids a dangling pointer,
// and prevents any new calls into a deleted capturer. If any method-call on the
// capturer is currently running on a different thread, this blocks until it
// completes.
- (void)releaseCapturer;

@end

namespace webrtc {

class API_AVAILABLE(macos(14.0)) ScreenCapturerSck final
    : public DesktopCapturer {
 public:
  explicit ScreenCapturerSck(const DesktopCaptureOptions& options);
  ScreenCapturerSck(const DesktopCaptureOptions& options,
                    SCContentSharingPickerMode modes);
  ScreenCapturerSck(const ScreenCapturerSck&) = delete;
  ScreenCapturerSck& operator=(const ScreenCapturerSck&) = delete;

  ~ScreenCapturerSck() override;

  // DesktopCapturer interface. All these methods run on the caller's thread.
  void Start(DesktopCapturer::Callback* callback) override;
  void SetMaxFrameRate(uint32_t max_frame_rate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  // Creates the SckPickerHandle if needed and not already done.
  void EnsurePickerHandle();
  // Prep for implementing DelegatedSourceListController interface, for now used
  // by Start(). Triggers SCContentSharingPicker. Runs on the caller's thread.
  void EnsureVisible();
  // Helper functions to forward SCContentSharingPickerObserver notifications to
  // source_list_observer_.
  void NotifySourceSelection(SCContentFilter* filter, SCStream* stream);
  void NotifySourceCancelled(SCStream* stream);
  void NotifySourceError();

  // Called after a SCStreamDelegate stop notification.
  void NotifyCaptureStopped(SCStream* stream);

  // Called by SckHelper when shareable content is returned by ScreenCaptureKit.
  // `content` will be nil if an error occurred. May run on an arbitrary thread.
  void OnShareableContentCreated(SCShareableContent* content, NSError* error);

  // Start capture with the given filter. Creates or updates stream_ as needed.
  void StartWithFilter(SCContentFilter* filter)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Called by SckHelper to notify of a newly captured frame. May run on an
  // arbitrary thread.
  void OnNewIOSurface(IOSurfaceRef io_surface, NSDictionary* attachment);

 private:
  // Called when starting the capturer or the configuration has changed (either
  // from a SelectSource() call, or the screen-resolution has changed). This
  // tells SCK to fetch new shareable content, and the completion-handler will
  // either start a new stream, or reconfigure the existing stream. Runs on the
  // caller's thread.
  void StartOrReconfigureCapturer();

  // Calls to the public API must happen on a single thread.
  webrtc::SequenceChecker api_checker_;

  // Helper object to receive Objective-C callbacks from ScreenCaptureKit and
  // call into this C++ object. The helper may outlive this C++ instance, if a
  // completion-handler is passed to ScreenCaptureKit APIs and the C++ object is
  // deleted before the handler executes.
  SckHelper* __strong helper_;

  // Callback for returning captured frames, or errors, to the caller.
  Callback* callback_ RTC_GUARDED_BY(api_checker_) = nullptr;

  // Helper class that tracks the number of capturers needing
  // SCContentSharingPicker to stay active.
  std::unique_ptr<SckPickerHandleInterface> picker_handle_
      RTC_GUARDED_BY(api_checker_);

  // Flag to track if we have added ourselves as observer to picker_handle_.
  bool picker_handle_registered_ RTC_GUARDED_BY(api_checker_) = false;

  // Options passed to the constructor. May be accessed on any thread, but the
  // options are unchanged during the capturer's lifetime.
  const DesktopCaptureOptions capture_options_;

  // Modes to use iff using the system picker.
  // See docs on SCContentSharingPickerMode.
  const SCContentSharingPickerMode picker_modes_;

  // Signals that a permanent error occurred. This may be set on any thread, and
  // is read by CaptureFrame() which runs on the caller's thread.
  std::atomic<bool> permanent_error_ = false;

  // Guards some variables that may be accessed on different threads.
  Mutex lock_;

  // Provides captured desktop frames.
  SCStream* __strong stream_ RTC_GUARDED_BY(lock_);

  // Current filter on stream_.
  SCContentFilter* __strong filter_ RTC_GUARDED_BY(lock_);

  // Currently selected display, or 0 if the full desktop is selected. This
  // capturer does not support full-desktop capture, and will fall back to the
  // first display.
  CGDirectDisplayID current_display_ RTC_GUARDED_BY(lock_) = 0;

  // Configured maximum frame rate in frames per second.
  uint32_t max_frame_rate_ RTC_GUARDED_BY(lock_) = 0;

  // Used by CaptureFrame() to detect if the screen configuration has changed.
  MacDesktopConfiguration desktop_config_ RTC_GUARDED_BY(api_checker_);

  Mutex latest_frame_lock_ RTC_ACQUIRED_AFTER(lock_);
  std::unique_ptr<SharedDesktopFrame> latest_frame_
      RTC_GUARDED_BY(latest_frame_lock_);

  int32_t latest_frame_dpi_ RTC_GUARDED_BY(latest_frame_lock_) = kStandardDPI;

  // Tracks whether the latest frame contains new data since it was returned to
  // the caller. This is used to set the DesktopFrame's `updated_region`
  // property. The flag is cleared after the frame is sent to OnCaptureResult(),
  // and is set when SCK reports a new frame with non-empty "dirty" rectangles.
  // TODO: crbug.com/327458809 - Replace this flag with ScreenCapturerHelper to
  // more accurately track the dirty rectangles from the
  // SCStreamFrameInfoDirtyRects attachment.
  bool frame_is_dirty_ RTC_GUARDED_BY(latest_frame_lock_) = true;

  // Tracks whether a reconfigure is needed.
  bool frame_needs_reconfigure_ RTC_GUARDED_BY(latest_frame_lock_) = false;
  // If a reconfigure is needed, this will be set to the size in pixels required
  // to fit the entire source without downscaling.
  std::optional<CGSize> frame_reconfigure_img_size_
      RTC_GUARDED_BY(latest_frame_lock_);
};

/* Helper class for stringifying SCContentSharingPickerMode. Needed as
 * SCContentSharingPickerMode is a typedef to NSUInteger which we cannot add a
 * AbslStringify function for. */
struct StringifiableSCContentSharingPickerMode {
  const SCContentSharingPickerMode modes_;

  template <typename Sink>
  friend void AbslStringify(Sink& sink,
                            const StringifiableSCContentSharingPickerMode& m) {
    auto modes = m.modes_;
    if (@available(macos 14, *)) {
      bool empty = true;
      const std::tuple<SCContentSharingPickerMode, const char*> all_modes[] = {
          {SCContentSharingPickerModeSingleWindow, "SingleWindow"},
          {SCContentSharingPickerModeMultipleWindows, "MultiWindow"},
          {SCContentSharingPickerModeSingleApplication, "SingleApp"},
          {SCContentSharingPickerModeMultipleApplications, "MultiApp"},
          {SCContentSharingPickerModeSingleDisplay, "SingleDisplay"}};
      for (const auto& [mode, text] : all_modes) {
        if (modes & mode) {
          modes = modes & (~mode);
          absl::Format(&sink, "%s%s", empty ? "" : "|", text);
          empty = false;
        }
      }
      if (modes) {
        absl::Format(&sink, "%sRemaining=%v", empty ? "" : "|", modes);
      }
      return;
    }
    absl::Format(&sink, "%v", modes);
  }
};

ScreenCapturerSck::ScreenCapturerSck(const DesktopCaptureOptions& options,
                                     SCContentSharingPickerMode modes)
    : api_checker_(SequenceChecker::kDetached),
      capture_options_(options),
      picker_modes_(modes) {
  helper_ = [[SckHelper alloc] initWithCapturer:this];
}

ScreenCapturerSck::ScreenCapturerSck(const DesktopCaptureOptions& options)
    : ScreenCapturerSck(options, SCContentSharingPickerModeSingleDisplay) {}

ScreenCapturerSck::~ScreenCapturerSck() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " destroyed.";
  [stream_ stopCaptureWithCompletionHandler:nil];
  [helper_ releaseCapturer];
}

void ScreenCapturerSck::Start(DesktopCapturer::Callback* callback) {
  RTC_DCHECK_RUN_ON(&api_checker_);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  callback_ = callback;
  desktop_config_ =
      capture_options_.configuration_monitor()->desktop_configuration();
  if (capture_options_.allow_sck_system_picker()) {
    EnsureVisible();
    return;
  }
  StartOrReconfigureCapturer();
}

void ScreenCapturerSck::SetMaxFrameRate(uint32_t max_frame_rate) {
  RTC_DCHECK_RUN_ON(&api_checker_);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " SetMaxFrameRate("
                   << max_frame_rate << ").";
  bool stream_started = false;
  {
    MutexLock lock(&lock_);
    if (max_frame_rate_ == max_frame_rate) {
      return;
    }

    max_frame_rate_ = max_frame_rate;
    stream_started = stream_;
  }
  if (stream_started) {
    StartOrReconfigureCapturer();
  }
}

void ScreenCapturerSck::CaptureFrame() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  int64_t capture_start_time_millis = webrtc::TimeMillis();

  if (permanent_error_) {
    RTC_LOG(LS_VERBOSE) << "ScreenCapturerSck " << this
                        << " CaptureFrame() -> ERROR_PERMANENT";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  MacDesktopConfiguration new_config =
      capture_options_.configuration_monitor()->desktop_configuration();
  if (!desktop_config_.Equals(new_config)) {
    desktop_config_ = new_config;
    StartOrReconfigureCapturer();
  }

  std::unique_ptr<DesktopFrame> frame;
  bool needs_reconfigure = false;
  {
    MutexLock lock(&latest_frame_lock_);
    if (latest_frame_) {
      frame = latest_frame_->Share();
      if (frame_is_dirty_) {
        frame->mutable_updated_region()->AddRect(
            DesktopRect::MakeSize(frame->size()));
        frame_is_dirty_ = false;
      }
    }
    needs_reconfigure = frame_needs_reconfigure_;
    frame_needs_reconfigure_ = false;
  }

  if (needs_reconfigure) {
    StartOrReconfigureCapturer();
  }

  if (frame) {
    RTC_LOG(LS_VERBOSE) << "ScreenCapturerSck " << this
                        << " CaptureFrame() -> SUCCESS";
    frame->set_capture_time_ms(webrtc::TimeSince(capture_start_time_millis));
    callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
  } else {
    RTC_LOG(LS_VERBOSE) << "ScreenCapturerSck " << this
                        << " CaptureFrame() -> ERROR_TEMPORARY";
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
  }
}

void ScreenCapturerSck::EnsurePickerHandle() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  if (!picker_handle_ && capture_options_.allow_sck_system_picker()) {
    picker_handle_ = CreateSckPickerHandle();
    RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this
                     << " Created picker handle. allow_sck_system_picker="
                     << capture_options_.allow_sck_system_picker()
                     << ", source="
                     << (picker_handle_ ? picker_handle_->Source() : -1)
                     << ", modes="
                     << StringifiableSCContentSharingPickerMode{
                            .modes_ = picker_modes_};
  }
}

void ScreenCapturerSck::EnsureVisible() {
  RTC_DCHECK_RUN_ON(&api_checker_);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  EnsurePickerHandle();
  if (picker_handle_) {
    if (!picker_handle_registered_) {
      picker_handle_registered_ = true;
      [picker_handle_->GetPicker() addObserver:helper_];
    }
  } else {
    // We reached the maximum number of streams.
    RTC_LOG(LS_ERROR)
        << "ScreenCapturerSck " << this
        << " EnsureVisible() reached the maximum number of streams.";
    permanent_error_ = true;
    return;
  }
  SCContentSharingPicker* picker = picker_handle_->GetPicker();
  SCStream* stream;
  {
    MutexLock lock(&lock_);
    stream = stream_;
    stream_ = nil;
    filter_ = nil;
    MutexLock lock2(&latest_frame_lock_);
    frame_needs_reconfigure_ = false;
    frame_reconfigure_img_size_ = std::nullopt;
  }
  [stream removeStreamOutput:helper_ type:SCStreamOutputTypeScreen error:nil];
  [stream stopCaptureWithCompletionHandler:nil];
  SCContentSharingPickerConfiguration* config = picker.defaultConfiguration;
  config.allowedPickerModes = picker_modes_;
  picker.defaultConfiguration = config;
  SCShareableContentStyle style = SCShareableContentStyleNone;
  // Pick a sensible style to start out with, based on our current mode.
  if (@available(macOS 15, *)) {
    // Stick with None because if we use Display, the picker doesn't let us
    // pick a window when first opened. Behaves like Window in 14 except doesn't
    // change window focus.
  } else {
    // Default to Display because if using Window the picker automatically hides
    // our current window to show others. Saves a click compared to None when
    // picking a display.
    style = SCShareableContentStyleDisplay;
  }
  if (picker_modes_ == SCContentSharingPickerModeSingleDisplay) {
    style = SCShareableContentStyleDisplay;
  } else if (picker_modes_ == SCContentSharingPickerModeSingleWindow ||
             picker_modes_ == SCContentSharingPickerModeMultipleWindows) {
    style = SCShareableContentStyleWindow;
  } else if (picker_modes_ == SCContentSharingPickerModeSingleApplication ||
             picker_modes_ == SCContentSharingPickerModeMultipleApplications) {
    style = SCShareableContentStyleApplication;
  }
  // This dies silently if maximumStreamCount streams are already running. We
  // need our own stream count bookkeeping because of this, and to be able to
  // unset `active`.
  [picker presentPickerForStream:stream usingContentStyle:style];
}

void ScreenCapturerSck::NotifySourceSelection(SCContentFilter* filter,
                                              SCStream* stream) {
  MutexLock lock(&lock_);
  if (stream_ != stream) {
    // The picker selected a source for another capturer.
    RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__
                     << ". stream_ != stream.";
    return;
  }
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__
                   << ". Starting.";
  StartWithFilter(filter);
}

void ScreenCapturerSck::NotifySourceCancelled(SCStream* stream) {
  MutexLock lock(&lock_);
  if (stream_ != stream) {
    // The picker was cancelled for another capturer.
    return;
  }
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  if (!stream_) {
    // The initial picker was cancelled. There is no stream to fall back to.
    permanent_error_ = true;
  }
}

void ScreenCapturerSck::NotifySourceError() {
  {
    MutexLock lock(&lock_);
    if (stream_) {
      // The picker failed to start. But fear not, it was not our picker,
      // we already have a stream!
      return;
    }
  }
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  permanent_error_ = true;
}

void ScreenCapturerSck::NotifyCaptureStopped(SCStream* stream) {
  MutexLock lock(&lock_);
  if (stream_ != stream) {
    return;
  }
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  permanent_error_ = true;
}

bool ScreenCapturerSck::GetSourceList(SourceList* sources) {
  RTC_DCHECK_RUN_ON(&api_checker_);
  sources->clear();
  EnsurePickerHandle();
  if (picker_handle_) {
    sources->push_back({picker_handle_->Source(), std::string()});
  }
  return true;
}

bool ScreenCapturerSck::SelectSource(SourceId id) {
  if (capture_options_.allow_sck_system_picker()) {
    return true;
  }

  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " SelectSource(id=" << id
                   << ").";
  bool stream_started = false;
  {
    MutexLock lock(&lock_);
    if (current_display_ == id) {
      return true;
    }
    current_display_ = id;

    if (stream_) {
      stream_started = true;
    }
  }

  // If the capturer was already started, reconfigure it. Otherwise, wait until
  // Start() gets called.
  if (stream_started) {
    StartOrReconfigureCapturer();
  }

  return true;
}

void ScreenCapturerSck::OnShareableContentCreated(SCShareableContent* content,
                                                  NSError* error) {
  if (!content) {
    RTC_LOG(LS_ERROR) << "ScreenCapturerSck " << this
                      << " getShareableContent failed with error code "
                      << (error ? error.code : 0) << ".";
    permanent_error_ = true;
    return;
  }

  if (!content.displays.count) {
    RTC_LOG(LS_ERROR) << "ScreenCapturerSck " << this
                      << " getShareableContent returned no displays.";
    permanent_error_ = true;
    return;
  }

  MutexLock lock(&lock_);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__
                   << ". current_display_=" << current_display_;
  SCDisplay* captured_display;
  for (SCDisplay* display in content.displays) {
    if (current_display_ == display.displayID) {
      captured_display = display;
      break;
    }
  }
  if (!captured_display) {
    if (current_display_ ==
        static_cast<CGDirectDisplayID>(kFullDesktopScreenId)) {
      RTC_LOG(LS_WARNING) << "ScreenCapturerSck " << this
                          << " Full screen "
                             "capture is not supported, falling back to first "
                             "display.";
    } else {
      RTC_LOG(LS_WARNING) << "ScreenCapturerSck " << this << " Display "
                          << current_display_
                          << " not found, falling back to "
                             "first display.";
    }
    captured_display = content.displays.firstObject;
  }

  SCContentFilter* filter =
      [[SCContentFilter alloc] initWithDisplay:captured_display
                              excludingWindows:@[]];
  StartWithFilter(filter);
}

void ScreenCapturerSck::StartWithFilter(SCContentFilter* __strong filter) {
  lock_.AssertHeld();
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
  config.pixelFormat = kCVPixelFormatType_32BGRA;
  config.colorSpaceName = kCGColorSpaceSRGB;
  config.showsCursor = capture_options_.prefer_cursor_embedded();
  config.captureResolution = SCCaptureResolutionNominal;
  config.minimumFrameInterval = max_frame_rate_ > 0 ?
      CMTimeMake(1, static_cast<int32_t>(max_frame_rate_)) :
      kCMTimeZero;

  {
    MutexLock lock(&latest_frame_lock_);
    latest_frame_dpi_ = filter.pointPixelScale * kStandardDPI;
    if (filter_ != filter) {
      frame_reconfigure_img_size_ = std::nullopt;
    }
    auto sourceImgRect = frame_reconfigure_img_size_.value_or(
        CGSizeMake(filter.contentRect.size.width * filter.pointPixelScale,
                   filter.contentRect.size.height * filter.pointPixelScale));
    config.width = sourceImgRect.width;
    config.height = sourceImgRect.height;
  }

  filter_ = filter;

  if (stream_) {
    RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this
                     << " Updating stream configuration to size="
                     << config.width << "x" << config.height
                     << " and max_frame_rate=" << max_frame_rate_ << ".";
    [stream_ updateContentFilter:filter completionHandler:nil];
    [stream_ updateConfiguration:config completionHandler:nil];
  } else {
    RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " Creating new stream.";
    stream_ = [[SCStream alloc] initWithFilter:filter
                                 configuration:config
                                      delegate:helper_];

    // TODO: crbug.com/327458809 - Choose an appropriate sampleHandlerQueue for
    // best performance.
    NSError* add_stream_output_error;
    bool add_stream_output_result =
        [stream_ addStreamOutput:helper_
                            type:SCStreamOutputTypeScreen
              sampleHandlerQueue:nil
                           error:&add_stream_output_error];
    if (!add_stream_output_result) {
      stream_ = nil;
      filter_ = nil;
      RTC_LOG(LS_ERROR) << "ScreenCapturerSck " << this
                        << " addStreamOutput failed.";
      permanent_error_ = true;
      return;
    }

    auto handler = ^(NSError* error) {
      if (error) {
        // It should be safe to access `this` here, because the C++ destructor
        // calls stopCaptureWithCompletionHandler on the stream, which cancels
        // this handler.
        permanent_error_ = true;
        RTC_LOG(LS_ERROR) << "ScreenCapturerSck " << this
                          << " Starting failed.";
      } else {
        RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " Capture started.";
      }
    };

    [stream_ startCaptureWithCompletionHandler:handler];
  }
}

void ScreenCapturerSck::OnNewIOSurface(IOSurfaceRef io_surface,
                                       NSDictionary* attachment) {
  bool has_frame_to_process = false;
  if (auto status_nr = (NSNumber*)attachment[SCStreamFrameInfoStatus]) {
    auto status = (SCFrameStatus)[status_nr integerValue];
    has_frame_to_process =
        status == SCFrameStatusComplete || status == SCFrameStatusStarted;
  }
  if (!has_frame_to_process) {
    return;
  }

  double scale_factor = 1;
  if (auto factor = (NSNumber*)attachment[SCStreamFrameInfoScaleFactor]) {
    scale_factor = [factor floatValue];
  }
  double content_scale = 1;
  if (auto scale = (NSNumber*)attachment[SCStreamFrameInfoContentScale]) {
    content_scale = [scale floatValue];
  }
  CGRect content_rect = {};
  if (const auto* rect_dict =
          (__bridge CFDictionaryRef)attachment[SCStreamFrameInfoContentRect]) {
    if (!CGRectMakeWithDictionaryRepresentation(rect_dict, &content_rect)) {
      content_rect = CGRect();
    }
  }
  CGRect bounding_rect = {};
  if (const auto* rect_dict =
          (__bridge CFDictionaryRef)attachment[SCStreamFrameInfoBoundingRect]) {
    if (!CGRectMakeWithDictionaryRepresentation(rect_dict, &bounding_rect)) {
      bounding_rect = CGRect();
    }
  }
  CGRect overlay_rect = {};
  if (@available(macOS 14.2, *)) {
    if (const auto* rect_dict = (__bridge CFDictionaryRef)
            attachment[SCStreamFrameInfoPresenterOverlayContentRect]) {
      if (!CGRectMakeWithDictionaryRepresentation(rect_dict, &overlay_rect)) {
        overlay_rect = CGRect();
      }
    }
  }
  const auto* dirty_rects = (NSArray*)attachment[SCStreamFrameInfoDirtyRects];

  auto img_bounding_rect = CGRectMake(scale_factor * bounding_rect.origin.x,
                                      scale_factor * bounding_rect.origin.y,
                                      scale_factor * bounding_rect.size.width,
                                      scale_factor * bounding_rect.size.height);

  webrtc::ScopedCFTypeRef<IOSurfaceRef> scoped_io_surface(
      io_surface, webrtc::RetainPolicy::RETAIN);
  std::unique_ptr<DesktopFrameIOSurface> desktop_frame_io_surface =
      DesktopFrameIOSurface::Wrap(scoped_io_surface, img_bounding_rect);
  if (!desktop_frame_io_surface) {
    RTC_LOG(LS_ERROR) << "Failed to lock IOSurface.";
    return;
  }

  const size_t width = IOSurfaceGetWidth(io_surface);
  const size_t height = IOSurfaceGetHeight(io_surface);

  RTC_LOG(LS_VERBOSE) << "ScreenCapturerSck " << this << " " << __func__
                      << ". New surface: width=" << width
                      << ", height=" << height << ", content_rect="
                      << NSStringFromRect(content_rect).UTF8String
                      << ", bounding_rect="
                      << NSStringFromRect(bounding_rect).UTF8String
                      << ", overlay_rect=("
                      << NSStringFromRect(overlay_rect).UTF8String
                      << ", scale_factor=" << scale_factor
                      << ", content_scale=" << content_scale
                      << ". Cropping to rect "
                      << NSStringFromRect(img_bounding_rect).UTF8String << ".";

  std::unique_ptr<SharedDesktopFrame> frame =
      SharedDesktopFrame::Wrap(std::move(desktop_frame_io_surface));

  bool dirty;
  {
    MutexLock lock(&latest_frame_lock_);
    // Mark the frame as dirty if it has a different size, and ignore any
    // DirtyRects attachment in this case. This is because SCK does not apply a
    // correct attachment to the frame in the case where the stream was
    // reconfigured.
    dirty = !latest_frame_ || !latest_frame_->size().equals(frame->size());
  }

  if (!dirty) {
    if (!dirty_rects) {
      // This is never expected to happen - SCK attaches a non-empty dirty-rects
      // list to every frame, even when nothing has changed.
      return;
    }
    for (NSUInteger i = 0; i < dirty_rects.count; i++) {
      const auto* rect_ptr = (__bridge CFDictionaryRef)dirty_rects[i];
      if (CFGetTypeID(rect_ptr) != CFDictionaryGetTypeID()) {
        // This is never expected to happen - the dirty-rects attachment should
        // always be an array of dictionaries.
        return;
      }
      CGRect rect{};
      CGRectMakeWithDictionaryRepresentation(rect_ptr, &rect);
      if (!CGRectIsEmpty(rect)) {
        dirty = true;
        break;
      }
    }
  }

  MutexLock lock(&latest_frame_lock_);
  if (content_scale > 0 && content_scale < 1) {
    frame_needs_reconfigure_ = true;
    double scale = 1 / content_scale;
    frame_reconfigure_img_size_ =
        CGSizeMake(std::ceil(scale * width), std::ceil(scale * height));
  }
  if (dirty) {
    frame->set_dpi(DesktopVector(latest_frame_dpi_, latest_frame_dpi_));
    frame->set_may_contain_cursor(capture_options_.prefer_cursor_embedded());

    frame_is_dirty_ = true;
    std::swap(latest_frame_, frame);
  }
}

void ScreenCapturerSck::StartOrReconfigureCapturer() {
  if (capture_options_.allow_sck_system_picker()) {
    MutexLock lock(&lock_);
    if (filter_) {
      StartWithFilter(filter_);
    }
    return;
  }

  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  // The copy is needed to avoid capturing `this` in the Objective-C block.
  // Accessing `helper_` inside the block is equivalent to `this->helper_` and
  // would crash (UAF) if `this` is deleted before the block is executed.
  SckHelper* local_helper = helper_;
  auto handler = ^(SCShareableContent* content, NSError* error) {
    [local_helper onShareableContentCreated:content error:error];
  };

  [SCShareableContent getShareableContentWithCompletionHandler:handler];
}

bool ScreenCapturerSckAvailable() {
  static bool available = ([] {
    if (@available(macOS 14.0, *)) {
      return true;
    }
    return false;
  })();
  return available;
}

std::unique_ptr<DesktopCapturer> CreateScreenCapturerSck(
    const DesktopCaptureOptions& options) {
  if (@available(macOS 14.0, *)) {
    return std::make_unique<ScreenCapturerSck>(options);
  }
  return nullptr;
}

bool GenericCapturerSckWithPickerAvailable() {
  bool available = false;
  if (@available(macOS 14.0, *)) {
    available = true;
  }
  return available;
}

std::unique_ptr<DesktopCapturer> CreateGenericCapturerSck(
    const DesktopCaptureOptions& options) {
  if (@available(macOS 14.0, *)) {
    if (options.allow_sck_system_picker()) {
      return std::make_unique<ScreenCapturerSck>(
          options,
          SCContentSharingPickerModeSingleDisplay |
              SCContentSharingPickerModeMultipleWindows);
    }
  }
  return nullptr;
}

}  // namespace webrtc

@implementation SckHelper {
  // This lock is to prevent the capturer being destroyed while an instance
  // method is still running on another thread.
  webrtc::Mutex _capturer_lock;
  webrtc::ScreenCapturerSck* _capturer;
}

- (instancetype)initWithCapturer:(webrtc::ScreenCapturerSck*)capturer {
  self = [super init];
  if (self) {
    _capturer = capturer;
  }
  return self;
}

- (void)onShareableContentCreated:(SCShareableContent*)content
                            error:(NSError*)error {
  webrtc::MutexLock lock(&_capturer_lock);
  if (_capturer) {
    _capturer->OnShareableContentCreated(content, error);
  }
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__
                   << ".";
  if (_capturer) {
    _capturer->NotifyCaptureStopped(stream);
  }
}

- (void)userDidStopStream:(SCStream*)stream NS_SWIFT_NAME(userDidStopStream(_:))
                              API_AVAILABLE(macos(14.4)) {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__
                   << ".";
  if (_capturer) {
    _capturer->NotifyCaptureStopped(stream);
  }
}

- (void)contentSharingPicker:(SCContentSharingPicker*)picker
         didUpdateWithFilter:(SCContentFilter*)filter
                   forStream:(SCStream*)stream {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__
                   << ".";
  if (_capturer) {
    _capturer->NotifySourceSelection(filter, stream);
  }
}

- (void)contentSharingPicker:(SCContentSharingPicker*)picker
          didCancelForStream:(SCStream*)stream {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__
                   << ".";
  if (_capturer) {
    _capturer->NotifySourceCancelled(stream);
  }
}

- (void)contentSharingPickerStartDidFailWithError:(NSError*)error {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__
                   << ". error.code=" << error.code;
  if (_capturer) {
    _capturer->NotifySourceError();
  }
}

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (!pixelBuffer) {
    return;
  }

  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  if (!ioSurface) {
    return;
  }

  CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(
      sampleBuffer, /*createIfNecessary=*/false);
  if (!attachmentsArray || CFArrayGetCount(attachmentsArray) <= 0) {
    RTC_LOG(LS_ERROR) << "Discarding frame with no attachments.";
    return;
  }

  CFDictionaryRef attachment =
      static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, 0));

  webrtc::MutexLock lock(&_capturer_lock);
  if (_capturer) {
    _capturer->OnNewIOSurface(ioSurface, (__bridge NSDictionary*)attachment);
  }
}

- (void)releaseCapturer {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__
                   << ".";
  _capturer = nullptr;
}

@end
