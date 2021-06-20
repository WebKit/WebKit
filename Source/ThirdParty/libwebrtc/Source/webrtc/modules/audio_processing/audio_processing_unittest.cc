/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/include/audio_processing.h"

#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <queue>

#include "absl/flags/flag.h"
#include "common_audio/include/audio_util.h"
#include "common_audio/resampler/include/push_resampler.h"
#include "common_audio/resampler/push_sinc_resampler.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "modules/audio_processing/audio_processing_impl.h"
#include "modules/audio_processing/common.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "modules/audio_processing/test/protobuf_utils.h"
#include "modules/audio_processing/test/test_utils.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gtest_prod_util.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/protobuf_utils.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/swap_queue.h"
#include "rtc_base/system/arch.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/test/unittest.pb.h"
#else
#include "modules/audio_processing/test/unittest.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

ABSL_FLAG(bool,
          write_apm_ref_data,
          false,
          "Write ApmTest.Process results to file, instead of comparing results "
          "to the existing reference data file.");

namespace webrtc {
namespace {

// TODO(ekmeyerson): Switch to using StreamConfig and ProcessingConfig where
// applicable.

const int32_t kChannels[] = {1, 2};
const int kSampleRates[] = {8000, 16000, 32000, 48000};

#if defined(WEBRTC_AUDIOPROC_FIXED_PROFILE)
// Android doesn't support 48kHz.
const int kProcessSampleRates[] = {8000, 16000, 32000};
#elif defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE)
const int kProcessSampleRates[] = {8000, 16000, 32000, 48000};
#endif

enum StreamDirection { kForward = 0, kReverse };

void ConvertToFloat(const int16_t* int_data, ChannelBuffer<float>* cb) {
  ChannelBuffer<int16_t> cb_int(cb->num_frames(), cb->num_channels());
  Deinterleave(int_data, cb->num_frames(), cb->num_channels(),
               cb_int.channels());
  for (size_t i = 0; i < cb->num_channels(); ++i) {
    S16ToFloat(cb_int.channels()[i], cb->num_frames(), cb->channels()[i]);
  }
}

void ConvertToFloat(const Int16FrameData& frame, ChannelBuffer<float>* cb) {
  ConvertToFloat(frame.data.data(), cb);
}

// Number of channels including the keyboard channel.
size_t TotalChannelsFromLayout(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
      return 1;
    case AudioProcessing::kMonoAndKeyboard:
    case AudioProcessing::kStereo:
      return 2;
    case AudioProcessing::kStereoAndKeyboard:
      return 3;
  }
  RTC_NOTREACHED();
  return 0;
}

void MixStereoToMono(const float* stereo,
                     float* mono,
                     size_t samples_per_channel) {
  for (size_t i = 0; i < samples_per_channel; ++i)
    mono[i] = (stereo[i * 2] + stereo[i * 2 + 1]) / 2;
}

void MixStereoToMono(const int16_t* stereo,
                     int16_t* mono,
                     size_t samples_per_channel) {
  for (size_t i = 0; i < samples_per_channel; ++i)
    mono[i] = (stereo[i * 2] + stereo[i * 2 + 1]) >> 1;
}

void CopyLeftToRightChannel(int16_t* stereo, size_t samples_per_channel) {
  for (size_t i = 0; i < samples_per_channel; i++) {
    stereo[i * 2 + 1] = stereo[i * 2];
  }
}

void VerifyChannelsAreEqual(const int16_t* stereo, size_t samples_per_channel) {
  for (size_t i = 0; i < samples_per_channel; i++) {
    EXPECT_EQ(stereo[i * 2 + 1], stereo[i * 2]);
  }
}

void SetFrameTo(Int16FrameData* frame, int16_t value) {
  for (size_t i = 0; i < frame->samples_per_channel * frame->num_channels;
       ++i) {
    frame->data[i] = value;
  }
}

void SetFrameTo(Int16FrameData* frame, int16_t left, int16_t right) {
  ASSERT_EQ(2u, frame->num_channels);
  for (size_t i = 0; i < frame->samples_per_channel * 2; i += 2) {
    frame->data[i] = left;
    frame->data[i + 1] = right;
  }
}

void ScaleFrame(Int16FrameData* frame, float scale) {
  for (size_t i = 0; i < frame->samples_per_channel * frame->num_channels;
       ++i) {
    frame->data[i] = FloatS16ToS16(frame->data[i] * scale);
  }
}

bool FrameDataAreEqual(const Int16FrameData& frame1,
                       const Int16FrameData& frame2) {
  if (frame1.samples_per_channel != frame2.samples_per_channel) {
    return false;
  }
  if (frame1.num_channels != frame2.num_channels) {
    return false;
  }
  if (memcmp(
          frame1.data.data(), frame2.data.data(),
          frame1.samples_per_channel * frame1.num_channels * sizeof(int16_t))) {
    return false;
  }
  return true;
}

rtc::ArrayView<int16_t> GetMutableFrameData(Int16FrameData* frame) {
  int16_t* ptr = frame->data.data();
  const size_t len = frame->samples_per_channel * frame->num_channels;
  return rtc::ArrayView<int16_t>(ptr, len);
}

rtc::ArrayView<const int16_t> GetFrameData(const Int16FrameData& frame) {
  const int16_t* ptr = frame.data.data();
  const size_t len = frame.samples_per_channel * frame.num_channels;
  return rtc::ArrayView<const int16_t>(ptr, len);
}

void EnableAllAPComponents(AudioProcessing* ap) {
  AudioProcessing::Config apm_config = ap->GetConfig();
  apm_config.echo_canceller.enabled = true;
#if defined(WEBRTC_AUDIOPROC_FIXED_PROFILE)
  apm_config.echo_canceller.mobile_mode = true;

  apm_config.gain_controller1.enabled = true;
  apm_config.gain_controller1.mode =
      AudioProcessing::Config::GainController1::kAdaptiveDigital;
#elif defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE)
  apm_config.echo_canceller.mobile_mode = false;

  apm_config.gain_controller1.enabled = true;
  apm_config.gain_controller1.mode =
      AudioProcessing::Config::GainController1::kAdaptiveAnalog;
  apm_config.gain_controller1.analog_level_minimum = 0;
  apm_config.gain_controller1.analog_level_maximum = 255;
#endif

  apm_config.noise_suppression.enabled = true;

  apm_config.high_pass_filter.enabled = true;
  apm_config.level_estimation.enabled = true;
  apm_config.voice_detection.enabled = true;
  apm_config.pipeline.maximum_internal_processing_rate = 48000;
  ap->ApplyConfig(apm_config);
}

// These functions are only used by ApmTest.Process.
template <class T>
T AbsValue(T a) {
  return a > 0 ? a : -a;
}

int16_t MaxAudioFrame(const Int16FrameData& frame) {
  const size_t length = frame.samples_per_channel * frame.num_channels;
  int16_t max_data = AbsValue(frame.data[0]);
  for (size_t i = 1; i < length; i++) {
    max_data = std::max(max_data, AbsValue(frame.data[i]));
  }

  return max_data;
}

void OpenFileAndWriteMessage(const std::string& filename,
                             const MessageLite& msg) {
  FILE* file = fopen(filename.c_str(), "wb");
  ASSERT_TRUE(file != NULL);

  int32_t size = rtc::checked_cast<int32_t>(msg.ByteSizeLong());
  ASSERT_GT(size, 0);
  std::unique_ptr<uint8_t[]> array(new uint8_t[size]);
  ASSERT_TRUE(msg.SerializeToArray(array.get(), size));

  ASSERT_EQ(1u, fwrite(&size, sizeof(size), 1, file));
  ASSERT_EQ(static_cast<size_t>(size),
            fwrite(array.get(), sizeof(array[0]), size, file));
  fclose(file);
}

std::string ResourceFilePath(const std::string& name, int sample_rate_hz) {
  rtc::StringBuilder ss;
  // Resource files are all stereo.
  ss << name << sample_rate_hz / 1000 << "_stereo";
  return test::ResourcePath(ss.str(), "pcm");
}

// Temporary filenames unique to this process. Used to be able to run these
// tests in parallel as each process needs to be running in isolation they can't
// have competing filenames.
std::map<std::string, std::string> temp_filenames;

std::string OutputFilePath(const std::string& name,
                           int input_rate,
                           int output_rate,
                           int reverse_input_rate,
                           int reverse_output_rate,
                           size_t num_input_channels,
                           size_t num_output_channels,
                           size_t num_reverse_input_channels,
                           size_t num_reverse_output_channels,
                           StreamDirection file_direction) {
  rtc::StringBuilder ss;
  ss << name << "_i" << num_input_channels << "_" << input_rate / 1000 << "_ir"
     << num_reverse_input_channels << "_" << reverse_input_rate / 1000 << "_";
  if (num_output_channels == 1) {
    ss << "mono";
  } else if (num_output_channels == 2) {
    ss << "stereo";
  } else {
    RTC_NOTREACHED();
  }
  ss << output_rate / 1000;
  if (num_reverse_output_channels == 1) {
    ss << "_rmono";
  } else if (num_reverse_output_channels == 2) {
    ss << "_rstereo";
  } else {
    RTC_NOTREACHED();
  }
  ss << reverse_output_rate / 1000;
  ss << "_d" << file_direction << "_pcm";

  std::string filename = ss.str();
  if (temp_filenames[filename].empty())
    temp_filenames[filename] = test::TempFilename(test::OutputPath(), filename);
  return temp_filenames[filename];
}

void ClearTempFiles() {
  for (auto& kv : temp_filenames)
    remove(kv.second.c_str());
}

// Only remove "out" files. Keep "ref" files.
void ClearTempOutFiles() {
  for (auto it = temp_filenames.begin(); it != temp_filenames.end();) {
    const std::string& filename = it->first;
    if (filename.substr(0, 3).compare("out") == 0) {
      remove(it->second.c_str());
      temp_filenames.erase(it++);
    } else {
      it++;
    }
  }
}

void OpenFileAndReadMessage(const std::string& filename, MessageLite* msg) {
  FILE* file = fopen(filename.c_str(), "rb");
  ASSERT_TRUE(file != NULL);
  ReadMessageFromFile(file, msg);
  fclose(file);
}

// Reads a 10 ms chunk of int16 interleaved audio from the given (assumed
// stereo) file, converts to deinterleaved float (optionally downmixing) and
// returns the result in |cb|. Returns false if the file ended (or on error) and
// true otherwise.
//
// |int_data| and |float_data| are just temporary space that must be
// sufficiently large to hold the 10 ms chunk.
bool ReadChunk(FILE* file,
               int16_t* int_data,
               float* float_data,
               ChannelBuffer<float>* cb) {
  // The files always contain stereo audio.
  size_t frame_size = cb->num_frames() * 2;
  size_t read_count = fread(int_data, sizeof(int16_t), frame_size, file);
  if (read_count != frame_size) {
    // Check that the file really ended.
    RTC_DCHECK(feof(file));
    return false;  // This is expected.
  }

  S16ToFloat(int_data, frame_size, float_data);
  if (cb->num_channels() == 1) {
    MixStereoToMono(float_data, cb->channels()[0], cb->num_frames());
  } else {
    Deinterleave(float_data, cb->num_frames(), 2, cb->channels());
  }

  return true;
}

// Returns the reference file name that matches the current CPU
// architecture/optimizations.
std::string GetReferenceFilename() {
#if defined(WEBRTC_AUDIOPROC_FIXED_PROFILE)
  return test::ResourcePath("audio_processing/output_data_fixed", "pb");
#elif defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE)
  if (GetCPUInfo(kAVX2) != 0) {
    return test::ResourcePath("audio_processing/output_data_float_avx2", "pb");
  }
  return test::ResourcePath("audio_processing/output_data_float", "pb");
#endif
}

class ApmTest : public ::testing::Test {
 protected:
  ApmTest();
  virtual void SetUp();
  virtual void TearDown();

  static void SetUpTestSuite() {}

  static void TearDownTestSuite() { ClearTempFiles(); }

  // Used to select between int and float interface tests.
  enum Format { kIntFormat, kFloatFormat };

  void Init(int sample_rate_hz,
            int output_sample_rate_hz,
            int reverse_sample_rate_hz,
            size_t num_input_channels,
            size_t num_output_channels,
            size_t num_reverse_channels,
            bool open_output_file);
  void Init(AudioProcessing* ap);
  void EnableAllComponents();
  bool ReadFrame(FILE* file, Int16FrameData* frame);
  bool ReadFrame(FILE* file, Int16FrameData* frame, ChannelBuffer<float>* cb);
  void ReadFrameWithRewind(FILE* file, Int16FrameData* frame);
  void ReadFrameWithRewind(FILE* file,
                           Int16FrameData* frame,
                           ChannelBuffer<float>* cb);
  void ProcessDelayVerificationTest(int delay_ms,
                                    int system_delay_ms,
                                    int delay_min,
                                    int delay_max);
  void TestChangingChannelsInt16Interface(
      size_t num_channels,
      AudioProcessing::Error expected_return);
  void TestChangingForwardChannels(size_t num_in_channels,
                                   size_t num_out_channels,
                                   AudioProcessing::Error expected_return);
  void TestChangingReverseChannels(size_t num_rev_channels,
                                   AudioProcessing::Error expected_return);
  void RunQuantizedVolumeDoesNotGetStuckTest(int sample_rate);
  void RunManualVolumeChangeIsPossibleTest(int sample_rate);
  void StreamParametersTest(Format format);
  int ProcessStreamChooser(Format format);
  int AnalyzeReverseStreamChooser(Format format);
  void ProcessDebugDump(const std::string& in_filename,
                        const std::string& out_filename,
                        Format format,
                        int max_size_bytes);
  void VerifyDebugDumpTest(Format format);

