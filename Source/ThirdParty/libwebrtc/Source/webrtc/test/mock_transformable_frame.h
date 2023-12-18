/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_MOCK_TRANSFORMABLE_FRAME_H_
#define TEST_MOCK_TRANSFORMABLE_FRAME_H_

#include "api/frame_transformer_interface.h"
#include "test/gmock.h"

namespace webrtc {

class MockTransformableAudioFrame : public TransformableAudioFrameInterface {
 public:
  MOCK_METHOD(rtc::ArrayView<const uint8_t>, GetData, (), (const, override));
  MOCK_METHOD(rtc::ArrayView<const uint32_t>,
              GetContributingSources,
              (),
              (const, override));
  MOCK_METHOD(absl::optional<uint64_t>,
              AbsoluteCaptureTimestamp,
              (),
              (const, override));
  MOCK_METHOD(void, SetData, (rtc::ArrayView<const uint8_t>), (override));
  MOCK_METHOD(uint8_t, GetPayloadType, (), (const, override));
  MOCK_METHOD(uint32_t, GetSsrc, (), (const, override));
  MOCK_METHOD(uint32_t, GetTimestamp, (), (const, override));
  MOCK_METHOD(void, SetRTPTimestamp, (uint32_t), (override));
  MOCK_METHOD(Direction, GetDirection, (), (const, override));
  MOCK_METHOD(std::string, GetMimeType, (), (const, override));
};

}  // namespace webrtc

#endif  // TEST_MOCK_TRANSFORMABLE_FRAME_H_
