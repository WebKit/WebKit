/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_MOCK_VOE_OBSERVER_H_
#define WEBRTC_VOICE_ENGINE_MOCK_VOE_OBSERVER_H_

#include "webrtc/test/gmock.h"
#include "webrtc/voice_engine/include/voe_base.h"

namespace webrtc {

class MockVoEObserver: public VoiceEngineObserver {
 public:
  MockVoEObserver() {}
  virtual ~MockVoEObserver() {}

  MOCK_METHOD2(CallbackOnError, void(int channel, int error_code));
};

}

#endif  // WEBRTC_VOICE_ENGINE_MOCK_VOE_OBSERVER_H_