  const std::string output_path_;
  const std::string ref_filename_;
  std::unique_ptr<AudioProcessing> apm_;
  Int16FrameData frame_;
  Int16FrameData revframe_;
  std::unique_ptr<ChannelBuffer<float> > float_cb_;
  std::unique_ptr<ChannelBuffer<float> > revfloat_cb_;
  int output_sample_rate_hz_;
  size_t num_output_channels_;
  FILE* far_file_;
  FILE* near_file_;
  FILE* out_file_;
};

ApmTest::ApmTest()
    : output_path_(test::OutputPath()),
      ref_filename_(GetReferenceFilename()),
      output_sample_rate_hz_(0),
      num_output_channels_(0),
      far_file_(NULL),
      near_file_(NULL),
      out_file_(NULL) {
  apm_.reset(AudioProcessingBuilderForTesting().Create());
  AudioProcessing::Config apm_config = apm_->GetConfig();
  apm_config.gain_controller1.analog_gain_controller.enabled = false;
  apm_config.pipeline.maximum_internal_processing_rate = 48000;
  apm_->ApplyConfig(apm_config);
}

void ApmTest::SetUp() {
  ASSERT_TRUE(apm_.get() != NULL);

  Init(32000, 32000, 32000, 2, 2, 2, false);
}

void ApmTest::TearDown() {
  if (far_file_) {
    ASSERT_EQ(0, fclose(far_file_));
  }
  far_file_ = NULL;

  if (near_file_) {
    ASSERT_EQ(0, fclose(near_file_));
  }
  near_file_ = NULL;

  if (out_file_) {
    ASSERT_EQ(0, fclose(out_file_));
  }
  out_file_ = NULL;
}

void ApmTest::Init(AudioProcessing* ap) {
  ASSERT_EQ(
      kNoErr,
      ap->Initialize({{{frame_.sample_rate_hz, frame_.num_channels},
                       {output_sample_rate_hz_, num_output_channels_},
                       {revframe_.sample_rate_hz, revframe_.num_channels},
                       {revframe_.sample_rate_hz, revframe_.num_channels}}}));
}

void ApmTest::Init(int sample_rate_hz,
                   int output_sample_rate_hz,
                   int reverse_sample_rate_hz,
                   size_t num_input_channels,
                   size_t num_output_channels,
                   size_t num_reverse_channels,
                   bool open_output_file) {
  SetContainerFormat(sample_rate_hz, num_input_channels, &frame_, &float_cb_);
  output_sample_rate_hz_ = output_sample_rate_hz;
  num_output_channels_ = num_output_channels;

  SetContainerFormat(reverse_sample_rate_hz, num_reverse_channels, &revframe_,
                     &revfloat_cb_);
  Init(apm_.get());

  if (far_file_) {
    ASSERT_EQ(0, fclose(far_file_));
  }
  std::string filename = ResourceFilePath("far", sample_rate_hz);
  far_file_ = fopen(filename.c_str(), "rb");
  ASSERT_TRUE(far_file_ != NULL) << "Could not open file " << filename << "\n";

  if (near_file_) {
    ASSERT_EQ(0, fclose(near_file_));
  }
  filename = ResourceFilePath("near", sample_rate_hz);
  near_file_ = fopen(filename.c_str(), "rb");
  ASSERT_TRUE(near_file_ != NULL) << "Could not open file " << filename << "\n";

  if (open_output_file) {
    if (out_file_) {
      ASSERT_EQ(0, fclose(out_file_));
    }
    filename = OutputFilePath(
        "out", sample_rate_hz, output_sample_rate_hz, reverse_sample_rate_hz,
        reverse_sample_rate_hz, num_input_channels, num_output_channels,
        num_reverse_channels, num_reverse_channels, kForward);
    out_file_ = fopen(filename.c_str(), "wb");
    ASSERT_TRUE(out_file_ != NULL)
        << "Could not open file " << filename << "\n";
  }
}

void ApmTest::EnableAllComponents() {
  EnableAllAPComponents(apm_.get());
}

bool ApmTest::ReadFrame(FILE* file,
                        Int16FrameData* frame,
                        ChannelBuffer<float>* cb) {
  // The files always contain stereo audio.
  size_t frame_size = frame->samples_per_channel * 2;
  size_t read_count =
      fread(frame->data.data(), sizeof(int16_t), frame_size, file);
  if (read_count != frame_size) {
    // Check that the file really ended.
    EXPECT_NE(0, feof(file));
    return false;  // This is expected.
  }

  if (frame->num_channels == 1) {
    MixStereoToMono(frame->data.data(), frame->data.data(),
                    frame->samples_per_channel);
  }

  if (cb) {
    ConvertToFloat(*frame, cb);
  }
  return true;
}

bool ApmTest::ReadFrame(FILE* file, Int16FrameData* frame) {
  return ReadFrame(file, frame, NULL);
}

// If the end of the file has been reached, rewind it and attempt to read the
// frame again.
void ApmTest::ReadFrameWithRewind(FILE* file,
                                  Int16FrameData* frame,
                                  ChannelBuffer<float>* cb) {
  if (!ReadFrame(near_file_, &frame_, cb)) {
    rewind(near_file_);
    ASSERT_TRUE(ReadFrame(near_file_, &frame_, cb));
  }
}

void ApmTest::ReadFrameWithRewind(FILE* file, Int16FrameData* frame) {
  ReadFrameWithRewind(file, frame, NULL);
}

int ApmTest::ProcessStreamChooser(Format format) {
  if (format == kIntFormat) {
    return apm_->ProcessStream(
        frame_.data.data(),
        StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
        StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
        frame_.data.data());
  }
  return apm_->ProcessStream(
      float_cb_->channels(),
      StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
      StreamConfig(output_sample_rate_hz_, num_output_channels_),
      float_cb_->channels());
}

int ApmTest::AnalyzeReverseStreamChooser(Format format) {
  if (format == kIntFormat) {
    return apm_->ProcessReverseStream(
        revframe_.data.data(),
        StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
        StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
        revframe_.data.data());
  }
  return apm_->AnalyzeReverseStream(
      revfloat_cb_->channels(),
      StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels));
}

void ApmTest::ProcessDelayVerificationTest(int delay_ms,
                                           int system_delay_ms,
                                           int delay_min,
                                           int delay_max) {
  // The |revframe_| and |frame_| should include the proper frame information,
  // hence can be used for extracting information.
  Int16FrameData tmp_frame;
  std::queue<Int16FrameData*> frame_queue;
  bool causal = true;

  tmp_frame.CopyFrom(revframe_);
  SetFrameTo(&tmp_frame, 0);

  EXPECT_EQ(apm_->kNoError, apm_->Initialize());
  // Initialize the |frame_queue| with empty frames.
  int frame_delay = delay_ms / 10;
  while (frame_delay < 0) {
    Int16FrameData* frame = new Int16FrameData();
    frame->CopyFrom(tmp_frame);
    frame_queue.push(frame);
    frame_delay++;
    causal = false;
  }
  while (frame_delay > 0) {
    Int16FrameData* frame = new Int16FrameData();
    frame->CopyFrom(tmp_frame);
    frame_queue.push(frame);
    frame_delay--;
  }
  // Run for 4.5 seconds, skipping statistics from the first 2.5 seconds.  We
  // need enough frames with audio to have reliable estimates, but as few as
  // possible to keep processing time down.  4.5 seconds seemed to be a good
  // compromise for this recording.
  for (int frame_count = 0; frame_count < 450; ++frame_count) {
    Int16FrameData* frame = new Int16FrameData();
    frame->CopyFrom(tmp_frame);
    // Use the near end recording, since that has more speech in it.
    ASSERT_TRUE(ReadFrame(near_file_, frame));
    frame_queue.push(frame);
    Int16FrameData* reverse_frame = frame;
    Int16FrameData* process_frame = frame_queue.front();
    if (!causal) {
      reverse_frame = frame_queue.front();
      // When we call ProcessStream() the frame is modified, so we can't use the
      // pointer directly when things are non-causal. Use an intermediate frame
      // and copy the data.
      process_frame = &tmp_frame;
      process_frame->CopyFrom(*frame);
    }
    EXPECT_EQ(apm_->kNoError, apm_->ProcessReverseStream(
                                  reverse_frame->data.data(),
                                  StreamConfig(reverse_frame->sample_rate_hz,
                                               reverse_frame->num_channels),
                                  StreamConfig(reverse_frame->sample_rate_hz,
                                               reverse_frame->num_channels),
                                  reverse_frame->data.data()));
    EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(system_delay_ms));
    EXPECT_EQ(apm_->kNoError,
              apm_->ProcessStream(process_frame->data.data(),
                                  StreamConfig(process_frame->sample_rate_hz,
                                               process_frame->num_channels),
                                  StreamConfig(process_frame->sample_rate_hz,
                                               process_frame->num_channels),
                                  process_frame->data.data()));
    frame = frame_queue.front();
    frame_queue.pop();
    delete frame;

    if (frame_count == 250) {
      // Discard the first delay metrics to avoid convergence effects.
      static_cast<void>(apm_->GetStatistics());
    }
  }

  rewind(near_file_);
  while (!frame_queue.empty()) {
    Int16FrameData* frame = frame_queue.front();
    frame_queue.pop();
    delete frame;
  }
  // Calculate expected delay estimate and acceptable regions. Further,
  // limit them w.r.t. AEC delay estimation support.
  const size_t samples_per_ms =
      rtc::SafeMin<size_t>(16u, frame_.samples_per_channel / 10);
  const int expected_median =
      rtc::SafeClamp<int>(delay_ms - system_delay_ms, delay_min, delay_max);
  const int expected_median_high = rtc::SafeClamp<int>(
      expected_median + rtc::dchecked_cast<int>(96 / samples_per_ms), delay_min,
      delay_max);
  const int expected_median_low = rtc::SafeClamp<int>(
      expected_median - rtc::dchecked_cast<int>(96 / samples_per_ms), delay_min,
      delay_max);
  // Verify delay metrics.
  AudioProcessingStats stats = apm_->GetStatistics();
  ASSERT_TRUE(stats.delay_median_ms.has_value());
  int32_t median = *stats.delay_median_ms;
  EXPECT_GE(expected_median_high, median);
  EXPECT_LE(expected_median_low, median);
}

void ApmTest::StreamParametersTest(Format format) {
  // No errors when the components are disabled.
  EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(format));

  // -- Missing AGC level --
  AudioProcessing::Config apm_config = apm_->GetConfig();
  apm_config.gain_controller1.enabled = true;
  apm_->ApplyConfig(apm_config);
  EXPECT_EQ(apm_->kStreamParameterNotSetError, ProcessStreamChooser(format));

  // Resets after successful ProcessStream().
  apm_->set_stream_analog_level(127);
  EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(format));
  EXPECT_EQ(apm_->kStreamParameterNotSetError, ProcessStreamChooser(format));

  // Other stream parameters set correctly.
  apm_config.echo_canceller.enabled = true;
  apm_config.echo_canceller.mobile_mode = false;
  apm_->ApplyConfig(apm_config);
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
  EXPECT_EQ(apm_->kStreamParameterNotSetError, ProcessStreamChooser(format));
  apm_config.gain_controller1.enabled = false;
  apm_->ApplyConfig(apm_config);

  // -- Missing delay --
  EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(format));
  EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(format));

  // Resets after successful ProcessStream().
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
  EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(format));
  EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(format));

  // Other stream parameters set correctly.
  apm_config.gain_controller1.enabled = true;
  apm_->ApplyConfig(apm_config);
  apm_->set_stream_analog_level(127);
  EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(format));
  apm_config.gain_controller1.enabled = false;
  apm_->ApplyConfig(apm_config);

  // -- No stream parameters --
  EXPECT_EQ(apm_->kNoError, AnalyzeReverseStreamChooser(format));
  EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(format));

  // -- All there --
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(100));
  apm_->set_stream_analog_level(127);
  EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(format));
}

TEST_F(ApmTest, StreamParametersInt) {
  StreamParametersTest(kIntFormat);
}

TEST_F(ApmTest, StreamParametersFloat) {
  StreamParametersTest(kFloatFormat);
}

void ApmTest::TestChangingChannelsInt16Interface(
    size_t num_channels,
    AudioProcessing::Error expected_return) {
  frame_.num_channels = num_channels;

  EXPECT_EQ(expected_return,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_EQ(expected_return,
            apm_->ProcessReverseStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
}

void ApmTest::TestChangingForwardChannels(
    size_t num_in_channels,
    size_t num_out_channels,
    AudioProcessing::Error expected_return) {
  const StreamConfig input_stream = {frame_.sample_rate_hz, num_in_channels};
  const StreamConfig output_stream = {output_sample_rate_hz_, num_out_channels};

  EXPECT_EQ(expected_return,
            apm_->ProcessStream(float_cb_->channels(), input_stream,
                                output_stream, float_cb_->channels()));
}

void ApmTest::TestChangingReverseChannels(
    size_t num_rev_channels,
    AudioProcessing::Error expected_return) {
  const ProcessingConfig processing_config = {
      {{frame_.sample_rate_hz, apm_->num_input_channels()},
       {output_sample_rate_hz_, apm_->num_output_channels()},
       {frame_.sample_rate_hz, num_rev_channels},
       {frame_.sample_rate_hz, num_rev_channels}}};

  EXPECT_EQ(
      expected_return,
      apm_->ProcessReverseStream(
          float_cb_->channels(), processing_config.reverse_input_stream(),
          processing_config.reverse_output_stream(), float_cb_->channels()));
}

TEST_F(ApmTest, ChannelsInt16Interface) {
  // Testing number of invalid and valid channels.
  Init(16000, 16000, 16000, 4, 4, 4, false);

  TestChangingChannelsInt16Interface(0, apm_->kBadNumberChannelsError);

  for (size_t i = 1; i < 4; i++) {
    TestChangingChannelsInt16Interface(i, kNoErr);
    EXPECT_EQ(i, apm_->num_input_channels());
  }
}

TEST_F(ApmTest, Channels) {
  // Testing number of invalid and valid channels.
  Init(16000, 16000, 16000, 4, 4, 4, false);

  TestChangingForwardChannels(0, 1, apm_->kBadNumberChannelsError);
  TestChangingReverseChannels(0, apm_->kBadNumberChannelsError);

  for (size_t i = 1; i < 4; ++i) {
    for (size_t j = 0; j < 1; ++j) {
      // Output channels much be one or match input channels.
      if (j == 1 || i == j) {
        TestChangingForwardChannels(i, j, kNoErr);
        TestChangingReverseChannels(i, kNoErr);

        EXPECT_EQ(i, apm_->num_input_channels());
        EXPECT_EQ(j, apm_->num_output_channels());
        // The number of reverse channels used for processing to is always 1.
        EXPECT_EQ(1u, apm_->num_reverse_channels());
      } else {
        TestChangingForwardChannels(i, j,
                                    AudioProcessing::kBadNumberChannelsError);
      }
    }
  }
}

TEST_F(ApmTest, SampleRatesInt) {
  // Testing some valid sample rates.
  for (int sample_rate : {8000, 12000, 16000, 32000, 44100, 48000, 96000}) {
    SetContainerFormat(sample_rate, 2, &frame_, &float_cb_);
    EXPECT_NOERR(ProcessStreamChooser(kIntFormat));
  }
}

// This test repeatedly reconfigures the pre-amplifier in APM, processes a
// number of frames, and checks that output signal has the right level.
TEST_F(ApmTest, PreAmplifier) {
  // Fill the audio frame with a sawtooth pattern.
  rtc::ArrayView<int16_t> frame_data = GetMutableFrameData(&frame_);
  const size_t samples_per_channel = frame_.samples_per_channel;
  for (size_t i = 0; i < samples_per_channel; i++) {
    for (size_t ch = 0; ch < frame_.num_channels; ++ch) {
      frame_data[i + ch * samples_per_channel] = 10000 * ((i % 3) - 1);
    }
  }
  // Cache the frame in tmp_frame.
  Int16FrameData tmp_frame;
  tmp_frame.CopyFrom(frame_);

  auto compute_power = [](const Int16FrameData& frame) {
    rtc::ArrayView<const int16_t> data = GetFrameData(frame);
    return std::accumulate(data.begin(), data.end(), 0.0f,
                           [](float a, float b) { return a + b * b; }) /
           data.size() / 32768 / 32768;
  };

  const float input_power = compute_power(tmp_frame);
  // Double-check that the input data is large compared to the error kEpsilon.
  constexpr float kEpsilon = 1e-4f;
  RTC_DCHECK_GE(input_power, 10 * kEpsilon);

  // 1. Enable pre-amp with 0 dB gain.
  AudioProcessing::Config config = apm_->GetConfig();
  config.pre_amplifier.enabled = true;
  config.pre_amplifier.fixed_gain_factor = 1.0f;
  apm_->ApplyConfig(config);

  for (int i = 0; i < 20; ++i) {
    frame_.CopyFrom(tmp_frame);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kIntFormat));
  }
  float output_power = compute_power(frame_);
  EXPECT_NEAR(output_power, input_power, kEpsilon);
  config = apm_->GetConfig();
  EXPECT_EQ(config.pre_amplifier.fixed_gain_factor, 1.0f);

  // 2. Change pre-amp gain via ApplyConfig.
  config.pre_amplifier.fixed_gain_factor = 2.0f;
  apm_->ApplyConfig(config);

  for (int i = 0; i < 20; ++i) {
    frame_.CopyFrom(tmp_frame);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kIntFormat));
  }
  output_power = compute_power(frame_);
  EXPECT_NEAR(output_power, 4 * input_power, kEpsilon);
  config = apm_->GetConfig();
  EXPECT_EQ(config.pre_amplifier.fixed_gain_factor, 2.0f);

  // 3. Change pre-amp gain via a RuntimeSetting.
  apm_->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCapturePreGain(1.5f));

  for (int i = 0; i < 20; ++i) {
    frame_.CopyFrom(tmp_frame);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kIntFormat));
  }
  output_power = compute_power(frame_);
  EXPECT_NEAR(output_power, 2.25 * input_power, kEpsilon);
  config = apm_->GetConfig();
  EXPECT_EQ(config.pre_amplifier.fixed_gain_factor, 1.5f);
}

