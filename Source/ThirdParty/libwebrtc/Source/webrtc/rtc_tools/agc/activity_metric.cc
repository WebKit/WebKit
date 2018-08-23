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
#include <stdlib.h>

#include <algorithm>
#include <memory>

#include "api/audio/audio_frame.h"
#include "modules/audio_processing/agc/agc.h"
#include "modules/audio_processing/agc/loudness_histogram.h"
#include "modules/audio_processing/agc/utility.h"
#include "modules/audio_processing/vad/common.h"
#include "modules/audio_processing/vad/pitch_based_vad.h"
#include "modules/audio_processing/vad/standalone_vad.h"
#include "modules/audio_processing/vad/vad_audio_proc.h"
#include "rtc_base/flags.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "test/gtest.h"

static const int kAgcAnalWindowSamples = 100;
static const float kDefaultActivityThreshold = 0.3f;

DEFINE_bool(standalone_vad, true, "enable stand-alone VAD");
DEFINE_string(true_vad,
              "",
              "name of a file containing true VAD in 'int'"
              " format");
DEFINE_string(video_vad,
              "",
              "name of a file containing video VAD (activity"
              " probabilities) in double format. One activity per 10ms is"
              " required. If no file is given the video information is not"
              " incorporated. Negative activity is interpreted as video is"
              " not adapted and the statistics are not computed during"
              " the learning phase. Note that the negative video activities"
              " are ONLY allowed at the beginning.");
DEFINE_string(result,
              "",
              "name of a file to write the results. The results"
              " will be appended to the end of the file. This is optional.");
DEFINE_string(audio_content,
              "",
              "name of a file where audio content is written"
              " to, in double format.");
DEFINE_float(activity_threshold,
             kDefaultActivityThreshold,
             "Activity threshold");
DEFINE_bool(help, false, "prints this message");

namespace webrtc {

// TODO(turajs) A new CL will be committed soon where ExtractFeatures will
// notify the caller of "silence" input, instead of bailing out. We would not
// need the following function when such a change is made.

// Add some dither to quiet frames. This avoids the ExtractFeatures skip a
// silence frame. Otherwise true VAD would drift with respect to the audio.
// We only consider mono inputs.
static void DitherSilence(AudioFrame* frame) {
  ASSERT_EQ(1u, frame->num_channels_);
  const double kRmsSilence = 5;
  const double sum_squared_silence =
      kRmsSilence * kRmsSilence * frame->samples_per_channel_;
  double sum_squared = 0;
  int16_t* frame_data = frame->mutable_data();
  for (size_t n = 0; n < frame->samples_per_channel_; n++)
    sum_squared += frame_data[n] * frame_data[n];
  if (sum_squared <= sum_squared_silence) {
    for (size_t n = 0; n < frame->samples_per_channel_; n++)
      frame_data[n] = (rand() & 0xF) - 8;  // NOLINT: ignore non-threadsafe.
  }
}

class AgcStat {
 public:
  AgcStat()
      : video_index_(0),
        activity_threshold_(kDefaultActivityThreshold),
        audio_content_(LoudnessHistogram::Create(kAgcAnalWindowSamples)),
        audio_processing_(new VadAudioProc()),
        vad_(new PitchBasedVad()),
        standalone_vad_(StandaloneVad::Create()),
        audio_content_fid_(NULL) {
    for (size_t n = 0; n < kMaxNumFrames; n++)
      video_vad_[n] = 0.5;
  }

  ~AgcStat() {
    if (audio_content_fid_ != NULL) {
      fclose(audio_content_fid_);
    }
  }

  void set_audio_content_file(FILE* audio_content_fid) {
    audio_content_fid_ = audio_content_fid;
  }

  int AddAudio(const AudioFrame& frame, double p_video, int* combined_vad) {
    if (frame.num_channels_ != 1 ||
        frame.samples_per_channel_ != kSampleRateHz / 100 ||
        frame.sample_rate_hz_ != kSampleRateHz)
      return -1;
    video_vad_[video_index_++] = p_video;
    AudioFeatures features;
    const int16_t* frame_data = frame.data();
    audio_processing_->ExtractFeatures(frame_data, frame.samples_per_channel_,
                                       &features);
    if (FLAG_standalone_vad) {
      standalone_vad_->AddAudio(frame_data, frame.samples_per_channel_);
    }
    if (features.num_frames > 0) {
      double p[kMaxNumFrames] = {0.5, 0.5, 0.5, 0.5};
      if (FLAG_standalone_vad) {
        standalone_vad_->GetActivity(p, kMaxNumFrames);
      }
      // TODO(turajs) combining and limiting are used in the source files as
      // well they can be moved to utility.
      // Combine Video and stand-alone VAD.
      for (size_t n = 0; n < features.num_frames; n++) {
        double p_active = p[n] * video_vad_[n];
        double p_passive = (1 - p[n]) * (1 - video_vad_[n]);
        p[n] = rtc::SafeClamp(p_active / (p_active + p_passive), 0.01, 0.99);
      }
      if (vad_->VoicingProbability(features, p) < 0)
        return -1;
      for (size_t n = 0; n < features.num_frames; n++) {
        audio_content_->Update(features.rms[n], p[n]);
        double ac = audio_content_->AudioContent();
        if (audio_content_fid_ != NULL) {
          fwrite(&ac, sizeof(ac), 1, audio_content_fid_);
        }
        if (ac > kAgcAnalWindowSamples * activity_threshold_) {
          combined_vad[n] = 1;
        } else {
          combined_vad[n] = 0;
        }
      }
      video_index_ = 0;
    }
    return static_cast<int>(features.num_frames);
  }

