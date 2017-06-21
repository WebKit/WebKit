/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstring>
#include <numeric>

#include "webrtc/base/array_view.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/race_checker.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/audio_device/audio_device_impl.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/modules/audio_device/include/mock_audio_transport.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Ge;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;

namespace webrtc {
namespace {

// #define ENABLE_DEBUG_PRINTF
#ifdef ENABLE_DEBUG_PRINTF
#define PRINTD(...) fprintf(stderr, __VA_ARGS__);
#else
#define PRINTD(...) ((void)0)
#endif
#define PRINT(...) fprintf(stderr, __VA_ARGS__);

// Don't run these tests in combination with sanitizers.
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
#define SKIP_TEST_IF_NOT(requirements_satisfied) \
  do {                                           \
    if (!requirements_satisfied) {               \
      return;                                    \
    }                                            \
  } while (false)
#else
// Or if other audio-related requirements are not met.
#define SKIP_TEST_IF_NOT(requirements_satisfied) \
  do {                                           \
    return;                                      \
  } while (false)
#endif

// Number of callbacks (input or output) the tests waits for before we set
// an event indicating that the test was OK.
static constexpr size_t kNumCallbacks = 10;
// Max amount of time we wait for an event to be set while counting callbacks.
static constexpr size_t kTestTimeOutInMilliseconds = 10 * 1000;
// Average number of audio callbacks per second assuming 10ms packet size.
static constexpr size_t kNumCallbacksPerSecond = 100;
// Run the full-duplex test during this time (unit is in seconds).
static constexpr size_t kFullDuplexTimeInSec = 5;
// Length of round-trip latency measurements. Number of deteced impulses
// shall be kImpulseFrequencyInHz * kMeasureLatencyTimeInSec - 1 since the
// last transmitted pulse is not used.
static constexpr size_t kMeasureLatencyTimeInSec = 10;
// Sets the number of impulses per second in the latency test.
static constexpr size_t kImpulseFrequencyInHz = 1;
// Utilized in round-trip latency measurements to avoid capturing noise samples.
static constexpr int kImpulseThreshold = 1000;

enum class TransportType {
  kInvalid,
  kPlay,
  kRecord,
  kPlayAndRecord,
};

// Interface for processing the audio stream. Real implementations can e.g.
// run audio in loopback, read audio from a file or perform latency
// measurements.
class AudioStream {
 public:
  virtual void Write(rtc::ArrayView<const int16_t> source, size_t channels) = 0;
  virtual void Read(rtc::ArrayView<int16_t> destination, size_t channels) = 0;

  virtual ~AudioStream() = default;
};

// Converts index corresponding to position within a 10ms buffer into a
// delay value in milliseconds.
// Example: index=240, frames_per_10ms_buffer=480 => 5ms as output.
int IndexToMilliseconds(size_t index, size_t frames_per_10ms_buffer) {
  return rtc::checked_cast<int>(
      10.0 * (static_cast<double>(index) / frames_per_10ms_buffer) + 0.5);
}

}  // namespace

// Simple first in first out (FIFO) class that wraps a list of 16-bit audio
// buffers of fixed size and allows Write and Read operations. The idea is to
// store recorded audio buffers (using Write) and then read (using Read) these
// stored buffers with as short delay as possible when the audio layer needs
// data to play out. The number of buffers in the FIFO will stabilize under
// normal conditions since there will be a balance between Write and Read calls.
// The container is a std::list container and access is protected with a lock
// since both sides (playout and recording) are driven by its own thread.
// Note that, we know by design that the size of the audio buffer will not
// change over time and that both sides will use the same size.
class FifoAudioStream : public AudioStream {
 public:
  void Write(rtc::ArrayView<const int16_t> source, size_t channels) override {
    EXPECT_EQ(channels, 1u);
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
    const size_t size = [&] {
      rtc::CritScope lock(&lock_);
      fifo_.push_back(Buffer16(source.data(), source.size()));
      return fifo_.size();
    }();
    if (size > max_size_) {
      max_size_ = size;
    }
    // Add marker once per second to signal that audio is active.
    if (write_count_++ % 100 == 0) {
      PRINT(".");
    }
    written_elements_ += size;
  }

