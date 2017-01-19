/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_AFTER_STREAMING_H_
#define SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_AFTER_STREAMING_H_

#include <memory>

#include "webrtc/voice_engine/channel_proxy.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/before_streaming_fixture.h"

// This fixture will, in addition to the work done by its superclasses,
// start play back on construction.
class AfterStreamingFixture : public BeforeStreamingFixture {
 public:
  AfterStreamingFixture();
  virtual ~AfterStreamingFixture() {}

 protected:
  std::unique_ptr<webrtc::voe::ChannelProxy> channel_proxy_;
};

#endif  // SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_AFTER_STREAMING_H_
