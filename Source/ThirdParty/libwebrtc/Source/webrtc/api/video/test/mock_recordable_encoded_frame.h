/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_TEST_MOCK_RECORDABLE_ENCODED_FRAME_H_
#define API_VIDEO_TEST_MOCK_RECORDABLE_ENCODED_FRAME_H_

#include "api/video/recordable_encoded_frame.h"
#include "test/gmock.h"

namespace webrtc {
class MockRecordableEncodedFrame : public RecordableEncodedFrame {
 public:
  MOCK_CONST_METHOD0(encoded_buffer,
                     rtc::scoped_refptr<const EncodedImageBufferInterface>());
  MOCK_CONST_METHOD0(color_space, absl::optional<webrtc::ColorSpace>());
  MOCK_CONST_METHOD0(codec, VideoCodecType());
  MOCK_CONST_METHOD0(is_key_frame, bool());
  MOCK_CONST_METHOD0(resolution, EncodedResolution());
  MOCK_CONST_METHOD0(render_time, Timestamp());
};
}  // namespace webrtc
#endif  // API_VIDEO_TEST_MOCK_RECORDABLE_ENCODED_FRAME_H_