  void Read(rtc::ArrayView<int16_t> destination, size_t channels) override {
    EXPECT_EQ(channels, 1u);
    rtc::CritScope lock(&lock_);
    if (fifo_.empty()) {
      std::fill(destination.begin(), destination.end(), 0);
    } else {
      const Buffer16& buffer = fifo_.front();
      RTC_CHECK_EQ(buffer.size(), destination.size());
      std::copy(buffer.begin(), buffer.end(), destination.begin());
      fifo_.pop_front();
    }
  }

  size_t size() const {
    rtc::CritScope lock(&lock_);
    return fifo_.size();
  }

  size_t max_size() const {
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
    return max_size_;
  }

  size_t average_size() const {
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
    return 0.5 + static_cast<float>(written_elements_ / write_count_);
  }

  using Buffer16 = rtc::BufferT<int16_t>;

  rtc::CriticalSection lock_;
  rtc::RaceChecker race_checker_;

  std::list<Buffer16> fifo_ GUARDED_BY(lock_);
  size_t write_count_ GUARDED_BY(race_checker_) = 0;
  size_t max_size_ GUARDED_BY(race_checker_) = 0;
  size_t written_elements_ GUARDED_BY(race_checker_) = 0;
};

// Inserts periodic impulses and measures the latency between the time of
// transmission and time of receiving the same impulse.
class LatencyAudioStream : public AudioStream {
 public:
  LatencyAudioStream() {
    // Delay thread checkers from being initialized until first callback from
    // respective thread.
    read_thread_checker_.DetachFromThread();
    write_thread_checker_.DetachFromThread();
  }

  // Insert periodic impulses in first two samples of |destination|.
  void Read(rtc::ArrayView<int16_t> destination, size_t channels) override {
    RTC_DCHECK_RUN_ON(&read_thread_checker_);
    EXPECT_EQ(channels, 1u);
    if (read_count_ == 0) {
      PRINT("[");
    }
    read_count_++;
    std::fill(destination.begin(), destination.end(), 0);
    if (read_count_ % (kNumCallbacksPerSecond / kImpulseFrequencyInHz) == 0) {
      PRINT(".");
      {
        rtc::CritScope lock(&lock_);
        if (!pulse_time_) {
          pulse_time_ = rtc::Optional<int64_t>(rtc::TimeMillis());
        }
      }
      constexpr int16_t impulse = std::numeric_limits<int16_t>::max();
      std::fill_n(destination.begin(), 2, impulse);
    }
  }

  // Detect received impulses in |source|, derive time between transmission and
  // detection and add the calculated delay to list of latencies.
  void Write(rtc::ArrayView<const int16_t> source, size_t channels) override {
    EXPECT_EQ(channels, 1u);
    RTC_DCHECK_RUN_ON(&write_thread_checker_);
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
    rtc::CritScope lock(&lock_);
    write_count_++;
    if (!pulse_time_) {
      // Avoid detection of new impulse response until a new impulse has
      // been transmitted (sets |pulse_time_| to value larger than zero).
      return;
    }
    // Find index (element position in vector) of the max element.
    const size_t index_of_max =
        std::max_element(source.begin(), source.end()) - source.begin();
    // Derive time between transmitted pulse and received pulse if the level
    // is high enough (removes noise).
    const size_t max = source[index_of_max];
    if (max > kImpulseThreshold) {
      PRINTD("(%zu, %zu)", max, index_of_max);
      int64_t now_time = rtc::TimeMillis();
      int extra_delay = IndexToMilliseconds(index_of_max, source.size());
      PRINTD("[%d]", rtc::checked_cast<int>(now_time - pulse_time_));
      PRINTD("[%d]", extra_delay);
      // Total latency is the difference between transmit time and detection
      // tome plus the extra delay within the buffer in which we detected the
      // received impulse. It is transmitted at sample 0 but can be received
      // at sample N where N > 0. The term |extra_delay| accounts for N and it
      // is a value between 0 and 10ms.
      latencies_.push_back(now_time - *pulse_time_ + extra_delay);
      pulse_time_.reset();
    } else {
      PRINTD("-");
    }
  }

  size_t num_latency_values() const {
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
    return latencies_.size();
  }

  int min_latency() const {
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
    if (latencies_.empty())
      return 0;
    return *std::min_element(latencies_.begin(), latencies_.end());
  }

  int max_latency() const {
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
    if (latencies_.empty())
      return 0;
    return *std::max_element(latencies_.begin(), latencies_.end());
  }

