/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc/agc.h"
#include "modules/audio_processing/transient/transient_suppressor.h"
#include "modules/audio_processing/transient/transient_suppressor_impl.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

ABSL_FLAG(std::string, in_file_name, "", "PCM file that contains the signal.");
ABSL_FLAG(std::string,
          detection_file_name,
          "",
          "PCM file that contains the detection signal.");
ABSL_FLAG(std::string,
          reference_file_name,
          "",
          "PCM file that contains the reference signal.");

ABSL_FLAG(int,
          chunk_size_ms,
          10,
          "Time between each chunk of samples in milliseconds.");

ABSL_FLAG(int,
          sample_rate_hz,
          16000,
          "Sampling frequency of the signal in Hertz.");
ABSL_FLAG(int,
          detection_rate_hz,
          0,
          "Sampling frequency of the detection signal in Hertz.");

ABSL_FLAG(int, num_channels, 1, "Number of channels.");

namespace webrtc {

const char kUsage[] =
    "\nDetects and suppresses transients from file.\n\n"
    "This application loads the signal from the in_file_name with a specific\n"
    "num_channels and sample_rate_hz, the detection signal from the\n"
    "detection_file_name with a specific detection_rate_hz, and the reference\n"
    "signal from the reference_file_name with sample_rate_hz, divides them\n"
    "into chunk_size_ms blocks, computes its voice value and depending on the\n"
    "voice_threshold does the respective restoration. You can always get the\n"
    "all-voiced or all-unvoiced cases by setting the voice_threshold to 0 or\n"
    "1 respectively.\n\n";

// Read next buffers from the test files (signed 16-bit host-endian PCM
// format). audio_buffer has int16 samples, detection_buffer has float samples
// with range [-32768,32767], and reference_buffer has float samples with range
// [-1,1]. Return true iff all the buffers were filled completely.
bool ReadBuffers(FILE* in_file,
                 size_t audio_buffer_size,
                 int num_channels,
                 int16_t* audio_buffer,
                 FILE* detection_file,
                 size_t detection_buffer_size,
                 float* detection_buffer,
                 FILE* reference_file,
                 float* reference_buffer) {
  std::unique_ptr<int16_t[]> tmpbuf;
  int16_t* read_ptr = audio_buffer;
  if (num_channels > 1) {
    tmpbuf.reset(new int16_t[num_channels * audio_buffer_size]);
    read_ptr = tmpbuf.get();
  }
  if (fread(read_ptr, sizeof(*read_ptr), num_channels * audio_buffer_size,
            in_file) != num_channels * audio_buffer_size) {
    return false;
  }
  // De-interleave.
  if (num_channels > 1) {
    for (int i = 0; i < num_channels; ++i) {
      for (size_t j = 0; j < audio_buffer_size; ++j) {
        audio_buffer[i * audio_buffer_size + j] =
            read_ptr[i + j * num_channels];
      }
    }
  }
  if (detection_file) {
    std::unique_ptr<int16_t[]> ibuf(new int16_t[detection_buffer_size]);
    if (fread(ibuf.get(), sizeof(ibuf[0]), detection_buffer_size,
              detection_file) != detection_buffer_size)
      return false;
    for (size_t i = 0; i < detection_buffer_size; ++i)
      detection_buffer[i] = ibuf[i];
  }
  if (reference_file) {
    std::unique_ptr<int16_t[]> ibuf(new int16_t[audio_buffer_size]);
    if (fread(ibuf.get(), sizeof(ibuf[0]), audio_buffer_size, reference_file) !=
        audio_buffer_size)
      return false;
    S16ToFloat(ibuf.get(), audio_buffer_size, reference_buffer);
  }
  return true;
}

// Write a number of samples to an open signed 16-bit host-endian PCM file.
static void WritePCM(FILE* f,
                     size_t num_samples,
                     int num_channels,
                     const float* buffer) {
  std::unique_ptr<int16_t[]> ibuf(new int16_t[num_channels * num_samples]);
  // Interleave.
  for (int i = 0; i < num_channels; ++i) {
    for (size_t j = 0; j < num_samples; ++j) {
      ibuf[i + j * num_channels] = FloatS16ToS16(buffer[i * num_samples + j]);
    }
  }
  fwrite(ibuf.get(), sizeof(ibuf[0]), num_channels * num_samples, f);
}

// This application tests the transient suppression by providing a processed
// PCM file, which has to be listened to in order to evaluate the
// performance.
// It gets an audio file, and its voice gain information, and the suppressor
// process it giving the output file "suppressed_keystrokes.pcm".
void void_main() {
  // TODO(aluebs): Remove all FileWrappers.
  // Prepare the input file.
  FILE* in_file = fopen(absl::GetFlag(FLAGS_in_file_name).c_str(), "rb");
  ASSERT_TRUE(in_file != NULL);

  // Prepare the detection file.
  FILE* detection_file = NULL;
  if (!absl::GetFlag(FLAGS_detection_file_name).empty()) {
    detection_file =
        fopen(absl::GetFlag(FLAGS_detection_file_name).c_str(), "rb");
  }

  // Prepare the reference file.
  FILE* reference_file = NULL;
  if (!absl::GetFlag(FLAGS_reference_file_name).empty()) {
    reference_file =
        fopen(absl::GetFlag(FLAGS_reference_file_name).c_str(), "rb");
  }

  // Prepare the output file.
  std::string out_file_name = test::OutputPath() + "suppressed_keystrokes.pcm";
  FILE* out_file = fopen(out_file_name.c_str(), "wb");
  ASSERT_TRUE(out_file != NULL);

  int detection_rate_hz = absl::GetFlag(FLAGS_detection_rate_hz);
  if (detection_rate_hz == 0) {
    detection_rate_hz = absl::GetFlag(FLAGS_sample_rate_hz);
  }

  Agc agc;

  TransientSuppressorImpl suppressor(TransientSuppressor::VadMode::kDefault,
                                     absl::GetFlag(FLAGS_sample_rate_hz),
                                     detection_rate_hz,
                                     absl::GetFlag(FLAGS_num_channels));

  const size_t audio_buffer_size = absl::GetFlag(FLAGS_chunk_size_ms) *
                                   absl::GetFlag(FLAGS_sample_rate_hz) / 1000;
  const size_t detection_buffer_size =
      absl::GetFlag(FLAGS_chunk_size_ms) * detection_rate_hz / 1000;

  // int16 and float variants of the same data.
  std::unique_ptr<int16_t[]> audio_buffer_i(
      new int16_t[absl::GetFlag(FLAGS_num_channels) * audio_buffer_size]);
  std::unique_ptr<float[]> audio_buffer_f(
      new float[absl::GetFlag(FLAGS_num_channels) * audio_buffer_size]);

  std::unique_ptr<float[]> detection_buffer, reference_buffer;

  if (detection_file)
    detection_buffer.reset(new float[detection_buffer_size]);
  if (reference_file)
    reference_buffer.reset(new float[audio_buffer_size]);

  while (ReadBuffers(
      in_file, audio_buffer_size, absl::GetFlag(FLAGS_num_channels),
      audio_buffer_i.get(), detection_file, detection_buffer_size,
      detection_buffer.get(), reference_file, reference_buffer.get())) {
    agc.Process({audio_buffer_i.get(), audio_buffer_size});

    for (size_t i = 0;
         i < absl::GetFlag(FLAGS_num_channels) * audio_buffer_size; ++i) {
      audio_buffer_f[i] = audio_buffer_i[i];
    }

    suppressor.Suppress(audio_buffer_f.get(), audio_buffer_size,
                        absl::GetFlag(FLAGS_num_channels),
                        detection_buffer.get(), detection_buffer_size,
                        reference_buffer.get(), audio_buffer_size,
                        agc.voice_probability(), true);

    // Write result to out file.
    WritePCM(out_file, audio_buffer_size, absl::GetFlag(FLAGS_num_channels),
             audio_buffer_f.get());
  }

  fclose(in_file);
  if (detection_file) {
    fclose(detection_file);
  }
  if (reference_file) {
    fclose(reference_file);
  }
  fclose(out_file);
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  if (args.size() != 1) {
    printf("%s", webrtc::kUsage);
    return 1;
  }
  RTC_CHECK_GT(absl::GetFlag(FLAGS_chunk_size_ms), 0);
  RTC_CHECK_GT(absl::GetFlag(FLAGS_sample_rate_hz), 0);
  RTC_CHECK_GT(absl::GetFlag(FLAGS_num_channels), 0);

  webrtc::void_main();
  return 0;
}
