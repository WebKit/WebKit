/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <limits>
#include <list>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "modules/audio_device/audio_device_impl.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/mock_audio_transport.h"
#include "modules/audio_device/ios/audio_device_ios.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/format_macros.h"
#include "rtc_base/logging.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/timeutils.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

#import "sdk/objc/components/audio/RTCAudioSession+Private.h"
#import "sdk/objc/components/audio/RTCAudioSession.h"

using std::cout;
using std::endl;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

// #define ENABLE_DEBUG_PRINTF
#ifdef ENABLE_DEBUG_PRINTF
#define PRINTD(...) fprintf(stderr, __VA_ARGS__);
#else
#define PRINTD(...) ((void)0)
#endif
#define PRINT(...) fprintf(stderr, __VA_ARGS__);

namespace webrtc {

// Number of callbacks (input or output) the tests waits for before we set
// an event indicating that the test was OK.
static const size_t kNumCallbacks = 10;
// Max amount of time we wait for an event to be set while counting callbacks.
static const int kTestTimeOutInMilliseconds = 10 * 1000;
// Number of bits per PCM audio sample.
static const size_t kBitsPerSample = 16;
// Number of bytes per PCM audio sample.
static const size_t kBytesPerSample = kBitsPerSample / 8;
// Average number of audio callbacks per second assuming 10ms packet size.
static const size_t kNumCallbacksPerSecond = 100;
// Play out a test file during this time (unit is in seconds).
static const int kFilePlayTimeInSec = 15;
// Run the full-duplex test during this time (unit is in seconds).
// Note that first |kNumIgnoreFirstCallbacks| are ignored.
static const int kFullDuplexTimeInSec = 10;
// Wait for the callback sequence to stabilize by ignoring this amount of the
// initial callbacks (avoids initial FIFO access).
// Only used in the RunPlayoutAndRecordingInFullDuplex test.
static const size_t kNumIgnoreFirstCallbacks = 50;
// Sets the number of impulses per second in the latency test.
// TODO(henrika): fine tune this setting for iOS.
static const int kImpulseFrequencyInHz = 1;
// Length of round-trip latency measurements. Number of transmitted impulses
// is kImpulseFrequencyInHz * kMeasureLatencyTimeInSec - 1.
// TODO(henrika): fine tune this setting for iOS.
static const int kMeasureLatencyTimeInSec = 5;
// Utilized in round-trip latency measurements to avoid capturing noise samples.
// TODO(henrika): fine tune this setting for iOS.
static const int kImpulseThreshold = 50;
static const char kTag[] = "[..........] ";

enum TransportType {
  kPlayout = 0x1,
  kRecording = 0x2,
};

// Interface for processing the audio stream. Real implementations can e.g.
// run audio in loopback, read audio from a file or perform latency
// measurements.
class AudioStreamInterface {
 public:
  virtual void Write(const void* source, size_t num_frames) = 0;
  virtual void Read(void* destination, size_t num_frames) = 0;

 protected:
  virtual ~AudioStreamInterface() {}
};

// Reads audio samples from a PCM file where the file is stored in memory at
// construction.
class FileAudioStream : public AudioStreamInterface {
 public:
  FileAudioStream(size_t num_callbacks,
                  const std::string& file_name,
                  int sample_rate)
      : file_size_in_bytes_(0), sample_rate_(sample_rate), file_pos_(0) {
    file_size_in_bytes_ = test::GetFileSize(file_name);
    sample_rate_ = sample_rate;
    EXPECT_GE(file_size_in_callbacks(), num_callbacks)
        << "Size of test file is not large enough to last during the test.";
    const size_t num_16bit_samples =
        test::GetFileSize(file_name) / kBytesPerSample;
    file_.reset(new int16_t[num_16bit_samples]);
    FILE* audio_file = fopen(file_name.c_str(), "rb");
    EXPECT_NE(audio_file, nullptr);
    size_t num_samples_read =
        fread(file_.get(), sizeof(int16_t), num_16bit_samples, audio_file);
    EXPECT_EQ(num_samples_read, num_16bit_samples);
    fclose(audio_file);
  }

  // AudioStreamInterface::Write() is not implemented.
  void Write(const void* source, size_t num_frames) override {}

  // Read samples from file stored in memory (at construction) and copy
  // |num_frames| (<=> 10ms) to the |destination| byte buffer.
  void Read(void* destination, size_t num_frames) override {
    memcpy(destination, static_cast<int16_t*>(&file_[file_pos_]),
           num_frames * sizeof(int16_t));
    file_pos_ += num_frames;
  }

