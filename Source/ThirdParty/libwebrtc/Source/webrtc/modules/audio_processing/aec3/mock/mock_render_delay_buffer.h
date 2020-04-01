/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_RENDER_DELAY_BUFFER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_RENDER_DELAY_BUFFER_H_

#include <vector>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/downsampled_render_buffer.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/render_delay_buffer.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {

class MockRenderDelayBuffer : public RenderDelayBuffer {
 public:
  MockRenderDelayBuffer(int sample_rate_hz, size_t num_channels);
  virtual ~MockRenderDelayBuffer();

  MOCK_METHOD0(Reset, void());
  MOCK_METHOD1(Insert,
               RenderDelayBuffer::BufferingEvent(
                   const std::vector<std::vector<std::vector<float>>>& block));
  MOCK_METHOD0(PrepareCaptureProcessing, RenderDelayBuffer::BufferingEvent());
  MOCK_METHOD1(AlignFromDelay, bool(size_t delay));
  MOCK_METHOD0(AlignFromExternalDelay, void());
  MOCK_CONST_METHOD0(Delay, size_t());
  MOCK_CONST_METHOD0(MaxDelay, size_t());
  MOCK_METHOD0(GetRenderBuffer, RenderBuffer*());
  MOCK_CONST_METHOD0(GetDownsampledRenderBuffer,
                     const DownsampledRenderBuffer&());
  MOCK_CONST_METHOD1(CausalDelay, bool(size_t delay));
  MOCK_METHOD1(SetAudioBufferDelay, void(int delay_ms));
  MOCK_METHOD0(HasReceivedBufferDelay, bool());

 private:
  RenderBuffer* FakeGetRenderBuffer() { return &render_buffer_; }
  const DownsampledRenderBuffer& FakeGetDownsampledRenderBuffer() const {
    return downsampled_render_buffer_;
  }
  BlockBuffer block_buffer_;
  SpectrumBuffer spectrum_buffer_;
  FftBuffer fft_buffer_;
  RenderBuffer render_buffer_;
  DownsampledRenderBuffer downsampled_render_buffer_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_RENDER_DELAY_BUFFER_H_
