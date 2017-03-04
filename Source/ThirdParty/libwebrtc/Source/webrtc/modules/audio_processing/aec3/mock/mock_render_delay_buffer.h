/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_RENDER_DELAY_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_RENDER_DELAY_BUFFER_H_

#include <vector>

#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/render_delay_buffer.h"
#include "webrtc/test/gmock.h"

namespace webrtc {
namespace test {

class MockRenderDelayBuffer : public RenderDelayBuffer {
 public:
  explicit MockRenderDelayBuffer(int sample_rate_hz)
      : block_(std::vector<std::vector<float>>(
            NumBandsForRate(sample_rate_hz),
            std::vector<float>(kBlockSize, 0.f))) {
    ON_CALL(*this, GetNext())
        .WillByDefault(
            testing::Invoke(this, &MockRenderDelayBuffer::FakeGetNext));
  }
  virtual ~MockRenderDelayBuffer() = default;

  MOCK_METHOD1(Insert, bool(std::vector<std::vector<float>>* block));
  MOCK_METHOD0(GetNext, const std::vector<std::vector<float>>&());
  MOCK_METHOD1(SetDelay, void(size_t delay));
  MOCK_CONST_METHOD0(Delay, size_t());
  MOCK_CONST_METHOD0(MaxDelay, size_t());
  MOCK_CONST_METHOD0(IsBlockAvailable, bool());
  MOCK_CONST_METHOD0(MaxApiJitter, size_t());

 private:
  const std::vector<std::vector<float>>& FakeGetNext() const { return block_; }
  std::vector<std::vector<float>> block_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_MOCK_MOCK_RENDER_DELAY_BUFFER_H_