  int file_size_in_seconds() const {
    return static_cast<int>(
        file_size_in_bytes_ / (kBytesPerSample * sample_rate_));
  }
  size_t file_size_in_callbacks() const {
    return file_size_in_seconds() * kNumCallbacksPerSecond;
  }

 private:
  size_t file_size_in_bytes_;
  int sample_rate_;
  std::unique_ptr<int16_t[]> file_;
  size_t file_pos_;
};

// Simple first in first out (FIFO) class that wraps a list of 16-bit audio
// buffers of fixed size and allows Write and Read operations. The idea is to
// store recorded audio buffers (using Write) and then read (using Read) these
// stored buffers with as short delay as possible when the audio layer needs
// data to play out. The number of buffers in the FIFO will stabilize under
// normal conditions since there will be a balance between Write and Read calls.
// The container is a std::list container and access is protected with a lock
// since both sides (playout and recording) are driven by its own thread.
class FifoAudioStream : public AudioStreamInterface {
 public:
  explicit FifoAudioStream(size_t frames_per_buffer)
      : frames_per_buffer_(frames_per_buffer),
        bytes_per_buffer_(frames_per_buffer_ * sizeof(int16_t)),
        fifo_(new AudioBufferList),
        largest_size_(0),
        total_written_elements_(0),
        write_count_(0) {
    EXPECT_NE(fifo_.get(), nullptr);
  }

  ~FifoAudioStream() { Flush(); }

  // Allocate new memory, copy |num_frames| samples from |source| into memory
  // and add pointer to the memory location to end of the list.
  // Increases the size of the FIFO by one element.
  void Write(const void* source, size_t num_frames) override {
    ASSERT_EQ(num_frames, frames_per_buffer_);
    PRINTD("+");
    if (write_count_++ < kNumIgnoreFirstCallbacks) {
      return;
    }
    int16_t* memory = new int16_t[frames_per_buffer_];
    memcpy(static_cast<int16_t*>(&memory[0]), source, bytes_per_buffer_);
    rtc::CritScope lock(&lock_);
    fifo_->push_back(memory);
    const size_t size = fifo_->size();
    if (size > largest_size_) {
      largest_size_ = size;
      PRINTD("(%" PRIuS ")", largest_size_);
    }
    total_written_elements_ += size;
  }

  // Read pointer to data buffer from front of list, copy |num_frames| of stored
  // data into |destination| and delete the utilized memory allocation.
  // Decreases the size of the FIFO by one element.
  void Read(void* destination, size_t num_frames) override {
    ASSERT_EQ(num_frames, frames_per_buffer_);
    PRINTD("-");
    rtc::CritScope lock(&lock_);
    if (fifo_->empty()) {
      memset(destination, 0, bytes_per_buffer_);
    } else {
      int16_t* memory = fifo_->front();
      fifo_->pop_front();
      memcpy(destination, static_cast<int16_t*>(&memory[0]), bytes_per_buffer_);
      delete memory;
    }
  }

  size_t size() const { return fifo_->size(); }

  size_t largest_size() const { return largest_size_; }

  size_t average_size() const {
    return (total_written_elements_ == 0)
               ? 0.0
               : 0.5 +
                     static_cast<float>(total_written_elements_) /
                         (write_count_ - kNumIgnoreFirstCallbacks);
  }

 private:
  void Flush() {
    for (auto it = fifo_->begin(); it != fifo_->end(); ++it) {
      delete *it;
    }
    fifo_->clear();
  }

  using AudioBufferList = std::list<int16_t*>;
  rtc::CriticalSection lock_;
  const size_t frames_per_buffer_;
  const size_t bytes_per_buffer_;
  std::unique_ptr<AudioBufferList> fifo_;
  size_t largest_size_;
  size_t total_written_elements_;
  size_t write_count_;
};

// Inserts periodic impulses and measures the latency between the time of
// transmission and time of receiving the same impulse.
// Usage requires a special hardware called Audio Loopback Dongle.
// See http://source.android.com/devices/audio/loopback.html for details.
class LatencyMeasuringAudioStream : public AudioStreamInterface {
 public:
  explicit LatencyMeasuringAudioStream(size_t frames_per_buffer)
      : frames_per_buffer_(frames_per_buffer),
        bytes_per_buffer_(frames_per_buffer_ * sizeof(int16_t)),
        play_count_(0),
        rec_count_(0),
        pulse_time_(0) {}

