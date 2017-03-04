/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_TEST_FAKEVIDEOTRACKSOURCE_H_
#define WEBRTC_PC_TEST_FAKEVIDEOTRACKSOURCE_H_

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/media/base/fakevideocapturer.h"
#include "webrtc/pc/videotracksource.h"

namespace webrtc {

class FakeVideoTrackSource : public VideoTrackSource {
 public:
  static rtc::scoped_refptr<FakeVideoTrackSource> Create(bool is_screencast) {
    return new rtc::RefCountedObject<FakeVideoTrackSource>(is_screencast);
  }

  static rtc::scoped_refptr<FakeVideoTrackSource> Create() {
    return Create(false);
  }

  cricket::FakeVideoCapturer* fake_video_capturer() {
    return &fake_video_capturer_;
  }

  bool is_screencast() const override { return is_screencast_; }

 protected:
  explicit FakeVideoTrackSource(bool is_screencast)
      : VideoTrackSource(&fake_video_capturer_, false /* remote */),
        is_screencast_(is_screencast) {}
  virtual ~FakeVideoTrackSource() {}

 private:
  cricket::FakeVideoCapturer fake_video_capturer_;
  const bool is_screencast_;
};

}  // namespace webrtc

#endif  // WEBRTC_PC_TEST_FAKEVIDEOTRACKSOURCE_H_
