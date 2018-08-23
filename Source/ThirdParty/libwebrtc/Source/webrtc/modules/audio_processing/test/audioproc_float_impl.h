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

#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {
namespace test {

// This function implements the audio processing simulation utility.
int AudioprocFloatImpl(std::unique_ptr<AudioProcessingBuilder> ap_builder,
                       int argc,
                       char* argv[]);

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_AUDIOPROC_FLOAT_IMPL_H_