  // Insert periodic impulses in first two samples of |destination|.
  void Read(void* destination, size_t num_frames) override {
    ASSERT_EQ(num_frames, frames_per_buffer_);
    if (play_count_ == 0) {
      PRINT("[");
    }
    play_count_++;
    memset(destination, 0, bytes_per_buffer_);
    if (play_count_ % (kNumCallbacksPerSecond / kImpulseFrequencyInHz) == 0) {
      if (pulse_time_ == 0) {
        pulse_time_ = rtc::TimeMillis();
      }
      PRINT(".");
      const int16_t impulse = std::numeric_limits<int16_t>::max();
      int16_t* ptr16 = static_cast<int16_t*>(destination);
      for (size_t i = 0; i < 2; ++i) {
        ptr16[i] = impulse;
      }
    }
  }

  // Detect received impulses in |source|, derive time between transmission and
  // detection and add the calculated delay to list of latencies.
  void Write(const void* source, size_t num_frames) override {
    ASSERT_EQ(num_frames, frames_per_buffer_);
    rec_count_++;
    if (pulse_time_ == 0) {
      // Avoid detection of new impulse response until a new impulse has
      // been transmitted (sets |pulse_time_| to value larger than zero).
      return;
    }
    const int16_t* ptr16 = static_cast<const int16_t*>(source);
    std::vector<int16_t> vec(ptr16, ptr16 + num_frames);
    // Find max value in the audio buffer.
    int max = *std::max_element(vec.begin(), vec.end());
    // Find index (element position in vector) of the max element.
    int index_of_max =
        std::distance(vec.begin(), std::find(vec.begin(), vec.end(), max));
    if (max > kImpulseThreshold) {
      PRINTD("(%d,%d)", max, index_of_max);
      int64_t now_time = rtc::TimeMillis();
      int extra_delay = IndexToMilliseconds(static_cast<double>(index_of_max));
      PRINTD("[%d]", static_cast<int>(now_time - pulse_time_));
      PRINTD("[%d]", extra_delay);
      // Total latency is the difference between transmit time and detection
      // tome plus the extra delay within the buffer in which we detected the
      // received impulse. It is transmitted at sample 0 but can be received
      // at sample N where N > 0. The term |extra_delay| accounts for N and it
      // is a value between 0 and 10ms.
      latencies_.push_back(now_time - pulse_time_ + extra_delay);
      pulse_time_ = 0;
    } else {
      PRINTD("-");
    }
  }

  size_t num_latency_values() const { return latencies_.size(); }

  int min_latency() const {
    if (latencies_.empty())
      return 0;
    return *std::min_element(latencies_.begin(), latencies_.end());
  }

  int max_latency() const {
    if (latencies_.empty())
      return 0;
    return *std::max_element(latencies_.begin(), latencies_.end());
  }

  int average_latency() const {
    if (latencies_.empty())
      return 0;
    return 0.5 +
           static_cast<double>(
               std::accumulate(latencies_.begin(), latencies_.end(), 0)) /
               latencies_.size();
  }

  void PrintResults() const {
    PRINT("] ");
    for (auto it = latencies_.begin(); it != latencies_.end(); ++it) {
      PRINT("%d ", *it);
    }
    PRINT("\n");
    PRINT("%s[min, max, avg]=[%d, %d, %d] ms\n", kTag, min_latency(),
          max_latency(), average_latency());
  }

  int IndexToMilliseconds(double index) const {
    return 10.0 * (index / frames_per_buffer_) + 0.5;
  }

 private:
  const size_t frames_per_buffer_;
  const size_t bytes_per_buffer_;
  size_t play_count_;
  size_t rec_count_;
  int64_t pulse_time_;
  std::vector<int> latencies_;
};
// Mocks the AudioTransport object and proxies actions for the two callbacks
// (RecordedDataIsAvailable and NeedMorePlayData) to different implementations
// of AudioStreamInterface.
class MockAudioTransportIOS : public test::MockAudioTransport {
 public:
  explicit MockAudioTransportIOS(int type)
      : num_callbacks_(0),
        type_(type),
        play_count_(0),
        rec_count_(0),
        audio_stream_(nullptr) {}

  virtual ~MockAudioTransportIOS() {}