  void Reset() { audio_content_->Reset(); }

  void SetActivityThreshold(double activity_threshold) {
    activity_threshold_ = activity_threshold;
  }

 private:
  int video_index_;
  double activity_threshold_;
  double video_vad_[kMaxNumFrames];
  std::unique_ptr<LoudnessHistogram> audio_content_;
  std::unique_ptr<VadAudioProc> audio_processing_;
  std::unique_ptr<PitchBasedVad> vad_;
  std::unique_ptr<StandaloneVad> standalone_vad_;

  FILE* audio_content_fid_;
};

void void_main(int argc, char* argv[]) {
  webrtc::AgcStat agc_stat;

  FILE* pcm_fid = fopen(argv[1], "rb");
  ASSERT_TRUE(pcm_fid != NULL) << "Cannot open PCM file " << argv[1];

  if (argc < 2) {
    fprintf(stderr, "\nNot Enough arguments\n");
  }

  FILE* true_vad_fid = NULL;
  ASSERT_GT(strlen(FLAG_true_vad), 0u) << "Specify the file containing true "
                                          "VADs using --true_vad flag.";
  true_vad_fid = fopen(FLAG_true_vad, "rb");
  ASSERT_TRUE(true_vad_fid != NULL)
      << "Cannot open the active list " << FLAG_true_vad;

  FILE* results_fid = NULL;
  if (strlen(FLAG_result) > 0) {
    // True if this is the first time writing to this function and we add a
    // header to the beginning of the file.
    bool write_header;
    // Open in the read mode. If it fails, the file doesn't exist and has to
    // write a header for it. Otherwise no need to write a header.
    results_fid = fopen(FLAG_result, "r");
    if (results_fid == NULL) {
      write_header = true;
    } else {
      fclose(results_fid);
      write_header = false;
    }
    // Open in append mode.
    results_fid = fopen(FLAG_result, "a");
    ASSERT_TRUE(results_fid != NULL)
        << "Cannot open the file, " << FLAG_result << ", to write the results.";
    // Write the header if required.
    if (write_header) {
      fprintf(results_fid,
              "%% Total Active,  Misdetection,  "
              "Total inactive,  False Positive,  On-sets,  Missed segments,  "
              "Average response\n");
    }
  }

  FILE* video_vad_fid = NULL;
  if (strlen(FLAG_video_vad) > 0) {
    video_vad_fid = fopen(FLAG_video_vad, "rb");
    ASSERT_TRUE(video_vad_fid != NULL)
        << "Cannot open the file, " << FLAG_video_vad
        << " to read video-based VAD decisions.\n";
  }

  // AgsStat will be the owner of this file and will close it at its
  // destructor.
  FILE* audio_content_fid = NULL;
  if (strlen(FLAG_audio_content) > 0) {
    audio_content_fid = fopen(FLAG_audio_content, "wb");
    ASSERT_TRUE(audio_content_fid != NULL)
        << "Cannot open file, " << FLAG_audio_content
        << " to write audio-content.\n";
    agc_stat.set_audio_content_file(audio_content_fid);
  }

  webrtc::AudioFrame frame;
  frame.num_channels_ = 1;
  frame.sample_rate_hz_ = 16000;
  frame.samples_per_channel_ = frame.sample_rate_hz_ / 100;
  const size_t kSamplesToRead =
      frame.num_channels_ * frame.samples_per_channel_;

  agc_stat.SetActivityThreshold(FLAG_activity_threshold);

  int ret_val = 0;
  int num_frames = 0;
  int agc_vad[kMaxNumFrames];
  uint8_t true_vad[kMaxNumFrames];
  double p_video = 0.5;
  int total_active = 0;
  int total_passive = 0;
  int total_false_positive = 0;
  int total_missed_detection = 0;
  int onset_adaptation = 0;
  int num_onsets = 0;
  bool onset = false;
  uint8_t previous_true_vad = 0;
  int num_not_adapted = 0;
  size_t true_vad_index = 0;
  bool in_false_positive_region = false;
  int total_false_positive_duration = 0;
  bool video_adapted = false;
  while (kSamplesToRead == fread(frame.mutable_data(), sizeof(int16_t),
                                 kSamplesToRead, pcm_fid)) {
    assert(true_vad_index < kMaxNumFrames);
    ASSERT_EQ(1u, fread(&true_vad[true_vad_index], sizeof(*true_vad), 1,
                        true_vad_fid))
        << "Size mismatch between True-VAD and the PCM file.\n";
    if (video_vad_fid != NULL) {
      ASSERT_EQ(1u, fread(&p_video, sizeof(p_video), 1, video_vad_fid))
          << "Not enough video-based VAD probabilities.";
    }

    // Negative video activity indicates that the video-based VAD is not yet
    // adapted. Disregards the learning phase in statistics.
    if (p_video < 0) {
      if (video_adapted) {
        fprintf(stderr,
                "Negative video probabilities ONLY allowed at the "
                "beginning of the sequence, not in the middle.\n");
        exit(1);
      }
      continue;
    } else {
      video_adapted = true;
    }

    num_frames++;
    uint8_t last_true_vad;
    if (true_vad_index == 0) {
      last_true_vad = previous_true_vad;
    } else {
      last_true_vad = true_vad[true_vad_index - 1];
    }
    if (last_true_vad == 1 && true_vad[true_vad_index] == 0) {
      agc_stat.Reset();
    }
    true_vad_index++;

    DitherSilence(&frame);

    ret_val = agc_stat.AddAudio(frame, p_video, agc_vad);
    ASSERT_GE(ret_val, 0);

    if (ret_val > 0) {
      ASSERT_EQ(true_vad_index, static_cast<size_t>(ret_val));
      for (int n = 0; n < ret_val; n++) {
        if (true_vad[n] == 1) {
          total_active++;
          if (previous_true_vad == 0) {
            num_onsets++;
            onset = true;
          }
          if (agc_vad[n] == 0) {
            total_missed_detection++;
            if (onset)
              onset_adaptation++;
          } else {
            in_false_positive_region = false;
            onset = false;
          }
        } else if (true_vad[n] == 0) {
          // Check if |on_set| flag is still up. If so it means that we totally
          // missed an active region
          if (onset)
            num_not_adapted++;
          onset = false;

          total_passive++;
          if (agc_vad[n] == 1) {
            total_false_positive++;
            in_false_positive_region = true;
          }
          if (in_false_positive_region) {
            total_false_positive_duration++;
          }
        } else {
          ASSERT_TRUE(false) << "Invalid value for true-VAD.\n";
        }
        previous_true_vad = true_vad[n];
      }
      true_vad_index = 0;
    }
  }

  if (results_fid != NULL) {
    fprintf(results_fid, "%4d  %4d  %4d  %4d  %4d  %4d  %4.0f %4.0f\n",
            total_active, total_missed_detection, total_passive,
            total_false_positive, num_onsets, num_not_adapted,
            static_cast<float>(onset_adaptation) / (num_onsets + 1e-12),
            static_cast<float>(total_false_positive_duration) /
                (total_passive + 1e-12));
  }
  fprintf(stdout, "%4d %4d %4d %4d %4d %4d %4.0f %4.0f\n", total_active,
          total_missed_detection, total_passive, total_false_positive,
          num_onsets, num_not_adapted,
          static_cast<float>(onset_adaptation) / (num_onsets + 1e-12),
          static_cast<float>(total_false_positive_duration) /
              (total_passive + 1e-12));

  fclose(true_vad_fid);
  fclose(pcm_fid);
  if (video_vad_fid != NULL) {
    fclose(video_vad_fid);
  }
  if (results_fid != NULL) {
    fclose(results_fid);
  }
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  if (argc == 1) {
    // Print usage information.
    std::cout
        << "\nCompute the number of misdetected and false-positive frames. "
           "Not\n"
           " that for each frame of audio (10 ms) there should be one true\n"
           " activity. If any video-based activity is given, there should also "
           "be\n"
           " one probability per frame.\n"
           "Run with --help for more details on available flags.\n"
           "\nUsage:\n\n"
           "activity_metric input_pcm [options]\n"
           "where 'input_pcm' is the input audio sampled at 16 kHz in 16 bits "
           "format.\n\n";
    return 0;
  }
  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }
  webrtc::void_main(argc, argv);
  return 0;
}
