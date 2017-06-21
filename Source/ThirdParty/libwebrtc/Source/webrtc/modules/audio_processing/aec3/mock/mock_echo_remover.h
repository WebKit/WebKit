/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_ECHO_REMOVER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_ECHO_REMOVER_H_

#include <vector>

#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/echo_path_variability.h"
#include "webrtc/modules/audio_processing/aec3/echo_remover.h"
#include "webrtc/modules/audio_processing/aec3/render_buffer.h"
#include "webrtc/test/gmock.h"

namespace webrtc {
namespace test {

class MockEchoRemover : public EchoRemover {
 public:
  virtual ~MockEchoRemover() = default;

  MOCK_METHOD5(ProcessCapture,
               void(const rtc::Optional<size_t>& echo_path_delay_samples,
                    const EchoPathVariability& echo_path_variability,
                    bool capture_signal_saturation,
                    const RenderBuffer& render_buffer,
                    std::vector<std::vector<float>>* capture));

  MOCK_METHOD1(UpdateEchoLeakageStatus, void(bool leakage_detected));
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_ECHO_REMOVER_H_