  // Set default actions of the mock object. We are delegating to fake
  // implementations (of AudioStreamInterface) here.
  void HandleCallbacks(rtc::Event* test_is_done,
                       AudioStreamInterface* audio_stream,
                       size_t num_callbacks) {
    test_is_done_ = test_is_done;
    audio_stream_ = audio_stream;
    num_callbacks_ = num_callbacks;
    if (play_mode()) {
      ON_CALL(*this, NeedMorePlayData(_, _, _, _, _, _, _, _))
          .WillByDefault(
              Invoke(this, &MockAudioTransportIOS::RealNeedMorePlayData));
    }
    if (rec_mode()) {
      ON_CALL(*this, RecordedDataIsAvailable(_, _, _, _, _, _, _, _, _, _))
          .WillByDefault(Invoke(
              this, &MockAudioTransportIOS::RealRecordedDataIsAvailable));
    }
  }

  int32_t RealRecordedDataIsAvailable(const void* audioSamples,
                                      const size_t nSamples,
                                      const size_t nBytesPerSample,
                                      const size_t nChannels,
                                      const uint32_t samplesPerSec,
                                      const uint32_t totalDelayMS,
                                      const int32_t clockDrift,
                                      const uint32_t currentMicLevel,
                                      const bool keyPressed,
                                      uint32_t& newMicLevel) {
    EXPECT_TRUE(rec_mode()) << "No test is expecting these callbacks.";
    rec_count_++;
    // Process the recorded audio stream if an AudioStreamInterface
    // implementation exists.
    if (audio_stream_) {
      audio_stream_->Write(audioSamples, nSamples);
    }
    if (ReceivedEnoughCallbacks()) {
      if (test_is_done_) {
        test_is_done_->Set();
      }
    }
    return 0;
  }

  int32_t RealNeedMorePlayData(const size_t nSamples,
                               const size_t nBytesPerSample,
                               const size_t nChannels,
                               const uint32_t samplesPerSec,
                               void* audioSamples,
                               size_t& nSamplesOut,
                               int64_t* elapsed_time_ms,
                               int64_t* ntp_time_ms) {
    EXPECT_TRUE(play_mode()) << "No test is expecting these callbacks.";
    play_count_++;
    nSamplesOut = nSamples;
    // Read (possibly processed) audio stream samples to be played out if an
    // AudioStreamInterface implementation exists.
    if (audio_stream_) {
      audio_stream_->Read(audioSamples, nSamples);
    } else {
      memset(audioSamples, 0, nSamples * nBytesPerSample);
    }
    if (ReceivedEnoughCallbacks()) {
      if (test_is_done_) {
        test_is_done_->Set();
      }
    }
    return 0;
  }

  bool ReceivedEnoughCallbacks() {
    bool recording_done = false;
    if (rec_mode())
      recording_done = rec_count_ >= num_callbacks_;
    else
      recording_done = true;

    bool playout_done = false;
    if (play_mode())
      playout_done = play_count_ >= num_callbacks_;
    else
      playout_done = true;

    return recording_done && playout_done;
  }

  bool play_mode() const { return type_ & kPlayout; }
  bool rec_mode() const { return type_ & kRecording; }

 private:
  rtc::Event* test_is_done_;
  size_t num_callbacks_;
  int type_;
  size_t play_count_;
  size_t rec_count_;
  AudioStreamInterface* audio_stream_;
};

// AudioDeviceTest test fixture.
class AudioDeviceTest : public ::testing::Test {
 protected:
  AudioDeviceTest() {
    old_sev_ = rtc::LogMessage::GetLogToDebug();
    // Set suitable logging level here. Change to rtc::LS_INFO for more verbose
    // output. See webrtc/rtc_base/logging.h for complete list of options.
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);
    // Add extra logging fields here (timestamps and thread id).
    // rtc::LogMessage::LogTimestamps();
    rtc::LogMessage::LogThreads();
    // Creates an audio device using a default audio layer.
    audio_device_ = CreateAudioDevice(AudioDeviceModule::kPlatformDefaultAudio);
    EXPECT_NE(audio_device_.get(), nullptr);
    EXPECT_EQ(0, audio_device_->Init());
    EXPECT_EQ(0,
              audio_device()->GetPlayoutAudioParameters(&playout_parameters_));
    EXPECT_EQ(0, audio_device()->GetRecordAudioParameters(&record_parameters_));
  }
  virtual ~AudioDeviceTest() {
    EXPECT_EQ(0, audio_device_->Terminate());
    rtc::LogMessage::LogToDebug(old_sev_);
  }