// This test a simple test that ensures that the emulated analog mic gain
// functionality runs without crashing.
TEST_F(ApmTest, AnalogMicGainEmulation) {
  // Fill the audio frame with a sawtooth pattern.
  rtc::ArrayView<int16_t> frame_data = GetMutableFrameData(&frame_);
  const size_t samples_per_channel = frame_.samples_per_channel;
  for (size_t i = 0; i < samples_per_channel; i++) {
    for (size_t ch = 0; ch < frame_.num_channels; ++ch) {
      frame_data[i + ch * samples_per_channel] = 100 * ((i % 3) - 1);
    }
  }
  // Cache the frame in tmp_frame.
  Int16FrameData tmp_frame;
  tmp_frame.CopyFrom(frame_);

  // Enable the analog gain emulation.
  AudioProcessing::Config config = apm_->GetConfig();
  config.capture_level_adjustment.enabled = true;
  config.capture_level_adjustment.analog_mic_gain_emulation.enabled = true;
  config.capture_level_adjustment.analog_mic_gain_emulation.initial_level = 21;
  config.gain_controller1.enabled = true;
  config.gain_controller1.mode =
      AudioProcessing::Config::GainController1::Mode::kAdaptiveAnalog;
  config.gain_controller1.analog_gain_controller.enabled = true;
  apm_->ApplyConfig(config);

  // Process a number of frames to ensure that the code runs without crashes.
  for (int i = 0; i < 20; ++i) {
    frame_.CopyFrom(tmp_frame);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kIntFormat));
  }
}

// This test repeatedly reconfigures the capture level adjustment functionality
// in APM, processes a number of frames, and checks that output signal has the
// right level.
TEST_F(ApmTest, CaptureLevelAdjustment) {
  // Fill the audio frame with a sawtooth pattern.
  rtc::ArrayView<int16_t> frame_data = GetMutableFrameData(&frame_);
  const size_t samples_per_channel = frame_.samples_per_channel;
  for (size_t i = 0; i < samples_per_channel; i++) {
    for (size_t ch = 0; ch < frame_.num_channels; ++ch) {
      frame_data[i + ch * samples_per_channel] = 100 * ((i % 3) - 1);
    }
  }
  // Cache the frame in tmp_frame.
  Int16FrameData tmp_frame;
  tmp_frame.CopyFrom(frame_);

  auto compute_power = [](const Int16FrameData& frame) {
    rtc::ArrayView<const int16_t> data = GetFrameData(frame);
    return std::accumulate(data.begin(), data.end(), 0.0f,
                           [](float a, float b) { return a + b * b; }) /
           data.size() / 32768 / 32768;
  };

  const float input_power = compute_power(tmp_frame);
  // Double-check that the input data is large compared to the error kEpsilon.
  constexpr float kEpsilon = 1e-20f;
  RTC_DCHECK_GE(input_power, 10 * kEpsilon);

  // 1. Enable pre-amp with 0 dB gain.
  AudioProcessing::Config config = apm_->GetConfig();
  config.capture_level_adjustment.enabled = true;
  config.capture_level_adjustment.pre_gain_factor = 0.5f;
  config.capture_level_adjustment.post_gain_factor = 4.f;
  const float expected_output_power1 =
      config.capture_level_adjustment.pre_gain_factor *
      config.capture_level_adjustment.pre_gain_factor *
      config.capture_level_adjustment.post_gain_factor *
      config.capture_level_adjustment.post_gain_factor * input_power;
  apm_->ApplyConfig(config);

  for (int i = 0; i < 20; ++i) {
    frame_.CopyFrom(tmp_frame);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kIntFormat));
  }
  float output_power = compute_power(frame_);
  EXPECT_NEAR(output_power, expected_output_power1, kEpsilon);
  config = apm_->GetConfig();
  EXPECT_EQ(config.capture_level_adjustment.pre_gain_factor, 0.5f);
  EXPECT_EQ(config.capture_level_adjustment.post_gain_factor, 4.f);

  // 2. Change pre-amp gain via ApplyConfig.
  config.capture_level_adjustment.pre_gain_factor = 1.0f;
  config.capture_level_adjustment.post_gain_factor = 2.f;
  const float expected_output_power2 =
      config.capture_level_adjustment.pre_gain_factor *
      config.capture_level_adjustment.pre_gain_factor *
      config.capture_level_adjustment.post_gain_factor *
      config.capture_level_adjustment.post_gain_factor * input_power;
  apm_->ApplyConfig(config);

  for (int i = 0; i < 20; ++i) {
    frame_.CopyFrom(tmp_frame);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kIntFormat));
  }
  output_power = compute_power(frame_);
  EXPECT_NEAR(output_power, expected_output_power2, kEpsilon);
  config = apm_->GetConfig();
  EXPECT_EQ(config.capture_level_adjustment.pre_gain_factor, 1.0f);
  EXPECT_EQ(config.capture_level_adjustment.post_gain_factor, 2.f);

  // 3. Change pre-amp gain via a RuntimeSetting.
  constexpr float kPreGain3 = 0.5f;
  constexpr float kPostGain3 = 3.f;
  const float expected_output_power3 =
      kPreGain3 * kPreGain3 * kPostGain3 * kPostGain3 * input_power;

  apm_->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCapturePreGain(kPreGain3));
  apm_->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCapturePostGain(kPostGain3));

  for (int i = 0; i < 20; ++i) {
    frame_.CopyFrom(tmp_frame);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kIntFormat));
  }
  output_power = compute_power(frame_);
  EXPECT_NEAR(output_power, expected_output_power3, kEpsilon);
  config = apm_->GetConfig();
  EXPECT_EQ(config.capture_level_adjustment.pre_gain_factor, 0.5f);
  EXPECT_EQ(config.capture_level_adjustment.post_gain_factor, 3.f);
}

TEST_F(ApmTest, GainControl) {
  AudioProcessing::Config config = apm_->GetConfig();
  config.gain_controller1.enabled = false;
  apm_->ApplyConfig(config);
  config.gain_controller1.enabled = true;
  apm_->ApplyConfig(config);

  // Testing gain modes
  for (auto mode :
       {AudioProcessing::Config::GainController1::kAdaptiveDigital,
        AudioProcessing::Config::GainController1::kFixedDigital,
        AudioProcessing::Config::GainController1::kAdaptiveAnalog}) {
    config.gain_controller1.mode = mode;
    apm_->ApplyConfig(config);
    apm_->set_stream_analog_level(100);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kFloatFormat));
  }

  // Testing target levels
  for (int target_level_dbfs : {0, 15, 31}) {
    config.gain_controller1.target_level_dbfs = target_level_dbfs;
    apm_->ApplyConfig(config);
    apm_->set_stream_analog_level(100);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kFloatFormat));
  }

  // Testing compression gains
  for (int compression_gain_db : {0, 10, 90}) {
    config.gain_controller1.compression_gain_db = compression_gain_db;
    apm_->ApplyConfig(config);
    apm_->set_stream_analog_level(100);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kFloatFormat));
  }

  // Testing limiter off/on
  for (bool enable : {false, true}) {
    config.gain_controller1.enable_limiter = enable;
    apm_->ApplyConfig(config);
    apm_->set_stream_analog_level(100);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kFloatFormat));
  }

  // Testing level limits
  std::array<int, 4> kMinLevels = {0, 0, 255, 65000};
  std::array<int, 4> kMaxLevels = {255, 1024, 65535, 65535};
  for (size_t i = 0; i < kMinLevels.size(); ++i) {
    int min_level = kMinLevels[i];
    int max_level = kMaxLevels[i];
    config.gain_controller1.analog_level_minimum = min_level;
    config.gain_controller1.analog_level_maximum = max_level;
    apm_->ApplyConfig(config);
    apm_->set_stream_analog_level((min_level + max_level) / 2);
    EXPECT_EQ(apm_->kNoError, ProcessStreamChooser(kFloatFormat));
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
using ApmDeathTest = ApmTest;

TEST_F(ApmDeathTest, GainControlDiesOnTooLowTargetLevelDbfs) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.target_level_dbfs = -1;
  EXPECT_DEATH(apm_->ApplyConfig(config), "");
}

TEST_F(ApmDeathTest, GainControlDiesOnTooHighTargetLevelDbfs) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.target_level_dbfs = 32;
  EXPECT_DEATH(apm_->ApplyConfig(config), "");
}

TEST_F(ApmDeathTest, GainControlDiesOnTooLowCompressionGainDb) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.compression_gain_db = -1;
  EXPECT_DEATH(apm_->ApplyConfig(config), "");
}

TEST_F(ApmDeathTest, GainControlDiesOnTooHighCompressionGainDb) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.compression_gain_db = 91;
  EXPECT_DEATH(apm_->ApplyConfig(config), "");
}

TEST_F(ApmDeathTest, GainControlDiesOnTooLowAnalogLevelLowerLimit) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.analog_level_minimum = -1;
  EXPECT_DEATH(apm_->ApplyConfig(config), "");
}

TEST_F(ApmDeathTest, GainControlDiesOnTooHighAnalogLevelUpperLimit) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.analog_level_maximum = 65536;
  EXPECT_DEATH(apm_->ApplyConfig(config), "");
}

TEST_F(ApmDeathTest, GainControlDiesOnInvertedAnalogLevelLimits) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.analog_level_minimum = 512;
  config.gain_controller1.analog_level_maximum = 255;
  EXPECT_DEATH(apm_->ApplyConfig(config), "");
}

TEST_F(ApmDeathTest, ApmDiesOnTooLowAnalogLevel) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.analog_level_minimum = 255;
  config.gain_controller1.analog_level_maximum = 512;
  apm_->ApplyConfig(config);
  EXPECT_DEATH(apm_->set_stream_analog_level(254), "");
}

TEST_F(ApmDeathTest, ApmDiesOnTooHighAnalogLevel) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.analog_level_minimum = 255;
  config.gain_controller1.analog_level_maximum = 512;
  apm_->ApplyConfig(config);
  EXPECT_DEATH(apm_->set_stream_analog_level(513), "");
}
#endif

void ApmTest::RunQuantizedVolumeDoesNotGetStuckTest(int sample_rate) {
  Init(sample_rate, sample_rate, sample_rate, 2, 2, 2, false);
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.mode =
      AudioProcessing::Config::GainController1::kAdaptiveAnalog;
  apm_->ApplyConfig(config);

  int out_analog_level = 0;
  for (int i = 0; i < 2000; ++i) {
    ReadFrameWithRewind(near_file_, &frame_);
    // Ensure the audio is at a low level, so the AGC will try to increase it.
    ScaleFrame(&frame_, 0.25);

    // Always pass in the same volume.
    apm_->set_stream_analog_level(100);
    EXPECT_EQ(apm_->kNoError,
              apm_->ProcessStream(
                  frame_.data.data(),
                  StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                  StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                  frame_.data.data()));
    out_analog_level = apm_->recommended_stream_analog_level();
  }

  // Ensure the AGC is still able to reach the maximum.
  EXPECT_EQ(255, out_analog_level);
}

// Verifies that despite volume slider quantization, the AGC can continue to
// increase its volume.
TEST_F(ApmTest, QuantizedVolumeDoesNotGetStuck) {
  for (size_t i = 0; i < arraysize(kSampleRates); ++i) {
    RunQuantizedVolumeDoesNotGetStuckTest(kSampleRates[i]);
  }
}

void ApmTest::RunManualVolumeChangeIsPossibleTest(int sample_rate) {
  Init(sample_rate, sample_rate, sample_rate, 2, 2, 2, false);
  auto config = apm_->GetConfig();
  config.gain_controller1.enabled = true;
  config.gain_controller1.mode =
      AudioProcessing::Config::GainController1::kAdaptiveAnalog;
  apm_->ApplyConfig(config);

  int out_analog_level = 100;
  for (int i = 0; i < 1000; ++i) {
    ReadFrameWithRewind(near_file_, &frame_);
    // Ensure the audio is at a low level, so the AGC will try to increase it.
    ScaleFrame(&frame_, 0.25);

    apm_->set_stream_analog_level(out_analog_level);
    EXPECT_EQ(apm_->kNoError,
              apm_->ProcessStream(
                  frame_.data.data(),
                  StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                  StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                  frame_.data.data()));
    out_analog_level = apm_->recommended_stream_analog_level();
  }

  // Ensure the volume was raised.
  EXPECT_GT(out_analog_level, 100);
  int highest_level_reached = out_analog_level;
  // Simulate a user manual volume change.
  out_analog_level = 100;

  for (int i = 0; i < 300; ++i) {
    ReadFrameWithRewind(near_file_, &frame_);
    ScaleFrame(&frame_, 0.25);

    apm_->set_stream_analog_level(out_analog_level);
    EXPECT_EQ(apm_->kNoError,
              apm_->ProcessStream(
                  frame_.data.data(),
                  StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                  StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                  frame_.data.data()));
    out_analog_level = apm_->recommended_stream_analog_level();
    // Check that AGC respected the manually adjusted volume.
    EXPECT_LT(out_analog_level, highest_level_reached);
  }
  // Check that the volume was still raised.
  EXPECT_GT(out_analog_level, 100);
}

TEST_F(ApmTest, ManualVolumeChangeIsPossible) {
  for (size_t i = 0; i < arraysize(kSampleRates); ++i) {
    RunManualVolumeChangeIsPossibleTest(kSampleRates[i]);
  }
}

TEST_F(ApmTest, HighPassFilter) {
  // Turn HP filter on/off
  AudioProcessing::Config apm_config;
  apm_config.high_pass_filter.enabled = true;
  apm_->ApplyConfig(apm_config);
  apm_config.high_pass_filter.enabled = false;
  apm_->ApplyConfig(apm_config);
}

TEST_F(ApmTest, AllProcessingDisabledByDefault) {
  AudioProcessing::Config config = apm_->GetConfig();
  EXPECT_FALSE(config.echo_canceller.enabled);
  EXPECT_FALSE(config.high_pass_filter.enabled);
  EXPECT_FALSE(config.gain_controller1.enabled);
  EXPECT_FALSE(config.level_estimation.enabled);
  EXPECT_FALSE(config.noise_suppression.enabled);
  EXPECT_FALSE(config.voice_detection.enabled);
}

