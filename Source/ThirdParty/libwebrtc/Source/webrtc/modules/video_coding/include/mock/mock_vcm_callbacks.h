/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_MOCK_MOCK_VCM_CALLBACKS_H_
#define MODULES_VIDEO_CODING_INCLUDE_MOCK_MOCK_VCM_CALLBACKS_H_

#include "modules/video_coding/include/video_coding_defines.h"
#include "test/gmock.h"

namespace webrtc {

class MockVCMFrameTypeCallback : public VCMFrameTypeCallback {
 public:
  MOCK_METHOD0(RequestKeyFrame, int32_t());
};

class MockPacketRequestCallback : public VCMPacketRequestCallback {
 public:
  MOCK_METHOD2(ResendPackets,
               int32_t(const uint16_t* sequenceNumbers, uint16_t length));
};

class MockVCMReceiveCallback : public VCMReceiveCallback {
 public:
  MockVCMReceiveCallback() {}
  virtual ~MockVCMReceiveCallback() {}

  MOCK_METHOD3(FrameToRender,
               int32_t(VideoFrame&, absl::optional<uint8_t>, VideoContentType));
  MOCK_METHOD1(ReceivedDecodedReferenceFrame, int32_t(const uint64_t));
  MOCK_METHOD1(OnIncomingPayloadType, void(int));
  MOCK_METHOD1(OnDecoderImplementationName, void(const char*));
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_INCLUDE_MOCK_MOCK_VCM_CALLBACKS_H_
