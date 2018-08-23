/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Declaration of abstract class VideoCapturer

#ifndef MEDIA_BASE_VIDEOCAPTURER_H_
#define MEDIA_BASE_VIDEOCAPTURER_H_

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "api/video/video_source_interface.h"
#include "media/base/videoadapter.h"
#include "media/base/videobroadcaster.h"
#include "media/base/videocommon.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread_checker.h"
#include "rtc_base/timestampaligner.h"

namespace webrtc {
class VideoFrame;
}

namespace cricket {

// Current state of the capturer.
enum CaptureState {
  CS_STOPPED,   // The capturer has been stopped or hasn't started yet.
  CS_STARTING,  // The capturer is in the process of starting. Note, it may
                // still fail to start.
  CS_RUNNING,   // The capturer has been started successfully and is now
                // capturing.
  CS_FAILED,    // The capturer failed to start.
};

// VideoCapturer is an abstract class that defines the interfaces for video
// capturing. The subclasses implement the video capturer for various types of
// capturers and various platforms.
//
// The captured frames may need to be adapted (for example, cropping).
// Video adaptation is built into and enabled by default. After a frame has
// been captured from the device, it is sent to the video adapter, then out to
// the sinks.
//
// Programming model:
//   Create an object of a subclass of VideoCapturer
//   Initialize
//   SignalStateChange.connect()
//   AddOrUpdateSink()
//   Find the capture format for Start() by either calling GetSupportedFormats()
//   and selecting one of the supported or calling GetBestCaptureFormat().
//   video_adapter()->OnOutputFormatRequest(desired_encoding_format)
//   Start()
//   GetCaptureFormat() optionally
//   Stop()
//
// Assumption:
//   The Start() and Stop() methods are called by a single thread (E.g., the
//   media engine thread). Hence, the VideoCapture subclasses dont need to be
//   thread safe.
//
class VideoCapturer : public sigslot::has_slots<>,
                      public rtc::VideoSourceInterface<webrtc::VideoFrame> {
 public:
  VideoCapturer();

  ~VideoCapturer() override;

  // Gets the id of the underlying device, which is available after the capturer
  // is initialized. Can be used to determine if two capturers reference the
  // same device.
  const std::string& GetId() const { return id_; }

  // Get the capture formats supported by the video capturer. The supported
  // formats are non empty after the device has been opened successfully.
  const std::vector<VideoFormat>* GetSupportedFormats() const;

  // Get the best capture format for the desired format. The best format is the
  // same as one of the supported formats except that the frame interval may be
  // different. If the application asks for 16x9 and the camera does not support
  // 16x9 HD or the application asks for 16x10, we find the closest 4x3 and then
  // crop; Otherwise, we find what the application asks for. Note that we assume
  // that for HD, the desired format is always 16x9. The subclasses can override
  // the default implementation.
  // Parameters
  //   desired: the input desired format. If desired.fourcc is not kAnyFourcc,
  //            the best capture format has the exactly same fourcc. Otherwise,
  //            the best capture format uses a fourcc in GetPreferredFourccs().
  //   best_format: the output of the best capture format.
  // Return false if there is no such a best format, that is, the desired format
  // is not supported.
  virtual bool GetBestCaptureFormat(const VideoFormat& desired,
                                    VideoFormat* best_format);

  // TODO(hellner): deprecate (make private) the Start API in favor of this one.
  //                Also remove CS_STARTING as it is implied by the return
  //                value of StartCapturing().
  bool StartCapturing(const VideoFormat& capture_format);
  // Start the video capturer with the specified capture format.
  // Parameter
  //   capture_format: The caller got this parameter by either calling
  //                   GetSupportedFormats() and selecting one of the supported
  //                   or calling GetBestCaptureFormat().
  // Return
  //   CS_STARTING:  The capturer is trying to start. Success or failure will
  //                 be notified via the |SignalStateChange| callback.
  //   CS_RUNNING:   if the capturer is started and capturing.
  //   CS_FAILED:    if the capturer failes to start..
  //   CS_NO_DEVICE: if the capturer has no device and fails to start.
  virtual CaptureState Start(const VideoFormat& capture_format) = 0;

  // Get the current capture format, which is set by the Start() call.
  // Note that the width and height of the captured frames may differ from the
  // capture format. For example, the capture format is HD but the captured
  // frames may be smaller than HD.
  const VideoFormat* GetCaptureFormat() const { return capture_format_.get(); }

  // Stop the video capturer.
  virtual void Stop() = 0;
  // Check if the video capturer is running.
  virtual bool IsRunning() = 0;
  CaptureState capture_state() const { return capture_state_; }

  virtual bool apply_rotation();

  // Returns true if the capturer is screencasting. This can be used to
  // implement screencast specific behavior.
  virtual bool IsScreencast() const = 0;

  // Caps the VideoCapturer's format according to max_format. It can e.g. be
  // used to prevent cameras from capturing at a resolution or framerate that
  // the capturer is capable of but not performing satisfactorily at.
  // The capping is an upper bound for each component of the capturing format.
  // The fourcc component is ignored.
  void ConstrainSupportedFormats(const VideoFormat& max_format);

  void set_enable_camera_list(bool enable_camera_list) {
    enable_camera_list_ = enable_camera_list;
  }
  bool enable_camera_list() { return enable_camera_list_; }

  // Signal all capture state changes that are not a direct result of calling
  // Start().
  sigslot::signal2<VideoCapturer*, CaptureState> SignalStateChange;

  // If true, run video adaptation. By default, video adaptation is enabled
  // and users must call video_adapter()->OnOutputFormatRequest()
  // to receive frames.
  bool enable_video_adapter() const { return enable_video_adapter_; }
  void set_enable_video_adapter(bool enable_video_adapter) {
    enable_video_adapter_ = enable_video_adapter;
  }

  bool GetInputSize(int* width, int* height);

  // Implements VideoSourceInterface
  void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

 protected:
  // OnSinkWantsChanged can be overridden to change the default behavior
  // when a sink changes its VideoSinkWants by calling AddOrUpdateSink.
  virtual void OnSinkWantsChanged(const rtc::VideoSinkWants& wants);

  // Reports the appropriate frame size after adaptation. Returns true
  // if a frame is wanted. Returns false if there are no interested
  // sinks, or if the VideoAdapter decides to drop the frame.

  // This function also implements timestamp translation/filtering.
  // |camera_time_ns| is the camera's timestamp for the captured
  // frame; it is expected to have good accuracy, but it may use an
  // arbitrary epoch and a small possibly free-running with a frequency
  // slightly different from the system clock. |system_time_us| is the
  // monotonic system time (in the same scale as rtc::TimeMicros) when
  // the frame was captured; the application is expected to read the
  // system time as soon as possible after frame capture, but it may
  // suffer scheduling jitter or poor system clock resolution. The
  // output |translated_camera_time_us| is a combined timestamp,
  // taking advantage of the supposedly higher accuracy in the camera
  // timestamp, but using the same epoch and frequency as system time.
  bool AdaptFrame(int width,
                  int height,
                  int64_t camera_time_us,
                  int64_t system_time_us,
                  int* out_width,
                  int* out_height,
                  int* crop_width,
                  int* crop_height,
                  int* crop_x,
                  int* crop_y,
                  int64_t* translated_camera_time_us);

  // Called when a frame has been captured and converted to a
  // VideoFrame. OnFrame can be called directly by an implementation
  // that does not use SignalFrameCaptured or OnFrameCaptured. The
  // orig_width and orig_height are used only to produce stats.
  void OnFrame(const webrtc::VideoFrame& frame,
               int orig_width,
               int orig_height);

  VideoAdapter* video_adapter() { return &video_adapter_; }

  void SetCaptureState(CaptureState state);

  // subclasses override this virtual method to provide a vector of fourccs, in
  // order of preference, that are expected by the media engine.
  virtual bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) = 0;