TEST_F(ApmTest, NoProcessingWhenAllComponentsDisabled) {
  for (size_t i = 0; i < arraysize(kSampleRates); i++) {
    Init(kSampleRates[i], kSampleRates[i], kSampleRates[i], 2, 2, 2, false);
    SetFrameTo(&frame_, 1000, 2000);
    Int16FrameData frame_copy;
    frame_copy.CopyFrom(frame_);
    for (int j = 0; j < 1000; j++) {
      EXPECT_EQ(apm_->kNoError,
                apm_->ProcessStream(
                    frame_.data.data(),
                    StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                    StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                    frame_.data.data()));
      EXPECT_TRUE(FrameDataAreEqual(frame_, frame_copy));
      EXPECT_EQ(apm_->kNoError,
                apm_->ProcessReverseStream(
                    frame_.data.data(),
                    StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                    StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                    frame_.data.data()));
      EXPECT_TRUE(FrameDataAreEqual(frame_, frame_copy));
    }
  }
}

TEST_F(ApmTest, NoProcessingWhenAllComponentsDisabledFloat) {
  // Test that ProcessStream copies input to output even with no processing.
  const size_t kSamples = 160;
  const int sample_rate = 16000;
  const float src[kSamples] = {-1.0f, 0.0f, 1.0f};
  float dest[kSamples] = {};

  auto src_channels = &src[0];
  auto dest_channels = &dest[0];

  apm_.reset(AudioProcessingBuilderForTesting().Create());
  EXPECT_NOERR(apm_->ProcessStream(&src_channels, StreamConfig(sample_rate, 1),
                                   StreamConfig(sample_rate, 1),
                                   &dest_channels));

  for (size_t i = 0; i < kSamples; ++i) {
    EXPECT_EQ(src[i], dest[i]);
  }

  // Same for ProcessReverseStream.
  float rev_dest[kSamples] = {};
  auto rev_dest_channels = &rev_dest[0];

  StreamConfig input_stream = {sample_rate, 1};
  StreamConfig output_stream = {sample_rate, 1};
  EXPECT_NOERR(apm_->ProcessReverseStream(&src_channels, input_stream,
                                          output_stream, &rev_dest_channels));

  for (size_t i = 0; i < kSamples; ++i) {
    EXPECT_EQ(src[i], rev_dest[i]);
  }
}

TEST_F(ApmTest, IdenticalInputChannelsResultInIdenticalOutputChannels) {
  EnableAllComponents();

  for (size_t i = 0; i < arraysize(kProcessSampleRates); i++) {
    Init(kProcessSampleRates[i], kProcessSampleRates[i], kProcessSampleRates[i],
         2, 2, 2, false);
    int analog_level = 127;
    ASSERT_EQ(0, feof(far_file_));
    ASSERT_EQ(0, feof(near_file_));
    while (ReadFrame(far_file_, &revframe_) && ReadFrame(near_file_, &frame_)) {
      CopyLeftToRightChannel(revframe_.data.data(),
                             revframe_.samples_per_channel);

      ASSERT_EQ(
          kNoErr,
          apm_->ProcessReverseStream(
              revframe_.data.data(),
              StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
              StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
              revframe_.data.data()));

      CopyLeftToRightChannel(frame_.data.data(), frame_.samples_per_channel);

      ASSERT_EQ(kNoErr, apm_->set_stream_delay_ms(0));
      apm_->set_stream_analog_level(analog_level);
      ASSERT_EQ(kNoErr,
                apm_->ProcessStream(
                    frame_.data.data(),
                    StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                    StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                    frame_.data.data()));
      analog_level = apm_->recommended_stream_analog_level();

      VerifyChannelsAreEqual(frame_.data.data(), frame_.samples_per_channel);
    }
    rewind(far_file_);
    rewind(near_file_);
  }
}

TEST_F(ApmTest, SplittingFilter) {
  // Verify the filter is not active through undistorted audio when:
  // 1. No components are enabled...
  SetFrameTo(&frame_, 1000);
  Int16FrameData frame_copy;
  frame_copy.CopyFrom(frame_);
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_TRUE(FrameDataAreEqual(frame_, frame_copy));

  // 2. Only the level estimator is enabled...
  auto apm_config = apm_->GetConfig();
  SetFrameTo(&frame_, 1000);
  frame_copy.CopyFrom(frame_);
  apm_config.level_estimation.enabled = true;
  apm_->ApplyConfig(apm_config);
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_TRUE(FrameDataAreEqual(frame_, frame_copy));
  apm_config.level_estimation.enabled = false;
  apm_->ApplyConfig(apm_config);

  // 3. Only GetStatistics-reporting VAD is enabled...
  SetFrameTo(&frame_, 1000);
  frame_copy.CopyFrom(frame_);
  apm_config.voice_detection.enabled = true;
  apm_->ApplyConfig(apm_config);
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_TRUE(FrameDataAreEqual(frame_, frame_copy));
  apm_config.voice_detection.enabled = false;
  apm_->ApplyConfig(apm_config);

  // 4. Both the VAD and the level estimator are enabled...
  SetFrameTo(&frame_, 1000);
  frame_copy.CopyFrom(frame_);
  apm_config.voice_detection.enabled = true;
  apm_config.level_estimation.enabled = true;
  apm_->ApplyConfig(apm_config);
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_TRUE(FrameDataAreEqual(frame_, frame_copy));
  apm_config.voice_detection.enabled = false;
  apm_config.level_estimation.enabled = false;
  apm_->ApplyConfig(apm_config);

  // Check the test is valid. We should have distortion from the filter
  // when AEC is enabled (which won't affect the audio).
  apm_config.echo_canceller.enabled = true;
  apm_config.echo_canceller.mobile_mode = false;
  apm_->ApplyConfig(apm_config);
  frame_.samples_per_channel = 320;
  frame_.num_channels = 2;
  frame_.sample_rate_hz = 32000;
  SetFrameTo(&frame_, 1000);
  frame_copy.CopyFrom(frame_);
  EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(0));
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_FALSE(FrameDataAreEqual(frame_, frame_copy));
}

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
void ApmTest::ProcessDebugDump(const std::string& in_filename,
                               const std::string& out_filename,
                               Format format,
                               int max_size_bytes) {
  TaskQueueForTest worker_queue("ApmTest_worker_queue");
  FILE* in_file = fopen(in_filename.c_str(), "rb");
  ASSERT_TRUE(in_file != NULL);
  audioproc::Event event_msg;
  bool first_init = true;

  while (ReadMessageFromFile(in_file, &event_msg)) {
    if (event_msg.type() == audioproc::Event::INIT) {
      const audioproc::Init msg = event_msg.init();
      int reverse_sample_rate = msg.sample_rate();
      if (msg.has_reverse_sample_rate()) {
        reverse_sample_rate = msg.reverse_sample_rate();
      }
      int output_sample_rate = msg.sample_rate();
      if (msg.has_output_sample_rate()) {
        output_sample_rate = msg.output_sample_rate();
      }

      Init(msg.sample_rate(), output_sample_rate, reverse_sample_rate,
           msg.num_input_channels(), msg.num_output_channels(),
           msg.num_reverse_channels(), false);
      if (first_init) {
        // AttachAecDump() writes an additional init message. Don't start
        // recording until after the first init to avoid the extra message.
        auto aec_dump =
            AecDumpFactory::Create(out_filename, max_size_bytes, &worker_queue);
        EXPECT_TRUE(aec_dump);
        apm_->AttachAecDump(std::move(aec_dump));
        first_init = false;
      }

    } else if (event_msg.type() == audioproc::Event::REVERSE_STREAM) {
      const audioproc::ReverseStream msg = event_msg.reverse_stream();

      if (msg.channel_size() > 0) {
        ASSERT_EQ(revframe_.num_channels,
                  static_cast<size_t>(msg.channel_size()));
        for (int i = 0; i < msg.channel_size(); ++i) {
          memcpy(revfloat_cb_->channels()[i], msg.channel(i).data(),
                 msg.channel(i).size());
        }
      } else {
        memcpy(revframe_.data.data(), msg.data().data(), msg.data().size());
        if (format == kFloatFormat) {
          // We're using an int16 input file; convert to float.
          ConvertToFloat(revframe_, revfloat_cb_.get());
        }
      }
      AnalyzeReverseStreamChooser(format);

    } else if (event_msg.type() == audioproc::Event::STREAM) {
      const audioproc::Stream msg = event_msg.stream();
      // ProcessStream could have changed this for the output frame.
      frame_.num_channels = apm_->num_input_channels();

      apm_->set_stream_analog_level(msg.level());
      EXPECT_NOERR(apm_->set_stream_delay_ms(msg.delay()));
      if (msg.has_keypress()) {
        apm_->set_stream_key_pressed(msg.keypress());
      } else {
        apm_->set_stream_key_pressed(true);
      }

      if (msg.input_channel_size() > 0) {
        ASSERT_EQ(frame_.num_channels,
                  static_cast<size_t>(msg.input_channel_size()));
        for (int i = 0; i < msg.input_channel_size(); ++i) {
          memcpy(float_cb_->channels()[i], msg.input_channel(i).data(),
                 msg.input_channel(i).size());
        }
      } else {
        memcpy(frame_.data.data(), msg.input_data().data(),
               msg.input_data().size());
        if (format == kFloatFormat) {
          // We're using an int16 input file; convert to float.
          ConvertToFloat(frame_, float_cb_.get());
        }
      }
      ProcessStreamChooser(format);
    }
  }
  apm_->DetachAecDump();
  fclose(in_file);
}

void ApmTest::VerifyDebugDumpTest(Format format) {
  rtc::ScopedFakeClock fake_clock;
  const std::string in_filename = test::ResourcePath("ref03", "aecdump");
  std::string format_string;
  switch (format) {
    case kIntFormat:
      format_string = "_int";
      break;
    case kFloatFormat:
      format_string = "_float";
      break;
  }
  const std::string ref_filename = test::TempFilename(
      test::OutputPath(), std::string("ref") + format_string + "_aecdump");
  const std::string out_filename = test::TempFilename(
      test::OutputPath(), std::string("out") + format_string + "_aecdump");
  const std::string limited_filename = test::TempFilename(
      test::OutputPath(), std::string("limited") + format_string + "_aecdump");
  const size_t logging_limit_bytes = 100000;
  // We expect at least this many bytes in the created logfile.
  const size_t logging_expected_bytes = 95000;
  EnableAllComponents();
  ProcessDebugDump(in_filename, ref_filename, format, -1);
  ProcessDebugDump(ref_filename, out_filename, format, -1);
  ProcessDebugDump(ref_filename, limited_filename, format, logging_limit_bytes);

  FILE* ref_file = fopen(ref_filename.c_str(), "rb");
  FILE* out_file = fopen(out_filename.c_str(), "rb");
  FILE* limited_file = fopen(limited_filename.c_str(), "rb");
  ASSERT_TRUE(ref_file != NULL);
  ASSERT_TRUE(out_file != NULL);
  ASSERT_TRUE(limited_file != NULL);
  std::unique_ptr<uint8_t[]> ref_bytes;
  std::unique_ptr<uint8_t[]> out_bytes;
  std::unique_ptr<uint8_t[]> limited_bytes;

  size_t ref_size = ReadMessageBytesFromFile(ref_file, &ref_bytes);
  size_t out_size = ReadMessageBytesFromFile(out_file, &out_bytes);
  size_t limited_size = ReadMessageBytesFromFile(limited_file, &limited_bytes);
  size_t bytes_read = 0;
  size_t bytes_read_limited = 0;
  while (ref_size > 0 && out_size > 0) {
    bytes_read += ref_size;
    bytes_read_limited += limited_size;
    EXPECT_EQ(ref_size, out_size);
    EXPECT_GE(ref_size, limited_size);
    EXPECT_EQ(0, memcmp(ref_bytes.get(), out_bytes.get(), ref_size));
    EXPECT_EQ(0, memcmp(ref_bytes.get(), limited_bytes.get(), limited_size));
    ref_size = ReadMessageBytesFromFile(ref_file, &ref_bytes);
    out_size = ReadMessageBytesFromFile(out_file, &out_bytes);
    limited_size = ReadMessageBytesFromFile(limited_file, &limited_bytes);
  }
  EXPECT_GT(bytes_read, 0u);
  EXPECT_GT(bytes_read_limited, logging_expected_bytes);
  EXPECT_LE(bytes_read_limited, logging_limit_bytes);
  EXPECT_NE(0, feof(ref_file));
  EXPECT_NE(0, feof(out_file));
  EXPECT_NE(0, feof(limited_file));
  ASSERT_EQ(0, fclose(ref_file));
  ASSERT_EQ(0, fclose(out_file));
  ASSERT_EQ(0, fclose(limited_file));
  remove(ref_filename.c_str());
  remove(out_filename.c_str());
  remove(limited_filename.c_str());
}

TEST_F(ApmTest, VerifyDebugDumpInt) {
  VerifyDebugDumpTest(kIntFormat);
}

TEST_F(ApmTest, VerifyDebugDumpFloat) {
  VerifyDebugDumpTest(kFloatFormat);
}
#endif

