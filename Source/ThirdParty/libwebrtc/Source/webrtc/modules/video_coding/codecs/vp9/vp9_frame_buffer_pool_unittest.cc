/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp9/vp9_frame_buffer_pool.h"

#include <cstddef>

#include "api/video/video_codec_constants.h"
#include "test/gtest.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_frame_buffer.h"

namespace webrtc {

TEST(Vp9FrameBufferPoolTest, VpxGetFrameBufferEnforcesSizeLimit) {
  Vp9FrameBufferPool pool;
  vpx_codec_frame_buffer fb;

  const size_t kSizeLimit = static_cast<size_t>(kMaxFrameSizePixels) * 3 * 2;

  // Valid size under/equal to the limit should succeed (returns 0).
  EXPECT_EQ(Vp9FrameBufferPool::VpxGetFrameBuffer(&pool, kSizeLimit, &fb), 0);
  EXPECT_EQ(Vp9FrameBufferPool::VpxReleaseFrameBuffer(&pool, &fb), 0);

  // Large size exceeding the limit should be rejected (returns -1).
  EXPECT_EQ(Vp9FrameBufferPool::VpxGetFrameBuffer(&pool, kSizeLimit + 1, &fb),
            -1);
}

}  // namespace webrtc
