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
#include "modules/audio_coding/test/opus_test.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

TEST(AudioCodingModuleTest, TestAllCodecs) {
  webrtc::TestAllCodecs().Perform();
}

#if defined(WEBRTC_ANDROID)
TEST(AudioCodingModuleTest, DISABLED_TestEncodeDecode) {
#else
TEST(AudioCodingModuleTest, TestEncodeDecode) {
#endif
  webrtc::EncodeDecodeTest().Perform();
}

TEST(AudioCodingModuleTest, TestRedFec) {
  webrtc::TestRedFec().Perform();
}

// Disabled on ios as flaky, see https://crbug.com/webrtc/7057
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
TEST(AudioCodingModuleTest, DISABLED_TestStereo) {
#else
TEST(AudioCodingModuleTest, TestStereo) {
#endif
  webrtc::TestStereo().Perform();
}

TEST(AudioCodingModuleTest, TestWebRtcVadDtx) {
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