// TODO(andrew): expand test to verify output.
TEST_F(ApmTest, DebugDump) {
  TaskQueueForTest worker_queue("ApmTest_worker_queue");
  const std::string filename =
      test::TempFilename(test::OutputPath(), "debug_aec");
  {
    auto aec_dump = AecDumpFactory::Create("", -1, &worker_queue);
    EXPECT_FALSE(aec_dump);
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // Stopping without having started should be OK.
  apm_->DetachAecDump();

  auto aec_dump = AecDumpFactory::Create(filename, -1, &worker_queue);
  EXPECT_TRUE(aec_dump);
  apm_->AttachAecDump(std::move(aec_dump));
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessReverseStream(
                revframe_.data.data(),
                StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
                StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
                revframe_.data.data()));
  apm_->DetachAecDump();

  // Verify the file has been written.
  FILE* fid = fopen(filename.c_str(), "r");
  ASSERT_TRUE(fid != NULL);

  // Clean it up.
  ASSERT_EQ(0, fclose(fid));
  ASSERT_EQ(0, remove(filename.c_str()));
#else
  // Verify the file has NOT been written.
  ASSERT_TRUE(fopen(filename.c_str(), "r") == NULL);
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

// TODO(andrew): expand test to verify output.
TEST_F(ApmTest, DebugDumpFromFileHandle) {
  TaskQueueForTest worker_queue("ApmTest_worker_queue");

  const std::string filename =
      test::TempFilename(test::OutputPath(), "debug_aec");
  FileWrapper f = FileWrapper::OpenWriteOnly(filename.c_str());
  ASSERT_TRUE(f.is_open());

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // Stopping without having started should be OK.
  apm_->DetachAecDump();

  auto aec_dump = AecDumpFactory::Create(std::move(f), -1, &worker_queue);
  EXPECT_TRUE(aec_dump);
  apm_->AttachAecDump(std::move(aec_dump));
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessReverseStream(
                revframe_.data.data(),
                StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
                StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
                revframe_.data.data()));
  EXPECT_EQ(apm_->kNoError,
            apm_->ProcessStream(
                frame_.data.data(),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                frame_.data.data()));
  apm_->DetachAecDump();

  // Verify the file has been written.
  FILE* fid = fopen(filename.c_str(), "r");
  ASSERT_TRUE(fid != NULL);

  // Clean it up.
  ASSERT_EQ(0, fclose(fid));
  ASSERT_EQ(0, remove(filename.c_str()));
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

// TODO(andrew): Add a test to process a few frames with different combinations
// of enabled components.

TEST_F(ApmTest, Process) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  audioproc::OutputData ref_data;

  if (!absl::GetFlag(FLAGS_write_apm_ref_data)) {
    OpenFileAndReadMessage(ref_filename_, &ref_data);
  } else {
    // Write the desired tests to the protobuf reference file.
    for (size_t i = 0; i < arraysize(kChannels); i++) {
      for (size_t j = 0; j < arraysize(kChannels); j++) {
        for (size_t l = 0; l < arraysize(kProcessSampleRates); l++) {
          audioproc::Test* test = ref_data.add_test();
          test->set_num_reverse_channels(kChannels[i]);
          test->set_num_input_channels(kChannels[j]);
          test->set_num_output_channels(kChannels[j]);
          test->set_sample_rate(kProcessSampleRates[l]);
          test->set_use_aec_extended_filter(false);
        }
      }
    }
#if defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE)
    // To test the extended filter mode.
    audioproc::Test* test = ref_data.add_test();
    test->set_num_reverse_channels(2);
    test->set_num_input_channels(2);
    test->set_num_output_channels(2);
    test->set_sample_rate(AudioProcessing::kSampleRate32kHz);
    test->set_use_aec_extended_filter(true);
#endif
  }

  for (int i = 0; i < ref_data.test_size(); i++) {
    printf("Running test %d of %d...\n", i + 1, ref_data.test_size());

    audioproc::Test* test = ref_data.mutable_test(i);
    // TODO(ajm): We no longer allow different input and output channels. Skip
    // these tests for now, but they should be removed from the set.
    if (test->num_input_channels() != test->num_output_channels())
      continue;

    apm_.reset(AudioProcessingBuilderForTesting().Create());
    AudioProcessing::Config apm_config = apm_->GetConfig();
    apm_config.gain_controller1.analog_gain_controller.enabled = false;
    apm_->ApplyConfig(apm_config);

    EnableAllComponents();

    Init(test->sample_rate(), test->sample_rate(), test->sample_rate(),
         static_cast<size_t>(test->num_input_channels()),
         static_cast<size_t>(test->num_output_channels()),
         static_cast<size_t>(test->num_reverse_channels()), true);

    int frame_count = 0;
    int has_voice_count = 0;
    int analog_level = 127;
    int analog_level_average = 0;
    int max_output_average = 0;
    float rms_dbfs_average = 0.0f;
#if defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE)
    int stats_index = 0;
#endif

    while (ReadFrame(far_file_, &revframe_) && ReadFrame(near_file_, &frame_)) {
      EXPECT_EQ(
          apm_->kNoError,
          apm_->ProcessReverseStream(
              revframe_.data.data(),
              StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
              StreamConfig(revframe_.sample_rate_hz, revframe_.num_channels),
              revframe_.data.data()));

      EXPECT_EQ(apm_->kNoError, apm_->set_stream_delay_ms(0));
      apm_->set_stream_analog_level(analog_level);

      EXPECT_EQ(apm_->kNoError,
                apm_->ProcessStream(
                    frame_.data.data(),
                    StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                    StreamConfig(frame_.sample_rate_hz, frame_.num_channels),
                    frame_.data.data()));

      // Ensure the frame was downmixed properly.
      EXPECT_EQ(static_cast<size_t>(test->num_output_channels()),
                frame_.num_channels);

      max_output_average += MaxAudioFrame(frame_);

      analog_level = apm_->recommended_stream_analog_level();
      analog_level_average += analog_level;
      AudioProcessingStats stats = apm_->GetStatistics();
      EXPECT_TRUE(stats.voice_detected);
      EXPECT_TRUE(stats.output_rms_dbfs);
      has_voice_count += *stats.voice_detected ? 1 : 0;
      rms_dbfs_average += *stats.output_rms_dbfs;

      size_t frame_size = frame_.samples_per_channel * frame_.num_channels;
      size_t write_count =
          fwrite(frame_.data.data(), sizeof(int16_t), frame_size, out_file_);
      ASSERT_EQ(frame_size, write_count);

      // Reset in case of downmixing.
      frame_.num_channels = static_cast<size_t>(test->num_input_channels());
      frame_count++;

#if defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE)
      const int kStatsAggregationFrameNum = 100;  // 1 second.
      if (frame_count % kStatsAggregationFrameNum == 0) {
        // Get echo and delay metrics.
        AudioProcessingStats stats = apm_->GetStatistics();

        // Echo metrics.
        const float echo_return_loss = stats.echo_return_loss.value_or(-1.0f);
        const float echo_return_loss_enhancement =
            stats.echo_return_loss_enhancement.value_or(-1.0f);
        const float residual_echo_likelihood =
            stats.residual_echo_likelihood.value_or(-1.0f);
        const float residual_echo_likelihood_recent_max =
            stats.residual_echo_likelihood_recent_max.value_or(-1.0f);

        if (!absl::GetFlag(FLAGS_write_apm_ref_data)) {
          const audioproc::Test::EchoMetrics& reference =
              test->echo_metrics(stats_index);
          constexpr float kEpsilon = 0.01;
          EXPECT_NEAR(echo_return_loss, reference.echo_return_loss(), kEpsilon);
          EXPECT_NEAR(echo_return_loss_enhancement,
                      reference.echo_return_loss_enhancement(), kEpsilon);
          EXPECT_NEAR(residual_echo_likelihood,
                      reference.residual_echo_likelihood(), kEpsilon);
          EXPECT_NEAR(residual_echo_likelihood_recent_max,
                      reference.residual_echo_likelihood_recent_max(),
                      kEpsilon);
          ++stats_index;
        } else {
          audioproc::Test::EchoMetrics* message_echo = test->add_echo_metrics();
          message_echo->set_echo_return_loss(echo_return_loss);
          message_echo->set_echo_return_loss_enhancement(
              echo_return_loss_enhancement);
          message_echo->set_residual_echo_likelihood(residual_echo_likelihood);
          message_echo->set_residual_echo_likelihood_recent_max(
              residual_echo_likelihood_recent_max);
        }
      }
#endif  // defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE).
    }
    max_output_average /= frame_count;
    analog_level_average /= frame_count;
    rms_dbfs_average /= frame_count;

    if (!absl::GetFlag(FLAGS_write_apm_ref_data)) {
      const int kIntNear = 1;
      // When running the test on a N7 we get a {2, 6} difference of
      // |has_voice_count| and |max_output_average| is up to 18 higher.
      // All numbers being consistently higher on N7 compare to ref_data.
      // TODO(bjornv): If we start getting more of these offsets on Android we
      // should consider a different approach. Either using one slack for all,
      // or generate a separate android reference.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
      const int kHasVoiceCountOffset = 3;
      const int kHasVoiceCountNear = 8;
      const int kMaxOutputAverageOffset = 9;
      const int kMaxOutputAverageNear = 26;
#else
      const int kHasVoiceCountOffset = 0;
      const int kHasVoiceCountNear = kIntNear;
      const int kMaxOutputAverageOffset = 0;
      const int kMaxOutputAverageNear = kIntNear;
#endif
      EXPECT_NEAR(test->has_voice_count(),
                  has_voice_count - kHasVoiceCountOffset, kHasVoiceCountNear);

      EXPECT_NEAR(test->analog_level_average(), analog_level_average, kIntNear);
      EXPECT_NEAR(test->max_output_average(),
                  max_output_average - kMaxOutputAverageOffset,
                  kMaxOutputAverageNear);
#if defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE)
      const double kFloatNear = 0.002;
      EXPECT_NEAR(test->rms_dbfs_average(), rms_dbfs_average, kFloatNear);
#endif
    } else {
      test->set_has_voice_count(has_voice_count);

      test->set_analog_level_average(analog_level_average);
      test->set_max_output_average(max_output_average);

#if defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE)
      test->set_rms_dbfs_average(rms_dbfs_average);
#endif
    }

    rewind(far_file_);
    rewind(near_file_);
  }

  if (absl::GetFlag(FLAGS_write_apm_ref_data)) {
    OpenFileAndWriteMessage(ref_filename_, ref_data);
  }
}

TEST_F(ApmTest, NoErrorsWithKeyboardChannel) {
  struct ChannelFormat {
    AudioProcessing::ChannelLayout in_layout;
    AudioProcessing::ChannelLayout out_layout;
  };
  ChannelFormat cf[] = {
      {AudioProcessing::kMonoAndKeyboard, AudioProcessing::kMono},
      {AudioProcessing::kStereoAndKeyboard, AudioProcessing::kMono},
      {AudioProcessing::kStereoAndKeyboard, AudioProcessing::kStereo},
  };

  std::unique_ptr<AudioProcessing> ap(
      AudioProcessingBuilderForTesting().Create());
  // Enable one component just to ensure some processing takes place.
  AudioProcessing::Config config;
  config.noise_suppression.enabled = true;
  ap->ApplyConfig(config);
  for (size_t i = 0; i < arraysize(cf); ++i) {
    const int in_rate = 44100;
    const int out_rate = 48000;
    ChannelBuffer<float> in_cb(SamplesFromRate(in_rate),
                               TotalChannelsFromLayout(cf[i].in_layout));
    ChannelBuffer<float> out_cb(SamplesFromRate(out_rate),
                                ChannelsFromLayout(cf[i].out_layout));
    bool has_keyboard = cf[i].in_layout == AudioProcessing::kMonoAndKeyboard ||
                        cf[i].in_layout == AudioProcessing::kStereoAndKeyboard;
    StreamConfig in_sc(in_rate, ChannelsFromLayout(cf[i].in_layout),
                       has_keyboard);
    StreamConfig out_sc(out_rate, ChannelsFromLayout(cf[i].out_layout));

    // Run over a few chunks.
    for (int j = 0; j < 10; ++j) {
      EXPECT_NOERR(ap->ProcessStream(in_cb.channels(), in_sc, out_sc,
                                     out_cb.channels()));
    }
  }
}

// Compares the reference and test arrays over a region around the expected
// delay. Finds the highest SNR in that region and adds the variance and squared
// error results to the supplied accumulators.
void UpdateBestSNR(const float* ref,
                   const float* test,
                   size_t length,
                   int expected_delay,
                   double* variance_acc,
                   double* sq_error_acc) {
  double best_snr = std::numeric_limits<double>::min();
  double best_variance = 0;
  double best_sq_error = 0;
  // Search over a region of eight samples around the expected delay.
  for (int delay = std::max(expected_delay - 4, 0); delay <= expected_delay + 4;
       ++delay) {
    double sq_error = 0;
    double variance = 0;
    for (size_t i = 0; i < length - delay; ++i) {
      double error = test[i + delay] - ref[i];
      sq_error += error * error;
      variance += ref[i] * ref[i];
    }

    if (sq_error == 0) {
      *variance_acc += variance;
      return;
    }
    double snr = variance / sq_error;
    if (snr > best_snr) {
      best_snr = snr;
      best_variance = variance;
      best_sq_error = sq_error;
    }
  }

  *variance_acc += best_variance;
  *sq_error_acc += best_sq_error;
}

// Used to test a multitude of sample rate and channel combinations. It works
// by first producing a set of reference files (in SetUpTestCase) that are
// assumed to be correct, as the used parameters are verified by other tests
// in this collection. Primarily the reference files are all produced at
// "native" rates which do not involve any resampling.

// Each test pass produces an output file with a particular format. The output
// is matched against the reference file closest to its internal processing
// format. If necessary the output is resampled back to its process format.
// Due to the resampling distortion, we don't expect identical results, but
// enforce SNR thresholds which vary depending on the format. 0 is a special
// case SNR which corresponds to inf, or zero error.
typedef std::tuple<int, int, int, int, double, double> AudioProcessingTestData;
class AudioProcessingTest
    : public ::testing::TestWithParam<AudioProcessingTestData> {
 public:
  AudioProcessingTest()
      : input_rate_(std::get<0>(GetParam())),
        output_rate_(std::get<1>(GetParam())),
        reverse_input_rate_(std::get<2>(GetParam())),
        reverse_output_rate_(std::get<3>(GetParam())),
        expected_snr_(std::get<4>(GetParam())),
        expected_reverse_snr_(std::get<5>(GetParam())) {}

  virtual ~AudioProcessingTest() {}

  static void SetUpTestSuite() {
    // Create all needed output reference files.
    const int kNativeRates[] = {8000, 16000, 32000, 48000};
    const size_t kNumChannels[] = {1, 2};
    for (size_t i = 0; i < arraysize(kNativeRates); ++i) {
      for (size_t j = 0; j < arraysize(kNumChannels); ++j) {
        for (size_t k = 0; k < arraysize(kNumChannels); ++k) {
          // The reference files always have matching input and output channels.
          ProcessFormat(kNativeRates[i], kNativeRates[i], kNativeRates[i],
                        kNativeRates[i], kNumChannels[j], kNumChannels[j],
                        kNumChannels[k], kNumChannels[k], "ref");
        }
      }
    }
  }

  void TearDown() {
    // Remove "out" files after each test.
    ClearTempOutFiles();
  }

  static void TearDownTestSuite() { ClearTempFiles(); }

  // Runs a process pass on files with the given parameters and dumps the output
  // to a file specified with |output_file_prefix|. Both forward and reverse
  // output streams are dumped.
  static void ProcessFormat(int input_rate,
                            int output_rate,
                            int reverse_input_rate,
                            int reverse_output_rate,
                            size_t num_input_channels,
                            size_t num_output_channels,
                            size_t num_reverse_input_channels,
                            size_t num_reverse_output_channels,
                            const std::string& output_file_prefix) {
    std::unique_ptr<AudioProcessing> ap(
        AudioProcessingBuilderForTesting().Create());
    AudioProcessing::Config apm_config = ap->GetConfig();
    apm_config.gain_controller1.analog_gain_controller.enabled = false;
    ap->ApplyConfig(apm_config);

    EnableAllAPComponents(ap.get());

    ProcessingConfig processing_config = {
        {{input_rate, num_input_channels},
         {output_rate, num_output_channels},
         {reverse_input_rate, num_reverse_input_channels},
         {reverse_output_rate, num_reverse_output_channels}}};
    ap->Initialize(processing_config);

    FILE* far_file =
        fopen(ResourceFilePath("far", reverse_input_rate).c_str(), "rb");
    FILE* near_file = fopen(ResourceFilePath("near", input_rate).c_str(), "rb");
    FILE* out_file = fopen(
        OutputFilePath(
            output_file_prefix, input_rate, output_rate, reverse_input_rate,
            reverse_output_rate, num_input_channels, num_output_channels,
            num_reverse_input_channels, num_reverse_output_channels, kForward)
            .c_str(),
        "wb");
    FILE* rev_out_file = fopen(
        OutputFilePath(
            output_file_prefix, input_rate, output_rate, reverse_input_rate,
            reverse_output_rate, num_input_channels, num_output_channels,
            num_reverse_input_channels, num_reverse_output_channels, kReverse)
            .c_str(),
        "wb");
    ASSERT_TRUE(far_file != NULL);
    ASSERT_TRUE(near_file != NULL);
    ASSERT_TRUE(out_file != NULL);
    ASSERT_TRUE(rev_out_file != NULL);

    ChannelBuffer<float> fwd_cb(SamplesFromRate(input_rate),
                                num_input_channels);
    ChannelBuffer<float> rev_cb(SamplesFromRate(reverse_input_rate),
                                num_reverse_input_channels);
    ChannelBuffer<float> out_cb(SamplesFromRate(output_rate),
                                num_output_channels);
    ChannelBuffer<float> rev_out_cb(SamplesFromRate(reverse_output_rate),
                                    num_reverse_output_channels);

    // Temporary buffers.
    const int max_length =
        2 * std::max(std::max(out_cb.num_frames(), rev_out_cb.num_frames()),
                     std::max(fwd_cb.num_frames(), rev_cb.num_frames()));
    std::unique_ptr<float[]> float_data(new float[max_length]);
    std::unique_ptr<int16_t[]> int_data(new int16_t[max_length]);

    int analog_level = 127;
    while (ReadChunk(far_file, int_data.get(), float_data.get(), &rev_cb) &&
           ReadChunk(near_file, int_data.get(), float_data.get(), &fwd_cb)) {
      EXPECT_NOERR(ap->ProcessReverseStream(
          rev_cb.channels(), processing_config.reverse_input_stream(),
          processing_config.reverse_output_stream(), rev_out_cb.channels()));

      EXPECT_NOERR(ap->set_stream_delay_ms(0));
      ap->set_stream_analog_level(analog_level);

      EXPECT_NOERR(ap->ProcessStream(
          fwd_cb.channels(), StreamConfig(input_rate, num_input_channels),
          StreamConfig(output_rate, num_output_channels), out_cb.channels()));

      // Dump forward output to file.
      Interleave(out_cb.channels(), out_cb.num_frames(), out_cb.num_channels(),
                 float_data.get());
      size_t out_length = out_cb.num_channels() * out_cb.num_frames();

      ASSERT_EQ(out_length, fwrite(float_data.get(), sizeof(float_data[0]),
                                   out_length, out_file));

      // Dump reverse output to file.
      Interleave(rev_out_cb.channels(), rev_out_cb.num_frames(),
                 rev_out_cb.num_channels(), float_data.get());
      size_t rev_out_length =
          rev_out_cb.num_channels() * rev_out_cb.num_frames();

      ASSERT_EQ(rev_out_length, fwrite(float_data.get(), sizeof(float_data[0]),
                                       rev_out_length, rev_out_file));

      analog_level = ap->recommended_stream_analog_level();
    }
    fclose(far_file);
    fclose(near_file);
    fclose(out_file);
    fclose(rev_out_file);
  }

 protected:
  int input_rate_;
  int output_rate_;
  int reverse_input_rate_;
  int reverse_output_rate_;
  double expected_snr_;
  double expected_reverse_snr_;
};

