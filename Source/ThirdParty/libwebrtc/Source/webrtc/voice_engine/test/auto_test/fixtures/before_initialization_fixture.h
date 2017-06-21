/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_H_
#define SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_H_

#include "webrtc/common_types.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_common.h"

// This convenient fixture sets up all voice engine interfaces automatically for
// use by testing subclasses. It allocates each interface and releases it once
// which means that if a tests allocates additional interfaces from the voice
// engine and forgets to release it, this test will fail in the destructor.
// It will not call any init methods.
//
// Implementation note:
// The interface fetching is done in the constructor and not SetUp() since
// this relieves our subclasses from calling SetUp in the superclass if they
// choose to override SetUp() themselves. This is fine as googletest will
// construct new test objects for each method.
class BeforeInitializationFixture : public testing::Test {
 public:
  BeforeInitializationFixture();
  virtual ~BeforeInitializationFixture();

 protected:
  // Use this sleep function to sleep in tests.
  void Sleep(long milliseconds);

  webrtc::VoiceEngine*        voice_engine_;
  webrtc::VoEBase*            voe_base_;
  webrtc::VoECodec*           voe_codec_;
  webrtc::VoERTP_RTCP*        voe_rtp_rtcp_;
  webrtc::VoENetwork*         voe_network_;
  webrtc::VoEFile*            voe_file_;
};

#endif  // SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_H_
