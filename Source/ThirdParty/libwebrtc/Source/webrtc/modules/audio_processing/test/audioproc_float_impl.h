/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_AUDIOPROC_FLOAT_IMPL_H_
#define MODULES_AUDIO_PROCESSING_TEST_AUDIOPROC_FLOAT_IMPL_H_

#include <memory>

#include "api/audio/audio_processing.h"

namespace webrtc {
namespace test {

// This function implements the audio processing simulation utility. Pass
// `input_aecdump` to provide the content of an AEC dump file as a string; if
// `input_aecdump` is not passed, a WAV or AEC input dump file must be specified
// via the `argv` argument. Pass `processed_capture_samples` to write in it the
// samples processed on the capture side; if `processed_capture_samples` is not
// passed, the output file can optionally be specified via the `argv` argument.
// Any audio_processing object specified in the input is used for the
// simulation. Note that when the audio_processing object is specified all
// functionality that relies on using the internal builder is deactivated,
// since the AudioProcessing object is already created and the builder is not
// used in the simulation.
int AudioprocFloatImpl(rtc::scoped_refptr<AudioProcessing> audio_processing,
                       int argc,
                       char* argv[]);

// This function implements the audio processing simulation utility. Pass
// `input_aecdump` to provide the content of an AEC dump file as a string; if
// `input_aecdump` is not passed, a WAV or AEC input dump file must be specified
// via the `argv` argument. Pass `processed_capture_samples` to write in it the
// samples processed on the capture side; if `processed_capture_samples` is not
// passed, the output file can optionally be specified via the `argv` argument.
int AudioprocFloatImpl(std::unique_ptr<AudioProcessingBuilder> ap_builder,
                       int argc,
                       char* argv[],
                       absl::string_view input_aecdump,
                       std::vector<float>* processed_capture_samples);

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_AUDIOPROC_FLOAT_IMPL_H_
