/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKEVIDEOTRACKSOURCE_H_
#define PC_TEST_FAKEVIDEOTRACKSOURCE_H_

#include "api/mediastreaminterface.h"
#include "pc/videotracksource.h"

namespace webrtc {

// A minimal implementation of VideoTrackSource, which doesn't produce
// any frames.
class FakeVideoTrackSource : public VideoTrackSource {
 public:
  static rtc::scoped_refptr<FakeVideoTrackSource> Create(bool is_screencast) {
    return new rtc::RefCountedObject<FakeVideoTrackSource>(is_screencast);
  }

  static rtc::scoped_refptr<FakeVideoTrackSource> Create() {
    return Create(false);
  }

  bool is_screencast() const override { return is_screencast_; }
  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {}
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) {}

 protected:
  explicit FakeVideoTrackSource(bool is_screencast)
      : VideoTrackSource(false /* remote */), is_screencast_(is_screencast) {}
  ~FakeVideoTrackSource() override = default;

  // Unused, since we override AddOrUpdateSink and RemoveSink above.
  rtc::VideoSourceInterface<VideoFrame>* source() override { return nullptr; }

 private:
  const bool is_screencast_;
};

}  // namespace webrtc

#endif  // PC_TEST_FAKEVIDEOTRACKSOURCE_H_