TEST_P(AudioProcessingTest, Formats) {
  struct ChannelFormat {
    int num_input;
    int num_output;
    int num_reverse_input;
    int num_reverse_output;
  };
  ChannelFormat cf[] = {
      {1, 1, 1, 1}, {1, 1, 2, 1}, {2, 1, 1, 1},
      {2, 1, 2, 1}, {2, 2, 1, 1}, {2, 2, 2, 2},
  };

  for (size_t i = 0; i < arraysize(cf); ++i) {
    ProcessFormat(input_rate_, output_rate_, reverse_input_rate_,
                  reverse_output_rate_, cf[i].num_input, cf[i].num_output,
                  cf[i].num_reverse_input, cf[i].num_reverse_output, "out");

    // Verify output for both directions.
    std::vector<StreamDirection> stream_directions;
    stream_directions.push_back(kForward);
    stream_directions.push_back(kReverse);
    for (StreamDirection file_direction : stream_directions) {
      const int in_rate = file_direction ? reverse_input_rate_ : input_rate_;
      const int out_rate = file_direction ? reverse_output_rate_ : output_rate_;
      const int out_num =
          file_direction ? cf[i].num_reverse_output : cf[i].num_output;
      const double expected_snr =
          file_direction ? expected_reverse_snr_ : expected_snr_;

      const int min_ref_rate = std::min(in_rate, out_rate);
      int ref_rate;

      if (min_ref_rate > 32000) {
        ref_rate = 48000;
      } else if (min_ref_rate > 16000) {
        ref_rate = 32000;
      } else if (min_ref_rate > 8000) {
        ref_rate = 16000;
      } else {
        ref_rate = 8000;
      }

      FILE* out_file = fopen(
          OutputFilePath("out", input_rate_, output_rate_, reverse_input_rate_,
                         reverse_output_rate_, cf[i].num_input,
                         cf[i].num_output, cf[i].num_reverse_input,
                         cf[i].num_reverse_output, file_direction)
              .c_str(),
          "rb");
      // The reference files always have matching input and output channels.
      FILE* ref_file =
          fopen(OutputFilePath("ref", ref_rate, ref_rate, ref_rate, ref_rate,
                               cf[i].num_output, cf[i].num_output,
                               cf[i].num_reverse_output,
                               cf[i].num_reverse_output, file_direction)
                    .c_str(),
                "rb");
      ASSERT_TRUE(out_file != NULL);
      ASSERT_TRUE(ref_file != NULL);

      const size_t ref_length = SamplesFromRate(ref_rate) * out_num;
      const size_t out_length = SamplesFromRate(out_rate) * out_num;
      // Data from the reference file.
      std::unique_ptr<float[]> ref_data(new float[ref_length]);
      // Data from the output file.
      std::unique_ptr<float[]> out_data(new float[out_length]);
      // Data from the resampled output, in case the reference and output rates
      // don't match.
      std::unique_ptr<float[]> cmp_data(new float[ref_length]);

      PushResampler<float> resampler;
      resampler.InitializeIfNeeded(out_rate, ref_rate, out_num);

      // Compute the resampling delay of the output relative to the reference,
      // to find the region over which we should search for the best SNR.
      float expected_delay_sec = 0;
      if (in_rate != ref_rate) {
        // Input resampling delay.
        expected_delay_sec +=
            PushSincResampler::AlgorithmicDelaySeconds(in_rate);
      }
      if (out_rate != ref_rate) {
        // Output resampling delay.
        expected_delay_sec +=
            PushSincResampler::AlgorithmicDelaySeconds(ref_rate);
        // Delay of converting the output back to its processing rate for
        // testing.
        expected_delay_sec +=
            PushSincResampler::AlgorithmicDelaySeconds(out_rate);
      }
      int expected_delay =
          std::floor(expected_delay_sec * ref_rate + 0.5f) * out_num;

      double variance = 0;
      double sq_error = 0;
      while (fread(out_data.get(), sizeof(out_data[0]), out_length, out_file) &&
             fread(ref_data.get(), sizeof(ref_data[0]), ref_length, ref_file)) {
        float* out_ptr = out_data.get();
        if (out_rate != ref_rate) {
          // Resample the output back to its internal processing rate if
          // necssary.
          ASSERT_EQ(ref_length,
                    static_cast<size_t>(resampler.Resample(
                        out_ptr, out_length, cmp_data.get(), ref_length)));
          out_ptr = cmp_data.get();
        }

        // Update the |sq_error| and |variance| accumulators with the highest
        // SNR of reference vs output.
        UpdateBestSNR(ref_data.get(), out_ptr, ref_length, expected_delay,
                      &variance, &sq_error);
      }

      std::cout << "(" << input_rate_ << ", " << output_rate_ << ", "
                << reverse_input_rate_ << ", " << reverse_output_rate_ << ", "
                << cf[i].num_input << ", " << cf[i].num_output << ", "
                << cf[i].num_reverse_input << ", " << cf[i].num_reverse_output
                << ", " << file_direction << "): ";
      if (sq_error > 0) {
        double snr = 10 * log10(variance / sq_error);
        EXPECT_GE(snr, expected_snr);
        EXPECT_NE(0, expected_snr);
        std::cout << "SNR=" << snr << " dB" << std::endl;
      } else {
        std::cout << "SNR=inf dB" << std::endl;
      }

      fclose(out_file);
      fclose(ref_file);
    }
  }
}

#if defined(WEBRTC_AUDIOPROC_FLOAT_PROFILE)
INSTANTIATE_TEST_SUITE_P(
    CommonFormats,
    AudioProcessingTest,
    ::testing::Values(std::make_tuple(48000, 48000, 48000, 48000, 0, 0),
                      std::make_tuple(48000, 48000, 32000, 48000, 40, 30),
                      std::make_tuple(48000, 48000, 16000, 48000, 40, 20),
                      std::make_tuple(48000, 44100, 48000, 44100, 20, 20),
                      std::make_tuple(48000, 44100, 32000, 44100, 20, 15),
                      std::make_tuple(48000, 44100, 16000, 44100, 20, 15),
                      std::make_tuple(48000, 32000, 48000, 32000, 30, 35),
                      std::make_tuple(48000, 32000, 32000, 32000, 30, 0),
                      std::make_tuple(48000, 32000, 16000, 32000, 30, 20),
                      std::make_tuple(48000, 16000, 48000, 16000, 25, 20),
                      std::make_tuple(48000, 16000, 32000, 16000, 25, 20),
                      std::make_tuple(48000, 16000, 16000, 16000, 25, 0),

                      std::make_tuple(44100, 48000, 48000, 48000, 30, 0),
                      std::make_tuple(44100, 48000, 32000, 48000, 30, 30),
                      std::make_tuple(44100, 48000, 16000, 48000, 30, 20),
                      std::make_tuple(44100, 44100, 48000, 44100, 20, 20),
                      std::make_tuple(44100, 44100, 32000, 44100, 20, 15),
                      std::make_tuple(44100, 44100, 16000, 44100, 20, 15),
                      std::make_tuple(44100, 32000, 48000, 32000, 30, 35),
                      std::make_tuple(44100, 32000, 32000, 32000, 30, 0),
                      std::make_tuple(44100, 32000, 16000, 32000, 30, 20),
                      std::make_tuple(44100, 16000, 48000, 16000, 25, 20),
                      std::make_tuple(44100, 16000, 32000, 16000, 25, 20),
                      std::make_tuple(44100, 16000, 16000, 16000, 25, 0),

                      std::make_tuple(32000, 48000, 48000, 48000, 15, 0),
                      std::make_tuple(32000, 48000, 32000, 48000, 15, 30),
                      std::make_tuple(32000, 48000, 16000, 48000, 15, 20),
                      std::make_tuple(32000, 44100, 48000, 44100, 19, 20),
                      std::make_tuple(32000, 44100, 32000, 44100, 19, 15),
                      std::make_tuple(32000, 44100, 16000, 44100, 19, 15),
                      std::make_tuple(32000, 32000, 48000, 32000, 40, 35),
                      std::make_tuple(32000, 32000, 32000, 32000, 0, 0),
                      std::make_tuple(32000, 32000, 16000, 32000, 39, 20),
                      std::make_tuple(32000, 16000, 48000, 16000, 25, 20),
                      std::make_tuple(32000, 16000, 32000, 16000, 25, 20),
                      std::make_tuple(32000, 16000, 16000, 16000, 25, 0),

                      std::make_tuple(16000, 48000, 48000, 48000, 9, 0),
                      std::make_tuple(16000, 48000, 32000, 48000, 9, 30),
                      std::make_tuple(16000, 48000, 16000, 48000, 9, 20),
                      std::make_tuple(16000, 44100, 48000, 44100, 15, 20),
                      std::make_tuple(16000, 44100, 32000, 44100, 15, 15),
                      std::make_tuple(16000, 44100, 16000, 44100, 15, 15),
                      std::make_tuple(16000, 32000, 48000, 32000, 25, 35),
                      std::make_tuple(16000, 32000, 32000, 32000, 25, 0),
                      std::make_tuple(16000, 32000, 16000, 32000, 25, 20),
                      std::make_tuple(16000, 16000, 48000, 16000, 39, 20),
                      std::make_tuple(16000, 16000, 32000, 16000, 39, 20),
                      std::make_tuple(16000, 16000, 16000, 16000, 0, 0)));

#elif defined(WEBRTC_AUDIOPROC_FIXED_PROFILE)
INSTANTIATE_TEST_SUITE_P(
    CommonFormats,
    AudioProcessingTest,
    ::testing::Values(std::make_tuple(48000, 48000, 48000, 48000, 19, 0),
                      std::make_tuple(48000, 48000, 32000, 48000, 19, 30),
                      std::make_tuple(48000, 48000, 16000, 48000, 19, 20),
                      std::make_tuple(48000, 44100, 48000, 44100, 15, 20),
                      std::make_tuple(48000, 44100, 32000, 44100, 15, 15),
                      std::make_tuple(48000, 44100, 16000, 44100, 15, 15),
                      std::make_tuple(48000, 32000, 48000, 32000, 19, 35),
                      std::make_tuple(48000, 32000, 32000, 32000, 19, 0),
                      std::make_tuple(48000, 32000, 16000, 32000, 19, 20),
                      std::make_tuple(48000, 16000, 48000, 16000, 20, 20),
                      std::make_tuple(48000, 16000, 32000, 16000, 20, 20),
                      std::make_tuple(48000, 16000, 16000, 16000, 20, 0),

                      std::make_tuple(44100, 48000, 48000, 48000, 15, 0),
                      std::make_tuple(44100, 48000, 32000, 48000, 15, 30),
                      std::make_tuple(44100, 48000, 16000, 48000, 15, 20),
                      std::make_tuple(44100, 44100, 48000, 44100, 15, 20),
                      std::make_tuple(44100, 44100, 32000, 44100, 15, 15),
                      std::make_tuple(44100, 44100, 16000, 44100, 15, 15),
                      std::make_tuple(44100, 32000, 48000, 32000, 18, 35),
                      std::make_tuple(44100, 32000, 32000, 32000, 18, 0),
                      std::make_tuple(44100, 32000, 16000, 32000, 18, 20),
                      std::make_tuple(44100, 16000, 48000, 16000, 19, 20),
                      std::make_tuple(44100, 16000, 32000, 16000, 19, 20),
                      std::make_tuple(44100, 16000, 16000, 16000, 19, 0),

                      std::make_tuple(32000, 48000, 48000, 48000, 17, 0),
                      std::make_tuple(32000, 48000, 32000, 48000, 17, 30),
                      std::make_tuple(32000, 48000, 16000, 48000, 17, 20),
                      std::make_tuple(32000, 44100, 48000, 44100, 20, 20),
                      std::make_tuple(32000, 44100, 32000, 44100, 20, 15),
                      std::make_tuple(32000, 44100, 16000, 44100, 20, 15),
                      std::make_tuple(32000, 32000, 48000, 32000, 27, 35),
                      std::make_tuple(32000, 32000, 32000, 32000, 0, 0),
                      std::make_tuple(32000, 32000, 16000, 32000, 30, 20),
                      std::make_tuple(32000, 16000, 48000, 16000, 20, 20),
                      std::make_tuple(32000, 16000, 32000, 16000, 20, 20),
                      std::make_tuple(32000, 16000, 16000, 16000, 20, 0),

                      std::make_tuple(16000, 48000, 48000, 48000, 11, 0),
                      std::make_tuple(16000, 48000, 32000, 48000, 11, 30),
                      std::make_tuple(16000, 48000, 16000, 48000, 11, 20),
                      std::make_tuple(16000, 44100, 48000, 44100, 15, 20),
                      std::make_tuple(16000, 44100, 32000, 44100, 15, 15),
                      std::make_tuple(16000, 44100, 16000, 44100, 15, 15),
                      std::make_tuple(16000, 32000, 48000, 32000, 24, 35),
                      std::make_tuple(16000, 32000, 32000, 32000, 24, 0),
                      std::make_tuple(16000, 32000, 16000, 32000, 25, 20),
                      std::make_tuple(16000, 16000, 48000, 16000, 28, 20),
                      std::make_tuple(16000, 16000, 32000, 16000, 28, 20),
                      std::make_tuple(16000, 16000, 16000, 16000, 0, 0)));