  int average_latency() const {
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
    if (latencies_.empty())
      return 0;
    return 0.5 + static_cast<double>(
                     std::accumulate(latencies_.begin(), latencies_.end(), 0)) /
                     latencies_.size();
  }

  void PrintResults() const {
    RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
    PRINT("] ");
    for (auto it = latencies_.begin(); it != latencies_.end(); ++it) {
      PRINTD("%d ", *it);
    }
    PRINT("\n");
    PRINT("[..........] [min, max, avg]=[%d, %d, %d] ms\n", min_latency(),
          max_latency(), average_latency());
  }

  rtc::CriticalSection lock_;
  rtc::RaceChecker race_checker_;
  rtc::ThreadChecker read_thread_checker_;
  rtc::ThreadChecker write_thread_checker_;

  rtc::Optional<int64_t> pulse_time_ GUARDED_BY(lock_);
  std::vector<int> latencies_ GUARDED_BY(race_checker_);
  size_t read_count_ ACCESS_ON(read_thread_checker_) = 0;
  size_t write_count_ ACCESS_ON(write_thread_checker_) = 0;
};

// Mocks the AudioTransport object and proxies actions for the two callbacks
// (RecordedDataIsAvailable and NeedMorePlayData) to different implementations
// of AudioStreamInterface.
class MockAudioTransport : public test::MockAudioTransport {
 public:
  explicit MockAudioTransport(TransportType type) : type_(type) {}
  ~MockAudioTransport() {}

  // Set default actions of the mock object. We are delegating to fake
  // implementation where the number of callbacks is counted and an event
  // is set after a certain number of callbacks. Audio parameters are also
  // checked.
  void HandleCallbacks(rtc::Event* event,
                       AudioStream* audio_stream,
                       int num_callbacks) {
    event_ = event;
    audio_stream_ = audio_stream;
    num_callbacks_ = num_callbacks;
    if (play_mode()) {
      ON_CALL(*this, NeedMorePlayData(_, _, _, _, _, _, _, _))
          .WillByDefault(
              Invoke(this, &MockAudioTransport::RealNeedMorePlayData));
    }
    if (rec_mode()) {
      ON_CALL(*this, RecordedDataIsAvailable(_, _, _, _, _, _, _, _, _, _))
          .WillByDefault(
              Invoke(this, &MockAudioTransport::RealRecordedDataIsAvailable));
    }
  }

  int32_t RealRecordedDataIsAvailable(const void* audio_buffer,
                                      const size_t samples_per_channel,
                                      const size_t bytes_per_frame,
                                      const size_t channels,
                                      const uint32_t sample_rate,
                                      const uint32_t total_delay_ms,
                                      const int32_t clock_drift,
                                      const uint32_t current_mic_level,
                                      const bool typing_status,
                                      uint32_t& new_mic_level) {
    EXPECT_TRUE(rec_mode()) << "No test is expecting these callbacks.";
    LOG(INFO) << "+";
    // Store audio parameters once in the first callback. For all other
    // callbacks, verify that the provided audio parameters are maintained and
    // that each callback corresponds to 10ms for any given sample rate.
    if (!record_parameters_.is_complete()) {
      record_parameters_.reset(sample_rate, channels, samples_per_channel);
    } else {
      EXPECT_EQ(samples_per_channel, record_parameters_.frames_per_buffer());
      EXPECT_EQ(bytes_per_frame, record_parameters_.GetBytesPerFrame());
      EXPECT_EQ(channels, record_parameters_.channels());
      EXPECT_EQ(static_cast<int>(sample_rate),
                record_parameters_.sample_rate());
      EXPECT_EQ(samples_per_channel,
                record_parameters_.frames_per_10ms_buffer());
    }
    rec_count_++;
    // Write audio data to audio stream object if one has been injected.
    if (audio_stream_) {
      audio_stream_->Write(
          rtc::MakeArrayView(static_cast<const int16_t*>(audio_buffer),
                             samples_per_channel * channels),
          channels);
    }
    // Signal the event after given amount of callbacks.
    if (ReceivedEnoughCallbacks()) {
      event_->Set();
    }
    return 0;
  }

