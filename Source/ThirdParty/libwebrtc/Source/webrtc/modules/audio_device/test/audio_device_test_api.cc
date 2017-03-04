/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <memory>

#include "webrtc/base/location.h"
#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/audio_device_impl.h"
#include "webrtc/modules/audio_device/test/audio_device_test_defines.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

// Helper functions
#if defined(ANDROID)
char filenameStr[2][256] =
{ {0},
  {0},
}; // Allow two buffers for those API calls taking two filenames
int currentStr = 0;

const char* GetFilename(const char* filename)
{
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/admtest/%s", filename);
  return filenameStr[currentStr];
}
#elif !defined(WEBRTC_IOS)
const char* GetFilename(const char* filename) {
  std::string full_path_filename = webrtc::test::OutputPath() + filename;
  return full_path_filename.c_str();
}
#endif

using namespace webrtc;

class AudioEventObserverAPI: public AudioDeviceObserver {
 public:
  AudioEventObserverAPI(
      const rtc::scoped_refptr<AudioDeviceModule>& audioDevice)
      : error_(kRecordingError),
        warning_(kRecordingWarning),
        audio_device_(audioDevice) {}

  ~AudioEventObserverAPI() override {}

  void OnErrorIsReported(const ErrorCode error) override {
    TEST_LOG("\n[*** ERROR ***] => OnErrorIsReported(%d)\n\n", error);
    error_ = error;
  }

  void OnWarningIsReported(const WarningCode warning) override {
    TEST_LOG("\n[*** WARNING ***] => OnWarningIsReported(%d)\n\n", warning);
    warning_ = warning;
    EXPECT_EQ(0, audio_device_->StopRecording());
    EXPECT_EQ(0, audio_device_->StopPlayout());
  }

 public:
  ErrorCode error_;
  WarningCode warning_;
 private:
  rtc::scoped_refptr<AudioDeviceModule> audio_device_;
};

class AudioTransportAPI: public AudioTransport {
 public:
  AudioTransportAPI(const rtc::scoped_refptr<AudioDeviceModule>& audioDevice)
      : rec_count_(0),
        play_count_(0) {
  }

  ~AudioTransportAPI() override {}

  int32_t RecordedDataIsAvailable(const void* audioSamples,
                                  const size_t nSamples,
                                  const size_t nBytesPerSample,
                                  const size_t nChannels,
                                  const uint32_t sampleRate,
                                  const uint32_t totalDelay,
                                  const int32_t clockSkew,
                                  const uint32_t currentMicLevel,
                                  const bool keyPressed,
                                  uint32_t& newMicLevel) override {
    rec_count_++;
    if (rec_count_ % 100 == 0) {
      if (nChannels == 1) {
        // mono
        TEST_LOG("-");
      } else if ((nChannels == 2) && (nBytesPerSample == 2)) {
        // stereo but only using one channel
        TEST_LOG("-|");
      } else {
        // stereo
        TEST_LOG("--");
      }
    }
    return 0;
  }

  int32_t NeedMorePlayData(const size_t nSamples,
                           const size_t nBytesPerSample,
                           const size_t nChannels,
                           const uint32_t sampleRate,
                           void* audioSamples,
                           size_t& nSamplesOut,
                           int64_t* elapsed_time_ms,
                           int64_t* ntp_time_ms) override {
    play_count_++;
    if (play_count_ % 100 == 0) {
      if (nChannels == 1) {
        TEST_LOG("+");
      } else {
        TEST_LOG("++");
      }
    }
    nSamplesOut = 480;
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

 private:
  uint32_t rec_count_;
  uint32_t play_count_;
};

class AudioDeviceAPITest: public testing::Test {
 protected:
  AudioDeviceAPITest() {}

  ~AudioDeviceAPITest() override {}

  static void SetUpTestCase() {
    process_thread_ = ProcessThread::Create("ProcessThread");
    process_thread_->Start();

    // Windows:
    //      if (WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    //          user can select only the default (Core)
    const int32_t kId = 444;

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    TEST_LOG("WEBRTC_WINDOWS_CORE_AUDIO_BUILD is defined!\n\n");
    // create default implementation (=Core Audio) instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
    EXPECT_EQ(0, audio_device_.release()->Release());
    // explicitly specify usage of Core Audio (same as default)
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kWindowsCoreAudio)) != NULL);
#endif

#if defined(ANDROID)
    // Fails tests
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kLinuxAlsaAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kLinuxPulseAudio)) == NULL);
    // Create default implementation instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
#elif defined(WEBRTC_LINUX)
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    // create default implementation instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
    EXPECT_EQ(0, audio_device_->Terminate());
    EXPECT_EQ(0, audio_device_.release()->Release());
    // explicitly specify usage of Pulse Audio (same as default)
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kLinuxPulseAudio)) != NULL);
#endif

#if defined(WEBRTC_MAC)
    // Fails tests
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kWindowsCoreAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kLinuxAlsaAudio)) == NULL);
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kLinuxPulseAudio)) == NULL);
    // Create default implementation instance
    EXPECT_TRUE((audio_device_ = AudioDeviceModule::Create(
                kId, AudioDeviceModule::kPlatformDefaultAudio)) != NULL);
