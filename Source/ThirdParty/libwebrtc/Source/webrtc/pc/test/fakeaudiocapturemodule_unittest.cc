/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/pc/test/fakeaudiocapturemodule.h"

#include <algorithm>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread.h"

using std::min;

class FakeAdmTest : public testing::Test,
                    public webrtc::AudioTransport {
 protected:
  static const int kMsInSecond = 1000;

  FakeAdmTest()
      : push_iterations_(0),
        pull_iterations_(0),
        rec_buffer_bytes_(0) {
    memset(rec_buffer_, 0, sizeof(rec_buffer_));
  }

  void SetUp() override {
    fake_audio_capture_module_ = FakeAudioCaptureModule::Create();
    EXPECT_TRUE(fake_audio_capture_module_.get() != NULL);
  }

  // Callbacks inherited from webrtc::AudioTransport.
  // ADM is pushing data.
  int32_t RecordedDataIsAvailable(const void* audioSamples,
                                  const size_t nSamples,
                                  const size_t nBytesPerSample,
                                  const size_t nChannels,
                                  const uint32_t samplesPerSec,
                                  const uint32_t totalDelayMS,
                                  const int32_t clockDrift,
                                  const uint32_t currentMicLevel,
                                  const bool keyPressed,
                                  uint32_t& newMicLevel) override {
    rtc::CritScope cs(&crit_);
    rec_buffer_bytes_ = nSamples * nBytesPerSample;
    if ((rec_buffer_bytes_ == 0) ||
        (rec_buffer_bytes_ > FakeAudioCaptureModule::kNumberSamples *
         FakeAudioCaptureModule::kNumberBytesPerSample)) {
      ADD_FAILURE();
      return -1;
    }
    memcpy(rec_buffer_, audioSamples, rec_buffer_bytes_);
    ++push_iterations_;
    newMicLevel = currentMicLevel;
    return 0;
  }

  void PushCaptureData(int voe_channel,
                       const void* audio_data,
                       int bits_per_sample,
                       int sample_rate,
                       size_t number_of_channels,
                       size_t number_of_frames) override {}

  void PullRenderData(int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames,
                      void* audio_data,
                      int64_t* elapsed_time_ms,
                      int64_t* ntp_time_ms) override {}

  // ADM is pulling data.
  int32_t NeedMorePlayData(const size_t nSamples,
                           const size_t nBytesPerSample,
                           const size_t nChannels,
                           const uint32_t samplesPerSec,
                           void* audioSamples,
                           size_t& nSamplesOut,
                           int64_t* elapsed_time_ms,
                           int64_t* ntp_time_ms) override {
    rtc::CritScope cs(&crit_);
    ++pull_iterations_;
    const size_t audio_buffer_size = nSamples * nBytesPerSample;
    const size_t bytes_out = RecordedDataReceived() ?
        CopyFromRecBuffer(audioSamples, audio_buffer_size):
        GenerateZeroBuffer(audioSamples, audio_buffer_size);
    nSamplesOut = bytes_out / nBytesPerSample;
    *elapsed_time_ms = 0;
    *ntp_time_ms = 0;
    return 0;
  }

  int push_iterations() const {
    rtc::CritScope cs(&crit_);
    return push_iterations_;
  }
  int pull_iterations() const {
    rtc::CritScope cs(&crit_);
    return pull_iterations_;
  }

  rtc::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module_;

 private:
  bool RecordedDataReceived() const {
    return rec_buffer_bytes_ != 0;
  }
  size_t GenerateZeroBuffer(void* audio_buffer, size_t audio_buffer_size) {
    memset(audio_buffer, 0, audio_buffer_size);
    return audio_buffer_size;
  }
  size_t CopyFromRecBuffer(void* audio_buffer, size_t audio_buffer_size) {
    EXPECT_EQ(audio_buffer_size, rec_buffer_bytes_);
    const size_t min_buffer_size = min(audio_buffer_size, rec_buffer_bytes_);
    memcpy(audio_buffer, rec_buffer_, min_buffer_size);
    return min_buffer_size;
  }

  rtc::CriticalSection crit_;

  int push_iterations_;
  int pull_iterations_;

  char rec_buffer_[FakeAudioCaptureModule::kNumberSamples *
                   FakeAudioCaptureModule::kNumberBytesPerSample];
  size_t rec_buffer_bytes_;
};