  int playout_sample_rate() const { return playout_parameters_.sample_rate(); }
  int record_sample_rate() const { return record_parameters_.sample_rate(); }
  int playout_channels() const { return playout_parameters_.channels(); }
  int record_channels() const { return record_parameters_.channels(); }
  size_t playout_frames_per_10ms_buffer() const {
    return playout_parameters_.frames_per_10ms_buffer();
  }
  size_t record_frames_per_10ms_buffer() const {
    return record_parameters_.frames_per_10ms_buffer();
  }

  rtc::scoped_refptr<AudioDeviceModule> audio_device() const {
    return audio_device_;
  }

  AudioDeviceModuleImpl* audio_device_impl() const {
    return static_cast<AudioDeviceModuleImpl*>(audio_device_.get());
  }

  AudioDeviceBuffer* audio_device_buffer() const {
    return audio_device_impl()->GetAudioDeviceBuffer();
  }

  rtc::scoped_refptr<AudioDeviceModule> CreateAudioDevice(
      AudioDeviceModule::AudioLayer audio_layer) {
    rtc::scoped_refptr<AudioDeviceModule> module(
        AudioDeviceModule::Create(0, audio_layer));
    return module;
  }

  // Returns file name relative to the resource root given a sample rate.
  std::string GetFileName(int sample_rate) {
    EXPECT_TRUE(sample_rate == 48000 || sample_rate == 44100 ||
                sample_rate == 16000);
    char fname[64];
    snprintf(fname, sizeof(fname), "audio_device/audio_short%d",
             sample_rate / 1000);
    std::string file_name(webrtc::test::ResourcePath(fname, "pcm"));
    EXPECT_TRUE(test::FileExists(file_name));
#ifdef ENABLE_DEBUG_PRINTF
    PRINTD("file name: %s\n", file_name.c_str());
    const size_t bytes = test::GetFileSize(file_name);
    PRINTD("file size: %" PRIuS " [bytes]\n", bytes);
    PRINTD("file size: %" PRIuS " [samples]\n", bytes / kBytesPerSample);
    const int seconds =
        static_cast<int>(bytes / (sample_rate * kBytesPerSample));
    PRINTD("file size: %d [secs]\n", seconds);
    PRINTD("file size: %" PRIuS " [callbacks]\n",
           seconds * kNumCallbacksPerSecond);
#endif
    return file_name;
  }

  void StartPlayout() {
    EXPECT_FALSE(audio_device()->Playing());
    EXPECT_EQ(0, audio_device()->InitPlayout());
    EXPECT_TRUE(audio_device()->PlayoutIsInitialized());
    EXPECT_EQ(0, audio_device()->StartPlayout());
    EXPECT_TRUE(audio_device()->Playing());
  }

  void StopPlayout() {
    EXPECT_EQ(0, audio_device()->StopPlayout());
    EXPECT_FALSE(audio_device()->Playing());
  }

  void StartRecording() {
    EXPECT_FALSE(audio_device()->Recording());
    EXPECT_EQ(0, audio_device()->InitRecording());
    EXPECT_TRUE(audio_device()->RecordingIsInitialized());
    EXPECT_EQ(0, audio_device()->StartRecording());
    EXPECT_TRUE(audio_device()->Recording());
  }

  void StopRecording() {
    EXPECT_EQ(0, audio_device()->StopRecording());
    EXPECT_FALSE(audio_device()->Recording());
  }

  rtc::Event test_is_done_;
  rtc::scoped_refptr<AudioDeviceModule> audio_device_;
  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;
  rtc::LoggingSeverity old_sev_;
};

TEST_F(AudioDeviceTest, ConstructDestruct) {
  // Using the test fixture to create and destruct the audio device module.
}

TEST_F(AudioDeviceTest, InitTerminate) {
  // Initialization is part of the test fixture.
  EXPECT_TRUE(audio_device()->Initialized());
  EXPECT_EQ(0, audio_device()->Terminate());
  EXPECT_FALSE(audio_device()->Initialized());
}

// Tests that playout can be initiated, started and stopped. No audio callback
// is registered in this test.
// Failing when running on real iOS devices: bugs.webrtc.org/6889.
TEST_F(AudioDeviceTest, DISABLED_StartStopPlayout) {
  StartPlayout();
  StopPlayout();
  StartPlayout();
  StopPlayout();
}

// Tests that recording can be initiated, started and stopped. No audio callback
// is registered in this test.
// Can sometimes fail when running on real devices: bugs.webrtc.org/7888.
TEST_F(AudioDeviceTest, DISABLED_StartStopRecording) {
  StartRecording();
  StopRecording();
  StartRecording();
  StopRecording();
}