#endif

    if (audio_device_ == NULL) {
      FAIL() << "Failed creating audio device object!";
    }

    process_thread_->RegisterModule(audio_device_, RTC_FROM_HERE);

    AudioDeviceModule::AudioLayer audio_layer =
        AudioDeviceModule::kPlatformDefaultAudio;
    EXPECT_EQ(0, audio_device_->ActiveAudioLayer(&audio_layer));
    if (audio_layer == AudioDeviceModule::kLinuxAlsaAudio) {
      linux_alsa_ = true;
    }
  }

  static void TearDownTestCase() {
    if (process_thread_) {
      process_thread_->DeRegisterModule(audio_device_);
      process_thread_->Stop();
      process_thread_.reset();
    }
    if (event_observer_) {
      delete event_observer_;
      event_observer_ = NULL;
    }
    if (audio_transport_) {
      delete audio_transport_;
      audio_transport_ = NULL;
    }
    if (audio_device_)
      EXPECT_EQ(0, audio_device_.release()->Release());
    PRINT_TEST_RESULTS;
  }

  void SetUp() override {
    if (linux_alsa_) {
      FAIL() << "API Test is not available on ALSA on Linux!";
    }
    EXPECT_EQ(0, audio_device_->Init());
    EXPECT_TRUE(audio_device_->Initialized());
  }

  void TearDown() override { EXPECT_EQ(0, audio_device_->Terminate()); }

  void CheckVolume(uint32_t expected, uint32_t actual) {
    // Mac and Windows have lower resolution on the volume settings.
#if defined(WEBRTC_MAC) || defined(_WIN32)
    int diff = abs(static_cast<int>(expected - actual));
    EXPECT_LE(diff, 5);
#else
    EXPECT_TRUE((actual == expected) || (actual == expected-1));
#endif
  }

  void CheckInitialPlayoutStates() {
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
    EXPECT_FALSE(audio_device_->Playing());
    EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
  }

  void CheckInitialRecordingStates() {
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
    EXPECT_FALSE(audio_device_->Recording());
    EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
  }

  // TODO(henrika): Get rid of globals.
  static bool linux_alsa_;
  static std::unique_ptr<ProcessThread> process_thread_;
  static rtc::scoped_refptr<AudioDeviceModule> audio_device_;
  static AudioTransportAPI* audio_transport_;
  static AudioEventObserverAPI* event_observer_;
};

// Must be initialized like this to handle static SetUpTestCase() above.
bool AudioDeviceAPITest::linux_alsa_ = false;
std::unique_ptr<ProcessThread> AudioDeviceAPITest::process_thread_;
rtc::scoped_refptr<AudioDeviceModule> AudioDeviceAPITest::audio_device_;
AudioTransportAPI* AudioDeviceAPITest::audio_transport_ = NULL;
AudioEventObserverAPI* AudioDeviceAPITest::event_observer_ = NULL;

TEST_F(AudioDeviceAPITest, RegisterEventObserver) {
  event_observer_ = new AudioEventObserverAPI(audio_device_);
  EXPECT_EQ(0, audio_device_->RegisterEventObserver(NULL));
  EXPECT_EQ(0, audio_device_->RegisterEventObserver(event_observer_));
  EXPECT_EQ(0, audio_device_->RegisterEventObserver(NULL));
}

TEST_F(AudioDeviceAPITest, RegisterAudioCallback) {
  audio_transport_ = new AudioTransportAPI(audio_device_);
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(audio_transport_));
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
}

TEST_F(AudioDeviceAPITest, Init) {
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Init());
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Init());
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
}

TEST_F(AudioDeviceAPITest, Terminate) {
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Init());
  EXPECT_TRUE(audio_device_->Initialized());
  EXPECT_EQ(0, audio_device_->Terminate());
  EXPECT_FALSE(audio_device_->Initialized());
}

TEST_F(AudioDeviceAPITest, PlayoutDevices) {
  EXPECT_GT(audio_device_->PlayoutDevices(), 0);
  EXPECT_GT(audio_device_->PlayoutDevices(), 0);
}

TEST_F(AudioDeviceAPITest, RecordingDevices) {
  EXPECT_GT(audio_device_->RecordingDevices(), 0);
  EXPECT_GT(audio_device_->RecordingDevices(), 0);
}

// TODO(henrika): uncomment when you have decided what to do with issue 3675.
#if 0
TEST_F(AudioDeviceAPITest, PlayoutDeviceName) {
  char name[kAdmMaxDeviceNameSize];
  char guid[kAdmMaxGuidSize];
  int16_t no_devices = audio_device_->PlayoutDevices();

  // fail tests
  EXPECT_EQ(-1, audio_device_->PlayoutDeviceName(-2, name, guid));
  EXPECT_EQ(-1, audio_device_->PlayoutDeviceName(no_devices, name, guid));
  EXPECT_EQ(-1, audio_device_->PlayoutDeviceName(0, NULL, guid));

  // bulk tests
  EXPECT_EQ(0, audio_device_->PlayoutDeviceName(0, name, NULL));
#ifdef _WIN32
  // shall be mapped to 0.
  EXPECT_EQ(0, audio_device_->PlayoutDeviceName(-1, name, NULL));
#else
  EXPECT_EQ(-1, audio_device_->PlayoutDeviceName(-1, name, NULL));
#endif
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->PlayoutDeviceName(i, name, guid));
    EXPECT_EQ(0, audio_device_->PlayoutDeviceName(i, name, NULL));
  }
}

