/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_MOCK_MOCK_PACKET_MANIPULATOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_MOCK_MOCK_PACKET_MANIPULATOR_H_

#include "webrtc/modules/video_coding/codecs/test/packet_manipulator.h"

#include <string>

#include "webrtc/test/gmock.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_frame.h"

namespace webrtc {
namespace test {

class MockPacketManipulator : public PacketManipulator {
 public:
  MOCK_METHOD1(ManipulatePackets, int(webrtc::EncodedImage* encoded_image));
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_MOCK_MOCK_PACKET_MANIPULATOR_H_
