/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/android/audio_manager.h"

#include <SLES/OpenSLES_Android.h>

#include "modules/audio_device/android/build_info.h"
#include "modules/audio_device/android/ensure_initialized.h"
#include "rtc_base/arraysize.h"
#include "test/gtest.h"

#define PRINT(...) fprintf(stderr, __VA_ARGS__);

namespace webrtc {

static const char kTag[] = "  ";

class AudioManagerTest : public ::testing::Test {
 protected:
  AudioManagerTest() {
    // One-time initialization of JVM and application context. Ensures that we
    // can do calls between C++ and Java.
    webrtc::audiodevicemodule::EnsureInitialized();
    audio_manager_.reset(new AudioManager());
    SetActiveAudioLayer();
    playout_parameters_ = audio_manager()->GetPlayoutAudioParameters();
    record_parameters_ = audio_manager()->GetRecordAudioParameters();
  }

  AudioManager* audio_manager() const { return audio_manager_.get(); }

  // A valid audio layer must always be set before calling Init(), hence we
  // might as well make it a part of the test fixture.
  void SetActiveAudioLayer() {
    EXPECT_EQ(0, audio_manager()->GetDelayEstimateInMilliseconds());
    audio_manager()->SetActiveAudioLayer(AudioDeviceModule::kAndroidJavaAudio);
    EXPECT_NE(0, audio_manager()->GetDelayEstimateInMilliseconds());
  }

  // One way to ensure that the engine object is valid is to create an
  // SL Engine interface since it exposes creation methods of all the OpenSL ES
  // object types and it is only supported on the engine object. This method
  // also verifies that the engine interface supports at least one interface.
  // Note that, the test below is not a full test of the SLEngineItf object
  // but only a simple sanity test to check that the global engine object is OK.
  void ValidateSLEngine(SLObjectItf engine_object) {
    EXPECT_NE(nullptr, engine_object);
    // Get the SL Engine interface which is exposed by the engine object.
    SLEngineItf engine;
    SLresult result =
        (*engine_object)->GetInterface(engine_object, SL_IID_ENGINE, &engine);
    EXPECT_EQ(result, SL_RESULT_SUCCESS) << "GetInterface() on engine failed";
    // Ensure that the SL Engine interface exposes at least one interface.
    SLuint32 object_id = SL_OBJECTID_ENGINE;
    SLuint32 num_supported_interfaces = 0;
    result = (*engine)->QueryNumSupportedInterfaces(engine, object_id,
                                                    &num_supported_interfaces);
    EXPECT_EQ(result, SL_RESULT_SUCCESS)
        << "QueryNumSupportedInterfaces() failed";
    EXPECT_GE(num_supported_interfaces, 1u);
  }