TEST_F(FakeAdmTest, TestProcess) {
  // Next process call must be some time in the future (or now).
  EXPECT_LE(0, fake_audio_capture_module_->TimeUntilNextProcess());
  // Process call updates TimeUntilNextProcess() but there are no guarantees on
  // timing so just check that Process can be called successfully.
  fake_audio_capture_module_->Process();
}

TEST_F(FakeAdmTest, PlayoutTest) {
  EXPECT_EQ(0, fake_audio_capture_module_->RegisterAudioCallback(this));

  bool stereo_available = false;
  EXPECT_EQ(0,
            fake_audio_capture_module_->StereoPlayoutIsAvailable(
                &stereo_available));
  EXPECT_TRUE(stereo_available);

  EXPECT_NE(0, fake_audio_capture_module_->StartPlayout());
  EXPECT_FALSE(fake_audio_capture_module_->PlayoutIsInitialized());
  EXPECT_FALSE(fake_audio_capture_module_->Playing());
  EXPECT_EQ(0, fake_audio_capture_module_->StopPlayout());

  EXPECT_EQ(0, fake_audio_capture_module_->InitPlayout());
  EXPECT_TRUE(fake_audio_capture_module_->PlayoutIsInitialized());
  EXPECT_FALSE(fake_audio_capture_module_->Playing());

  EXPECT_EQ(0, fake_audio_capture_module_->StartPlayout());
  EXPECT_TRUE(fake_audio_capture_module_->Playing());

  uint16_t delay_ms = 10;
  EXPECT_EQ(0, fake_audio_capture_module_->PlayoutDelay(&delay_ms));
  EXPECT_EQ(0, delay_ms);

  EXPECT_TRUE_WAIT(pull_iterations() > 0, kMsInSecond);
  EXPECT_GE(0, push_iterations());

  EXPECT_EQ(0, fake_audio_capture_module_->StopPlayout());
  EXPECT_FALSE(fake_audio_capture_module_->Playing());
}

TEST_F(FakeAdmTest, RecordTest) {
  EXPECT_EQ(0, fake_audio_capture_module_->RegisterAudioCallback(this));

  bool stereo_available = false;
  EXPECT_EQ(0, fake_audio_capture_module_->StereoRecordingIsAvailable(
      &stereo_available));
  EXPECT_FALSE(stereo_available);

  EXPECT_NE(0, fake_audio_capture_module_->StartRecording());
  EXPECT_FALSE(fake_audio_capture_module_->Recording());
  EXPECT_EQ(0, fake_audio_capture_module_->StopRecording());

  EXPECT_EQ(0, fake_audio_capture_module_->InitRecording());
  EXPECT_EQ(0, fake_audio_capture_module_->StartRecording());
  EXPECT_TRUE(fake_audio_capture_module_->Recording());

  EXPECT_TRUE_WAIT(push_iterations() > 0, kMsInSecond);
  EXPECT_GE(0, pull_iterations());

  EXPECT_EQ(0, fake_audio_capture_module_->StopRecording());
  EXPECT_FALSE(fake_audio_capture_module_->Recording());
}

TEST_F(FakeAdmTest, DuplexTest) {
  EXPECT_EQ(0, fake_audio_capture_module_->RegisterAudioCallback(this));

  EXPECT_EQ(0, fake_audio_capture_module_->InitPlayout());
  EXPECT_EQ(0, fake_audio_capture_module_->StartPlayout());

  EXPECT_EQ(0, fake_audio_capture_module_->InitRecording());
  EXPECT_EQ(0, fake_audio_capture_module_->StartRecording());

  EXPECT_TRUE_WAIT(push_iterations() > 0, kMsInSecond);
  EXPECT_TRUE_WAIT(pull_iterations() > 0, kMsInSecond);

  EXPECT_EQ(0, fake_audio_capture_module_->StopPlayout());
  EXPECT_EQ(0, fake_audio_capture_module_->StopRecording());
}