TEST_F(AudioDeviceAPITest, RecordingDeviceName) {
  char name[kAdmMaxDeviceNameSize];
  char guid[kAdmMaxGuidSize];
  int16_t no_devices = audio_device_->RecordingDevices();

  // fail tests
  EXPECT_EQ(-1, audio_device_->RecordingDeviceName(-2, name, guid));
  EXPECT_EQ(-1, audio_device_->RecordingDeviceName(no_devices, name, guid));
  EXPECT_EQ(-1, audio_device_->RecordingDeviceName(0, NULL, guid));

  // bulk tests
  EXPECT_EQ(0, audio_device_->RecordingDeviceName(0, name, NULL));
#ifdef _WIN32
  // shall me mapped to 0
  EXPECT_EQ(0, audio_device_->RecordingDeviceName(-1, name, NULL));
#else
  EXPECT_EQ(-1, audio_device_->RecordingDeviceName(-1, name, NULL));
#endif
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->RecordingDeviceName(i, name, guid));
    EXPECT_EQ(0, audio_device_->RecordingDeviceName(i, name, NULL));
  }
}

TEST_F(AudioDeviceAPITest, SetPlayoutDevice) {
  int16_t no_devices = audio_device_->PlayoutDevices();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(-1));
  EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(no_devices));

  // bulk tests
#ifdef _WIN32
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultCommunicationDevice));
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultDevice));
#else
  EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultCommunicationDevice));
  EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultDevice));
#endif
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
  }
}

TEST_F(AudioDeviceAPITest, SetRecordingDevice) {
  EXPECT_EQ(0, audio_device_->Init());
  int16_t no_devices = audio_device_->RecordingDevices();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetRecordingDevice(-1));
  EXPECT_EQ(-1, audio_device_->SetRecordingDevice(no_devices));

  // bulk tests
#ifdef _WIN32
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultDevice));
#else
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultCommunicationDevice) == -1);
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultDevice) == -1);
#endif
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
  }
}
#endif  // 0

TEST_F(AudioDeviceAPITest, PlayoutIsAvailable) {
  bool available;
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  // Availability check should not initialize.
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());

  EXPECT_EQ(0,
            audio_device_->SetPlayoutDevice(AudioDeviceModule::kDefaultDevice));
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
#endif

  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, RecordingIsAvailable) {
  bool available;
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultCommunicationDevice));
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  EXPECT_FALSE(audio_device_->RecordingIsInitialized());

  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      AudioDeviceModule::kDefaultDevice));
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  EXPECT_FALSE(audio_device_->RecordingIsInitialized());
#endif

  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, InitPlayout) {
  // check initial state
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());

  // ensure that device must be set before we can initialize
  EXPECT_EQ(-1, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_TRUE(audio_device_->PlayoutIsInitialized());

  // bulk tests
  bool available;
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_EQ(-1, audio_device_->SetPlayoutDevice(
        MACRO_DEFAULT_COMMUNICATION_DEVICE));
    EXPECT_EQ(0, audio_device_->StopPlayout());
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
  }

  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitPlayout());
    // Sleep is needed for e.g. iPhone since we after stopping then starting may
    // have a hangover time of a couple of ms before initialized.
    SleepMs(50);
    EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
  }

  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->StopPlayout());
      EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
      EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
      EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
      if (available) {
        EXPECT_EQ(0, audio_device_->InitPlayout());
        EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
      }
    }
  }
  EXPECT_EQ(0, audio_device_->StopPlayout());
}

TEST_F(AudioDeviceAPITest, InitRecording) {
  // check initial state
  EXPECT_FALSE(audio_device_->RecordingIsInitialized());

  // ensure that device must be set before we can initialize
  EXPECT_EQ(-1, audio_device_->InitRecording());
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->InitRecording());
  EXPECT_TRUE(audio_device_->RecordingIsInitialized());

  // bulk tests
  bool available;
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_TRUE(audio_device_->RecordingIsInitialized());
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_EQ(-1,
        audio_device_->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE));
    EXPECT_EQ(0, audio_device_->StopRecording());
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
  }

  EXPECT_EQ(0,
      audio_device_->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE));
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitRecording());
    SleepMs(50);
    EXPECT_TRUE(audio_device_->RecordingIsInitialized());
  }

  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->StopRecording());
      EXPECT_FALSE(audio_device_->RecordingIsInitialized());
      EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
      EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
      if (available) {
        EXPECT_EQ(0, audio_device_->InitRecording());
        EXPECT_TRUE(audio_device_->RecordingIsInitialized());
      }
    }
  }
  EXPECT_EQ(0, audio_device_->StopRecording());
}

TEST_F(AudioDeviceAPITest, StartAndStopPlayout) {
  bool available;
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));

  CheckInitialPlayoutStates();

  EXPECT_EQ(-1, audio_device_->StartPlayout());
  EXPECT_EQ(0, audio_device_->StopPlayout());

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available)
  {
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_EQ(0, audio_device_->StartPlayout());
    EXPECT_TRUE(audio_device_->Playing());
    EXPECT_EQ(-1, audio_device_->RegisterAudioCallback(audio_transport_));
    EXPECT_EQ(0, audio_device_->StopPlayout());
    EXPECT_FALSE(audio_device_->Playing());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
  }
