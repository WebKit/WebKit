/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <array>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "av1/ducky_encode.h"
#include "av1/encoder/encoder.h"
#include "test/video_source.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace aom {

constexpr int kMaxRefFrames = 7;

TEST(DuckyEncodeTest, ComputeFirstPassStats) {
  aom_rational_t frame_rate = { 30, 1 };
  VideoInfo video_info = { 352,        288,
                           frame_rate, AOM_IMG_FMT_I420,
                           1,          "bus_352x288_420_f20_b8.yuv" };
  video_info.file_path =
      libaom_test::GetDataPath() + "/" + video_info.file_path;
  DuckyEncode ducky_encode(video_info, kMaxRefFrames);
  std::vector<FIRSTPASS_STATS> frame_stats =
      ducky_encode.ComputeFirstPassStats();
  EXPECT_EQ(frame_stats.size(), static_cast<size_t>(video_info.frame_count));
  for (size_t i = 0; i < frame_stats.size(); ++i) {
    // FIRSTPASS_STATS's first element is frame
    EXPECT_EQ(frame_stats[i].frame, i);
  }
}

TEST(DuckyEncodeTest, EncodeFrame) {
  aom_rational_t frame_rate = { 30, 1 };
  VideoInfo video_info = { 352,        288,
                           frame_rate, AOM_IMG_FMT_I420,
                           17,         "bus_352x288_420_f20_b8.yuv" };
  video_info.file_path =
      libaom_test::GetDataPath() + "/" + video_info.file_path;
  DuckyEncode ducky_encode(video_info, kMaxRefFrames);
  std::vector<FIRSTPASS_STATS> frame_stats =
      ducky_encode.ComputeFirstPassStats();
  ducky_encode.StartEncode(frame_stats);
  // We set coding_frame_count to a arbitrary number that smaller than
  // 17 here.
  // TODO(angiebird): Set coding_frame_count properly, once the DuckyEncode can
  // provide proper information.
  int coding_frame_count = 5;
  EncodeFrameDecision decision = { aom::EncodeFrameMode::kNone,
                                   aom::EncodeGopMode::kNone,
                                   {} };
  for (int i = 0; i < coding_frame_count; ++i) {
    ducky_encode.AllocateBitstreamBuffer(video_info);
    EncodeFrameResult encode_frame_result = ducky_encode.EncodeFrame(decision);
  }
  ducky_encode.EndEncode();
}

TEST(DuckyEncodeTest, EncodeFrameWithQindex) {
  aom_rational_t frame_rate = { 30, 1 };
  VideoInfo video_info = { 352,        288,
                           frame_rate, AOM_IMG_FMT_I420,
                           17,         "bus_352x288_420_f20_b8.yuv" };
  video_info.file_path =
      libaom_test::GetDataPath() + "/" + video_info.file_path;
  DuckyEncode ducky_encode(video_info, kMaxRefFrames);
  std::vector<FIRSTPASS_STATS> frame_stats =
      ducky_encode.ComputeFirstPassStats();
  ducky_encode.StartEncode(frame_stats);
  // We set coding_frame_count to a arbitrary number that smaller than
  // 17 here.
  // TODO(angiebird): Set coding_frame_count properly, once the DuckyEncode can
  // provide proper information.
  int coding_frame_count = 5;
  int q_index = 0;
  EncodeFrameDecision decision = { aom::EncodeFrameMode::kQindex,
                                   aom::EncodeGopMode::kNone,
                                   { q_index, -1 } };
  for (int i = 0; i < coding_frame_count; ++i) {
    ducky_encode.AllocateBitstreamBuffer(video_info);
    EncodeFrameResult encode_frame_result = ducky_encode.EncodeFrame(decision);
    // TODO(angiebird): Check why distortion is not zero when q_index = 0
    EXPECT_EQ(encode_frame_result.dist, 0);
  }
  ducky_encode.EndEncode();
}

TEST(DuckyEncodeTest, EncodeFrameMode) {
  EXPECT_EQ(DUCKY_ENCODE_FRAME_MODE_NONE,
            static_cast<DUCKY_ENCODE_FRAME_MODE>(EncodeFrameMode::kNone));
  EXPECT_EQ(DUCKY_ENCODE_FRAME_MODE_QINDEX,
            static_cast<DUCKY_ENCODE_FRAME_MODE>(EncodeFrameMode::kQindex));
  EXPECT_EQ(
      DUCKY_ENCODE_FRAME_MODE_QINDEX_RDMULT,
      static_cast<DUCKY_ENCODE_FRAME_MODE>(EncodeFrameMode::kQindexRdmult));
}

}  // namespace aom
