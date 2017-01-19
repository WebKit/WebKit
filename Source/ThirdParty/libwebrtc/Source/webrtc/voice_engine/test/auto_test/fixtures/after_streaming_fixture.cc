/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

AfterStreamingFixture::AfterStreamingFixture()
    : BeforeStreamingFixture() {
  webrtc::VoiceEngineImpl* voe_impl =
      static_cast<webrtc::VoiceEngineImpl*>(voice_engine_);
  channel_proxy_ = voe_impl->GetChannelProxy(channel_);
  ResumePlaying();
}