#endif

  // repeat test but for kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available) {
    EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_EQ(0, audio_device_->StartPlayout());
    EXPECT_TRUE(audio_device_->Playing());
    EXPECT_EQ(-1, audio_device_->RegisterAudioCallback(audio_transport_));
    EXPECT_EQ(0, audio_device_->StopPlayout());
    EXPECT_FALSE(audio_device_->Playing());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
  }

  // repeat test for all devices
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
    if (available) {
      EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
      EXPECT_EQ(0, audio_device_->InitPlayout());
      EXPECT_EQ(0, audio_device_->StartPlayout());
      EXPECT_TRUE(audio_device_->Playing());
      EXPECT_EQ(-1, audio_device_->RegisterAudioCallback(audio_transport_));
      EXPECT_EQ(0, audio_device_->StopPlayout());
      EXPECT_FALSE(audio_device_->Playing());
      EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
    }
  }
}

TEST_F(AudioDeviceAPITest, StartAndStopRecording) {
  bool available;
  EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));

  CheckInitialRecordingStates();

  EXPECT_EQ(-1, audio_device_->StartRecording());
  EXPECT_EQ(0, audio_device_->StopRecording());

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available)
  {
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_EQ(0, audio_device_->StartRecording());
    EXPECT_TRUE(audio_device_->Recording());
    EXPECT_EQ(-1, audio_device_->RegisterAudioCallback(audio_transport_));
    EXPECT_EQ(0, audio_device_->StopRecording());
    EXPECT_FALSE(audio_device_->Recording());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
  }
#endif

  // repeat test but for kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available) {
    EXPECT_FALSE(audio_device_->RecordingIsInitialized());
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_EQ(0, audio_device_->StartRecording());
    EXPECT_TRUE(audio_device_->Recording());
    EXPECT_EQ(-1, audio_device_->RegisterAudioCallback(audio_transport_));
    EXPECT_EQ(0, audio_device_->StopRecording());
    EXPECT_FALSE(audio_device_->Recording());
    EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
  }

  // repeat test for all devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
    if (available) {
      EXPECT_FALSE(audio_device_->RecordingIsInitialized());
      EXPECT_EQ(0, audio_device_->InitRecording());
      EXPECT_EQ(0, audio_device_->StartRecording());
      EXPECT_TRUE(audio_device_->Recording());
      EXPECT_EQ(-1, audio_device_->RegisterAudioCallback(audio_transport_));
      EXPECT_EQ(0, audio_device_->StopRecording());
      EXPECT_FALSE(audio_device_->Recording());
      EXPECT_EQ(0, audio_device_->RegisterAudioCallback(NULL));
    }
  }
}


TEST_F(AudioDeviceAPITest, InitSpeaker) {
  // NOTE: By calling Terminate (in TearDown) followed by Init (in SetUp) we
  // ensure that any existing output mixer handle is set to NULL.
  // The mixer handle is closed and reopened again for each call to
  // SetPlayoutDevice.
  CheckInitialPlayoutStates();

  // kDefaultCommunicationDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));
  EXPECT_EQ(0, audio_device_->InitSpeaker());

  // fail tests
  bool available;
  EXPECT_EQ(0, audio_device_->PlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitPlayout());
    EXPECT_EQ(0, audio_device_->StartPlayout());
    EXPECT_EQ(-1, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->StopPlayout());
  }

  // kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->InitSpeaker());

  // repeat test for all devices
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->InitSpeaker());
  }
}

TEST_F(AudioDeviceAPITest, InitMicrophone) {
  // NOTE: By calling Terminate (in TearDown) followed by Init (in SetUp) we
  // ensure that any existing output mixer handle is set to NULL.
  // The mixer handle is closed and reopened again for each call to
  // SetRecordingDevice.
  CheckInitialRecordingStates();

  // kDefaultCommunicationDevice
  EXPECT_EQ(0,
      audio_device_->SetRecordingDevice(MACRO_DEFAULT_COMMUNICATION_DEVICE));
  EXPECT_EQ(0, audio_device_->InitMicrophone());

  // fail tests
  bool available;
  EXPECT_EQ(0, audio_device_->RecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitRecording());
    EXPECT_EQ(0, audio_device_->StartRecording());
    EXPECT_EQ(-1, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->StopRecording());
  }

  // kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->InitMicrophone());

  // repeat test for all devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->InitMicrophone());
  }
}

TEST_F(AudioDeviceAPITest, SpeakerVolumeIsAvailable) {
  CheckInitialPlayoutStates();
  bool available;

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
    EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
  }
}

// Tests the following methods:
// SetSpeakerVolume
// SpeakerVolume
// MaxSpeakerVolume
// MinSpeakerVolume
// NOTE: Disabled on mac due to issue 257.
#ifndef WEBRTC_MAC
TEST_F(AudioDeviceAPITest, SpeakerVolumeTests) {
  uint32_t vol(0);
  uint32_t volume(0);
  uint32_t maxVolume(0);
  uint32_t minVolume(0);
  uint16_t stepSize(0);
  bool available;
  CheckInitialPlayoutStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetSpeakerVolume(0));
  // speaker must be initialized first
  EXPECT_EQ(-1, audio_device_->SpeakerVolume(&volume));
  EXPECT_EQ(-1, audio_device_->MaxSpeakerVolume(&maxVolume));
  EXPECT_EQ(-1, audio_device_->MinSpeakerVolume(&minVolume));
  EXPECT_EQ(-1, audio_device_->SpeakerVolumeStepSize(&stepSize));

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // use kDefaultCommunicationDevice and modify/retrieve the volume
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->MaxSpeakerVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->MinSpeakerVolume(&minVolume));
    EXPECT_EQ(0, audio_device_->SpeakerVolumeStepSize(&stepSize));
    for (vol = minVolume; vol < (unsigned int)maxVolume; vol += 20*stepSize) {
      EXPECT_EQ(0, audio_device_->SetSpeakerVolume(vol));
      EXPECT_EQ(0, audio_device_->SpeakerVolume(&volume));
      CheckVolume(volume, vol);
    }
  }
