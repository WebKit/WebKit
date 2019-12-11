/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/simulcast_dummy_buffer_helper.h"

#include <cstring>

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr char kIrrelatedSimulcastStreamFrameData[] = "Dummy!";

}  // namespace

rtc::scoped_refptr<webrtc::VideoFrameBuffer> CreateDummyFrameBuffer() {
  // Use i420 buffer here as default one and supported by all codecs.
  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(2, 2);
  memcpy(buffer->MutableDataY(), kIrrelatedSimulcastStreamFrameData, 2);
  memcpy(buffer->MutableDataY() + buffer->StrideY(),
         kIrrelatedSimulcastStreamFrameData + 2, 2);
  memcpy(buffer->MutableDataU(), kIrrelatedSimulcastStreamFrameData + 4, 1);
  memcpy(buffer->MutableDataV(), kIrrelatedSimulcastStreamFrameData + 5, 1);
  return buffer;
}

bool IsDummyFrameBuffer(
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> video_frame_buffer) {
  if (video_frame_buffer->width() != 2 || video_frame_buffer->height() != 2) {
    return false;
  }
  rtc::scoped_refptr<webrtc::I420BufferInterface> buffer =
      video_frame_buffer->ToI420();
  if (memcmp(buffer->DataY(), kIrrelatedSimulcastStreamFrameData, 2) != 0) {
    return false;
  }
  if (memcmp(buffer->DataY() + buffer->StrideY(),
             kIrrelatedSimulcastStreamFrameData + 2, 2) != 0) {
    return false;
  }
  if (memcmp(buffer->DataU(), kIrrelatedSimulcastStreamFrameData + 4, 1) != 0) {
    return false;
  }
  if (memcmp(buffer->DataV(), kIrrelatedSimulcastStreamFrameData + 5, 1) != 0) {
    return false;
  }
  return true;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
