/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/video_frame_tracking_id_injector.h"

#include "api/video/encoded_image.h"
#include "rtc_base/buffer.h"
#include "test/gtest.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

EncodedImage CreateEncodedImageOfSizeN(size_t n) {
  EncodedImage image;
  rtc::scoped_refptr<EncodedImageBuffer> buffer = EncodedImageBuffer::Create(n);
  for (size_t i = 0; i < n; ++i) {
    buffer->data()[i] = static_cast<uint8_t>(i);
  }
  image.SetEncodedData(buffer);
  return image;
}

TEST(VideoFrameTrackingIdInjectorTest, InjectExtractDiscardFalse) {
  VideoFrameTrackingIdInjector injector;
  EncodedImage source = CreateEncodedImageOfSizeN(10);
  EncodedImageExtractionResult out =
      injector.ExtractData(injector.InjectData(512, false, source));

  EXPECT_EQ(out.id, 512);
  EXPECT_FALSE(out.discard);
  EXPECT_EQ(out.image.size(), 10ul);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(source.data()[i], out.image.data()[i]);
  }
}

#if GTEST_HAS_DEATH_TEST
TEST(VideoFrameTrackingIdInjectorTest, InjectExtractDiscardTrue) {
  VideoFrameTrackingIdInjector injector;
  EncodedImage source = CreateEncodedImageOfSizeN(10);

  EXPECT_DEATH(injector.InjectData(512, true, source), "");
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