#endif

  // use kDefaultDevice and modify/retrieve the volume
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->MaxSpeakerVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->MinSpeakerVolume(&minVolume));
    EXPECT_EQ(0, audio_device_->SpeakerVolumeStepSize(&stepSize));
    uint32_t step = (maxVolume - minVolume) / 10;
    step = (step < stepSize ? stepSize : step);
    for (vol = minVolume; vol <= maxVolume; vol += step) {
      EXPECT_EQ(0, audio_device_->SetSpeakerVolume(vol));
      EXPECT_EQ(0, audio_device_->SpeakerVolume(&volume));
      CheckVolume(volume, vol);
    }
  }

  // use all (indexed) devices and modify/retrieve the volume
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->InitSpeaker());
      EXPECT_EQ(0, audio_device_->MaxSpeakerVolume(&maxVolume));
      EXPECT_EQ(0, audio_device_->MinSpeakerVolume(&minVolume));
      EXPECT_EQ(0, audio_device_->SpeakerVolumeStepSize(&stepSize));
      uint32_t step = (maxVolume - minVolume) / 10;
      step = (step < stepSize ? stepSize : step);
      for (vol = minVolume; vol <= maxVolume; vol += step) {
        EXPECT_EQ(0, audio_device_->SetSpeakerVolume(vol));
        EXPECT_EQ(0, audio_device_->SpeakerVolume(&volume));
        CheckVolume(volume, vol);
      }
    }
  }

  // restore reasonable level
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->MaxSpeakerVolume(&maxVolume));
    EXPECT_TRUE(audio_device_->SetSpeakerVolume(maxVolume < 10 ?
        maxVolume/3 : maxVolume/10) == 0);
  }
}
#endif  // !WEBRTC_MAC

TEST_F(AudioDeviceAPITest, AGC) {
  // NOTE: The AGC API only enables/disables the AGC. To ensure that it will
  // have an effect, use it in combination with MicrophoneVolumeIsAvailable.
  CheckInitialRecordingStates();
  EXPECT_FALSE(audio_device_->AGC());

  // set/get tests
  EXPECT_EQ(0, audio_device_->SetAGC(true));
  EXPECT_TRUE(audio_device_->AGC());
  EXPECT_EQ(0, audio_device_->SetAGC(false));
  EXPECT_FALSE(audio_device_->AGC());
}

TEST_F(AudioDeviceAPITest, MicrophoneVolumeIsAvailable) {
  CheckInitialRecordingStates();
  bool available;

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
    EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
  }
}

// Tests the methods:
// SetMicrophoneVolume
// MicrophoneVolume
// MaxMicrophoneVolume
// MinMicrophoneVolume

// Disabled on Mac and Linux,
// see https://bugs.chromium.org/p/webrtc/issues/detail?id=5414
#if defined(WEBRTC_MAC) || defined(WEBRTC_LINUX)
#define MAYBE_MicrophoneVolumeTests DISABLED_MicrophoneVolumeTests
#else
#define MAYBE_MicrophoneVolumeTests MicrophoneVolumeTests
#endif
TEST_F(AudioDeviceAPITest, MAYBE_MicrophoneVolumeTests) {
  uint32_t vol(0);
  uint32_t volume(0);
  uint32_t maxVolume(0);
  uint32_t minVolume(0);
  uint16_t stepSize(0);
  bool available;
  CheckInitialRecordingStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetMicrophoneVolume(0));
  // must be initialized first
  EXPECT_EQ(-1, audio_device_->MicrophoneVolume(&volume));
  EXPECT_EQ(-1, audio_device_->MaxMicrophoneVolume(&maxVolume));
  EXPECT_EQ(-1, audio_device_->MinMicrophoneVolume(&minVolume));
  EXPECT_EQ(-1, audio_device_->MicrophoneVolumeStepSize(&stepSize));

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // initialize kDefaultCommunicationDevice and modify/retrieve the volume
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->MaxMicrophoneVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->MinMicrophoneVolume(&minVolume));
    EXPECT_EQ(0, audio_device_->MicrophoneVolumeStepSize(&stepSize));
    for (vol = minVolume; vol < (unsigned int)maxVolume; vol += 10*stepSize)
    {
      EXPECT_EQ(0, audio_device_->SetMicrophoneVolume(vol));
      EXPECT_EQ(0, audio_device_->MicrophoneVolume(&volume));
      CheckVolume(volume, vol);
    }
  }