  int32_t RealNeedMorePlayData(const size_t samples_per_channel,
                               const size_t bytes_per_frame,
                               const size_t channels,
                               const uint32_t sample_rate,
                               void* audio_buffer,
                               size_t& samples_per_channel_out,
                               int64_t* elapsed_time_ms,
                               int64_t* ntp_time_ms) {
    EXPECT_TRUE(play_mode()) << "No test is expecting these callbacks.";
    LOG(INFO) << "-";
    // Store audio parameters once in the first callback. For all other
    // callbacks, verify that the provided audio parameters are maintained and
    // that each callback corresponds to 10ms for any given sample rate.
    if (!playout_parameters_.is_complete()) {
      playout_parameters_.reset(sample_rate, channels, samples_per_channel);
    } else {
      EXPECT_EQ(samples_per_channel, playout_parameters_.frames_per_buffer());
      EXPECT_EQ(bytes_per_frame, playout_parameters_.GetBytesPerFrame());
      EXPECT_EQ(channels, playout_parameters_.channels());
      EXPECT_EQ(static_cast<int>(sample_rate),
                playout_parameters_.sample_rate());
      EXPECT_EQ(samples_per_channel,
                playout_parameters_.frames_per_10ms_buffer());
    }
    play_count_++;
    samples_per_channel_out = samples_per_channel;
    // Read audio data from audio stream object if one has been injected.
    if (audio_stream_) {
      audio_stream_->Read(
          rtc::MakeArrayView(static_cast<int16_t*>(audio_buffer),
                             samples_per_channel * channels),
          channels);
    } else {
      // Fill the audio buffer with zeros to avoid disturbing audio.
      const size_t num_bytes = samples_per_channel * bytes_per_frame;
      std::memset(audio_buffer, 0, num_bytes);
    }
    // Signal the event after given amount of callbacks.
    if (ReceivedEnoughCallbacks()) {
      event_->Set();
    }
    return 0;
  }

  bool ReceivedEnoughCallbacks() {
    bool recording_done = false;
    if (rec_mode()) {
      recording_done = rec_count_ >= num_callbacks_;
    } else {
      recording_done = true;
    }
    bool playout_done = false;
    if (play_mode()) {
      playout_done = play_count_ >= num_callbacks_;
    } else {
      playout_done = true;
    }
    return recording_done && playout_done;
  }

  bool play_mode() const {
    return type_ == TransportType::kPlay ||
           type_ == TransportType::kPlayAndRecord;
  }

  bool rec_mode() const {
    return type_ == TransportType::kRecord ||
           type_ == TransportType::kPlayAndRecord;
  }

 private:
  TransportType type_ = TransportType::kInvalid;
  rtc::Event* event_ = nullptr;
  AudioStream* audio_stream_ = nullptr;
  size_t num_callbacks_ = 0;
  size_t play_count_ = 0;
  size_t rec_count_ = 0;
  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;
};

// AudioDeviceTest test fixture.
class AudioDeviceTest : public ::testing::Test {
 protected:
  AudioDeviceTest() : event_(false, false) {
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
    rtc::LogMessage::LogToDebug(rtc::LS_INFO);
    // Add extra logging fields here if needed for debugging.
    // rtc::LogMessage::LogTimestamps();
    // rtc::LogMessage::LogThreads();
    audio_device_ =
        AudioDeviceModule::Create(0, AudioDeviceModule::kPlatformDefaultAudio);
    EXPECT_NE(audio_device_.get(), nullptr);
    AudioDeviceModule::AudioLayer audio_layer;
    int got_platform_audio_layer =
        audio_device_->ActiveAudioLayer(&audio_layer);
    if (got_platform_audio_layer != 0 ||
        audio_layer == AudioDeviceModule::kLinuxAlsaAudio) {
      requirements_satisfied_ = false;
    }
    if (requirements_satisfied_) {
      EXPECT_EQ(0, audio_device_->Init());
      const int16_t num_playout_devices = audio_device_->PlayoutDevices();
      const int16_t num_record_devices = audio_device_->RecordingDevices();
      requirements_satisfied_ =
          num_playout_devices > 0 && num_record_devices > 0;
    }
#else
    requirements_satisfied_ = false;
#endif
    if (requirements_satisfied_) {
      EXPECT_EQ(0, audio_device_->SetPlayoutDevice(0));
      EXPECT_EQ(0, audio_device_->InitSpeaker());
      EXPECT_EQ(0, audio_device_->SetRecordingDevice(0));
      EXPECT_EQ(0, audio_device_->InitMicrophone());
      EXPECT_EQ(0, audio_device_->StereoPlayoutIsAvailable(&stereo_playout_));
      EXPECT_EQ(0, audio_device_->SetStereoPlayout(stereo_playout_));
      // Avoid asking for input stereo support and always record in mono
      // since asking can cause issues in combination with remote desktop.
      // See https://bugs.chromium.org/p/webrtc/issues/detail?id=7397 for
      // details.
      EXPECT_EQ(0, audio_device_->SetStereoRecording(false));
      EXPECT_EQ(0, audio_device_->SetAGC(false));
      EXPECT_FALSE(audio_device_->AGC());
    }
  }

