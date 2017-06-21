/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/test/conversational_speech/simulator.h"

#include <set>
#include <utility>
#include <vector>

#include "webrtc/base/array_view.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/test/conversational_speech/wavreader_interface.h"

namespace webrtc {
namespace test {
namespace {

using conversational_speech::MultiEndCall;
using conversational_speech::SpeakerOutputFilePaths;
using conversational_speech::WavReaderInterface;

// Combines output path and speaker names to define the output file paths for
// the near-end and far=end audio tracks.
std::unique_ptr<std::map<std::string, SpeakerOutputFilePaths>>
    InitSpeakerOutputFilePaths(const std::set<std::string>& speaker_names,
                               const std::string& output_path) {
  // Create map.
  auto speaker_output_file_paths_map = rtc::MakeUnique<
      std::map<std::string, SpeakerOutputFilePaths>>();

  // Add near-end and far-end output paths into the map.
  for (const auto& speaker_name : speaker_names) {
    const rtc::Pathname near_end_path(
        output_path, "s_" + speaker_name + "-near_end.wav");
    LOG(LS_VERBOSE) << "The near-end audio track will be created in "
        << near_end_path.pathname() << ".";

    const rtc::Pathname far_end_path(
        output_path, "s_" + speaker_name + "-far_end.wav");
    LOG(LS_VERBOSE) << "The far-end audio track will be created in "
        << far_end_path.pathname() << ".";

    // Add to map.
    speaker_output_file_paths_map->emplace(
        std::piecewise_construct,
        std::forward_as_tuple(speaker_name),
        std::forward_as_tuple(near_end_path.pathname(),
                              far_end_path.pathname()));
  }

  return speaker_output_file_paths_map;
}

// Class that provides one WavWriter for the near-end and one for the far-end
// output track of a speaker.
class SpeakerWavWriters {
 public:
  SpeakerWavWriters(
      const SpeakerOutputFilePaths& output_file_paths, int sample_rate)
          : near_end_wav_writer_(output_file_paths.near_end, sample_rate, 1u),
            far_end_wav_writer_(output_file_paths.far_end, sample_rate, 1u) {}
  WavWriter* near_end_wav_writer() {
    return &near_end_wav_writer_;
  }
  WavWriter* far_end_wav_writer() {
    return &far_end_wav_writer_;
  }
 private:
  WavWriter near_end_wav_writer_;
  WavWriter far_end_wav_writer_;
};

// Initializes one WavWriter instance for each speaker and both the near-end and
// far-end output tracks.
std::unique_ptr<std::map<std::string, SpeakerWavWriters>>
    InitSpeakersWavWriters(const std::map<std::string, SpeakerOutputFilePaths>&
                           speaker_output_file_paths, int sample_rate) {
  // Create map.
  auto speaker_wav_writers_map = rtc::MakeUnique<
      std::map<std::string, SpeakerWavWriters>>();

  // Add SpeakerWavWriters instance into the map.
  for (auto it = speaker_output_file_paths.begin();
      it != speaker_output_file_paths.end(); ++it) {
    speaker_wav_writers_map->emplace(
        std::piecewise_construct,
        std::forward_as_tuple(it->first),
        std::forward_as_tuple(it->second, sample_rate));
  }

  return speaker_wav_writers_map;
}

// Reads all the samples for each audio track.
std::unique_ptr<std::map<std::string, std::vector<int16_t>>> PreloadAudioTracks(
    const std::map<std::string, std::unique_ptr<WavReaderInterface>>&
        audiotrack_readers) {
  // Create map.
  auto audiotracks_map = rtc::MakeUnique<
      std::map<std::string, std::vector<int16_t>>>();

  // Add audio track vectors.
  for (auto it = audiotrack_readers.begin(); it != audiotrack_readers.end();
      ++it) {
    // Add map entry.
    audiotracks_map->emplace(
        std::piecewise_construct,
        std::forward_as_tuple(it->first),
        std::forward_as_tuple(it->second->NumSamples()));

    // Read samples.
    it->second->ReadInt16Samples(audiotracks_map->at(it->first));
  }

  return audiotracks_map;
}

// Writes all the values in |source_samples| via |wav_writer|. If the number of
// previously written samples in |wav_writer| is less than |interval_begin|, it
// adds zeros as left padding. The padding corresponds to intervals during which
// a speaker is not active.
void PadLeftWriteChunk(rtc::ArrayView<const int16_t> source_samples,
                       size_t interval_begin, WavWriter* wav_writer) {
  // Add left padding.
  RTC_CHECK(wav_writer);
  RTC_CHECK_GE(interval_begin, wav_writer->num_samples());
  size_t padding_size = interval_begin - wav_writer->num_samples();
  if (padding_size != 0) {
    const std::vector<int16_t> padding(padding_size, 0);
    wav_writer->WriteSamples(padding.data(), padding_size);
  }

  // Write source samples.
  wav_writer->WriteSamples(source_samples.data(), source_samples.size());
}

// Appends zeros via |wav_writer|. The number of zeros is always non-negative
// and equal to the difference between the previously written samples and
// |pad_samples|.
void PadRightWrite(WavWriter* wav_writer, size_t pad_samples) {
  RTC_CHECK(wav_writer);
  RTC_CHECK_GE(pad_samples, wav_writer->num_samples());
  size_t padding_size = pad_samples - wav_writer->num_samples();
  if (padding_size != 0) {
    const std::vector<int16_t> padding(padding_size, 0);
    wav_writer->WriteSamples(padding.data(), padding_size);
  }
}

}  // namespace

namespace conversational_speech {

std::unique_ptr<std::map<std::string, SpeakerOutputFilePaths>> Simulate(
    const MultiEndCall& multiend_call, const std::string& output_path) {
  // Set output file paths and initialize wav writers.
  const auto& speaker_names = multiend_call.speaker_names();
  auto speaker_output_file_paths = InitSpeakerOutputFilePaths(
      speaker_names, output_path);
  auto speakers_wav_writers = InitSpeakersWavWriters(
      *speaker_output_file_paths, multiend_call.sample_rate());

  // Preload all the input audio tracks.
  const auto& audiotrack_readers = multiend_call.audiotrack_readers();
  auto audiotracks = PreloadAudioTracks(audiotrack_readers);

  // TODO(alessiob): When speaker_names.size() == 2, near-end and far-end
  // across the 2 speakers are symmetric; hence, the code below could be
  // replaced by only creating the near-end or the far-end. However, this would
  // require to split the unit tests and document the behavior in README.md.
  // In practice, it should not be an issue since the files are not expected to
  // be signinificant.

  // Write near-end and far-end output tracks.
  for (const auto& speaking_turn : multiend_call.speaking_turns()) {
    const std::string& active_speaker_name = speaking_turn.speaker_name;
    auto source_audiotrack = audiotracks->at(
        speaking_turn.audiotrack_file_name);

    // Write active speaker's chunk to active speaker's near-end.
    PadLeftWriteChunk(source_audiotrack, speaking_turn.begin,
                      speakers_wav_writers->at(
                          active_speaker_name).near_end_wav_writer());

    // Write active speaker's chunk to other participants' far-ends.
    for (const std::string& speaker_name : speaker_names) {
      if (speaker_name == active_speaker_name)
        continue;
      PadLeftWriteChunk(source_audiotrack, speaking_turn.begin,
                        speakers_wav_writers->at(
                            speaker_name).far_end_wav_writer());
    }
  }

  // Finalize all the output tracks with right padding.
  // This is required to make all the output tracks duration equal.
  size_t duration_samples = multiend_call.total_duration_samples();
  for (const std::string& speaker_name : speaker_names) {
    PadRightWrite(speakers_wav_writers->at(speaker_name).near_end_wav_writer(),
                  duration_samples);
    PadRightWrite(speakers_wav_writers->at(speaker_name).far_end_wav_writer(),
                  duration_samples);
  }

  return speaker_output_file_paths;
}

}  // namespace conversational_speech
}  // namespace test
}  // namespace webrtc
