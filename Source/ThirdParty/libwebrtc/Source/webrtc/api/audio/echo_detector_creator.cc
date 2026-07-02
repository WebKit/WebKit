/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/audio/echo_detector_creator.h"

#include "api/audio/audio_processing.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "modules/audio_processing/residual_echo_detector.h"

namespace webrtc {

scoped_refptr<EchoDetector> CreateEchoDetector() {
  return make_ref_counted<ResidualEchoDetector>();
}

}  // namespace webrtc
