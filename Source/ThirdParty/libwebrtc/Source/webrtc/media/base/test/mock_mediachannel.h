/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_TEST_MOCK_MEDIACHANNEL_H_
#define WEBRTC_MEDIA_BASE_TEST_MOCK_MEDIACHANNEL_H_

#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/test/gmock.h"

namespace webrtc {

class MockVideoMediaChannel : public cricket::FakeVideoMediaChannel {
 public:
  MockVideoMediaChannel() :
      cricket::FakeVideoMediaChannel(NULL, cricket::VideoOptions()) {}
  MOCK_METHOD1(GetStats, bool(cricket::VideoMediaInfo*));
};

class MockVoiceMediaChannel : public cricket::FakeVoiceMediaChannel {
 public:
  MockVoiceMediaChannel() :
      cricket::FakeVoiceMediaChannel(NULL, cricket::AudioOptions()) {}
  MOCK_METHOD1(GetStats, bool(cricket::VoiceMediaInfo*));
};

}  // namespace webrtc

#endif  // WEBRTC_MEDIA_BASE_TEST_MOCK_MEDIACHANNEL_H_