#endif

// Produces a scoped trace debug output.
std::string ProduceDebugText(int render_input_sample_rate_hz,
                             int render_output_sample_rate_hz,
                             int capture_input_sample_rate_hz,
                             int capture_output_sample_rate_hz,
                             size_t render_input_num_channels,
                             size_t render_output_num_channels,
                             size_t capture_input_num_channels,
                             size_t capture_output_num_channels) {
  rtc::StringBuilder ss;
  ss << "Sample rates:"
        "\n Render input: "
     << render_input_sample_rate_hz
     << " Hz"
        "\n Render output: "
     << render_output_sample_rate_hz
     << " Hz"
        "\n Capture input: "
     << capture_input_sample_rate_hz
     << " Hz"
        "\n Capture output: "
     << capture_output_sample_rate_hz
     << " Hz"
        "\nNumber of channels:"
        "\n Render input: "
     << render_input_num_channels
     << "\n Render output: " << render_output_num_channels
     << "\n Capture input: " << capture_input_num_channels
     << "\n Capture output: " << capture_output_num_channels;
  return ss.Release();
}

// Validates that running the audio processing module using various combinations
// of sample rates and number of channels works as intended.
void RunApmRateAndChannelTest(
    rtc::ArrayView<const int> sample_rates_hz,
    rtc::ArrayView<const int> render_channel_counts,
    rtc::ArrayView<const int> capture_channel_counts) {
  std::unique_ptr<AudioProcessing> apm(
      AudioProcessingBuilderForTesting().Create());
  webrtc::AudioProcessing::Config apm_config;
  apm_config.echo_canceller.enabled = true;
  apm->ApplyConfig(apm_config);

  StreamConfig render_input_stream_config;
  StreamConfig render_output_stream_config;
  StreamConfig capture_input_stream_config;
  StreamConfig capture_output_stream_config;

  std::vector<float> render_input_frame_channels;
  std::vector<float*> render_input_frame;
  std::vector<float> render_output_frame_channels;
  std::vector<float*> render_output_frame;
  std::vector<float> capture_input_frame_channels;
  std::vector<float*> capture_input_frame;
  std::vector<float> capture_output_frame_channels;
  std::vector<float*> capture_output_frame;

  for (auto render_input_sample_rate_hz : sample_rates_hz) {
    for (auto render_output_sample_rate_hz : sample_rates_hz) {
      for (auto capture_input_sample_rate_hz : sample_rates_hz) {
        for (auto capture_output_sample_rate_hz : sample_rates_hz) {
          for (size_t render_input_num_channels : render_channel_counts) {
            for (size_t capture_input_num_channels : capture_channel_counts) {
              size_t render_output_num_channels = render_input_num_channels;
              size_t capture_output_num_channels = capture_input_num_channels;
              auto populate_audio_frame = [](int sample_rate_hz,
                                             size_t num_channels,
                                             StreamConfig* cfg,
                                             std::vector<float>* channels_data,
                                             std::vector<float*>* frame_data) {
                cfg->set_sample_rate_hz(sample_rate_hz);
                cfg->set_num_channels(num_channels);
                cfg->set_has_keyboard(false);

                size_t max_frame_size = ceil(sample_rate_hz / 100.f);
                channels_data->resize(num_channels * max_frame_size);
                std::fill(channels_data->begin(), channels_data->end(), 0.5f);
                frame_data->resize(num_channels);
                for (size_t channel = 0; channel < num_channels; ++channel) {
                  (*frame_data)[channel] =
                      &(*channels_data)[channel * max_frame_size];
                }
              };

              populate_audio_frame(
                  render_input_sample_rate_hz, render_input_num_channels,
                  &render_input_stream_config, &render_input_frame_channels,
                  &render_input_frame);
              populate_audio_frame(
                  render_output_sample_rate_hz, render_output_num_channels,
                  &render_output_stream_config, &render_output_frame_channels,
                  &render_output_frame);
              populate_audio_frame(
                  capture_input_sample_rate_hz, capture_input_num_channels,
                  &capture_input_stream_config, &capture_input_frame_channels,
                  &capture_input_frame);
              populate_audio_frame(
                  capture_output_sample_rate_hz, capture_output_num_channels,
                  &capture_output_stream_config, &capture_output_frame_channels,
                  &capture_output_frame);

              for (size_t frame = 0; frame < 2; ++frame) {
                SCOPED_TRACE(ProduceDebugText(
                    render_input_sample_rate_hz, render_output_sample_rate_hz,
                    capture_input_sample_rate_hz, capture_output_sample_rate_hz,
                    render_input_num_channels, render_output_num_channels,
                    render_input_num_channels, capture_output_num_channels));

                int result = apm->ProcessReverseStream(
                    &render_input_frame[0], render_input_stream_config,
                    render_output_stream_config, &render_output_frame[0]);
                EXPECT_EQ(result, AudioProcessing::kNoError);
                result = apm->ProcessStream(
                    &capture_input_frame[0], capture_input_stream_config,
                    capture_output_stream_config, &capture_output_frame[0]);
                EXPECT_EQ(result, AudioProcessing::kNoError);
              }
            }
          }
        }
      }
    }
  }
}

constexpr void Toggle(bool& b) {
  b ^= true;
}

}  // namespace

TEST(RuntimeSettingTest, TestDefaultCtor) {
  auto s = AudioProcessing::RuntimeSetting();
  EXPECT_EQ(AudioProcessing::RuntimeSetting::Type::kNotSpecified, s.type());
}

TEST(RuntimeSettingTest, TestUsageWithSwapQueue) {
  SwapQueue<AudioProcessing::RuntimeSetting> q(1);
  auto s = AudioProcessing::RuntimeSetting();
  ASSERT_TRUE(q.Insert(&s));
  ASSERT_TRUE(q.Remove(&s));
  EXPECT_EQ(AudioProcessing::RuntimeSetting::Type::kNotSpecified, s.type());
}

TEST(ApmConfiguration, EnablePostProcessing) {
  // Verify that apm uses a capture post processing module if one is provided.
  auto mock_post_processor_ptr =
      new ::testing::NiceMock<test::MockCustomProcessing>();
  auto mock_post_processor =
      std::unique_ptr<CustomProcessing>(mock_post_processor_ptr);
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetCapturePostProcessing(std::move(mock_post_processor))
          .Create();

  Int16FrameData audio;
  audio.num_channels = 1;
  SetFrameSampleRate(&audio, AudioProcessing::NativeRate::kSampleRate16kHz);

  EXPECT_CALL(*mock_post_processor_ptr, Process(::testing::_)).Times(1);
  apm->ProcessStream(audio.data.data(),
                     StreamConfig(audio.sample_rate_hz, audio.num_channels),
                     StreamConfig(audio.sample_rate_hz, audio.num_channels),
                     audio.data.data());
}

TEST(ApmConfiguration, EnablePreProcessing) {
  // Verify that apm uses a capture post processing module if one is provided.
  auto mock_pre_processor_ptr =
      new ::testing::NiceMock<test::MockCustomProcessing>();
  auto mock_pre_processor =
      std::unique_ptr<CustomProcessing>(mock_pre_processor_ptr);
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetRenderPreProcessing(std::move(mock_pre_processor))
          .Create();

  Int16FrameData audio;
  audio.num_channels = 1;
  SetFrameSampleRate(&audio, AudioProcessing::NativeRate::kSampleRate16kHz);

  EXPECT_CALL(*mock_pre_processor_ptr, Process(::testing::_)).Times(1);
  apm->ProcessReverseStream(
      audio.data.data(), StreamConfig(audio.sample_rate_hz, audio.num_channels),
      StreamConfig(audio.sample_rate_hz, audio.num_channels),
      audio.data.data());
}

TEST(ApmConfiguration, EnableCaptureAnalyzer) {
  // Verify that apm uses a capture analyzer if one is provided.
  auto mock_capture_analyzer_ptr =
      new ::testing::NiceMock<test::MockCustomAudioAnalyzer>();
  auto mock_capture_analyzer =
      std::unique_ptr<CustomAudioAnalyzer>(mock_capture_analyzer_ptr);
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetCaptureAnalyzer(std::move(mock_capture_analyzer))
          .Create();

  Int16FrameData audio;
  audio.num_channels = 1;
  SetFrameSampleRate(&audio, AudioProcessing::NativeRate::kSampleRate16kHz);

  EXPECT_CALL(*mock_capture_analyzer_ptr, Analyze(::testing::_)).Times(1);
  apm->ProcessStream(audio.data.data(),
                     StreamConfig(audio.sample_rate_hz, audio.num_channels),
                     StreamConfig(audio.sample_rate_hz, audio.num_channels),
                     audio.data.data());
}

TEST(ApmConfiguration, PreProcessingReceivesRuntimeSettings) {
  auto mock_pre_processor_ptr =
      new ::testing::NiceMock<test::MockCustomProcessing>();
  auto mock_pre_processor =
      std::unique_ptr<CustomProcessing>(mock_pre_processor_ptr);
  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetRenderPreProcessing(std::move(mock_pre_processor))
          .Create();
  apm->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCustomRenderSetting(0));

  // RuntimeSettings forwarded during 'Process*Stream' calls.
  // Therefore we have to make one such call.
  Int16FrameData audio;
  audio.num_channels = 1;
  SetFrameSampleRate(&audio, AudioProcessing::NativeRate::kSampleRate16kHz);

  EXPECT_CALL(*mock_pre_processor_ptr, SetRuntimeSetting(::testing::_))
      .Times(1);
  apm->ProcessReverseStream(
      audio.data.data(), StreamConfig(audio.sample_rate_hz, audio.num_channels),
      StreamConfig(audio.sample_rate_hz, audio.num_channels),
      audio.data.data());
}

class MyEchoControlFactory : public EchoControlFactory {
 public:
  std::unique_ptr<EchoControl> Create(int sample_rate_hz) {
    auto ec = new test::MockEchoControl();
    EXPECT_CALL(*ec, AnalyzeRender(::testing::_)).Times(1);
    EXPECT_CALL(*ec, AnalyzeCapture(::testing::_)).Times(2);
    EXPECT_CALL(*ec, ProcessCapture(::testing::_, ::testing::_, ::testing::_))
        .Times(2);
    return std::unique_ptr<EchoControl>(ec);
  }

  std::unique_ptr<EchoControl> Create(int sample_rate_hz,
                                      int num_render_channels,
                                      int num_capture_channels) {
    return Create(sample_rate_hz);
  }
};

TEST(ApmConfiguration, EchoControlInjection) {
  // Verify that apm uses an injected echo controller if one is provided.
  webrtc::Config webrtc_config;
  std::unique_ptr<EchoControlFactory> echo_control_factory(
      new MyEchoControlFactory());

  rtc::scoped_refptr<AudioProcessing> apm =
      AudioProcessingBuilderForTesting()
          .SetEchoControlFactory(std::move(echo_control_factory))
          .Create(webrtc_config);

  Int16FrameData audio;
  audio.num_channels = 1;
  SetFrameSampleRate(&audio, AudioProcessing::NativeRate::kSampleRate16kHz);
  apm->ProcessStream(audio.data.data(),
                     StreamConfig(audio.sample_rate_hz, audio.num_channels),
                     StreamConfig(audio.sample_rate_hz, audio.num_channels),
                     audio.data.data());
  apm->ProcessReverseStream(
      audio.data.data(), StreamConfig(audio.sample_rate_hz, audio.num_channels),
      StreamConfig(audio.sample_rate_hz, audio.num_channels),
      audio.data.data());
  apm->ProcessStream(audio.data.data(),
                     StreamConfig(audio.sample_rate_hz, audio.num_channels),
                     StreamConfig(audio.sample_rate_hz, audio.num_channels),
                     audio.data.data());
}

std::unique_ptr<AudioProcessing> CreateApm(bool mobile_aec) {
  Config old_config;
  std::unique_ptr<AudioProcessing> apm(
      AudioProcessingBuilderForTesting().Create(old_config));
  if (!apm) {
    return apm;
  }

  ProcessingConfig processing_config = {
      {{32000, 1}, {32000, 1}, {32000, 1}, {32000, 1}}};

  if (apm->Initialize(processing_config) != 0) {
    return nullptr;
  }

  // Disable all components except for an AEC and the residual echo detector.
  AudioProcessing::Config apm_config;
  apm_config.residual_echo_detector.enabled = true;
  apm_config.high_pass_filter.enabled = false;
  apm_config.gain_controller1.enabled = false;
  apm_config.gain_controller2.enabled = false;
  apm_config.echo_canceller.enabled = true;
  apm_config.echo_canceller.mobile_mode = mobile_aec;
  apm_config.noise_suppression.enabled = false;
  apm_config.level_estimation.enabled = false;
  apm_config.voice_detection.enabled = false;
  apm->ApplyConfig(apm_config);
  return apm;
}

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS) || defined(WEBRTC_MAC)
#define MAYBE_ApmStatistics DISABLED_ApmStatistics
#else
#define MAYBE_ApmStatistics ApmStatistics
#endif

TEST(MAYBE_ApmStatistics, AECEnabledTest) {
  // Set up APM with AEC3 and process some audio.
  std::unique_ptr<AudioProcessing> apm = CreateApm(false);
  ASSERT_TRUE(apm);
  AudioProcessing::Config apm_config;
  apm_config.echo_canceller.enabled = true;
  apm->ApplyConfig(apm_config);

  // Set up an audioframe.
  Int16FrameData frame;
  frame.num_channels = 1;
  SetFrameSampleRate(&frame, AudioProcessing::NativeRate::kSampleRate32kHz);

  // Fill the audio frame with a sawtooth pattern.
  int16_t* ptr = frame.data.data();
  for (size_t i = 0; i < frame.kMaxDataSizeSamples; i++) {
    ptr[i] = 10000 * ((i % 3) - 1);
  }

  // Do some processing.
  for (int i = 0; i < 200; i++) {
    EXPECT_EQ(apm->ProcessReverseStream(
                  frame.data.data(),
                  StreamConfig(frame.sample_rate_hz, frame.num_channels),
                  StreamConfig(frame.sample_rate_hz, frame.num_channels),
                  frame.data.data()),
              0);
    EXPECT_EQ(apm->set_stream_delay_ms(0), 0);
    EXPECT_EQ(apm->ProcessStream(
                  frame.data.data(),
                  StreamConfig(frame.sample_rate_hz, frame.num_channels),
                  StreamConfig(frame.sample_rate_hz, frame.num_channels),
                  frame.data.data()),
              0);
  }

  // Test statistics interface.
  AudioProcessingStats stats = apm->GetStatistics();
  // We expect all statistics to be set and have a sensible value.
  ASSERT_TRUE(stats.residual_echo_likelihood);
  EXPECT_GE(*stats.residual_echo_likelihood, 0.0);
  EXPECT_LE(*stats.residual_echo_likelihood, 1.0);
  ASSERT_TRUE(stats.residual_echo_likelihood_recent_max);
  EXPECT_GE(*stats.residual_echo_likelihood_recent_max, 0.0);
  EXPECT_LE(*stats.residual_echo_likelihood_recent_max, 1.0);
  ASSERT_TRUE(stats.echo_return_loss);
  EXPECT_NE(*stats.echo_return_loss, -100.0);
  ASSERT_TRUE(stats.echo_return_loss_enhancement);
  EXPECT_NE(*stats.echo_return_loss_enhancement, -100.0);
}

TEST(MAYBE_ApmStatistics, AECMEnabledTest) {
  // Set up APM with AECM and process some audio.
  std::unique_ptr<AudioProcessing> apm = CreateApm(true);
  ASSERT_TRUE(apm);

  // Set up an audioframe.
  Int16FrameData frame;
  frame.num_channels = 1;
  SetFrameSampleRate(&frame, AudioProcessing::NativeRate::kSampleRate32kHz);

  // Fill the audio frame with a sawtooth pattern.
  int16_t* ptr = frame.data.data();
  for (size_t i = 0; i < frame.kMaxDataSizeSamples; i++) {
    ptr[i] = 10000 * ((i % 3) - 1);
  }

  // Do some processing.
  for (int i = 0; i < 200; i++) {
    EXPECT_EQ(apm->ProcessReverseStream(
                  frame.data.data(),
                  StreamConfig(frame.sample_rate_hz, frame.num_channels),
                  StreamConfig(frame.sample_rate_hz, frame.num_channels),
                  frame.data.data()),
              0);
    EXPECT_EQ(apm->set_stream_delay_ms(0), 0);
    EXPECT_EQ(apm->ProcessStream(
                  frame.data.data(),
                  StreamConfig(frame.sample_rate_hz, frame.num_channels),
                  StreamConfig(frame.sample_rate_hz, frame.num_channels),
                  frame.data.data()),
              0);
  }

  // Test statistics interface.
  AudioProcessingStats stats = apm->GetStatistics();
  // We expect only the residual echo detector statistics to be set and have a
  // sensible value.
  EXPECT_TRUE(stats.residual_echo_likelihood);
  if (stats.residual_echo_likelihood) {
    EXPECT_GE(*stats.residual_echo_likelihood, 0.0);
    EXPECT_LE(*stats.residual_echo_likelihood, 1.0);
  }
  EXPECT_TRUE(stats.residual_echo_likelihood_recent_max);
  if (stats.residual_echo_likelihood_recent_max) {
    EXPECT_GE(*stats.residual_echo_likelihood_recent_max, 0.0);
    EXPECT_LE(*stats.residual_echo_likelihood_recent_max, 1.0);
  }
  EXPECT_FALSE(stats.echo_return_loss);
  EXPECT_FALSE(stats.echo_return_loss_enhancement);
}

TEST(ApmStatistics, ReportOutputRmsDbfs) {
  ProcessingConfig processing_config = {
      {{32000, 1}, {32000, 1}, {32000, 1}, {32000, 1}}};
  AudioProcessing::Config config;

  // Set up an audioframe.
  Int16FrameData frame;
  frame.num_channels = 1;
  SetFrameSampleRate(&frame, AudioProcessing::NativeRate::kSampleRate32kHz);

  // Fill the audio frame with a sawtooth pattern.
  int16_t* ptr = frame.data.data();
  for (size_t i = 0; i < frame.kMaxDataSizeSamples; i++) {
    ptr[i] = 10000 * ((i % 3) - 1);
  }

  std::unique_ptr<AudioProcessing> apm(
      AudioProcessingBuilderForTesting().Create());
  apm->Initialize(processing_config);

  // If not enabled, no metric should be reported.
  EXPECT_EQ(
      apm->ProcessStream(frame.data.data(),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         frame.data.data()),
      0);
  EXPECT_FALSE(apm->GetStatistics().output_rms_dbfs);

  // If enabled, metrics should be reported.
  config.level_estimation.enabled = true;
  apm->ApplyConfig(config);
  EXPECT_EQ(
      apm->ProcessStream(frame.data.data(),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         frame.data.data()),
      0);
  auto stats = apm->GetStatistics();
  EXPECT_TRUE(stats.output_rms_dbfs);
  EXPECT_GE(*stats.output_rms_dbfs, 0);

  // If re-disabled, the value is again not reported.
  config.level_estimation.enabled = false;
  apm->ApplyConfig(config);
  EXPECT_EQ(
      apm->ProcessStream(frame.data.data(),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         frame.data.data()),
      0);
  EXPECT_FALSE(apm->GetStatistics().output_rms_dbfs);
}

TEST(ApmStatistics, ReportHasVoice) {
  ProcessingConfig processing_config = {
      {{32000, 1}, {32000, 1}, {32000, 1}, {32000, 1}}};
  AudioProcessing::Config config;

  // Set up an audioframe.
  Int16FrameData frame;
  frame.num_channels = 1;
  SetFrameSampleRate(&frame, AudioProcessing::NativeRate::kSampleRate32kHz);

  // Fill the audio frame with a sawtooth pattern.
  int16_t* ptr = frame.data.data();
  for (size_t i = 0; i < frame.kMaxDataSizeSamples; i++) {
    ptr[i] = 10000 * ((i % 3) - 1);
  }

  std::unique_ptr<AudioProcessing> apm(
      AudioProcessingBuilderForTesting().Create());
  apm->Initialize(processing_config);

  // If not enabled, no metric should be reported.
  EXPECT_EQ(
      apm->ProcessStream(frame.data.data(),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         frame.data.data()),
      0);
  EXPECT_FALSE(apm->GetStatistics().voice_detected);

  // If enabled, metrics should be reported.
  config.voice_detection.enabled = true;
  apm->ApplyConfig(config);
  EXPECT_EQ(
      apm->ProcessStream(frame.data.data(),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         frame.data.data()),
      0);
  auto stats = apm->GetStatistics();
  EXPECT_TRUE(stats.voice_detected);

  // If re-disabled, the value is again not reported.
  config.voice_detection.enabled = false;
  apm->ApplyConfig(config);
  EXPECT_EQ(
      apm->ProcessStream(frame.data.data(),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         StreamConfig(frame.sample_rate_hz, frame.num_channels),
                         frame.data.data()),
      0);
  EXPECT_FALSE(apm->GetStatistics().voice_detected);
}

TEST(ApmConfiguration, HandlingOfRateAndChannelCombinations) {
  std::array<int, 3> sample_rates_hz = {16000, 32000, 48000};
  std::array<int, 2> render_channel_counts = {1, 7};
  std::array<int, 2> capture_channel_counts = {1, 7};
  RunApmRateAndChannelTest(sample_rates_hz, render_channel_counts,
                           capture_channel_counts);
}

TEST(ApmConfiguration, HandlingOfChannelCombinations) {
  std::array<int, 1> sample_rates_hz = {48000};
  std::array<int, 8> render_channel_counts = {1, 2, 3, 4, 5, 6, 7, 8};
  std::array<int, 8> capture_channel_counts = {1, 2, 3, 4, 5, 6, 7, 8};
  RunApmRateAndChannelTest(sample_rates_hz, render_channel_counts,
                           capture_channel_counts);
}

TEST(ApmConfiguration, HandlingOfRateCombinations) {
  std::array<int, 9> sample_rates_hz = {8000,  11025, 16000,  22050, 32000,
                                        48000, 96000, 192000, 384000};
  std::array<int, 1> render_channel_counts = {2};
  std::array<int, 1> capture_channel_counts = {2};
  RunApmRateAndChannelTest(sample_rates_hz, render_channel_counts,
                           capture_channel_counts);
}

TEST(ApmConfiguration, SelfAssignment) {
  // At some point memory sanitizer was complaining about self-assigment.
  // Make sure we don't regress.
  AudioProcessing::Config config;
  AudioProcessing::Config* config2 = &config;
  *config2 = *config2;  // Workaround -Wself-assign-overloaded
  SUCCEED();  // Real success is absence of defects from asan/msan/ubsan.
}

TEST(AudioProcessing, GainController1ConfigEqual) {
  AudioProcessing::Config::GainController1 a;
  AudioProcessing::Config::GainController1 b;
  EXPECT_EQ(a, b);

  Toggle(a.enabled);
  b.enabled = a.enabled;
  EXPECT_EQ(a, b);

  a.mode = AudioProcessing::Config::GainController1::Mode::kAdaptiveDigital;
  b.mode = a.mode;
  EXPECT_EQ(a, b);

  a.target_level_dbfs++;
  b.target_level_dbfs = a.target_level_dbfs;
  EXPECT_EQ(a, b);

  a.compression_gain_db++;
  b.compression_gain_db = a.compression_gain_db;
  EXPECT_EQ(a, b);

  Toggle(a.enable_limiter);
  b.enable_limiter = a.enable_limiter;
  EXPECT_EQ(a, b);

  a.analog_level_minimum++;
  b.analog_level_minimum = a.analog_level_minimum;
  EXPECT_EQ(a, b);

  a.analog_level_maximum--;
  b.analog_level_maximum = a.analog_level_maximum;
  EXPECT_EQ(a, b);

  auto& a_analog = a.analog_gain_controller;
  auto& b_analog = b.analog_gain_controller;

  Toggle(a_analog.enabled);
  b_analog.enabled = a_analog.enabled;
  EXPECT_EQ(a, b);

  a_analog.startup_min_volume++;
  b_analog.startup_min_volume = a_analog.startup_min_volume;
  EXPECT_EQ(a, b);

  a_analog.clipped_level_min++;
  b_analog.clipped_level_min = a_analog.clipped_level_min;
  EXPECT_EQ(a, b);

  Toggle(a_analog.enable_digital_adaptive);
  b_analog.enable_digital_adaptive = a_analog.enable_digital_adaptive;
  EXPECT_EQ(a, b);
}

// Checks that one differing parameter is sufficient to make two configs
// different.
TEST(AudioProcessing, GainController1ConfigNotEqual) {
  AudioProcessing::Config::GainController1 a;
  const AudioProcessing::Config::GainController1 b;

  Toggle(a.enabled);
  EXPECT_NE(a, b);
  a = b;

  a.mode = AudioProcessing::Config::GainController1::Mode::kAdaptiveDigital;
  EXPECT_NE(a, b);
  a = b;

  a.target_level_dbfs++;
  EXPECT_NE(a, b);
  a = b;

  a.compression_gain_db++;
  EXPECT_NE(a, b);
  a = b;

  Toggle(a.enable_limiter);
  EXPECT_NE(a, b);
  a = b;

  a.analog_level_minimum++;
  EXPECT_NE(a, b);
  a = b;

  a.analog_level_maximum--;
  EXPECT_NE(a, b);
  a = b;

  auto& a_analog = a.analog_gain_controller;
  const auto& b_analog = b.analog_gain_controller;

  Toggle(a_analog.enabled);
  EXPECT_NE(a, b);
  a_analog = b_analog;

  a_analog.startup_min_volume++;
  EXPECT_NE(a, b);
  a_analog = b_analog;

  a_analog.clipped_level_min++;
  EXPECT_NE(a, b);
  a_analog = b_analog;

  Toggle(a_analog.enable_digital_adaptive);
  EXPECT_NE(a, b);
  a_analog = b_analog;
}

TEST(AudioProcessing, GainController2ConfigEqual) {
  AudioProcessing::Config::GainController2 a;
  AudioProcessing::Config::GainController2 b;
  EXPECT_EQ(a, b);

  Toggle(a.enabled);
  b.enabled = a.enabled;
  EXPECT_EQ(a, b);

  a.fixed_digital.gain_db += 1.0f;
  b.fixed_digital.gain_db = a.fixed_digital.gain_db;
  EXPECT_EQ(a, b);

  auto& a_adaptive = a.adaptive_digital;
  auto& b_adaptive = b.adaptive_digital;

  Toggle(a_adaptive.enabled);
  b_adaptive.enabled = a_adaptive.enabled;
  EXPECT_EQ(a, b);

  Toggle(a_adaptive.dry_run);
  b_adaptive.dry_run = a_adaptive.dry_run;
  EXPECT_EQ(a, b);

  a_adaptive.noise_estimator = AudioProcessing::Config::GainController2::
      NoiseEstimator::kStationaryNoise;
  b_adaptive.noise_estimator = a_adaptive.noise_estimator;
  EXPECT_EQ(a, b);

  a_adaptive.vad_reset_period_ms++;
  b_adaptive.vad_reset_period_ms = a_adaptive.vad_reset_period_ms;
  EXPECT_EQ(a, b);

  a_adaptive.adjacent_speech_frames_threshold++;
  b_adaptive.adjacent_speech_frames_threshold =
      a_adaptive.adjacent_speech_frames_threshold;
  EXPECT_EQ(a, b);

  a_adaptive.max_gain_change_db_per_second += 1.0f;
  b_adaptive.max_gain_change_db_per_second =
      a_adaptive.max_gain_change_db_per_second;
  EXPECT_EQ(a, b);

  a_adaptive.max_output_noise_level_dbfs += 1.0f;
  b_adaptive.max_output_noise_level_dbfs =
      a_adaptive.max_output_noise_level_dbfs;
  EXPECT_EQ(a, b);

  Toggle(a_adaptive.sse2_allowed);
  b_adaptive.sse2_allowed = a_adaptive.sse2_allowed;
  EXPECT_EQ(a, b);

  Toggle(a_adaptive.avx2_allowed);
  b_adaptive.avx2_allowed = a_adaptive.avx2_allowed;
  EXPECT_EQ(a, b);

  Toggle(a_adaptive.neon_allowed);
  b_adaptive.neon_allowed = a_adaptive.neon_allowed;
  EXPECT_EQ(a, b);
}

// Checks that one differing parameter is sufficient to make two configs
// different.
TEST(AudioProcessing, GainController2ConfigNotEqual) {
  AudioProcessing::Config::GainController2 a;
  const AudioProcessing::Config::GainController2 b;

  Toggle(a.enabled);
  EXPECT_NE(a, b);
  a = b;

  a.fixed_digital.gain_db += 1.0f;
  EXPECT_NE(a, b);
  a.fixed_digital = b.fixed_digital;

  auto& a_adaptive = a.adaptive_digital;
  const auto& b_adaptive = b.adaptive_digital;

  Toggle(a_adaptive.enabled);
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;

  Toggle(a_adaptive.dry_run);
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;

  a_adaptive.noise_estimator = AudioProcessing::Config::GainController2::
      NoiseEstimator::kStationaryNoise;
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;

  a_adaptive.vad_reset_period_ms++;
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;

  a_adaptive.adjacent_speech_frames_threshold++;
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;

  a_adaptive.max_gain_change_db_per_second += 1.0f;
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;

  a_adaptive.max_output_noise_level_dbfs += 1.0f;
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;

  Toggle(a_adaptive.sse2_allowed);
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;

  Toggle(a_adaptive.avx2_allowed);
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;

  Toggle(a_adaptive.neon_allowed);
  EXPECT_NE(a, b);
  a_adaptive = b_adaptive;
}

}  // namespace webrtc