#endif

  // reinitialize kDefaultDevice and modify/retrieve the volume
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->MaxMicrophoneVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->MinMicrophoneVolume(&minVolume));
    EXPECT_EQ(0, audio_device_->MicrophoneVolumeStepSize(&stepSize));
    for (vol = minVolume; vol < maxVolume; vol += 10 * stepSize) {
      EXPECT_EQ(0, audio_device_->SetMicrophoneVolume(vol));
      EXPECT_EQ(0, audio_device_->MicrophoneVolume(&volume));
      CheckVolume(volume, vol);
    }
  }

  // use all (indexed) devices and modify/retrieve the volume
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
    if (available) {
      EXPECT_EQ(0, audio_device_->InitMicrophone());
      EXPECT_EQ(0, audio_device_->MaxMicrophoneVolume(&maxVolume));
      EXPECT_EQ(0, audio_device_->MinMicrophoneVolume(&minVolume));
      EXPECT_EQ(0, audio_device_->MicrophoneVolumeStepSize(&stepSize));
      for (vol = minVolume; vol < maxVolume; vol += 20 * stepSize) {
        EXPECT_EQ(0, audio_device_->SetMicrophoneVolume(vol));
        EXPECT_EQ(0, audio_device_->MicrophoneVolume(&volume));
        CheckVolume(volume, vol);
      }
    }
  }

  // restore reasonable level
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneVolumeIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->MaxMicrophoneVolume(&maxVolume));
    EXPECT_EQ(0, audio_device_->SetMicrophoneVolume(maxVolume/10));
  }
}

TEST_F(AudioDeviceAPITest, SpeakerMuteIsAvailable) {
  bool available;
  CheckInitialPlayoutStates();
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetPlayoutDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  EXPECT_FALSE(audio_device_->SpeakerIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->PlayoutDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetPlayoutDevice(i));
    EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
    EXPECT_FALSE(audio_device_->SpeakerIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, MicrophoneMuteIsAvailable) {
  bool available;
  CheckInitialRecordingStates();
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  // check for availability should not lead to initialization
#endif
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
    EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, MicrophoneBoostIsAvailable) {
  bool available;
  CheckInitialRecordingStates();
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // check the kDefaultCommunicationDevice
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  // check for availability should not lead to initialization
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
#endif

  // check the kDefaultDevice
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());

  // check all availiable devices
  int16_t no_devices = audio_device_->RecordingDevices();
  for (int i = 0; i < no_devices; i++) {
    EXPECT_EQ(0, audio_device_->SetRecordingDevice(i));
    EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
    EXPECT_FALSE(audio_device_->MicrophoneIsInitialized());
  }
}

TEST_F(AudioDeviceAPITest, SpeakerMuteTests) {
  bool available;
  bool enabled;
  CheckInitialPlayoutStates();
  // fail tests
  EXPECT_EQ(-1, audio_device_->SetSpeakerMute(true));
  // requires initialization
  EXPECT_EQ(-1, audio_device_->SpeakerMute(&enabled));

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // initialize kDefaultCommunicationDevice and modify/retrieve the mute state
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      AudioDeviceModule::kDefaultCommunicationDevice));
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(true));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(false));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_FALSE(enabled);
  }
#endif

  // reinitialize kDefaultDevice and modify/retrieve the mute state
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(true));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(false));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_FALSE(enabled);
  }

  // reinitialize the default device (0) and modify/retrieve the mute state
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(0));
  EXPECT_EQ(0, audio_device_->SpeakerMuteIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitSpeaker());
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(true));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetSpeakerMute(false));
    EXPECT_EQ(0, audio_device_->SpeakerMute(&enabled));
    EXPECT_FALSE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, MicrophoneMuteTests) {
  CheckInitialRecordingStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetMicrophoneMute(true));
  // requires initialization
  bool available;
  bool enabled;
  EXPECT_EQ(-1, audio_device_->MicrophoneMute(&enabled));

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // initialize kDefaultCommunicationDevice and modify/retrieve the mute
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(true));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(false));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_FALSE(enabled);
  }
#endif

  // reinitialize kDefaultDevice and modify/retrieve the mute
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(true));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(false));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_FALSE(enabled);
  }

  // reinitialize the default device (0) and modify/retrieve the Mute
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(0));
  EXPECT_EQ(0, audio_device_->MicrophoneMuteIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(true));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneMute(false));
    EXPECT_EQ(0, audio_device_->MicrophoneMute(&enabled));
    EXPECT_FALSE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, MicrophoneBoostTests) {
  bool available;
  bool enabled;
  CheckInitialRecordingStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->SetMicrophoneBoost(true));
  // requires initialization
  EXPECT_EQ(-1, audio_device_->MicrophoneBoost(&enabled));

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // initialize kDefaultCommunicationDevice and modify/retrieve the boost
  EXPECT_TRUE(audio_device_->SetRecordingDevice(
          AudioDeviceModule::kDefaultCommunicationDevice) == 0);
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  if (available)
  {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(true));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(false));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_FALSE(enabled);
  }
