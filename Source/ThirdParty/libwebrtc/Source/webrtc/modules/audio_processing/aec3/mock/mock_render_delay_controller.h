/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_RENDER_DELAY_CONTROLLER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_RENDER_DELAY_CONTROLLER_H_

#include "webrtc/base/array_view.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/downsampled_render_buffer.h"
#include "webrtc/modules/audio_processing/aec3/render_delay_controller.h"
#include "webrtc/test/gmock.h"

namespace webrtc {
namespace test {

class MockRenderDelayController : public RenderDelayController {
 public:
  virtual ~MockRenderDelayController() = default;

  MOCK_METHOD0(Reset, void());
  MOCK_METHOD1(SetDelay, void(size_t render_delay));
  MOCK_METHOD2(GetDelay,
               size_t(const DownsampledRenderBuffer& render_buffer,
                      rtc::ArrayView<const float> capture));
  MOCK_CONST_METHOD0(AlignmentHeadroomSamples, rtc::Optional<size_t>());
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_RENDER_DELAY_CONTROLLER_H_
