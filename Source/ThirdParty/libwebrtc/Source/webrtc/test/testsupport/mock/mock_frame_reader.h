/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_MOCK_MOCK_FRAME_READER_H_
#define TEST_TESTSUPPORT_MOCK_MOCK_FRAME_READER_H_

#include "test/gmock.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {
namespace test {

class MockFrameReader : public FrameReader {
 public:
  MOCK_METHOD(bool, Init, (), (override));
  MOCK_METHOD(rtc::scoped_refptr<I420Buffer>, ReadFrame, (), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(size_t, FrameLength, (), (override));
  MOCK_METHOD(int, NumberOfFrames, (), (override));
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_MOCK_MOCK_FRAME_READER_H_
