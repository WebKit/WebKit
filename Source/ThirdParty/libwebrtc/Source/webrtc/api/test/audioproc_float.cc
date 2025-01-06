/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/audioproc_float.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "api/audio/audio_processing.h"
#include "api/audio/builtin_audio_processing_builder.h"
#include "modules/audio_processing/test/audioproc_float_impl.h"

namespace webrtc {
namespace test {

int AudioprocFloat(int argc, char* argv[]) {
  return AudioprocFloatImpl(std::make_unique<BuiltinAudioProcessingBuilder>(),
                            argc, argv);
}

int AudioprocFloat(
    absl::Nonnull<std::unique_ptr<BuiltinAudioProcessingBuilder>> ap_builder,
    int argc,
    char* argv[]) {
  return AudioprocFloatImpl(std::move(ap_builder), argc, argv);
}

int AudioprocFloat(
    absl::Nonnull<std::unique_ptr<AudioProcessingBuilderInterface>> ap_builder,
    int argc,
    char* argv[]) {
  return AudioprocFloatImpl(std::move(ap_builder), argc, argv);
}

}  // namespace test
}  // namespace webrtc
