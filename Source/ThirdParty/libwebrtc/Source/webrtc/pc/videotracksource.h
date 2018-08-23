/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_VIDEOTRACKSOURCE_H_
#define PC_VIDEOTRACKSOURCE_H_

#include "api/mediastreaminterface.h"
#include "api/notifier.h"
#include "api/video/video_sink_interface.h"
#include "media/base/mediachannel.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

// VideoTrackSource is a convenience base class for implementations of
// VideoTrackSourceInterface.
class VideoTrackSource : public Notifier<VideoTrackSourceInterface> {
 public:
  explicit VideoTrackSource(bool remote);
  void SetState(SourceState new_state);

  SourceState state() const override { return state_; }
  bool remote() const override { return remote_; }

  bool is_screencast() const override { return false; }
  absl::optional<bool> needs_denoising() const override {
    return absl::nullopt;
  }

  bool GetStats(Stats* stats) override { return false; }

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

 protected:
  virtual rtc::VideoSourceInterface<VideoFrame>* source() = 0;

 private:
  rtc::ThreadChecker worker_thread_checker_;
  SourceState state_;
  const bool remote_;
};

}  // namespace webrtc

#endif  //  PC_VIDEOTRACKSOURCE_H_