  std::unique_ptr<AudioManager> audio_manager_;
  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;
};

TEST_F(AudioManagerTest, ConstructDestruct) {}

// It should not be possible to create an OpenSL engine object if Java based
// audio is requested in both directions.
TEST_F(AudioManagerTest, GetOpenSLEngineShouldFailForJavaAudioLayer) {
  audio_manager()->SetActiveAudioLayer(AudioDeviceModule::kAndroidJavaAudio);
  SLObjectItf engine_object = audio_manager()->GetOpenSLEngine();
  EXPECT_EQ(nullptr, engine_object);
}

// It should be possible to create an OpenSL engine object if OpenSL ES based
// audio is requested in any direction.
TEST_F(AudioManagerTest, GetOpenSLEngineShouldSucceedForOpenSLESAudioLayer) {
  // List of supported audio layers that uses OpenSL ES audio.
  const AudioDeviceModule::AudioLayer opensles_audio[] = {
      AudioDeviceModule::kAndroidOpenSLESAudio,
      AudioDeviceModule::kAndroidJavaInputAndOpenSLESOutputAudio};
  // Verify that the global (singleton) OpenSL Engine can be acquired for all
  // audio layes that uses OpenSL ES. Note that the engine is only created once.
  for (const AudioDeviceModule::AudioLayer audio_layer : opensles_audio) {
    audio_manager()->SetActiveAudioLayer(audio_layer);
    SLObjectItf engine_object = audio_manager()->GetOpenSLEngine();
    EXPECT_NE(nullptr, engine_object);
    // Perform a simple sanity check of the created engine object.
    ValidateSLEngine(engine_object);
  }
}

TEST_F(AudioManagerTest, InitClose) {
  EXPECT_TRUE(audio_manager()->Init());
  EXPECT_TRUE(audio_manager()->Close());
}

TEST_F(AudioManagerTest, IsAcousticEchoCancelerSupported) {
  PRINT("%sAcoustic Echo Canceler support: %s\n", kTag,
        audio_manager()->IsAcousticEchoCancelerSupported() ? "Yes" : "No");
}

TEST_F(AudioManagerTest, IsAutomaticGainControlSupported) {
  EXPECT_FALSE(audio_manager()->IsAutomaticGainControlSupported());
}

TEST_F(AudioManagerTest, IsNoiseSuppressorSupported) {
  PRINT("%sNoise Suppressor support: %s\n", kTag,
        audio_manager()->IsNoiseSuppressorSupported() ? "Yes" : "No");
}

TEST_F(AudioManagerTest, IsLowLatencyPlayoutSupported) {
  PRINT("%sLow latency output support: %s\n", kTag,
        audio_manager()->IsLowLatencyPlayoutSupported() ? "Yes" : "No");
}

TEST_F(AudioManagerTest, IsLowLatencyRecordSupported) {
  PRINT("%sLow latency input support: %s\n", kTag,
        audio_manager()->IsLowLatencyRecordSupported() ? "Yes" : "No");
}

TEST_F(AudioManagerTest, IsProAudioSupported) {
  PRINT("%sPro audio support: %s\n", kTag,
        audio_manager()->IsProAudioSupported() ? "Yes" : "No");
}

// Verify that playout side is configured for mono by default.
TEST_F(AudioManagerTest, IsStereoPlayoutSupported) {
  EXPECT_FALSE(audio_manager()->IsStereoPlayoutSupported());
}

// Verify that recording side is configured for mono by default.
TEST_F(AudioManagerTest, IsStereoRecordSupported) {
  EXPECT_FALSE(audio_manager()->IsStereoRecordSupported());
}

TEST_F(AudioManagerTest, ShowAudioParameterInfo) {
  const bool low_latency_out = audio_manager()->IsLowLatencyPlayoutSupported();
  const bool low_latency_in = audio_manager()->IsLowLatencyRecordSupported();
  PRINT("PLAYOUT:\n");
  PRINT("%saudio layer: %s\n", kTag,
        low_latency_out ? "Low latency OpenSL" : "Java/JNI based AudioTrack");
  PRINT("%ssample rate: %d Hz\n", kTag, playout_parameters_.sample_rate());
  PRINT("%schannels: %zu\n", kTag, playout_parameters_.channels());
  PRINT("%sframes per buffer: %zu <=> %.2f ms\n", kTag,
        playout_parameters_.frames_per_buffer(),
        playout_parameters_.GetBufferSizeInMilliseconds());
  PRINT("RECORD: \n");
  PRINT("%saudio layer: %s\n", kTag,
        low_latency_in ? "Low latency OpenSL" : "Java/JNI based AudioRecord");
  PRINT("%ssample rate: %d Hz\n", kTag, record_parameters_.sample_rate());
  PRINT("%schannels: %zu\n", kTag, record_parameters_.channels());
  PRINT("%sframes per buffer: %zu <=> %.2f ms\n", kTag,
        record_parameters_.frames_per_buffer(),
        record_parameters_.GetBufferSizeInMilliseconds());
}

// The audio device module only suppors the same sample rate in both directions.
// In addition, in full-duplex low-latency mode (OpenSL ES), both input and
// output must use the same native buffer size to allow for usage of the fast
// audio track in Android.
TEST_F(AudioManagerTest, VerifyAudioParameters) {
  const bool low_latency_out = audio_manager()->IsLowLatencyPlayoutSupported();
  const bool low_latency_in = audio_manager()->IsLowLatencyRecordSupported();
  EXPECT_EQ(playout_parameters_.sample_rate(),
            record_parameters_.sample_rate());
  if (low_latency_out && low_latency_in) {
    EXPECT_EQ(playout_parameters_.frames_per_buffer(),
              record_parameters_.frames_per_buffer());
  }
}

// Add device-specific information to the test for logging purposes.
TEST_F(AudioManagerTest, ShowDeviceInfo) {
  BuildInfo build_info;
  PRINT("%smodel: %s\n", kTag, build_info.GetDeviceModel().c_str());
  PRINT("%sbrand: %s\n", kTag, build_info.GetBrand().c_str());
  PRINT("%smanufacturer: %s\n", kTag,
        build_info.GetDeviceManufacturer().c_str());
}

// Add Android build information to the test for logging purposes.
TEST_F(AudioManagerTest, ShowBuildInfo) {
  BuildInfo build_info;
  PRINT("%sbuild release: %s\n", kTag, build_info.GetBuildRelease().c_str());
  PRINT("%sbuild id: %s\n", kTag, build_info.GetAndroidBuildId().c_str());
  PRINT("%sbuild type: %s\n", kTag, build_info.GetBuildType().c_str());
  PRINT("%sSDK version: %d\n", kTag, build_info.GetSdkVersion());
}

// Basic test of the AudioParameters class using default construction where
// all members are set to zero.
TEST_F(AudioManagerTest, AudioParametersWithDefaultConstruction) {
  AudioParameters params;
  EXPECT_FALSE(params.is_valid());
  EXPECT_EQ(0, params.sample_rate());
  EXPECT_EQ(0U, params.channels());
  EXPECT_EQ(0U, params.frames_per_buffer());
  EXPECT_EQ(0U, params.frames_per_10ms_buffer());
  EXPECT_EQ(0U, params.GetBytesPerFrame());
  EXPECT_EQ(0U, params.GetBytesPerBuffer());
  EXPECT_EQ(0U, params.GetBytesPer10msBuffer());
  EXPECT_EQ(0.0f, params.GetBufferSizeInMilliseconds());
}

// Basic test of the AudioParameters class using non default construction.
TEST_F(AudioManagerTest, AudioParametersWithNonDefaultConstruction) {
  const int kSampleRate = 48000;
  const size_t kChannels = 1;
  const size_t kFramesPerBuffer = 480;
  const size_t kFramesPer10msBuffer = 480;
  const size_t kBytesPerFrame = 2;
  const float kBufferSizeInMs = 10.0f;
  AudioParameters params(kSampleRate, kChannels, kFramesPerBuffer);
  EXPECT_TRUE(params.is_valid());
  EXPECT_EQ(kSampleRate, params.sample_rate());
  EXPECT_EQ(kChannels, params.channels());
  EXPECT_EQ(kFramesPerBuffer, params.frames_per_buffer());
  EXPECT_EQ(static_cast<size_t>(kSampleRate / 100),
            params.frames_per_10ms_buffer());
  EXPECT_EQ(kBytesPerFrame, params.GetBytesPerFrame());
  EXPECT_EQ(kBytesPerFrame * kFramesPerBuffer, params.GetBytesPerBuffer());
  EXPECT_EQ(kBytesPerFrame * kFramesPer10msBuffer,
            params.GetBytesPer10msBuffer());
  EXPECT_EQ(kBufferSizeInMs, params.GetBufferSizeInMilliseconds());
}

}  // namespace webrtc
