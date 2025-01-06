/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/audio/builtin_audio_processing_builder.h"

#include <utility>

#include "absl/base/nullability.h"
#include "api/audio/audio_processing.h"
#include "api/environment/environment.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "modules/audio_processing/audio_processing_impl.h"
#include "rtc_base/logging.h"

namespace webrtc {

absl::Nullable<scoped_refptr<AudioProcessing>>
BuiltinAudioProcessingBuilder::Build(const Environment& /*env*/) {
  // TODO: bugs.webrtc.org/369904700 - Pass `env` when AudioProcessingImpl gets
  // constructor that accepts it.
  return make_ref_counted<AudioProcessingImpl>(
      config_, std::move(capture_post_processing_),
      std::move(render_pre_processing_), std::move(echo_control_factory_),
      std::move(echo_detector_), std::move(capture_analyzer_));
}

}  // namespace webrtc
