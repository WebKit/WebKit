/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string>
#include <vector>

#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/test/EncodeDecodeTest.h"
#include "modules/audio_coding/test/PacketLossTest.h"
#include "modules/audio_coding/test/TestAllCodecs.h"
#include "modules/audio_coding/test/TestRedFec.h"
#include "modules/audio_coding/test/TestStereo.h"
#include "modules/audio_coding/test/TestVADDTX.h"
#include "modules/audio_coding/test/TwoWayCommunication.h"
#include "modules/audio_coding/test/iSACTest.h"
#include "modules/audio_coding/test/opus_test.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

// This parameter is used to describe how to run the tests. It is normally
// set to 0, and all tests are run in quite mode.
#define ACM_TEST_MODE 0

TEST(AudioCodingModuleTest, TestAllCodecs) {
  webrtc::TestAllCodecs(ACM_TEST_MODE).Perform();
}

#if defined(WEBRTC_ANDROID)
TEST(AudioCodingModuleTest, DISABLED_TestEncodeDecode) {
#else
TEST(AudioCodingModuleTest, TestEncodeDecode) {
#endif
  webrtc::EncodeDecodeTest(ACM_TEST_MODE).Perform();
}

#if defined(WEBRTC_CODEC_RED)
#if defined(WEBRTC_ANDROID)
TEST(AudioCodingModuleTest, DISABLED_TestRedFec) {
#else
TEST(AudioCodingModuleTest, TestRedFec) {
#endif
  webrtc::TestRedFec().Perform();
}
#endif

#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
#if defined(WEBRTC_ANDROID)
TEST(AudioCodingModuleTest, DISABLED_TestIsac) {
#else
TEST(AudioCodingModuleTest, TestIsac) {
#endif
  webrtc::ISACTest(ACM_TEST_MODE).Perform();
}
#endif

#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)) && \
    defined(WEBRTC_CODEC_ILBC)
#if defined(WEBRTC_ANDROID)
TEST(AudioCodingModuleTest, DISABLED_TwoWayCommunication) {
#else
TEST(AudioCodingModuleTest, TwoWayCommunication) {
#endif
  webrtc::TwoWayCommunication(ACM_TEST_MODE).Perform();
}
#endif

// Disabled on ios as flaky, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
TEST(AudioCodingModuleTest, DISABLED_TestStereo) {
#else
TEST(AudioCodingModuleTest, TestStereo) {
#endif
  webrtc::TestStereo(ACM_TEST_MODE).Perform();
}

// Disabled on ios as flaky, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
TEST(AudioCodingModuleTest, DISABLED_TestWebRtcVadDtx) {
#else
TEST(AudioCodingModuleTest, TestWebRtcVadDtx) {
#endif
  webrtc::TestWebRtcVadDtx().Perform();
}

TEST(AudioCodingModuleTest, TestOpusDtx) {
  webrtc::TestOpusDtx().Perform();
}

// Disabled on ios as flaky, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_IOS)
TEST(AudioCodingModuleTest, DISABLED_TestOpus) {
#else
TEST(AudioCodingModuleTest, TestOpus) {
#endif
  webrtc::OpusTest().Perform();
}

TEST(AudioCodingModuleTest, TestPacketLoss) {
  webrtc::PacketLossTest(1, 10, 10, 1).Perform();
}

TEST(AudioCodingModuleTest, TestPacketLossBurst) {
  webrtc::PacketLossTest(1, 10, 10, 2).Perform();
}

// Disabled on ios as flake, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_IOS)
TEST(AudioCodingModuleTest, DISABLED_TestPacketLossStereo) {
#else
TEST(AudioCodingModuleTest, TestPacketLossStereo) {
#endif
  webrtc::PacketLossTest(2, 10, 10, 1).Perform();
}

// Disabled on ios as flake, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_IOS)
TEST(AudioCodingModuleTest, DISABLED_TestPacketLossStereoBurst) {
#else
TEST(AudioCodingModuleTest, TestPacketLossStereoBurst) {
#endif
  webrtc::PacketLossTest(2, 10, 10, 2).Perform();
}

// The full API test is too long to run automatically on bots, but can be used
// for offline testing. User interaction is needed.
#ifdef ACM_TEST_FULL_API
TEST(AudioCodingModuleTest, TestAPI) {
  webrtc::APITest().Perform();
}
#endif
