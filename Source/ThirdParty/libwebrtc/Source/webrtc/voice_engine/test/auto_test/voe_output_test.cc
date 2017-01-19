/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/random.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/voe_standard_test.h"
#include "webrtc/voice_engine/test/channel_transport/channel_transport.h"

namespace {

const char kIp[] = "127.0.0.1";
const int kPort = 1234;
const webrtc::CodecInst kCodecInst = {120, "opus", 48000, 960, 2, 64000};

}  // namespace

namespace voetest {

using webrtc::Random;
using webrtc::test::VoiceChannelTransport;

// This test allows a check on the output signal in an end-to-end call.
class OutputTest {
 public:
  OutputTest(int16_t lower_bound, int16_t upper_bound);
  ~OutputTest();

  void Start();

  void EnableOutputCheck();
  void DisableOutputCheck();
  void SetOutputBound(int16_t lower_bound, int16_t upper_bound);
  void Mute();
  void Unmute();
  void SetBitRate(int rate);

 private:
  // This class checks all output values and count the number of samples that
  // go out of a defined range.
  class VoEOutputCheckMediaProcess : public VoEMediaProcess {
   public:
    VoEOutputCheckMediaProcess(int16_t lower_bound, int16_t upper_bound);

    void set_enabled(bool enabled) { enabled_ = enabled; }
    void Process(int channel,
                 ProcessingTypes type,
                 int16_t audio10ms[],
                 size_t length,
                 int samplingFreq,
                 bool isStereo) override;

   private:
    bool enabled_;
    int16_t lower_bound_;
    int16_t upper_bound_;
  };

  VoETestManager manager_;
  VoEOutputCheckMediaProcess output_checker_;

  int channel_;
};

OutputTest::OutputTest(int16_t lower_bound, int16_t upper_bound)
    : output_checker_(lower_bound, upper_bound) {
  EXPECT_TRUE(manager_.Init());
  manager_.GetInterfaces();

  VoEBase* base = manager_.BasePtr();
  VoECodec* codec = manager_.CodecPtr();
  VoENetwork* network = manager_.NetworkPtr();

  EXPECT_EQ(0, base->Init());

  channel_ = base->CreateChannel();

  // |network| will take care of the life time of |transport|.
  VoiceChannelTransport* transport =
      new VoiceChannelTransport(network, channel_);

  EXPECT_EQ(0, transport->SetSendDestination(kIp, kPort));
  EXPECT_EQ(0, transport->SetLocalReceiver(kPort));

  EXPECT_EQ(0, codec->SetSendCodec(channel_, kCodecInst));
  EXPECT_EQ(0, codec->SetOpusDtx(channel_, true));

  EXPECT_EQ(0, manager_.VolumeControlPtr()->SetSpeakerVolume(255));

  manager_.ExternalMediaPtr()->RegisterExternalMediaProcessing(
      channel_, ProcessingTypes::kPlaybackPerChannel, output_checker_);
}

OutputTest::~OutputTest() {
  EXPECT_EQ(0, manager_.NetworkPtr()->DeRegisterExternalTransport(channel_));
  EXPECT_EQ(0, manager_.ReleaseInterfaces());
}

void OutputTest::Start() {
  const std::string file_name =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  const webrtc::FileFormats kInputFormat = webrtc::kFileFormatPcm32kHzFile;

  ASSERT_EQ(0, manager_.FilePtr()->StartPlayingFileAsMicrophone(
      channel_, file_name.c_str(), true, false, kInputFormat, 1.0));

  VoEBase* base = manager_.BasePtr();
  ASSERT_EQ(0, base->StartPlayout(channel_));
  ASSERT_EQ(0, base->StartSend(channel_));
}

void OutputTest::EnableOutputCheck() {
  output_checker_.set_enabled(true);
}

void OutputTest::DisableOutputCheck() {
  output_checker_.set_enabled(false);
}

void OutputTest::Mute() {
  manager_.VolumeControlPtr()->SetInputMute(channel_, true);
}

void OutputTest::Unmute() {
  manager_.VolumeControlPtr()->SetInputMute(channel_, false);
}

void OutputTest::SetBitRate(int rate) {
  manager_.CodecPtr()->SetBitRate(channel_, rate);
}

OutputTest::VoEOutputCheckMediaProcess::VoEOutputCheckMediaProcess(
    int16_t lower_bound, int16_t upper_bound)
    : enabled_(false),
      lower_bound_(lower_bound),
      upper_bound_(upper_bound) {}

void OutputTest::VoEOutputCheckMediaProcess::Process(int channel,
                                                     ProcessingTypes type,
                                                     int16_t* audio10ms,
                                                     size_t length,
                                                     int samplingFreq,
                                                     bool isStereo) {
  if (!enabled_)
    return;
  const int num_channels = isStereo ? 2 : 1;
  for (size_t i = 0; i < length; ++i) {
    for (int c = 0; c < num_channels; ++c) {
      ASSERT_GE(audio10ms[i * num_channels + c], lower_bound_);
      ASSERT_LE(audio10ms[i * num_channels + c], upper_bound_);
    }
  }
}

// This test checks if the Opus does not produce high noise (noise pump) when
// DTX is enabled. The microphone is toggled on and off, and values of the
// output signal during muting should be bounded.
// We do not run this test on bots. Developers that want to see the result
// and/or listen to sound quality can run this test manually.
TEST(OutputTest, DISABLED_OpusDtxHasNoNoisePump) {
  const int kRuntimeMs = 20000;
  const uint32_t kUnmuteTimeMs = 1000;
  const int kCheckAfterMute = 2000;
  const uint32_t kCheckTimeMs = 2000;
  const int kMinOpusRate = 6000;
  const int kMaxOpusRate = 64000;

#if defined(OPUS_FIXED_POINT)
  const int16_t kDtxBoundForSilence = 20;
#else
  const int16_t kDtxBoundForSilence = 2;
#endif

  OutputTest test(-kDtxBoundForSilence, kDtxBoundForSilence);
  Random random(1234ull);

  int64_t start_time = rtc::TimeMillis();
  test.Start();
  while (rtc::TimeSince(start_time) < kRuntimeMs) {
    webrtc::SleepMs(random.Rand(kUnmuteTimeMs - kUnmuteTimeMs / 10,
                                kUnmuteTimeMs + kUnmuteTimeMs / 10));
    test.Mute();
    webrtc::SleepMs(kCheckAfterMute);
    test.EnableOutputCheck();
    webrtc::SleepMs(random.Rand(kCheckTimeMs - kCheckTimeMs / 10,
                                kCheckTimeMs + kCheckTimeMs / 10));
    test.DisableOutputCheck();
    test.SetBitRate(random.Rand(kMinOpusRate, kMaxOpusRate));
    test.Unmute();
  }
}

}  // namespace voetest
