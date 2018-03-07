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
#include "media/base/mediachannel.h"
#include "media/base/videosinkinterface.h"
#include "rtc_base/thread_checker.h"

// VideoTrackSource implements VideoTrackSourceInterface.
namespace webrtc {

class VideoTrackSource : public Notifier<VideoTrackSourceInterface> {
 public:
  VideoTrackSource(rtc::VideoSourceInterface<VideoFrame>* source, bool remote);
  void SetState(SourceState new_state);
  // OnSourceDestroyed clears this instance pointer to |source_|. It is useful
  // when the underlying rtc::VideoSourceInterface is destroyed before the
  // reference counted VideoTrackSource.
  void OnSourceDestroyed();

  SourceState state() const override { return state_; }
  bool remote() const override { return remote_; }

  bool is_screencast() const override { return false; }
  rtc::Optional<bool> needs_denoising() const override { return rtc::nullopt; }

  bool GetStats(Stats* stats) override { return false; }

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

 private:
  rtc::ThreadChecker worker_thread_checker_;
  rtc::VideoSourceInterface<VideoFrame>* source_;
  cricket::VideoOptions options_;
  SourceState state_;
  const bool remote_;
};

}  // namespace webrtc

#endif  //  PC_VIDEOTRACKSOURCE_H_