// Verify that calling StopPlayout() will leave us in an uninitialized state
// which will require a new call to InitPlayout(). This test does not call
// StartPlayout() while being uninitialized since doing so will hit a
// RTC_DCHECK.
TEST_F(AudioDeviceTest, StopPlayoutRequiresInitToRestart) {
  EXPECT_EQ(0, audio_device()->InitPlayout());
  EXPECT_EQ(0, audio_device()->StartPlayout());
  EXPECT_EQ(0, audio_device()->StopPlayout());
  EXPECT_FALSE(audio_device()->PlayoutIsInitialized());
}

// Verify that we can create two ADMs and start playing on the second ADM.
// Only the first active instance shall activate an audio session and the
// last active instance shall deactivate the audio session. The test does not
// explicitly verify correct audio session calls but instead focuses on
// ensuring that audio starts for both ADMs.

// Failing when running on real iOS devices: bugs.webrtc.org/6889.
TEST_F(AudioDeviceTest, DISABLED_StartPlayoutOnTwoInstances) {
  // Create and initialize a second/extra ADM instance. The default ADM is
  // created by the test harness.
  rtc::scoped_refptr<AudioDeviceModule> second_audio_device =
      CreateAudioDevice(AudioDeviceModule::kPlatformDefaultAudio);
  EXPECT_NE(second_audio_device.get(), nullptr);
  EXPECT_EQ(0, second_audio_device->Init());

  // Start playout for the default ADM but don't wait here. Instead use the
  // upcoming second stream for that. We set the same expectation on number
  // of callbacks as for the second stream.
  NiceMock<MockAudioTransportIOS> mock(kPlayout);
  mock.HandleCallbacks(nullptr, nullptr, 0);
  EXPECT_CALL(
      mock, NeedMorePlayData(playout_frames_per_10ms_buffer(), kBytesPerSample,
                             playout_channels(), playout_sample_rate(),
                             NotNull(), _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();

  // Initialize playout for the second ADM. If all is OK, the second ADM shall
  // reuse the audio session activated when the first ADM started playing.
  // This call will also ensure that we avoid a problem related to initializing
  // two different audio unit instances back to back (see webrtc:5166 for
  // details).
  EXPECT_EQ(0, second_audio_device->InitPlayout());
  EXPECT_TRUE(second_audio_device->PlayoutIsInitialized());

  // Start playout for the second ADM and verify that it starts as intended.
  // Passing this test ensures that initialization of the second audio unit
  // has been done successfully and that there is no conflict with the already
  // playing first ADM.
  MockAudioTransportIOS mock2(kPlayout);
  mock2.HandleCallbacks(&test_is_done_, nullptr, kNumCallbacks);
  EXPECT_CALL(
      mock2, NeedMorePlayData(playout_frames_per_10ms_buffer(), kBytesPerSample,
                              playout_channels(), playout_sample_rate(),
                              NotNull(), _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, second_audio_device->RegisterAudioCallback(&mock2));
  EXPECT_EQ(0, second_audio_device->StartPlayout());
  EXPECT_TRUE(second_audio_device->Playing());
  test_is_done_.Wait(kTestTimeOutInMilliseconds);
  EXPECT_EQ(0, second_audio_device->StopPlayout());
  EXPECT_FALSE(second_audio_device->Playing());
  EXPECT_FALSE(second_audio_device->PlayoutIsInitialized());

  EXPECT_EQ(0, second_audio_device->Terminate());
}

// Start playout and verify that the native audio layer starts asking for real
// audio samples to play out using the NeedMorePlayData callback.
TEST_F(AudioDeviceTest, StartPlayoutVerifyCallbacks) {
  MockAudioTransportIOS mock(kPlayout);
  mock.HandleCallbacks(&test_is_done_, nullptr, kNumCallbacks);
  EXPECT_CALL(mock, NeedMorePlayData(playout_frames_per_10ms_buffer(),
                                     kBytesPerSample, playout_channels(),
                                     playout_sample_rate(), NotNull(), _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  test_is_done_.Wait(kTestTimeOutInMilliseconds);
  StopPlayout();
}

// Start recording and verify that the native audio layer starts feeding real
// audio samples via the RecordedDataIsAvailable callback.
TEST_F(AudioDeviceTest, StartRecordingVerifyCallbacks) {
  MockAudioTransportIOS mock(kRecording);
  mock.HandleCallbacks(&test_is_done_, nullptr, kNumCallbacks);
  EXPECT_CALL(mock,
              RecordedDataIsAvailable(
                  NotNull(), record_frames_per_10ms_buffer(), kBytesPerSample,
                  record_channels(), record_sample_rate(),
                  _,  // TODO(henrika): fix delay
                  0, 0, false, _)).Times(AtLeast(kNumCallbacks));

  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartRecording();
  test_is_done_.Wait(kTestTimeOutInMilliseconds);
  StopRecording();
}

// Start playout and recording (full-duplex audio) and verify that audio is
// active in both directions.
TEST_F(AudioDeviceTest, StartPlayoutAndRecordingVerifyCallbacks) {
  MockAudioTransportIOS mock(kPlayout | kRecording);
  mock.HandleCallbacks(&test_is_done_, nullptr, kNumCallbacks);
  EXPECT_CALL(mock, NeedMorePlayData(playout_frames_per_10ms_buffer(),
                                     kBytesPerSample, playout_channels(),
                                     playout_sample_rate(), NotNull(), _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_CALL(mock,
              RecordedDataIsAvailable(
                  NotNull(), record_frames_per_10ms_buffer(), kBytesPerSample,
                  record_channels(), record_sample_rate(),
                  _,  // TODO(henrika): fix delay
                  0, 0, false, _)).Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  StartRecording();
  test_is_done_.Wait(kTestTimeOutInMilliseconds);
  StopRecording();
  StopPlayout();
}

// Start playout and read audio from an external PCM file when the audio layer
// asks for data to play out. Real audio is played out in this test but it does
// not contain any explicit verification that the audio quality is perfect.
TEST_F(AudioDeviceTest, RunPlayoutWithFileAsSource) {
  // TODO(henrika): extend test when mono output is supported.
  EXPECT_EQ(1, playout_channels());
  NiceMock<MockAudioTransportIOS> mock(kPlayout);
  const int num_callbacks = kFilePlayTimeInSec * kNumCallbacksPerSecond;
  std::string file_name = GetFileName(playout_sample_rate());
  std::unique_ptr<FileAudioStream> file_audio_stream(
      new FileAudioStream(num_callbacks, file_name, playout_sample_rate()));
  mock.HandleCallbacks(&test_is_done_, file_audio_stream.get(), num_callbacks);
  // SetMaxPlayoutVolume();
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  test_is_done_.Wait(kTestTimeOutInMilliseconds);
  StopPlayout();
}

TEST_F(AudioDeviceTest, Devices) {
  // Device enumeration is not supported. Verify fixed values only.
  EXPECT_EQ(1, audio_device()->PlayoutDevices());
  EXPECT_EQ(1, audio_device()->RecordingDevices());
}

// Start playout and recording and store recorded data in an intermediate FIFO
// buffer from which the playout side then reads its samples in the same order
// as they were stored. Under ideal circumstances, a callback sequence would
// look like: ...+-+-+-+-+-+-+-..., where '+' means 'packet recorded' and '-'
// means 'packet played'. Under such conditions, the FIFO would only contain
// one packet on average. However, under more realistic conditions, the size
// of the FIFO will vary more due to an unbalance between the two sides.
// This test tries to verify that the device maintains a balanced callback-
// sequence by running in loopback for ten seconds while measuring the size
// (max and average) of the FIFO. The size of the FIFO is increased by the
// recording side and decreased by the playout side.
// TODO(henrika): tune the final test parameters after running tests on several
// different devices.
TEST_F(AudioDeviceTest, RunPlayoutAndRecordingInFullDuplex) {
  EXPECT_EQ(record_channels(), playout_channels());
  EXPECT_EQ(record_sample_rate(), playout_sample_rate());
  NiceMock<MockAudioTransportIOS> mock(kPlayout | kRecording);
  std::unique_ptr<FifoAudioStream> fifo_audio_stream(
      new FifoAudioStream(playout_frames_per_10ms_buffer()));
  mock.HandleCallbacks(
      &test_is_done_, fifo_audio_stream.get(), kFullDuplexTimeInSec * kNumCallbacksPerSecond);
  // SetMaxPlayoutVolume();
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartRecording();
  StartPlayout();
  test_is_done_.Wait(std::max(kTestTimeOutInMilliseconds, 1000 * kFullDuplexTimeInSec));
  StopPlayout();
  StopRecording();
  EXPECT_LE(fifo_audio_stream->average_size(), 10u);
  EXPECT_LE(fifo_audio_stream->largest_size(), 20u);
}

// Measures loopback latency and reports the min, max and average values for
// a full duplex audio session.
// The latency is measured like so:
// - Insert impulses periodically on the output side.
// - Detect the impulses on the input side.
// - Measure the time difference between the transmit time and receive time.
// - Store time differences in a vector and calculate min, max and average.
// This test requires a special hardware called Audio Loopback Dongle.
// See http://source.android.com/devices/audio/loopback.html for details.
TEST_F(AudioDeviceTest, DISABLED_MeasureLoopbackLatency) {
  EXPECT_EQ(record_channels(), playout_channels());
  EXPECT_EQ(record_sample_rate(), playout_sample_rate());
  NiceMock<MockAudioTransportIOS> mock(kPlayout | kRecording);
  std::unique_ptr<LatencyMeasuringAudioStream> latency_audio_stream(
      new LatencyMeasuringAudioStream(playout_frames_per_10ms_buffer()));
  mock.HandleCallbacks(&test_is_done_,
                       latency_audio_stream.get(),
                       kMeasureLatencyTimeInSec * kNumCallbacksPerSecond);
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  // SetMaxPlayoutVolume();
  // DisableBuiltInAECIfAvailable();
  StartRecording();
  StartPlayout();
  test_is_done_.Wait(std::max(kTestTimeOutInMilliseconds, 1000 * kMeasureLatencyTimeInSec));
  StopPlayout();
  StopRecording();
  // Verify that the correct number of transmitted impulses are detected.
  EXPECT_EQ(latency_audio_stream->num_latency_values(),
            static_cast<size_t>(
                kImpulseFrequencyInHz * kMeasureLatencyTimeInSec - 1));
  latency_audio_stream->PrintResults();
}

// Verifies that the AudioDeviceIOS is_interrupted_ flag is reset correctly
// after an iOS AVAudioSessionInterruptionTypeEnded notification event.
// AudioDeviceIOS listens to RTCAudioSession interrupted notifications by:
// - In AudioDeviceIOS.InitPlayOrRecord registers its audio_session_observer_
//   callback with RTCAudioSession's delegate list.
// - When RTCAudioSession receives an iOS audio interrupted notification, it
//   passes the notification to callbacks in its delegate list which sets
//   AudioDeviceIOS's is_interrupted_ flag to true.
// - When AudioDeviceIOS.ShutdownPlayOrRecord is called, its
//   audio_session_observer_ callback is removed from RTCAudioSessions's
//   delegate list.
//   So if RTCAudioSession receives an iOS end audio interruption notification,
//   AudioDeviceIOS is not notified as its callback is not in RTCAudioSession's
//   delegate list. This causes AudioDeviceIOS's is_interrupted_ flag to be in
//   the wrong (true) state and the audio session will ignore audio changes.
// As RTCAudioSession keeps its own interrupted state, the fix is to initialize
// AudioDeviceIOS's is_interrupted_ flag to RTCAudioSession's isInterrupted
// flag in AudioDeviceIOS.InitPlayOrRecord.
TEST_F(AudioDeviceTest, testInterruptedAudioSession) {
  RTCAudioSession *session = [RTCAudioSession sharedInstance];
  std::unique_ptr<webrtc::AudioDeviceIOS> audio_device;
  audio_device.reset(new webrtc::AudioDeviceIOS());
  std::unique_ptr<webrtc::AudioDeviceBuffer> audio_buffer;
  audio_buffer.reset(new webrtc::AudioDeviceBuffer());
  audio_device->AttachAudioBuffer(audio_buffer.get());
  audio_device->Init();
  audio_device->InitPlayout();
  // Force interruption.
  [session notifyDidBeginInterruption];

  // Wait for notification to propagate.
  rtc::MessageQueueManager::ProcessAllMessageQueuesForTesting();
  EXPECT_TRUE(audio_device->is_interrupted_);

  // Force it for testing.
  audio_device->playing_ = false;
  audio_device->ShutdownPlayOrRecord();
  // Force it for testing.
  audio_device->audio_is_initialized_ = false;

  [session notifyDidEndInterruptionWithShouldResumeSession:YES];
  // Wait for notification to propagate.
  rtc::MessageQueueManager::ProcessAllMessageQueuesForTesting();
  EXPECT_TRUE(audio_device->is_interrupted_);

  audio_device->Init();
  audio_device->InitPlayout();
  EXPECT_FALSE(audio_device->is_interrupted_);
}

}  // namespace webrtc
