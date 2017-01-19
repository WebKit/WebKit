/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/transmit_mixer.h"

#include "webrtc/test/gtest.h"
#include "webrtc/voice_engine/include/voe_external_media.h"

namespace webrtc {
namespace voe {
namespace {

class MediaCallback : public VoEMediaProcess {
 public:
  virtual void Process(int channel, ProcessingTypes type,
                       int16_t audio[], size_t samples_per_channel,
                       int sample_rate_hz, bool is_stereo) {
  }
};

// TODO(andrew): Mock VoEMediaProcess, and verify the behavior when calling
// PrepareDemux().
TEST(TransmitMixerTest, RegisterExternalMediaCallback) {
  TransmitMixer* tm = NULL;
  ASSERT_EQ(0, TransmitMixer::Create(tm, 0));
  ASSERT_TRUE(tm != NULL);
  MediaCallback callback;
  EXPECT_EQ(-1, tm->RegisterExternalMediaProcessing(NULL,
                                                    kRecordingPreprocessing));
  EXPECT_EQ(-1, tm->RegisterExternalMediaProcessing(&callback,
                                                    kPlaybackPerChannel));
  EXPECT_EQ(-1, tm->RegisterExternalMediaProcessing(&callback,
                                                    kPlaybackAllChannelsMixed));
  EXPECT_EQ(-1, tm->RegisterExternalMediaProcessing(&callback,
                                                    kRecordingPerChannel));
  EXPECT_EQ(0, tm->RegisterExternalMediaProcessing(&callback,
                                                   kRecordingAllChannelsMixed));
  EXPECT_EQ(0, tm->RegisterExternalMediaProcessing(&callback,
                                                   kRecordingPreprocessing));
  EXPECT_EQ(-1, tm->DeRegisterExternalMediaProcessing(kPlaybackPerChannel));
  EXPECT_EQ(-1, tm->DeRegisterExternalMediaProcessing(
                    kPlaybackAllChannelsMixed));
  EXPECT_EQ(-1, tm->DeRegisterExternalMediaProcessing(kRecordingPerChannel));
  EXPECT_EQ(0, tm->DeRegisterExternalMediaProcessing(
                   kRecordingAllChannelsMixed));
  EXPECT_EQ(0, tm->DeRegisterExternalMediaProcessing(kRecordingPreprocessing));
  TransmitMixer::Destroy(tm);
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