#endif

  // reinitialize kDefaultDevice and modify/retrieve the boost
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(true));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(false));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_FALSE(enabled);
  }

  // reinitialize the default device (0) and modify/retrieve the boost
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(0));
  EXPECT_EQ(0, audio_device_->MicrophoneBoostIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->InitMicrophone());
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(true));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetMicrophoneBoost(false));
    EXPECT_EQ(0, audio_device_->MicrophoneBoost(&enabled));
    EXPECT_FALSE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, StereoPlayoutTests) {
  CheckInitialPlayoutStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
  // must be performed before initialization
  EXPECT_EQ(-1, audio_device_->SetStereoPlayout(true));
#endif

  // ensure that we can set the stereo mode for playout
  EXPECT_EQ(0, audio_device_->StopPlayout());
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());

  // initialize kDefaultCommunicationDevice and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));
  bool available;
  bool enabled;
  EXPECT_EQ(0, audio_device_->StereoPlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(false));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_FALSE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
  }

  // initialize kDefaultDevice and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->StereoPlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(false));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_FALSE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
  }

  // initialize default device (0) and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(0));
  EXPECT_EQ(0, audio_device_->StereoPlayoutIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(false));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_FALSE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoPlayout(true));
    EXPECT_EQ(0, audio_device_->StereoPlayout(&enabled));
    EXPECT_TRUE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, StereoRecordingTests) {
  CheckInitialRecordingStates();
  EXPECT_FALSE(audio_device_->Playing());

  // fail tests
  EXPECT_EQ(-1, audio_device_->InitRecording());
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitRecording());
  EXPECT_TRUE(audio_device_->RecordingIsInitialized());
  // must be performed before initialization
  EXPECT_EQ(-1, audio_device_->SetStereoRecording(true));
#endif
  // ensures that we can set the stereo mode for recording
  EXPECT_EQ(0, audio_device_->StopRecording());
  EXPECT_FALSE(audio_device_->RecordingIsInitialized());

  // initialize kDefaultCommunicationDevice and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));
  bool available;
  bool enabled;
  EXPECT_EQ(0, audio_device_->StereoRecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoRecording(true));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoRecording(false));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_FALSE(enabled);
  }

  // initialize kDefaultDevice and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));
  EXPECT_EQ(0, audio_device_->StereoRecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoRecording(true));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoRecording(false));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_FALSE(enabled);
  }

  // initialize default device (0) and modify/retrieve stereo support
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(0));
  EXPECT_EQ(0, audio_device_->StereoRecordingIsAvailable(&available));
  if (available) {
    EXPECT_EQ(0, audio_device_->SetStereoRecording(true));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_TRUE(enabled);
    EXPECT_EQ(0, audio_device_->SetStereoRecording(false));
    EXPECT_EQ(0, audio_device_->StereoRecording(&enabled));
    EXPECT_FALSE(enabled);
  }
}

TEST_F(AudioDeviceAPITest, PlayoutBufferTests) {
  AudioDeviceModule::BufferType bufferType;
  uint16_t sizeMS(0);

  CheckInitialPlayoutStates();
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD) || defined(ANDROID) || \
    defined(WEBRTC_IOS)
  EXPECT_EQ(AudioDeviceModule::kAdaptiveBufferSize, bufferType);
#else
  EXPECT_EQ(AudioDeviceModule::kFixedBufferSize, bufferType);
#endif

  // fail tests
  EXPECT_EQ(-1, audio_device_->InitPlayout());
  // must set device first
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_TRUE(audio_device_->PlayoutIsInitialized());
#endif
  EXPECT_TRUE(audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kAdaptiveBufferSize, 100) == -1);
  EXPECT_EQ(0, audio_device_->StopPlayout());
  EXPECT_TRUE(audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, kAdmMinPlayoutBufferSizeMs-1) == -1);
  EXPECT_TRUE(audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, kAdmMaxPlayoutBufferSizeMs+1) == -1);

  // bulk tests (all should be successful)
  EXPECT_FALSE(audio_device_->PlayoutIsInitialized());
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kAdaptiveBufferSize, 0));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kAdaptiveBufferSize, bufferType);
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kAdaptiveBufferSize, 10000));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kAdaptiveBufferSize, bufferType);
#endif
#if defined(ANDROID) || defined(WEBRTC_IOS)
  EXPECT_EQ(-1,
            audio_device_->SetPlayoutBuffer(AudioDeviceModule::kFixedBufferSize,
                                          kAdmMinPlayoutBufferSizeMs));
#else
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, kAdmMinPlayoutBufferSizeMs));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kFixedBufferSize, bufferType);
  EXPECT_EQ(kAdmMinPlayoutBufferSizeMs, sizeMS);
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, kAdmMaxPlayoutBufferSizeMs));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kFixedBufferSize, bufferType);
  EXPECT_EQ(kAdmMaxPlayoutBufferSizeMs, sizeMS);
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kFixedBufferSize, 100));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
  EXPECT_EQ(AudioDeviceModule::kFixedBufferSize, bufferType);
  EXPECT_EQ(100, sizeMS);
#endif

#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  // restore default
  EXPECT_EQ(0, audio_device_->SetPlayoutBuffer(
      AudioDeviceModule::kAdaptiveBufferSize, 0));
  EXPECT_EQ(0, audio_device_->PlayoutBuffer(&bufferType, &sizeMS));
#endif
}

TEST_F(AudioDeviceAPITest, PlayoutDelay) {
  // NOTE: this API is better tested in a functional test
  uint16_t sizeMS(0);
  CheckInitialPlayoutStates();
  // bulk tests
  EXPECT_EQ(0, audio_device_->PlayoutDelay(&sizeMS));
  EXPECT_EQ(0, audio_device_->PlayoutDelay(&sizeMS));
}

TEST_F(AudioDeviceAPITest, RecordingDelay) {
  // NOTE: this API is better tested in a functional test
  uint16_t sizeMS(0);
  CheckInitialRecordingStates();

  // bulk tests
  EXPECT_EQ(0, audio_device_->RecordingDelay(&sizeMS));
  EXPECT_EQ(0, audio_device_->RecordingDelay(&sizeMS));
}

