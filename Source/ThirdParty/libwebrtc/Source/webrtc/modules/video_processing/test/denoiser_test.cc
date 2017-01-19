/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <memory>

#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/modules/video_processing/video_denoiser.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/frame_utils.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

TEST(VideoDenoiserTest, CopyMem) {
  std::unique_ptr<DenoiserFilter> df_c(DenoiserFilter::Create(false, nullptr));
  std::unique_ptr<DenoiserFilter> df_sse_neon(
      DenoiserFilter::Create(true, nullptr));
  uint8_t src[16 * 16], dst[16 * 16];
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      src[i * 16 + j] = i * 16 + j;
    }
  }

  memset(dst, 0, 16 * 16);
  df_c->CopyMem16x16(src, 16, dst, 16);
  EXPECT_EQ(0, memcmp(src, dst, 16 * 16));

  memset(dst, 0, 16 * 16);
  df_sse_neon->CopyMem16x16(src, 16, dst, 16);
  EXPECT_EQ(0, memcmp(src, dst, 16 * 16));
}

TEST(VideoDenoiserTest, Variance) {
  std::unique_ptr<DenoiserFilter> df_c(DenoiserFilter::Create(false, nullptr));
  std::unique_ptr<DenoiserFilter> df_sse_neon(
      DenoiserFilter::Create(true, nullptr));
  uint8_t src[16 * 16], dst[16 * 16];
  uint32_t sum = 0, sse = 0, var;
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      src[i * 16 + j] = i * 16 + j;
    }
  }
  // Compute the 16x8 variance of the 16x16 block.
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 16; ++j) {
      sum += (i * 32 + j);
      sse += (i * 32 + j) * (i * 32 + j);
    }
  }
  var = sse - ((sum * sum) >> 7);
  memset(dst, 0, 16 * 16);
  EXPECT_EQ(var, df_c->Variance16x8(src, 16, dst, 16, &sse));
  EXPECT_EQ(var, df_sse_neon->Variance16x8(src, 16, dst, 16, &sse));
}

TEST(VideoDenoiserTest, MbDenoise) {
  std::unique_ptr<DenoiserFilter> df_c(DenoiserFilter::Create(false, nullptr));
  std::unique_ptr<DenoiserFilter> df_sse_neon(
      DenoiserFilter::Create(true, nullptr));
  uint8_t running_src[16 * 16], src[16 * 16];
  uint8_t dst[16 * 16], dst_sse_neon[16 * 16];

  // Test case: |diff| <= |3 + shift_inc1|
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 2;
    }
  }
  memset(dst, 0, 16 * 16);
  df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  memset(dst_sse_neon, 0, 16 * 16);
  df_sse_neon->MbDenoise(running_src, 16, dst_sse_neon, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_sse_neon, 16 * 16));

  // Test case: |diff| >= |4 + shift_inc1|
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 5;
    }
  }
  memset(dst, 0, 16 * 16);
  df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  memset(dst_sse_neon, 0, 16 * 16);
  df_sse_neon->MbDenoise(running_src, 16, dst_sse_neon, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_sse_neon, 16 * 16));

  // Test case: |diff| >= 8
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 8;
    }
  }
  memset(dst, 0, 16 * 16);
  df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  memset(dst_sse_neon, 0, 16 * 16);
  df_sse_neon->MbDenoise(running_src, 16, dst_sse_neon, 16, src, 16, 0, 1);
  EXPECT_EQ(0, memcmp(dst, dst_sse_neon, 16 * 16));

  // Test case: |diff| > 15
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 16; ++j) {
      running_src[i * 16 + j] = i * 11 + j;
      src[i * 16 + j] = i * 11 + j + 16;
    }
  }
  memset(dst, 0, 16 * 16);
  DenoiserDecision decision =
      df_c->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(COPY_BLOCK, decision);
  decision = df_sse_neon->MbDenoise(running_src, 16, dst, 16, src, 16, 0, 1);
  EXPECT_EQ(COPY_BLOCK, decision);
}

TEST(VideoDenoiserTest, Denoiser) {
  const int kWidth = 352;
  const int kHeight = 288;

  const std::string video_file =
      webrtc::test::ResourcePath("foreman_cif", "yuv");
  FILE* source_file = fopen(video_file.c_str(), "rb");
  ASSERT_TRUE(source_file != nullptr)
      << "Cannot open source file: " << video_file;

  // Create pure C denoiser.
  VideoDenoiser denoiser_c(false);
  // Create SSE or NEON denoiser.
  VideoDenoiser denoiser_sse_neon(true);

  for (;;) {
    rtc::scoped_refptr<VideoFrameBuffer> video_frame_buffer(
        test::ReadI420Buffer(kWidth, kHeight, source_file));
    if (!video_frame_buffer)
      break;

    rtc::scoped_refptr<VideoFrameBuffer> denoised_frame_c(
        denoiser_c.DenoiseFrame(video_frame_buffer, false));
    rtc::scoped_refptr<VideoFrameBuffer> denoised_frame_sse_neon(
        denoiser_sse_neon.DenoiseFrame(video_frame_buffer, false));

    // Denoising results should be the same for C and SSE/NEON denoiser.
    ASSERT_TRUE(
        test::FrameBufsEqual(denoised_frame_c, denoised_frame_sse_neon));
  }
  ASSERT_NE(0, feof(source_file)) << "Error reading source file";
}

}  // namespace webrtc
