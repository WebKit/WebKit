/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEOCAPTURERTRACKSOURCE_H_
#define WEBRTC_API_VIDEOCAPTURERTRACKSOURCE_H_

#include <memory>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/videotracksource.h"
#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocommon.h"

// VideoCapturerTrackSource implements VideoTrackSourceInterface. It owns a
// cricket::VideoCapturer and make sure the camera is started at a resolution
// that honors the constraints.
// The state is set depending on the result of starting the capturer.
// If the constraint can't be met or the capturer fails to start, the state
// transition to kEnded, otherwise it transitions to kLive.
namespace webrtc {

class MediaConstraintsInterface;

class VideoCapturerTrackSource : public VideoTrackSource,
                                 public sigslot::has_slots<> {
 public:
  // Creates an instance of VideoCapturerTrackSource.
  // VideoCapturerTrackSource take ownership of |capturer|.
  // |constraints| can be NULL and in that case the camera is opened using a
  // default resolution.
  static rtc::scoped_refptr<VideoTrackSourceInterface> Create(
      rtc::Thread* worker_thread,
      cricket::VideoCapturer* capturer,
      const webrtc::MediaConstraintsInterface* constraints,
      bool remote);

  static rtc::scoped_refptr<VideoTrackSourceInterface> Create(
      rtc::Thread* worker_thread,
      cricket::VideoCapturer* capturer,
      bool remote);

  bool is_screencast() const override {
    return video_capturer_->IsScreencast();
  }
  rtc::Optional<bool> needs_denoising() const override {
    return needs_denoising_;
  }

  bool GetStats(Stats* stats) override;

 protected:
  VideoCapturerTrackSource(rtc::Thread* worker_thread,
                           cricket::VideoCapturer* capturer,
                           bool remote);
  virtual ~VideoCapturerTrackSource();
  void Initialize(const webrtc::MediaConstraintsInterface* constraints);

 private:
  void Stop();

  void OnStateChange(cricket::VideoCapturer* capturer,
                     cricket::CaptureState capture_state);

  rtc::Thread* signaling_thread_;
  rtc::Thread* worker_thread_;
  rtc::AsyncInvoker invoker_;
  std::unique_ptr<cricket::VideoCapturer> video_capturer_;
  bool started_;
  cricket::VideoFormat format_;
  rtc::Optional<bool> needs_denoising_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEOCAPTURERTRACKSOURCE_H_
