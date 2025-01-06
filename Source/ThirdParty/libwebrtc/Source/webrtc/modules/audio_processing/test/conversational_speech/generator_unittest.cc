/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file consists of unit tests for webrtc::test::conversational_speech
// members. Part of them focus on accepting or rejecting different
// conversational speech setups. A setup is defined by a set of audio tracks and
// timing information).
// The docstring at the beginning of each TEST(ConversationalSpeechTest,
// MultiEndCallSetup*) function looks like the drawing below and indicates which
// setup is tested.
//
//    Accept:
//    A 0****.....
//    B .....1****
//
// The drawing indicates the following:
// - the illustrated setup should be accepted,
// - there are two speakers (namely, A and B),
// - A is the first speaking, B is the second one,
// - each character after the speaker's letter indicates a time unit (e.g., 100
//   ms),
// - "*" indicates speaking, "." listening,
// - numbers indicate the turn index in std::vector<Turn>.
//
// Note that the same speaker can appear in multiple lines in order to depict
// cases in which there are wrong offsets leading to self cross-talk (which is
// rejected).

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include <stdio.h>

#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "absl/strings/string_view.h"
#include "common_audio/wav_file.h"
#include "modules/audio_processing/test/conversational_speech/config.h"
#include "modules/audio_processing/test/conversational_speech/mock_wavreader_factory.h"
#include "modules/audio_processing/test/conversational_speech/multiend_call.h"
#include "modules/audio_processing/test/conversational_speech/simulator.h"
#include "modules/audio_processing/test/conversational_speech/timing.h"
#include "modules/audio_processing/test/conversational_speech/wavreader_factory.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {

using conversational_speech::LoadTiming;
using conversational_speech::MockWavReaderFactory;
using conversational_speech::MultiEndCall;
using conversational_speech::SaveTiming;
using conversational_speech::Turn;
using conversational_speech::WavReaderFactory;

const char* const audiotracks_path = "/path/to/audiotracks";
const char* const timing_filepath = "/path/to/timing_file.txt";
const char* const output_path = "/path/to/output_dir";

const std::vector<Turn> expected_timing = {
    {"A", "a1", 0, 0},    {"B", "b1", 0, 0}, {"A", "a2", 100, 0},
    {"B", "b2", -200, 0}, {"A", "a3", 0, 0}, {"A", "a3", 0, 0},
};
const std::size_t kNumberOfTurns = expected_timing.size();

// Default arguments for MockWavReaderFactory ctor.
// Fake audio track parameters.
constexpr int kDefaultSampleRate = 48000;
const std::map<std::string, const MockWavReaderFactory::Params>
    kDefaultMockWavReaderFactoryParamsMap = {
        {"t300", {kDefaultSampleRate, 1u, 14400u}},   // Mono, 0.3 seconds.
        {"t500", {kDefaultSampleRate, 1u, 24000u}},   // Mono, 0.5 seconds.
        {"t1000", {kDefaultSampleRate, 1u, 48000u}},  // Mono, 1.0 seconds.
        {"sr8000", {8000, 1u, 8000u}},     // 8kHz sample rate, mono, 1 second.
        {"sr16000", {16000, 1u, 16000u}},  // 16kHz sample rate, mono, 1 second.
        {"sr16000_stereo", {16000, 2u, 16000u}},  // Like sr16000, but stereo.
};
const MockWavReaderFactory::Params& kDefaultMockWavReaderFactoryParams =
    kDefaultMockWavReaderFactoryParamsMap.at("t500");

std::unique_ptr<MockWavReaderFactory> CreateMockWavReaderFactory() {
  return std::unique_ptr<MockWavReaderFactory>(
      new MockWavReaderFactory(kDefaultMockWavReaderFactoryParams,
                               kDefaultMockWavReaderFactoryParamsMap));
}

void CreateSineWavFile(absl::string_view filepath,
                       const MockWavReaderFactory::Params& params,
                       float frequency = 440.0f) {
  // Create samples.
  constexpr double two_pi = 2.0 * M_PI;
  std::vector<int16_t> samples(params.num_samples);
  for (std::size_t i = 0; i < params.num_samples; ++i) {
    // TODO(alessiob): the produced tone is not pure, improve.
    samples[i] = std::lround(
        32767.0f * std::sin(two_pi * i * frequency / params.sample_rate));
  }

  // Write samples.
  WavWriter wav_writer(filepath, params.sample_rate, params.num_channels);
  wav_writer.WriteSamples(samples.data(), params.num_samples);
}

// Parameters to generate audio tracks with CreateSineWavFile.
struct SineAudioTrackParams {
  MockWavReaderFactory::Params params;
  float frequency;
};

// Creates a temporary directory in which sine audio tracks are written.
std::string CreateTemporarySineAudioTracks(
    const std::map<std::string, SineAudioTrackParams>& sine_tracks_params) {
  // Create temporary directory.
  std::string temp_directory =
      OutputPath() + "TempConversationalSpeechAudioTracks";
  CreateDir(temp_directory);

  // Create sine tracks.
  for (const auto& it : sine_tracks_params) {
    const std::string temp_filepath = JoinFilename(temp_directory, it.first);
    CreateSineWavFile(temp_filepath, it.second.params, it.second.frequency);
  }

  return temp_directory;
}

void CheckAudioTrackParams(const WavReaderFactory& wav_reader_factory,
                           absl::string_view filepath,
                           const MockWavReaderFactory::Params& expeted_params) {
  auto wav_reader = wav_reader_factory.Create(filepath);
  EXPECT_EQ(expeted_params.sample_rate, wav_reader->SampleRate());
  EXPECT_EQ(expeted_params.num_channels, wav_reader->NumChannels());
  EXPECT_EQ(expeted_params.num_samples, wav_reader->NumSamples());
}

void DeleteFolderAndContents(absl::string_view dir) {
  if (!DirExists(dir)) {
    return;
  }
  std::optional<std::vector<std::string>> dir_content = ReadDirectory(dir);
  EXPECT_TRUE(dir_content);
  for (const auto& path : *dir_content) {
    if (DirExists(path)) {
      DeleteFolderAndContents(path);
    } else if (FileExists(path)) {
      // TODO(alessiob): Wrap with EXPECT_TRUE() once webrtc:7769 bug fixed.
      RemoveFile(path);
    } else {
      FAIL();
    }
  }
  // TODO(alessiob): Wrap with EXPECT_TRUE() once webrtc:7769 bug fixed.
  RemoveDir(dir);
}

}  // namespace

using ::testing::_;

TEST(ConversationalSpeechTest, Settings) {
  const conversational_speech::Config config(audiotracks_path, timing_filepath,
                                             output_path);

  // Test getters.
  EXPECT_EQ(audiotracks_path, config.audiotracks_path());
  EXPECT_EQ(timing_filepath, config.timing_filepath());
  EXPECT_EQ(output_path, config.output_path());
}

TEST(ConversationalSpeechTest, TimingSaveLoad) {
  // Save test timing.
  const std::string temporary_filepath =
      TempFilename(OutputPath(), "TempTimingTestFile");
  SaveTiming(temporary_filepath, expected_timing);

  // Create a std::vector<Turn> instance by loading from file.
  std::vector<Turn> actual_timing = LoadTiming(temporary_filepath);
  RemoveFile(temporary_filepath);

  // Check size.
  EXPECT_EQ(expected_timing.size(), actual_timing.size());

  // Check Turn instances.
  for (size_t index = 0; index < expected_timing.size(); ++index) {
    EXPECT_EQ(expected_timing[index], actual_timing[index])
        << "turn #" << index << " not matching";
  }
}

TEST(ConversationalSpeechTest, MultiEndCallCreate) {
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There are 5 unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(5);

  // Inject the mock wav reader factory.
  conversational_speech::MultiEndCall multiend_call(
      expected_timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_TRUE(multiend_call.valid());

  // Test.
  EXPECT_EQ(2u, multiend_call.speaker_names().size());
  EXPECT_EQ(5u, multiend_call.audiotrack_readers().size());
  EXPECT_EQ(6u, multiend_call.speaking_turns().size());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupDifferentSampleRates) {
  const std::vector<Turn> timing = {
      {"A", "sr8000", 0, 0},
      {"B", "sr16000", 0, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There are two unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(::testing::_)).Times(2);

  MultiEndCall multiend_call(timing, audiotracks_path,
                             std::move(mock_wavreader_factory));
  EXPECT_FALSE(multiend_call.valid());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupMultipleChannels) {
  const std::vector<Turn> timing = {
      {"A", "sr16000_stereo", 0, 0},
      {"B", "sr16000_stereo", 0, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There is one unique audio track to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(::testing::_)).Times(1);

  MultiEndCall multiend_call(timing, audiotracks_path,
                             std::move(mock_wavreader_factory));
  EXPECT_FALSE(multiend_call.valid());
}

TEST(ConversationalSpeechTest,
     MultiEndCallSetupDifferentSampleRatesAndMultipleNumChannels) {
  const std::vector<Turn> timing = {
      {"A", "sr8000", 0, 0},
      {"B", "sr16000_stereo", 0, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There are two unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(::testing::_)).Times(2);

  MultiEndCall multiend_call(timing, audiotracks_path,
                             std::move(mock_wavreader_factory));
  EXPECT_FALSE(multiend_call.valid());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupFirstOffsetNegative) {
  const std::vector<Turn> timing = {
      {"A", "t500", -100, 0},
      {"B", "t500", 0, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There is one unique audio track to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(1);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_FALSE(multiend_call.valid());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupSimple) {
  // Accept:
  // A 0****.....
  // B .....1****
  constexpr std::size_t expected_duration = kDefaultSampleRate;
  const std::vector<Turn> timing = {
      {"A", "t500", 0, 0},
      {"B", "t500", 0, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There is one unique audio track to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(1);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_TRUE(multiend_call.valid());

  // Test.
  EXPECT_EQ(2u, multiend_call.speaker_names().size());
  EXPECT_EQ(1u, multiend_call.audiotrack_readers().size());
  EXPECT_EQ(2u, multiend_call.speaking_turns().size());
  EXPECT_EQ(expected_duration, multiend_call.total_duration_samples());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupPause) {
  // Accept:
  // A 0****.......
  // B .......1****
  constexpr std::size_t expected_duration = kDefaultSampleRate * 1.2;
  const std::vector<Turn> timing = {
      {"A", "t500", 0, 0},
      {"B", "t500", 200, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There is one unique audio track to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(1);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_TRUE(multiend_call.valid());

  // Test.
  EXPECT_EQ(2u, multiend_call.speaker_names().size());
  EXPECT_EQ(1u, multiend_call.audiotrack_readers().size());
  EXPECT_EQ(2u, multiend_call.speaking_turns().size());
  EXPECT_EQ(expected_duration, multiend_call.total_duration_samples());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupCrossTalk) {
  // Accept:
  // A 0****....
  // B ....1****
  constexpr std::size_t expected_duration = kDefaultSampleRate * 0.9;
  const std::vector<Turn> timing = {
      {"A", "t500", 0, 0},
      {"B", "t500", -100, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There is one unique audio track to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(1);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_TRUE(multiend_call.valid());

  // Test.
  EXPECT_EQ(2u, multiend_call.speaker_names().size());
  EXPECT_EQ(1u, multiend_call.audiotrack_readers().size());
  EXPECT_EQ(2u, multiend_call.speaking_turns().size());
  EXPECT_EQ(expected_duration, multiend_call.total_duration_samples());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupInvalidOrder) {
  // Reject:
  // A ..0****
  // B .1****.  The n-th turn cannot start before the (n-1)-th one.
  const std::vector<Turn> timing = {
      {"A", "t500", 200, 0},
      {"B", "t500", -600, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There is one unique audio track to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(1);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_FALSE(multiend_call.valid());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupCrossTalkThree) {
  // Accept:
  // A 0****2****...
  // B ...1*********
  constexpr std::size_t expected_duration = kDefaultSampleRate * 1.3;
  const std::vector<Turn> timing = {
      {"A", "t500", 0, 0},
      {"B", "t1000", -200, 0},
      {"A", "t500", -800, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There are two unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(2);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_TRUE(multiend_call.valid());

  // Test.
  EXPECT_EQ(2u, multiend_call.speaker_names().size());
  EXPECT_EQ(2u, multiend_call.audiotrack_readers().size());
  EXPECT_EQ(3u, multiend_call.speaking_turns().size());
  EXPECT_EQ(expected_duration, multiend_call.total_duration_samples());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupSelfCrossTalkNearInvalid) {
  // Reject:
  // A 0****......
  // A ...1****...
  // B ......2****
  //      ^  Turn #1 overlaps with #0 which is from the same speaker.
  const std::vector<Turn> timing = {
      {"A", "t500", 0, 0},
      {"A", "t500", -200, 0},
      {"B", "t500", -200, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There is one unique audio track to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(1);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_FALSE(multiend_call.valid());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupSelfCrossTalkFarInvalid) {
  // Reject:
  // A 0*********
  // B 1**.......
  // C ...2**....
  // A ......3**.
  //         ^  Turn #3 overlaps with #0 which is from the same speaker.
  const std::vector<Turn> timing = {
      {"A", "t1000", 0, 0},
      {"B", "t300", -1000, 0},
      {"C", "t300", 0, 0},
      {"A", "t300", 0, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There are two unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(2);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_FALSE(multiend_call.valid());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupCrossTalkMiddleValid) {
  // Accept:
  // A 0*********..
  // B ..1****.....
  // C .......2****
  constexpr std::size_t expected_duration = kDefaultSampleRate * 1.2;
  const std::vector<Turn> timing = {
      {"A", "t1000", 0, 0},
      {"B", "t500", -800, 0},
      {"C", "t500", 0, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There are two unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(2);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_TRUE(multiend_call.valid());

  // Test.
  EXPECT_EQ(3u, multiend_call.speaker_names().size());
  EXPECT_EQ(2u, multiend_call.audiotrack_readers().size());
  EXPECT_EQ(3u, multiend_call.speaking_turns().size());
  EXPECT_EQ(expected_duration, multiend_call.total_duration_samples());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupCrossTalkMiddleInvalid) {
  // Reject:
  // A 0*********
  // B ..1****...
  // C ....2****.
  //       ^  Turn #2 overlaps both with #0 and #1 (cross-talk with 3+ speakers
  //          not permitted).
  const std::vector<Turn> timing = {
      {"A", "t1000", 0, 0},
      {"B", "t500", -800, 0},
      {"C", "t500", -300, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There are two unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(2);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_FALSE(multiend_call.valid());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupCrossTalkMiddleAndPause) {
  // Accept:
  // A 0*********..
  // B .2****......
  // C .......3****
  constexpr std::size_t expected_duration = kDefaultSampleRate * 1.2;
  const std::vector<Turn> timing = {
      {"A", "t1000", 0, 0},
      {"B", "t500", -900, 0},
      {"C", "t500", 100, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There are two unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(2);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_TRUE(multiend_call.valid());

  // Test.
  EXPECT_EQ(3u, multiend_call.speaker_names().size());
  EXPECT_EQ(2u, multiend_call.audiotrack_readers().size());
  EXPECT_EQ(3u, multiend_call.speaking_turns().size());
  EXPECT_EQ(expected_duration, multiend_call.total_duration_samples());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupCrossTalkFullOverlapValid) {
  // Accept:
  // A 0****
  // B 1****
  const std::vector<Turn> timing = {
      {"A", "t500", 0, 0},
      {"B", "t500", -500, 0},
  };
  auto mock_wavreader_factory = CreateMockWavReaderFactory();

  // There is one unique audio track to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(1);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_TRUE(multiend_call.valid());

  // Test.
  EXPECT_EQ(2u, multiend_call.speaker_names().size());
  EXPECT_EQ(1u, multiend_call.audiotrack_readers().size());
  EXPECT_EQ(2u, multiend_call.speaking_turns().size());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupLongSequence) {
  // Accept:
  // A 0****....3****.5**.
  // B .....1****...4**...
  // C ......2**.......6**..
  constexpr std::size_t expected_duration = kDefaultSampleRate * 1.9;
  const std::vector<Turn> timing = {
      {"A", "t500", 0, 0},    {"B", "t500", 0, 0},    {"C", "t300", -400, 0},
      {"A", "t500", 0, 0},    {"B", "t300", -100, 0}, {"A", "t300", -100, 0},
      {"C", "t300", -200, 0},
  };
  auto mock_wavreader_factory = std::unique_ptr<MockWavReaderFactory>(
      new MockWavReaderFactory(kDefaultMockWavReaderFactoryParams,
                               kDefaultMockWavReaderFactoryParamsMap));

  // There are two unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(2);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_TRUE(multiend_call.valid());

  // Test.
  EXPECT_EQ(3u, multiend_call.speaker_names().size());
  EXPECT_EQ(2u, multiend_call.audiotrack_readers().size());
  EXPECT_EQ(7u, multiend_call.speaking_turns().size());
  EXPECT_EQ(expected_duration, multiend_call.total_duration_samples());
}

TEST(ConversationalSpeechTest, MultiEndCallSetupLongSequenceInvalid) {
  // Reject:
  // A 0****....3****.6**
  // B .....1****...4**..
  // C ......2**.....5**..
  //                 ^ Turns #4, #5 and #6 overlapping (cross-talk with 3+
  //                   speakers not permitted).
  const std::vector<Turn> timing = {
      {"A", "t500", 0, 0},    {"B", "t500", 0, 0},    {"C", "t300", -400, 0},
      {"A", "t500", 0, 0},    {"B", "t300", -100, 0}, {"A", "t300", -200, 0},
      {"C", "t300", -200, 0},
  };
  auto mock_wavreader_factory = std::unique_ptr<MockWavReaderFactory>(
      new MockWavReaderFactory(kDefaultMockWavReaderFactoryParams,
                               kDefaultMockWavReaderFactoryParamsMap));

  // There are two unique audio tracks to read.
  EXPECT_CALL(*mock_wavreader_factory, Create(_)).Times(2);

  conversational_speech::MultiEndCall multiend_call(
      timing, audiotracks_path, std::move(mock_wavreader_factory));
  EXPECT_FALSE(multiend_call.valid());
}

TEST(ConversationalSpeechTest, MultiEndCallWavReaderAdaptorSine) {
  // Parameters with which wav files are created.
  constexpr int duration_seconds = 5;
  const int sample_rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000};

  for (int sample_rate : sample_rates) {
    const std::string temp_filename = OutputPath() + "TempSineWavFile_" +
                                      std::to_string(sample_rate) + ".wav";

    // Write wav file.
    const std::size_t num_samples = duration_seconds * sample_rate;
    MockWavReaderFactory::Params params = {sample_rate, 1u, num_samples};
    CreateSineWavFile(temp_filename, params);

    // Load wav file and check if params match.
    WavReaderFactory wav_reader_factory;
    MockWavReaderFactory::Params expeted_params = {sample_rate, 1u,
                                                   num_samples};
    CheckAudioTrackParams(wav_reader_factory, temp_filename, expeted_params);

    // Clean up.
    RemoveFile(temp_filename);
  }
}

TEST(ConversationalSpeechTest, DISABLED_MultiEndCallSimulator) {
  // Simulated call (one character corresponding to 500 ms):
  // A 0*********...........2*********.....
  // B ...........1*********.....3*********
  const std::vector<Turn> expected_timing = {
      {"A", "t5000_440.wav", 0, 0},
      {"B", "t5000_880.wav", 500, 0},
      {"A", "t5000_440.wav", 0, 0},
      {"B", "t5000_880.wav", -2500, 0},
  };
  const std::size_t expected_duration_seconds = 18;

  // Create temporary audio track files.
  const int sample_rate = 16000;
  const std::map<std::string, SineAudioTrackParams> sine_tracks_params = {
      {"t5000_440.wav", {{sample_rate, 1u, sample_rate * 5}, 440.0}},
      {"t5000_880.wav", {{sample_rate, 1u, sample_rate * 5}, 880.0}},
  };
  const std::string audiotracks_path =
      CreateTemporarySineAudioTracks(sine_tracks_params);

  // Set up the multi-end call.
  auto wavreader_factory =
      std::unique_ptr<WavReaderFactory>(new WavReaderFactory());
  MultiEndCall multiend_call(expected_timing, audiotracks_path,
                             std::move(wavreader_factory));

  // Simulate the call.
  std::string output_path = JoinFilename(audiotracks_path, "output");
  CreateDir(output_path);
  RTC_LOG(LS_VERBOSE) << "simulator output path: " << output_path;
  auto generated_audiotrak_pairs =
      conversational_speech::Simulate(multiend_call, output_path);
  EXPECT_EQ(2u, generated_audiotrak_pairs->size());

  // Check the output.
  WavReaderFactory wav_reader_factory;
  const MockWavReaderFactory::Params expeted_params = {
      sample_rate, 1u, sample_rate * expected_duration_seconds};
  for (const auto& it : *generated_audiotrak_pairs) {
    RTC_LOG(LS_VERBOSE) << "checking far/near-end for <" << it.first << ">";
    CheckAudioTrackParams(wav_reader_factory, it.second.near_end,
                          expeted_params);
    CheckAudioTrackParams(wav_reader_factory, it.second.far_end,
                          expeted_params);
  }

  // Clean.
  EXPECT_NO_FATAL_FAILURE(DeleteFolderAndContents(audiotracks_path));
}

}  // namespace test
}  // namespace webrtc
