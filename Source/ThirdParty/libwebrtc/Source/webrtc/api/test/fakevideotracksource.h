/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_TEST_FAKEVIDEOTRACKSOURCE_H_
#define WEBRTC_API_TEST_FAKEVIDEOTRACKSOURCE_H_

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/videotracksource.h"
#include "webrtc/media/base/fakevideocapturer.h"

namespace webrtc {

class FakeVideoTrackSource : public VideoTrackSource {
 public:
  static rtc::scoped_refptr<FakeVideoTrackSource> Create() {
    return new rtc::RefCountedObject<FakeVideoTrackSource>();
  }

  cricket::FakeVideoCapturer* fake_video_capturer() {
    return &fake_video_capturer_;
  }

 protected:
  FakeVideoTrackSource()
      : VideoTrackSource(&fake_video_capturer_,
                         false /* remote */) {}
  virtual ~FakeVideoTrackSource() {}

 private:
  cricket::FakeVideoCapturer fake_video_capturer_;
};

}  // namespace webrtc

#endif  // WEBRTC_API_TEST_FAKEVIDEOTRACKSOURCE_H_
