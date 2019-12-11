/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKEVIDEOTRACKRENDERER_H_
#define PC_TEST_FAKEVIDEOTRACKRENDERER_H_

#include "api/mediastreaminterface.h"
#include "media/base/fakevideorenderer.h"

namespace webrtc {

class FakeVideoTrackRenderer : public cricket::FakeVideoRenderer {
 public:
  explicit FakeVideoTrackRenderer(VideoTrackInterface* video_track)
      : video_track_(video_track) {
    video_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
  }
  ~FakeVideoTrackRenderer() { video_track_->RemoveSink(this); }

 private:
  rtc::scoped_refptr<VideoTrackInterface> video_track_;
};

}  // namespace webrtc

#endif  // PC_TEST_FAKEVIDEOTRACKRENDERER_H_
