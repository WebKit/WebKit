/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>
#include <iostream>
#include <vector>

#include "api/audio/audio_mixer.h"
#include "common_audio/wav_file.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_mixer/default_output_rate_calculator.h"
#include "rtc_base/flags.h"
#include "rtc_base/strings/string_builder.h"

DEFINE_bool(help, false, "Prints this message");
DEFINE_int(sampling_rate,
           16000,
           "Rate at which to mix (all input streams must have this rate)");

DEFINE_bool(
    stereo,
    false,
    "Enable stereo (interleaved). Inputs need not be as this parameter.");

DEFINE_bool(limiter, true, "Enable limiter.");
DEFINE_string(output_file,
              "mixed_file.wav",
              "File in which to store the mixed result.");
DEFINE_string(input_file_1, "", "First input. Default none.");
DEFINE_string(input_file_2, "", "Second input. Default none.");
DEFINE_string(input_file_3, "", "Third input. Default none.");
DEFINE_string(input_file_4, "", "Fourth input. Default none.");

namespace webrtc {
namespace test {

class FilePlayingSource : public AudioMixer::Source {
 public:
  explicit FilePlayingSource(std::string filename)
      : wav_reader_(new WavReader(filename)),
        sample_rate_hz_(wav_reader_->sample_rate()),
        samples_per_channel_(sample_rate_hz_ / 100),
        number_of_channels_(wav_reader_->num_channels()) {}

  AudioFrameInfo GetAudioFrameWithInfo(int target_rate_hz,
                                       AudioFrame* frame) override {
    frame->samples_per_channel_ = samples_per_channel_;
    frame->num_channels_ = number_of_channels_;
    frame->sample_rate_hz_ = target_rate_hz;

    RTC_CHECK_EQ(target_rate_hz, sample_rate_hz_);

    const size_t num_to_read = number_of_channels_ * samples_per_channel_;
    const size_t num_read =
        wav_reader_->ReadSamples(num_to_read, frame->mutable_data());

    file_has_ended_ = num_to_read != num_read;
    if (file_has_ended_) {
      frame->Mute();
    }
    return file_has_ended_ ? AudioFrameInfo::kMuted : AudioFrameInfo::kNormal;
  }

  int Ssrc() const override { return 0; }

  int PreferredSampleRate() const override { return sample_rate_hz_; }

  bool FileHasEnded() const { return file_has_ended_; }

  std::string ToString() const {
    rtc::StringBuilder ss;
    ss << "{rate: " << sample_rate_hz_ << ", channels: " << number_of_channels_
       << ", samples_tot: " << wav_reader_->num_samples() << "}";
    return ss.Release();
  }

 private:
  std::unique_ptr<WavReader> wav_reader_;
  int sample_rate_hz_;
  int samples_per_channel_;
  int number_of_channels_;
  bool file_has_ended_ = false;
};
}  // namespace test
}  // namespace webrtc

namespace {

const std::vector<std::string> parse_input_files() {
  std::vector<std::string> result;
  for (auto* x : {FLAG_input_file_1, FLAG_input_file_2, FLAG_input_file_3,
                  FLAG_input_file_4}) {
    if (strcmp(x, "") != 0) {
      result.push_back(x);
    }
  }
  return result;
}
}  // namespace

int main(int argc, char* argv[]) {
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, false);
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  rtc::scoped_refptr<webrtc::AudioMixerImpl> mixer(
      webrtc::AudioMixerImpl::Create(
          std::unique_ptr<webrtc::OutputRateCalculator>(
              new webrtc::DefaultOutputRateCalculator()),
          FLAG_limiter));

  const std::vector<std::string> input_files = parse_input_files();
  std::vector<webrtc::test::FilePlayingSource> sources;
  const int num_channels = FLAG_stereo ? 2 : 1;
  for (auto input_file : input_files) {
    sources.emplace_back(input_file);
  }

  for (auto& source : sources) {
    auto error = mixer->AddSource(&source);
    RTC_CHECK(error);
  }

  if (sources.empty()) {
    std::cout << "Need at least one source!\n";
    rtc::FlagList::Print(nullptr, false);
    return 1;
  }

  const size_t sample_rate = sources[0].PreferredSampleRate();
  for (const auto& source : sources) {
    RTC_CHECK_EQ(sample_rate, source.PreferredSampleRate());
  }

  // Print stats.
  std::cout << "Limiting is: " << (FLAG_limiter ? "on" : "off") << "\n"
            << "Channels: " << num_channels << "\n"
            << "Rate: " << sample_rate << "\n"
            << "Number of input streams: " << input_files.size() << "\n";
  for (const auto& source : sources) {
    std::cout << "\t" << source.ToString() << "\n";
  }
  std::cout << "Now mixing\n...\n";

  webrtc::WavWriter wav_writer(FLAG_output_file, sample_rate, num_channels);

  webrtc::AudioFrame frame;

  bool all_streams_finished = false;
  while (!all_streams_finished) {
    mixer->Mix(num_channels, &frame);
    RTC_CHECK_EQ(sample_rate / 100, frame.samples_per_channel_);
    RTC_CHECK_EQ(sample_rate, frame.sample_rate_hz_);
    RTC_CHECK_EQ(num_channels, frame.num_channels_);
    wav_writer.WriteSamples(frame.data(),
                            num_channels * frame.samples_per_channel_);

    all_streams_finished =
        std::all_of(sources.begin(), sources.end(),
                    [](const webrtc::test::FilePlayingSource& source) {
                      return source.FileHasEnded();
                    });
  }

  std::cout << "Done!\n" << std::endl;
}
