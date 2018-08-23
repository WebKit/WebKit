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

#include <utility>

#include "modules/audio_processing/test/audioproc_float_impl.h"

namespace webrtc {
namespace test {

int AudioprocFloat(std::unique_ptr<AudioProcessingBuilder> ap_builder,
                   int argc,
                   char* argv[]) {
  return AudioprocFloatImpl(std::move(ap_builder), argc, argv);
}

}  // namespace test
}  // namespace webrtc