TEST_F(AudioDeviceAPITest, CPULoad) {
  // NOTE: this API is better tested in a functional test
  uint16_t load(0);

  // bulk tests
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  EXPECT_EQ(0, audio_device_->CPULoad(&load));
  EXPECT_EQ(0, load);
#else
  EXPECT_EQ(-1, audio_device_->CPULoad(&load));
#endif
}

// TODO(kjellander): Fix flakiness causing failures on Windows.
// TODO(phoglund):  Fix flakiness causing failures on Linux.
#if !defined(_WIN32) && !defined(WEBRTC_LINUX)
TEST_F(AudioDeviceAPITest, StartAndStopRawOutputFileRecording) {
  // NOTE: this API is better tested in a functional test
  CheckInitialPlayoutStates();

  // fail tests
  EXPECT_EQ(-1, audio_device_->StartRawOutputFileRecording(NULL));

  // bulk tests
  EXPECT_EQ(0, audio_device_->StartRawOutputFileRecording(
      GetFilename("raw_output_not_playing.pcm")));
  EXPECT_EQ(0, audio_device_->StopRawOutputFileRecording());
  EXPECT_EQ(0, audio_device_->SetPlayoutDevice(
      MACRO_DEFAULT_COMMUNICATION_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitPlayout());
  EXPECT_EQ(0, audio_device_->StartPlayout());
#endif

  EXPECT_EQ(0, audio_device_->StartRawOutputFileRecording(
      GetFilename("raw_output_playing.pcm")));
  SleepMs(100);
  EXPECT_EQ(0, audio_device_->StopRawOutputFileRecording());
  EXPECT_EQ(0, audio_device_->StopPlayout());
  EXPECT_EQ(0, audio_device_->StartRawOutputFileRecording(
      GetFilename("raw_output_not_playing.pcm")));
  EXPECT_EQ(0, audio_device_->StopRawOutputFileRecording());

  // results after this test:
  //
  // - size of raw_output_not_playing.pcm shall be 0
  // - size of raw_output_playing.pcm shall be > 0
}

TEST_F(AudioDeviceAPITest, StartAndStopRawInputFileRecording) {
  // NOTE: this API is better tested in a functional test
  CheckInitialRecordingStates();
  EXPECT_FALSE(audio_device_->Playing());

  // fail tests
  EXPECT_EQ(-1, audio_device_->StartRawInputFileRecording(NULL));

  // bulk tests
  EXPECT_EQ(0, audio_device_->StartRawInputFileRecording(
      GetFilename("raw_input_not_recording.pcm")));
  EXPECT_EQ(0, audio_device_->StopRawInputFileRecording());
  EXPECT_EQ(0, audio_device_->SetRecordingDevice(MACRO_DEFAULT_DEVICE));

  // TODO(kjellander): Fix so these tests pass on Mac.
#if !defined(WEBRTC_MAC)
  EXPECT_EQ(0, audio_device_->InitRecording());
  EXPECT_EQ(0, audio_device_->StartRecording());
#endif
  EXPECT_EQ(0, audio_device_->StartRawInputFileRecording(
      GetFilename("raw_input_recording.pcm")));
  SleepMs(100);
  EXPECT_EQ(0, audio_device_->StopRawInputFileRecording());
  EXPECT_EQ(0, audio_device_->StopRecording());
  EXPECT_EQ(0, audio_device_->StartRawInputFileRecording(
      GetFilename("raw_input_not_recording.pcm")));
  EXPECT_EQ(0, audio_device_->StopRawInputFileRecording());

  // results after this test:
  //
  // - size of raw_input_not_recording.pcm shall be 0
  // - size of raw_input_not_recording.pcm shall be > 0
}
#endif  // !WIN32 && !WEBRTC_LINUX

TEST_F(AudioDeviceAPITest, RecordingSampleRate) {
  uint32_t sampleRate(0);

  // bulk tests
  EXPECT_EQ(0, audio_device_->RecordingSampleRate(&sampleRate));
#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  EXPECT_EQ(48000, sampleRate);
#elif defined(ANDROID)
  TEST_LOG("Recording sample rate is %u\n\n", sampleRate);
  EXPECT_TRUE((sampleRate == 44000) || (sampleRate == 16000));
#elif defined(WEBRTC_IOS)
  TEST_LOG("Recording sample rate is %u\n\n", sampleRate);
  EXPECT_TRUE((sampleRate == 44000) || (sampleRate == 16000) ||
              (sampleRate == 8000));
#endif

  // @TODO(xians) - add tests for all platforms here...
}

TEST_F(AudioDeviceAPITest, PlayoutSampleRate) {
  uint32_t sampleRate(0);

  // bulk tests
  EXPECT_EQ(0, audio_device_->PlayoutSampleRate(&sampleRate));
#if defined(_WIN32) && !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
  EXPECT_EQ(48000, sampleRate);
#elif defined(ANDROID)
  TEST_LOG("Playout sample rate is %u\n\n", sampleRate);
  EXPECT_TRUE((sampleRate == 44000) || (sampleRate == 16000));
#elif defined(WEBRTC_IOS)
  TEST_LOG("Playout sample rate is %u\n\n", sampleRate);
  EXPECT_TRUE((sampleRate == 44000) || (sampleRate == 16000) ||
              (sampleRate == 8000));
#endif
}
