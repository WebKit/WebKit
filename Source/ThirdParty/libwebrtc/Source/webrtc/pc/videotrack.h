/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_VIDEOTRACK_H_
#define PC_VIDEOTRACK_H_

#include <string>
#include <vector>

#include "media/base/videosourcebase.h"
#include "pc/mediastreamtrack.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

class VideoTrack : public MediaStreamTrack<VideoTrackInterface>,
                   public rtc::VideoSourceBase,
                   public ObserverInterface {
 public:
  static rtc::scoped_refptr<VideoTrack> Create(
      const std::string& label,
      VideoTrackSourceInterface* source,
      rtc::Thread* worker_thread);

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override;
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override;

  VideoTrackSourceInterface* GetSource() const override {
    return video_source_.get();
  }
  ContentHint content_hint() const override;
  void set_content_hint(ContentHint hint) override;
  bool set_enabled(bool enable) override;
  std::string kind() const override;

 protected:
  VideoTrack(const std::string& id,
             VideoTrackSourceInterface* video_source,
             rtc::Thread* worker_thread);
  ~VideoTrack();

 private:
  // Implements ObserverInterface. Observes |video_source_| state.
  void OnChanged() override;

  rtc::Thread* const worker_thread_;
  rtc::ThreadChecker signaling_thread_checker_;
  rtc::scoped_refptr<VideoTrackSourceInterface> video_source_;
  ContentHint content_hint_ RTC_GUARDED_BY(signaling_thread_checker_);
};

}  // namespace webrtc

#endif  // PC_VIDEOTRACK_H_
