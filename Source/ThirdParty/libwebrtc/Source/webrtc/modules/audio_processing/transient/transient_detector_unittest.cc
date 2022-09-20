/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/transient/transient_detector.h"

#include <memory>
#include <string>

#include "modules/audio_processing/transient/common.h"
#include "modules/audio_processing/transient/file_utils.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/file_wrapper.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {

static const int kSampleRatesHz[] = {ts::kSampleRate8kHz, ts::kSampleRate16kHz,
                                     ts::kSampleRate32kHz,
                                     ts::kSampleRate48kHz};
static const size_t kNumberOfSampleRates =
    sizeof(kSampleRatesHz) / sizeof(*kSampleRatesHz);

// This test is for the correctness of the transient detector.
// Checks the results comparing them with the ones stored in the detect files in
// the directory: resources/audio_processing/transient/
// The files contain all the results in double precision (Little endian).
// The audio files used with different sample rates are stored in the same
// directory.
#if defined(WEBRTC_IOS)
TEST(TransientDetectorTest, DISABLED_CorrectnessBasedOnFiles) {
#else
TEST(TransientDetectorTest, CorrectnessBasedOnFiles) {
#endif
  for (size_t i = 0; i < kNumberOfSampleRates; ++i) {
    int sample_rate_hz = kSampleRatesHz[i];

    // Prepare detect file.
    rtc::StringBuilder detect_file_name;
    detect_file_name << "audio_processing/transient/detect"
                     << (sample_rate_hz / 1000) << "kHz";

    FileWrapper detect_file = FileWrapper::OpenReadOnly(
        test::ResourcePath(detect_file_name.str(), "dat"));

    bool file_opened = detect_file.is_open();
    ASSERT_TRUE(file_opened) << "File could not be opened.\n"
                             << detect_file_name.str().c_str();

    // Prepare audio file.
    rtc::StringBuilder audio_file_name;
    audio_file_name << "audio_processing/transient/audio"
                    << (sample_rate_hz / 1000) << "kHz";

    FileWrapper audio_file = FileWrapper::OpenReadOnly(
        test::ResourcePath(audio_file_name.str(), "pcm"));

    // Create detector.
    TransientDetector detector(sample_rate_hz);

    const size_t buffer_length = sample_rate_hz * ts::kChunkSizeMs / 1000;
    std::unique_ptr<float[]> buffer(new float[buffer_length]);

    const float kTolerance = 0.02f;

    size_t frames_read = 0;

    while (ReadInt16FromFileToFloatBuffer(&audio_file, buffer_length,
                                          buffer.get()) == buffer_length) {
      ++frames_read;

      float detector_value =
          detector.Detect(buffer.get(), buffer_length, NULL, 0);
      double file_value;
      ASSERT_EQ(1u, ReadDoubleBufferFromFile(&detect_file, 1, &file_value))
          << "Detect test file is malformed.\n";

      // Compare results with data from the matlab test file.
      EXPECT_NEAR(file_value, detector_value, kTolerance)
          << "Frame: " << frames_read;
    }

    detect_file.Close();
    audio_file.Close();
  }
}

}  // namespace webrtc
