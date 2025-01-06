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

#include "modules/desktop_capture/mac/desktop_frame_iosurface.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/helpers/scoped_cftyperef.h"

using webrtc::DesktopFrameIOSurface;

namespace webrtc {
class ScreenCapturerSck;
}  // namespace webrtc

// The ScreenCaptureKit API was available in macOS 12.3, but full-screen capture was reported to be
// broken before macOS 13 - see http://crbug.com/40234870.
// Also, the `SCContentFilter` fields `contentRect` and `pointPixelScale` were introduced in
// macOS 14.
API_AVAILABLE(macos(14.0))
@interface SckHelper : NSObject <SCStreamDelegate, SCStreamOutput>

- (instancetype)initWithCapturer:(webrtc::ScreenCapturerSck*)capturer;

- (void)onShareableContentCreated:(SCShareableContent*)content;

// Called just before the capturer is destroyed. This avoids a dangling pointer, and prevents any
// new calls into a deleted capturer. If any method-call on the capturer is currently running on a
// different thread, this blocks until it completes.
- (void)releaseCapturer;

@end

namespace webrtc {

class API_AVAILABLE(macos(14.0)) ScreenCapturerSck final : public DesktopCapturer {
 public:
  explicit ScreenCapturerSck(const DesktopCaptureOptions& options);

  ScreenCapturerSck(const ScreenCapturerSck&) = delete;
  ScreenCapturerSck& operator=(const ScreenCapturerSck&) = delete;

  ~ScreenCapturerSck() override;

  // DesktopCapturer interface. All these methods run on the caller's thread.
  void Start(DesktopCapturer::Callback* callback) override;
  void SetMaxFrameRate(uint32_t max_frame_rate) override;
  void CaptureFrame() override;
  bool SelectSource(SourceId id) override;

  // Called by SckHelper when shareable content is returned by ScreenCaptureKit. `content` will be
  // nil if an error occurred. May run on an arbitrary thread.
  void OnShareableContentCreated(SCShareableContent* content);

  // Called by SckHelper to notify of a newly captured frame. May run on an arbitrary thread.
  void OnNewIOSurface(IOSurfaceRef io_surface, CFDictionaryRef attachment);

 private:
  // Called when starting the capturer or the configuration has changed (either from a
  // SelectSource() call, or the screen-resolution has changed). This tells SCK to fetch new
  // shareable content, and the completion-handler will either start a new stream, or reconfigure
  // the existing stream. Runs on the caller's thread.
  void StartOrReconfigureCapturer();

  // Helper object to receive Objective-C callbacks from ScreenCaptureKit and call into this C++
  // object. The helper may outlive this C++ instance, if a completion-handler is passed to
  // ScreenCaptureKit APIs and the C++ object is deleted before the handler executes.
  SckHelper* __strong helper_;

  // Callback for returning captured frames, or errors, to the caller. Only used on the caller's
  // thread.
  Callback* callback_ = nullptr;

  // Options passed to the constructor. May be accessed on any thread, but the options are
  // unchanged during the capturer's lifetime.
  DesktopCaptureOptions capture_options_;

  // Signals that a permanent error occurred. This may be set on any thread, and is read by
  // CaptureFrame() which runs on the caller's thread.
  std::atomic<bool> permanent_error_ = false;

  // Guards some variables that may be accessed on different threads.
  Mutex lock_;

  // Provides captured desktop frames.
  SCStream* __strong stream_ RTC_GUARDED_BY(lock_);

  // Currently selected display, or 0 if the full desktop is selected. This capturer does not
  // support full-desktop capture, and will fall back to the first display.
  CGDirectDisplayID current_display_ RTC_GUARDED_BY(lock_) = 0;

  // Used by CaptureFrame() to detect if the screen configuration has changed. Only used on the
  // caller's thread.
  MacDesktopConfiguration desktop_config_;

  Mutex latest_frame_lock_;
  std::unique_ptr<SharedDesktopFrame> latest_frame_ RTC_GUARDED_BY(latest_frame_lock_);

  int32_t latest_frame_dpi_ RTC_GUARDED_BY(latest_frame_lock_) = kStandardDPI;