  // mutators to set private attributes
  void SetId(const std::string& id) { id_ = id; }

  void SetCaptureFormat(const VideoFormat* format) {
    capture_format_.reset(format ? new VideoFormat(*format) : NULL);
  }

  void SetSupportedFormats(const std::vector<VideoFormat>& formats);

 private:
  void Construct();
  // Get the distance between the desired format and the supported format.
  // Return the max distance if they mismatch. See the implementation for
  // details.
  int64_t GetFormatDistance(const VideoFormat& desired,
                            const VideoFormat& supported);

  // Updates filtered_supported_formats_ so that it contains the formats in
  // supported_formats_ that fulfill all applied restrictions.
  void UpdateFilteredSupportedFormats();
  // Returns true if format doesn't fulfill all applied restrictions.
  bool ShouldFilterFormat(const VideoFormat& format) const;

  void UpdateInputSize(int width, int height);

  rtc::ThreadChecker thread_checker_;
  std::string id_;
  CaptureState capture_state_;
  std::unique_ptr<VideoFormat> capture_format_;
  std::vector<VideoFormat> supported_formats_;
  std::unique_ptr<VideoFormat> max_format_;
  std::vector<VideoFormat> filtered_supported_formats_;

  bool enable_camera_list_;
  int scaled_width_;  // Current output size from ComputeScale.
  int scaled_height_;

  rtc::VideoBroadcaster broadcaster_;
  bool enable_video_adapter_;
  VideoAdapter video_adapter_;

  rtc::CriticalSection frame_stats_crit_;
  // The captured frame size before potential adapation.
  bool input_size_valid_ RTC_GUARDED_BY(frame_stats_crit_) = false;
  int input_width_ RTC_GUARDED_BY(frame_stats_crit_);
  int input_height_ RTC_GUARDED_BY(frame_stats_crit_);

  // Whether capturer should apply rotation to the frame before
  // passing it on to the registered sinks.
  bool apply_rotation_;

  // State for the timestamp translation.
  rtc::TimestampAligner timestamp_aligner_;
  RTC_DISALLOW_COPY_AND_ASSIGN(VideoCapturer);
};

}  // namespace cricket

#endif  // MEDIA_BASE_VIDEOCAPTURER_H_