  virtual ~AudioDeviceTest() {
    if (audio_device_) {
      EXPECT_EQ(0, audio_device_->Terminate());
    }
  }

  bool requirements_satisfied() const { return requirements_satisfied_; }
  rtc::Event* event() { return &event_; }

  const rtc::scoped_refptr<AudioDeviceModule>& audio_device() const {
    return audio_device_;
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
    EXPECT_FALSE(audio_device()->PlayoutIsInitialized());
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
    EXPECT_FALSE(audio_device()->RecordingIsInitialized());
  }

 private:
  bool requirements_satisfied_ = true;
  rtc::Event event_;
  rtc::scoped_refptr<AudioDeviceModule> audio_device_;
  bool stereo_playout_ = false;
};

// Uses the test fixture to create, initialize and destruct the ADM.
TEST_F(AudioDeviceTest, ConstructDestruct) {}

TEST_F(AudioDeviceTest, InitTerminate) {
  SKIP_TEST_IF_NOT(requirements_satisfied());
  // Initialization is part of the test fixture.
  EXPECT_TRUE(audio_device()->Initialized());
  EXPECT_EQ(0, audio_device()->Terminate());
  EXPECT_FALSE(audio_device()->Initialized());
}

// Tests Start/Stop playout without any registered audio callback.
TEST_F(AudioDeviceTest, StartStopPlayout) {
  SKIP_TEST_IF_NOT(requirements_satisfied());
  StartPlayout();
  StopPlayout();
  StartPlayout();
  StopPlayout();
}

// Tests Start/Stop recording without any registered audio callback.
TEST_F(AudioDeviceTest, StartStopRecording) {
  SKIP_TEST_IF_NOT(requirements_satisfied());
  StartRecording();
  StopRecording();
  StartRecording();
  StopRecording();
}

// Start playout and verify that the native audio layer starts asking for real
// audio samples to play out using the NeedMorePlayData() callback.
// Note that we can't add expectations on audio parameters in EXPECT_CALL
// since parameter are not provided in the each callback. We therefore test and
// verify the parameters in the fake audio transport implementation instead.
TEST_F(AudioDeviceTest, StartPlayoutVerifyCallbacks) {
  SKIP_TEST_IF_NOT(requirements_satisfied());
  MockAudioTransport mock(TransportType::kPlay);
  mock.HandleCallbacks(event(), nullptr, kNumCallbacks);
  EXPECT_CALL(mock, NeedMorePlayData(_, _, _, _, NotNull(), _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  event()->Wait(kTestTimeOutInMilliseconds);
  StopPlayout();
}

// Start recording and verify that the native audio layer starts providing real
// audio samples using the RecordedDataIsAvailable() callback.
TEST_F(AudioDeviceTest, StartRecordingVerifyCallbacks) {
  SKIP_TEST_IF_NOT(requirements_satisfied());
  MockAudioTransport mock(TransportType::kRecord);
  mock.HandleCallbacks(event(), nullptr, kNumCallbacks);
  EXPECT_CALL(mock, RecordedDataIsAvailable(NotNull(), _, _, _, _, Ge(0u), 0, _,
                                            false, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartRecording();
  event()->Wait(kTestTimeOutInMilliseconds);
  StopRecording();
}

// Start playout and recording (full-duplex audio) and verify that audio is
// active in both directions.
TEST_F(AudioDeviceTest, StartPlayoutAndRecordingVerifyCallbacks) {
  SKIP_TEST_IF_NOT(requirements_satisfied());
  MockAudioTransport mock(TransportType::kPlayAndRecord);
  mock.HandleCallbacks(event(), nullptr, kNumCallbacks);
  EXPECT_CALL(mock, NeedMorePlayData(_, _, _, _, NotNull(), _, _, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_CALL(mock, RecordedDataIsAvailable(NotNull(), _, _, _, _, Ge(0u), 0, _,
                                            false, _))
      .Times(AtLeast(kNumCallbacks));
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  StartPlayout();
  StartRecording();
  event()->Wait(kTestTimeOutInMilliseconds);
  StopRecording();
  StopPlayout();
}

// Start playout and recording and store recorded data in an intermediate FIFO
// buffer from which the playout side then reads its samples in the same order
// as they were stored. Under ideal circumstances, a callback sequence would
// look like: ...+-+-+-+-+-+-+-..., where '+' means 'packet recorded' and '-'
// means 'packet played'. Under such conditions, the FIFO would contain max 1,
// with an average somewhere in (0,1) depending on how long the packets are
// buffered. However, under more realistic conditions, the size
// of the FIFO will vary more due to an unbalance between the two sides.
// This test tries to verify that the device maintains a balanced callback-
// sequence by running in loopback for a few seconds while measuring the size
// (max and average) of the FIFO. The size of the FIFO is increased by the
// recording side and decreased by the playout side.
TEST_F(AudioDeviceTest, RunPlayoutAndRecordingInFullDuplex) {
  SKIP_TEST_IF_NOT(requirements_satisfied());
  NiceMock<MockAudioTransport> mock(TransportType::kPlayAndRecord);
  FifoAudioStream audio_stream;
  mock.HandleCallbacks(event(), &audio_stream,
                       kFullDuplexTimeInSec * kNumCallbacksPerSecond);
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  // Run both sides in mono to make the loopback packet handling less complex.
  // The test works for stereo as well; the only requirement is that both sides
  // use the same configuration.
  EXPECT_EQ(0, audio_device()->SetStereoPlayout(false));
  EXPECT_EQ(0, audio_device()->SetStereoRecording(false));
  StartPlayout();
  StartRecording();
  event()->Wait(static_cast<int>(
      std::max(kTestTimeOutInMilliseconds, 1000 * kFullDuplexTimeInSec)));
  StopRecording();
  StopPlayout();
  // This thresholds is set rather high to accommodate differences in hardware
  // in several devices. The main idea is to capture cases where a very large
  // latency is built up.
  EXPECT_LE(audio_stream.average_size(), 5u);
  PRINT("\n");
}

// Measures loopback latency and reports the min, max and average values for
// a full duplex audio session.
// The latency is measured like so:
// - Insert impulses periodically on the output side.
// - Detect the impulses on the input side.
// - Measure the time difference between the transmit time and receive time.
// - Store time differences in a vector and calculate min, max and average.
// This test needs the '--gtest_also_run_disabled_tests' flag to run and also
// some sort of audio feedback loop. E.g. a headset where the mic is placed
// close to the speaker to ensure highest possible echo. It is also recommended
// to run the test at highest possible output volume.
TEST_F(AudioDeviceTest, DISABLED_MeasureLoopbackLatency) {
  SKIP_TEST_IF_NOT(requirements_satisfied());
  NiceMock<MockAudioTransport> mock(TransportType::kPlayAndRecord);
  LatencyAudioStream audio_stream;
  mock.HandleCallbacks(event(), &audio_stream,
                       kMeasureLatencyTimeInSec * kNumCallbacksPerSecond);
  EXPECT_EQ(0, audio_device()->RegisterAudioCallback(&mock));
  EXPECT_EQ(0, audio_device()->SetStereoPlayout(false));
  EXPECT_EQ(0, audio_device()->SetStereoRecording(false));
  StartPlayout();
  StartRecording();
  event()->Wait(static_cast<int>(
      std::max(kTestTimeOutInMilliseconds, 1000 * kMeasureLatencyTimeInSec)));
  StopRecording();
  StopPlayout();
  // Verify that the correct number of transmitted impulses are detected.
  EXPECT_EQ(audio_stream.num_latency_values(),
            static_cast<size_t>(
                kImpulseFrequencyInHz * kMeasureLatencyTimeInSec - 1));
  // Print out min, max and average delay values for debugging purposes.
  audio_stream.PrintResults();
}

}  // namespace webrtc
