/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_TESTSUPPORT_MOCK_MOCK_FRAME_READER_H_
#define WEBRTC_TEST_TESTSUPPORT_MOCK_MOCK_FRAME_READER_H_

#include "webrtc/test/testsupport/frame_reader.h"

#include "webrtc/test/gmock.h"

namespace webrtc {
namespace test {

class MockFrameReader : public FrameReader {
 public:
  MOCK_METHOD0(Init, bool());
  MOCK_METHOD0(ReadFrame, rtc::scoped_refptr<I420Buffer>());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(FrameLength, size_t());
  MOCK_METHOD0(NumberOfFrames, int());
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_TESTSUPPORT_MOCK_MOCK_FRAME_READER_H_