  // Tracks whether the latest frame contains new data since it was returned to the caller. This is
  // used to set the DesktopFrame's `updated_region` property. The flag is cleared after the frame
  // is sent to OnCaptureResult(), and is set when SCK reports a new frame with non-empty "dirty"
  // rectangles.
  // TODO: crbug.com/327458809 - Replace this flag with ScreenCapturerHelper to more accurately
  // track the dirty rectangles from the SCStreamFrameInfoDirtyRects attachment.
  bool frame_is_dirty_ RTC_GUARDED_BY(latest_frame_lock_) = true;
};

ScreenCapturerSck::ScreenCapturerSck(const DesktopCaptureOptions& options)
    : capture_options_(options) {
  helper_ = [[SckHelper alloc] initWithCapturer:this];
}

ScreenCapturerSck::~ScreenCapturerSck() {
  [stream_ stopCaptureWithCompletionHandler:nil];
  [helper_ releaseCapturer];
}

void ScreenCapturerSck::Start(DesktopCapturer::Callback* callback) {
  callback_ = callback;
  desktop_config_ = capture_options_.configuration_monitor()->desktop_configuration();
  StartOrReconfigureCapturer();
}

void ScreenCapturerSck::SetMaxFrameRate(uint32_t /* max_frame_rate */) {
  // TODO: crbug.com/327458809 - Implement this.
}

void ScreenCapturerSck::CaptureFrame() {
  int64_t capture_start_time_millis = rtc::TimeMillis();

  if (permanent_error_) {
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
  {
    MutexLock lock(&latest_frame_lock_);
    if (latest_frame_) {
      frame = latest_frame_->Share();
      frame->set_dpi(DesktopVector(latest_frame_dpi_, latest_frame_dpi_));
      if (frame_is_dirty_) {
        frame->mutable_updated_region()->AddRect(DesktopRect::MakeSize(frame->size()));
        frame_is_dirty_ = false;
      }
    }
  }

  if (frame) {
    frame->set_capture_time_ms(rtc::TimeSince(capture_start_time_millis));
    callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
  } else {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
  }
}

bool ScreenCapturerSck::SelectSource(SourceId id) {
  bool stream_started = false;
  {
    MutexLock lock(&lock_);
    current_display_ = id;

    if (stream_) {
      stream_started = true;
    }
  }

  // If the capturer was already started, reconfigure it. Otherwise, wait until Start() gets called.
  if (stream_started) {
    StartOrReconfigureCapturer();
  }

  return true;
}

void ScreenCapturerSck::OnShareableContentCreated(SCShareableContent* content) {
  if (!content) {
    RTC_LOG(LS_ERROR) << "getShareableContent failed.";
    permanent_error_ = true;
    return;
  }

  if (!content.displays.count) {
    RTC_LOG(LS_ERROR) << "getShareableContent returned no displays.";
    permanent_error_ = true;
    return;
  }

  SCDisplay* captured_display;
  {
    MutexLock lock(&lock_);
    for (SCDisplay* display in content.displays) {
      if (current_display_ == display.displayID) {
        captured_display = display;
        break;
      }
    }
    if (!captured_display) {
      if (current_display_ == static_cast<CGDirectDisplayID>(kFullDesktopScreenId)) {
        RTC_LOG(LS_WARNING)
            << "Full screen capture is not supported, falling back to first display.";
      } else {
        RTC_LOG(LS_WARNING) << "Display " << current_display_
                            << " not found, falling back to first display.";
      }
      captured_display = content.displays.firstObject;
    }
  }

  SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:captured_display
                                                    excludingWindows:@[]];
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
  config.pixelFormat = kCVPixelFormatType_32BGRA;
  config.showsCursor = capture_options_.prefer_cursor_embedded();
  config.width = filter.contentRect.size.width * filter.pointPixelScale;
  config.height = filter.contentRect.size.height * filter.pointPixelScale;
  config.captureResolution = SCCaptureResolutionNominal;

  {
    MutexLock lock(&latest_frame_lock_);
    latest_frame_dpi_ = filter.pointPixelScale * kStandardDPI;
  }

  MutexLock lock(&lock_);

  if (stream_) {
    RTC_LOG(LS_INFO) << "Updating stream configuration.";
    [stream_ updateContentFilter:filter completionHandler:nil];
    [stream_ updateConfiguration:config completionHandler:nil];
  } else {
    stream_ = [[SCStream alloc] initWithFilter:filter configuration:config delegate:helper_];

    // TODO: crbug.com/327458809 - Choose an appropriate sampleHandlerQueue for best performance.
    NSError* add_stream_output_error;
    bool add_stream_output_result = [stream_ addStreamOutput:helper_
                                                        type:SCStreamOutputTypeScreen
                                          sampleHandlerQueue:nil
                                                       error:&add_stream_output_error];
    if (!add_stream_output_result) {
      stream_ = nil;
      RTC_LOG(LS_ERROR) << "addStreamOutput failed.";
      permanent_error_ = true;
      return;
    }

    auto handler = ^(NSError* error) {
      if (error) {
        // It should be safe to access `this` here, because the C++ destructor calls
        // stopCaptureWithCompletionHandler on the stream, which cancels this handler.
        permanent_error_ = true;
        RTC_LOG(LS_ERROR) << "startCaptureWithCompletionHandler failed.";
      } else {
        RTC_LOG(LS_INFO) << "Capture started.";
      }
    };

    [stream_ startCaptureWithCompletionHandler:handler];
  }
}

void ScreenCapturerSck::OnNewIOSurface(IOSurfaceRef io_surface, CFDictionaryRef attachment) {
  rtc::ScopedCFTypeRef<IOSurfaceRef> scoped_io_surface(io_surface, rtc::RetainPolicy::RETAIN);
  std::unique_ptr<DesktopFrameIOSurface> desktop_frame_io_surface =
      DesktopFrameIOSurface::Wrap(scoped_io_surface);
  if (!desktop_frame_io_surface) {
    RTC_LOG(LS_ERROR) << "Failed to lock IOSurface.";
    return;
  }

  std::unique_ptr<SharedDesktopFrame> frame =
      SharedDesktopFrame::Wrap(std::move(desktop_frame_io_surface));

  bool dirty;
  {
    MutexLock lock(&latest_frame_lock_);
    // Mark the frame as dirty if it has a different size, and ignore any DirtyRects attachment in
    // this case. This is because SCK does not apply a correct attachment to the frame in the case
    // where the stream was reconfigured.
    dirty = !latest_frame_ || !latest_frame_->size().equals(frame->size());
  }

  if (!dirty) {
    const void* dirty_rects_ptr =
        CFDictionaryGetValue(attachment, (__bridge CFStringRef)SCStreamFrameInfoDirtyRects);
    if (!dirty_rects_ptr) {
      // This is never expected to happen - SCK attaches a non-empty dirty-rects list to every
      // frame, even when nothing has changed.
      return;
    }
    if (CFGetTypeID(dirty_rects_ptr) != CFArrayGetTypeID()) {
      return;
    }

    CFArrayRef dirty_rects_array = static_cast<CFArrayRef>(dirty_rects_ptr);
    int size = CFArrayGetCount(dirty_rects_array);
    for (int i = 0; i < size; i++) {
      const void* rect_ptr = CFArrayGetValueAtIndex(dirty_rects_array, i);
      if (CFGetTypeID(rect_ptr) != CFDictionaryGetTypeID()) {
        // This is never expected to happen - the dirty-rects attachment should always be an array
        // of dictionaries.
        return;
      }
      CGRect rect{};
      CGRectMakeWithDictionaryRepresentation(static_cast<CFDictionaryRef>(rect_ptr), &rect);
      if (!CGRectIsEmpty(rect)) {
        dirty = true;
        break;
      }
    }
  }

  if (dirty) {
    MutexLock lock(&latest_frame_lock_);
    frame_is_dirty_ = true;
    std::swap(latest_frame_, frame);
  }
}

void ScreenCapturerSck::StartOrReconfigureCapturer() {
  // The copy is needed to avoid capturing `this` in the Objective-C block. Accessing `helper_`
  // inside the block is equivalent to `this->helper_` and would crash (UAF) if `this` is
  // deleted before the block is executed.
  SckHelper* local_helper = helper_;
  auto handler = ^(SCShareableContent* content, NSError* /* error */) {
    [local_helper onShareableContentCreated:content];
  };

  [SCShareableContent getShareableContentWithCompletionHandler:handler];
}

std::unique_ptr<DesktopCapturer> CreateScreenCapturerSck(const DesktopCaptureOptions& options) {
  if (@available(macOS 14.0, *)) {
    return std::make_unique<ScreenCapturerSck>(options);
  } else {
    return nullptr;
  }
}

}  // namespace webrtc

@implementation SckHelper {
  // This lock is to prevent the capturer being destroyed while an instance method is still running
  // on another thread.
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

- (void)onShareableContentCreated:(SCShareableContent*)content {
  webrtc::MutexLock lock(&_capturer_lock);
  if (_capturer) {
    _capturer->OnShareableContentCreated(content);
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

  CFArrayRef attachmentsArray =
      CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, /*createIfNecessary=*/false);
  if (!attachmentsArray || CFArrayGetCount(attachmentsArray) <= 0) {
    RTC_LOG(LS_ERROR) << "Discarding frame with no attachments.";
    return;
  }

  CFDictionaryRef attachment =
      static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, 0));

  webrtc::MutexLock lock(&_capturer_lock);
  if (_capturer) {
    _capturer->OnNewIOSurface(ioSurface, attachment);
  }
}

- (void)releaseCapturer {
  webrtc::MutexLock lock(&_capturer_lock);
  _capturer = nullptr;
}

@end
