/*
 *  Copyright (c) 2019 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <memory>
#include <string>
#include <vector>
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/video_source.h"
#include "vp9/simple_encode.h"

namespace vp9 {
namespace {

double GetBitrateInKbps(size_t bit_size, int num_frames, int frame_rate_num,
                        int frame_rate_den) {
  return static_cast<double>(bit_size) / num_frames * frame_rate_num /
         frame_rate_den / 1000.0;
}

// Returns the number of unit in size of 4.
// For example, if size is 7, return 2.
int GetNumUnit4x4(int size) { return (size + 3) >> 2; }

class SimpleEncodeTest : public ::testing::Test {
 protected:
  const int width_ = 352;
  const int height_ = 288;
  const int frame_rate_num_ = 30;
  const int frame_rate_den_ = 1;
  const int target_bitrate_ = 1000;
  const int num_frames_ = 17;
  const int target_level_ = LEVEL_UNKNOWN;
  const std::string in_file_path_str_ =
      libvpx_test::GetDataPath() + "/bus_352x288_420_f20_b8.yuv";
};

TEST_F(SimpleEncodeTest, ComputeFirstPassStats) {
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  std::vector<std::vector<double>> frame_stats =
      simple_encode.ObserveFirstPassStats();
  EXPECT_EQ(frame_stats.size(), static_cast<size_t>(num_frames_));
  const size_t data_num = frame_stats[0].size();
  // Read ObserveFirstPassStats before changing FIRSTPASS_STATS.
  EXPECT_EQ(data_num, static_cast<size_t>(25));
  for (size_t i = 0; i < frame_stats.size(); ++i) {
    EXPECT_EQ(frame_stats[i].size(), data_num);
    // FIRSTPASS_STATS's first element is frame
    EXPECT_EQ(frame_stats[i][0], i);
    // FIRSTPASS_STATS's last element is count, and the count is 1 for single
    // frame stats
    EXPECT_EQ(frame_stats[i][data_num - 1], 1);
  }
}

TEST_F(SimpleEncodeTest, ObserveFirstPassMotionVectors) {
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  std::vector<std::vector<MotionVectorInfo>> fps_motion_vectors =
      simple_encode.ObserveFirstPassMotionVectors();
  EXPECT_EQ(fps_motion_vectors.size(), static_cast<size_t>(num_frames_));
  const size_t num_blocks = ((width_ + 15) >> 4) * ((height_ + 15) >> 4);
  EXPECT_EQ(num_blocks, fps_motion_vectors[0].size());
  for (size_t i = 0; i < fps_motion_vectors.size(); ++i) {
    EXPECT_EQ(num_blocks, fps_motion_vectors[i].size());
    for (size_t j = 0; j < num_blocks; ++j) {
      const int mv_count = fps_motion_vectors[i][j].mv_count;
      const int ref_count =
          (fps_motion_vectors[i][j].ref_frame[0] != kRefFrameTypeNone) +
          (fps_motion_vectors[i][j].ref_frame[1] != kRefFrameTypeNone);
      EXPECT_EQ(mv_count, ref_count);
    }
  }
}

TEST_F(SimpleEncodeTest, GetCodingFrameNum) {
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  const int num_coding_frames = simple_encode.GetCodingFrameNum();
  EXPECT_EQ(num_coding_frames, 19);
}

TEST_F(SimpleEncodeTest, EncodeFrame) {
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  int num_coding_frames = simple_encode.GetCodingFrameNum();
  EXPECT_GE(num_coding_frames, num_frames_);
  simple_encode.StartEncode();
  size_t total_data_bit_size = 0;
  int coded_show_frame_count = 0;
  int frame_coding_index = 0;
  while (coded_show_frame_count < num_frames_) {
    const GroupOfPicture group_of_picture =
        simple_encode.ObserveGroupOfPicture();
    const std::vector<EncodeFrameInfo> &encode_frame_list =
        group_of_picture.encode_frame_list;
    for (size_t group_index = 0; group_index < encode_frame_list.size();
         ++group_index) {
      EncodeFrameResult encode_frame_result;
      simple_encode.EncodeFrame(&encode_frame_result);
      EXPECT_EQ(encode_frame_result.show_idx,
                encode_frame_list[group_index].show_idx);
      EXPECT_EQ(encode_frame_result.frame_type,
                encode_frame_list[group_index].frame_type);
      EXPECT_EQ(encode_frame_list[group_index].coding_index,
                frame_coding_index);
      EXPECT_GE(encode_frame_result.psnr, 34)
          << "The psnr is supposed to be greater than 34 given the "
             "target_bitrate 1000 kbps";
      EXPECT_EQ(encode_frame_result.ref_frame_info,
                encode_frame_list[group_index].ref_frame_info);
      total_data_bit_size += encode_frame_result.coding_data_bit_size;
      ++frame_coding_index;
    }
    coded_show_frame_count += group_of_picture.show_frame_count;
  }
  const double bitrate = GetBitrateInKbps(total_data_bit_size, num_frames_,
                                          frame_rate_num_, frame_rate_den_);
  const double off_target_threshold = 150;
  EXPECT_LE(fabs(target_bitrate_ - bitrate), off_target_threshold);
  simple_encode.EndEncode();
}

TEST_F(SimpleEncodeTest, ObserveKeyFrameMap) {
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  std::vector<int> key_frame_map = simple_encode.ObserveKeyFrameMap();
  EXPECT_EQ(key_frame_map.size(), static_cast<size_t>(num_frames_));
  simple_encode.StartEncode();
  int coded_show_frame_count = 0;
  while (coded_show_frame_count < num_frames_) {
    const GroupOfPicture group_of_picture =
        simple_encode.ObserveGroupOfPicture();
    const std::vector<EncodeFrameInfo> &encode_frame_list =
        group_of_picture.encode_frame_list;
    for (size_t group_index = 0; group_index < encode_frame_list.size();
         ++group_index) {
      EncodeFrameResult encode_frame_result;
      simple_encode.EncodeFrame(&encode_frame_result);
      if (encode_frame_result.frame_type == kFrameTypeKey) {
        EXPECT_EQ(key_frame_map[encode_frame_result.show_idx], 1);
      } else {
        EXPECT_EQ(key_frame_map[encode_frame_result.show_idx], 0);
      }
    }
    coded_show_frame_count += group_of_picture.show_frame_count;
  }
  simple_encode.EndEncode();
}

TEST_F(SimpleEncodeTest, EncodeFrameWithTargetFrameBits) {
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  const int num_coding_frames = simple_encode.GetCodingFrameNum();
  simple_encode.StartEncode();
  for (int i = 0; i < num_coding_frames; ++i) {
    EncodeFrameInfo encode_frame_info = simple_encode.GetNextEncodeFrameInfo();
    int target_frame_bits;
    switch (encode_frame_info.frame_type) {
      case kFrameTypeInter: target_frame_bits = 20000; break;
      case kFrameTypeKey:
      case kFrameTypeAltRef:
      case kFrameTypeGolden: target_frame_bits = 100000; break;
      case kFrameTypeOverlay: target_frame_bits = 2000; break;
      default: target_frame_bits = 20000;
    }

    double percent_diff = 15;
    if (encode_frame_info.frame_type == kFrameTypeOverlay) {
      percent_diff = 100;
    }
    EncodeFrameResult encode_frame_result;
    simple_encode.EncodeFrameWithTargetFrameBits(
        &encode_frame_result, target_frame_bits, percent_diff);
    const int recode_count = encode_frame_result.recode_count;
    // TODO(angiebird): Replace 7 by RATE_CTRL_MAX_RECODE_NUM
    EXPECT_LE(recode_count, 7);
    EXPECT_GE(recode_count, 1);

    const double diff = fabs((double)encode_frame_result.coding_data_bit_size -
                             target_frame_bits);
    EXPECT_LE(diff * 100 / target_frame_bits, percent_diff);
  }
  simple_encode.EndEncode();
}

TEST_F(SimpleEncodeTest, EncodeFrameWithQuantizeIndex) {
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  const int num_coding_frames = simple_encode.GetCodingFrameNum();
  simple_encode.StartEncode();
  for (int i = 0; i < num_coding_frames; ++i) {
    const int assigned_quantize_index = 100 + i;
    EncodeFrameResult encode_frame_result;
    simple_encode.EncodeFrameWithQuantizeIndex(&encode_frame_result,
                                               assigned_quantize_index);
    EXPECT_EQ(encode_frame_result.quantize_index, assigned_quantize_index);
  }
  simple_encode.EndEncode();
}

// This test encodes the video using EncodeFrame(), where quantize indexes
// are selected by vp9 rate control.
// Encode stats and the quantize_indexes are collected.
// Then the test encodes the video again using EncodeFrameWithQuantizeIndex()
// using the quantize indexes collected from the first run.
// Then test whether the encode stats of the two encoding runs match.
TEST_F(SimpleEncodeTest, EncodeConsistencyTest) {
  std::vector<int> quantize_index_list;
  std::vector<uint64_t> ref_sse_list;
  std::vector<double> ref_psnr_list;
  std::vector<size_t> ref_bit_size_list;
  std::vector<FrameType> ref_frame_type_list;
  std::vector<int> ref_show_idx_list;
  {
    // The first encode.
    SimpleEncode simple_encode(width_, height_, frame_rate_num_,
                               frame_rate_den_, target_bitrate_, num_frames_,
                               target_level_, in_file_path_str_.c_str());
    simple_encode.ComputeFirstPassStats();
    const int num_coding_frames = simple_encode.GetCodingFrameNum();
    simple_encode.StartEncode();
    for (int i = 0; i < num_coding_frames; ++i) {
      EncodeFrameResult encode_frame_result;
      simple_encode.EncodeFrame(&encode_frame_result);
      quantize_index_list.push_back(encode_frame_result.quantize_index);
      ref_sse_list.push_back(encode_frame_result.sse);
      ref_psnr_list.push_back(encode_frame_result.psnr);
      ref_bit_size_list.push_back(encode_frame_result.coding_data_bit_size);
      ref_frame_type_list.push_back(encode_frame_result.frame_type);
      ref_show_idx_list.push_back(encode_frame_result.show_idx);
    }
    simple_encode.EndEncode();
  }
  {
    // The second encode with quantize index got from the first encode.
    SimpleEncode simple_encode(width_, height_, frame_rate_num_,
                               frame_rate_den_, target_bitrate_, num_frames_,
                               target_level_, in_file_path_str_.c_str());
    simple_encode.ComputeFirstPassStats();
    const int num_coding_frames = simple_encode.GetCodingFrameNum();
    EXPECT_EQ(static_cast<size_t>(num_coding_frames),
              quantize_index_list.size());
    simple_encode.StartEncode();
    for (int i = 0; i < num_coding_frames; ++i) {
      EncodeFrameResult encode_frame_result;
      simple_encode.EncodeFrameWithQuantizeIndex(&encode_frame_result,
                                                 quantize_index_list[i]);
      EXPECT_EQ(encode_frame_result.quantize_index, quantize_index_list[i]);
      EXPECT_EQ(encode_frame_result.sse, ref_sse_list[i]);
      EXPECT_DOUBLE_EQ(encode_frame_result.psnr, ref_psnr_list[i]);
      EXPECT_EQ(encode_frame_result.coding_data_bit_size, ref_bit_size_list[i]);
      EXPECT_EQ(encode_frame_result.frame_type, ref_frame_type_list[i]);
      EXPECT_EQ(encode_frame_result.show_idx, ref_show_idx_list[i]);
    }
    simple_encode.EndEncode();
  }
}

// Test the information (partition info and motion vector info) stored in
// encoder is the same between two encode runs.
TEST_F(SimpleEncodeTest, EncodeConsistencyTest2) {
  const int num_rows_4x4 = GetNumUnit4x4(width_);
  const int num_cols_4x4 = GetNumUnit4x4(height_);
  const int num_units_4x4 = num_rows_4x4 * num_cols_4x4;
  // The first encode.
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  const int num_coding_frames = simple_encode.GetCodingFrameNum();
  std::vector<PartitionInfo> partition_info_list(num_units_4x4 *
                                                 num_coding_frames);
  std::vector<MotionVectorInfo> motion_vector_info_list(num_units_4x4 *
                                                        num_coding_frames);
  simple_encode.StartEncode();
  for (int i = 0; i < num_coding_frames; ++i) {
    EncodeFrameResult encode_frame_result;
    simple_encode.EncodeFrame(&encode_frame_result);
    for (int j = 0; j < num_rows_4x4 * num_cols_4x4; ++j) {
      partition_info_list[i * num_units_4x4 + j] =
          encode_frame_result.partition_info[j];
      motion_vector_info_list[i * num_units_4x4 + j] =
          encode_frame_result.motion_vector_info[j];
    }
  }
  simple_encode.EndEncode();
  // The second encode.
  SimpleEncode simple_encode_2(width_, height_, frame_rate_num_,
                               frame_rate_den_, target_bitrate_, num_frames_,
                               target_level_, in_file_path_str_.c_str());
  simple_encode_2.ComputeFirstPassStats();
  const int num_coding_frames_2 = simple_encode_2.GetCodingFrameNum();
  simple_encode_2.StartEncode();
  for (int i = 0; i < num_coding_frames_2; ++i) {
    EncodeFrameResult encode_frame_result;
    simple_encode_2.EncodeFrame(&encode_frame_result);
    for (int j = 0; j < num_rows_4x4 * num_cols_4x4; ++j) {
      EXPECT_EQ(encode_frame_result.partition_info[j].row,
                partition_info_list[i * num_units_4x4 + j].row);
      EXPECT_EQ(encode_frame_result.partition_info[j].column,
                partition_info_list[i * num_units_4x4 + j].column);
      EXPECT_EQ(encode_frame_result.partition_info[j].row_start,
                partition_info_list[i * num_units_4x4 + j].row_start);
      EXPECT_EQ(encode_frame_result.partition_info[j].column_start,
                partition_info_list[i * num_units_4x4 + j].column_start);
      EXPECT_EQ(encode_frame_result.partition_info[j].width,
                partition_info_list[i * num_units_4x4 + j].width);
      EXPECT_EQ(encode_frame_result.partition_info[j].height,
                partition_info_list[i * num_units_4x4 + j].height);

      EXPECT_EQ(encode_frame_result.motion_vector_info[j].mv_count,
                motion_vector_info_list[i * num_units_4x4 + j].mv_count);
      EXPECT_EQ(encode_frame_result.motion_vector_info[j].ref_frame[0],
                motion_vector_info_list[i * num_units_4x4 + j].ref_frame[0]);
      EXPECT_EQ(encode_frame_result.motion_vector_info[j].ref_frame[1],
                motion_vector_info_list[i * num_units_4x4 + j].ref_frame[1]);
      EXPECT_EQ(encode_frame_result.motion_vector_info[j].mv_row[0],
                motion_vector_info_list[i * num_units_4x4 + j].mv_row[0]);
      EXPECT_EQ(encode_frame_result.motion_vector_info[j].mv_column[0],
                motion_vector_info_list[i * num_units_4x4 + j].mv_column[0]);
      EXPECT_EQ(encode_frame_result.motion_vector_info[j].mv_row[1],
                motion_vector_info_list[i * num_units_4x4 + j].mv_row[1]);
      EXPECT_EQ(encode_frame_result.motion_vector_info[j].mv_column[1],
                motion_vector_info_list[i * num_units_4x4 + j].mv_column[1]);
    }
  }
  simple_encode_2.EndEncode();
}

// Test the information stored in encoder is the same between two encode runs.
TEST_F(SimpleEncodeTest, EncodeConsistencyTest3) {
  std::vector<int> quantize_index_list;
  const int num_rows_4x4 = GetNumUnit4x4(width_);
  const int num_cols_4x4 = GetNumUnit4x4(height_);
  const int num_units_4x4 = num_rows_4x4 * num_cols_4x4;
  // The first encode.
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  const int num_coding_frames = simple_encode.GetCodingFrameNum();
  std::vector<PartitionInfo> partition_info_list(num_units_4x4 *
                                                 num_coding_frames);
  simple_encode.StartEncode();
  for (int i = 0; i < num_coding_frames; ++i) {
    EncodeFrameResult encode_frame_result;
    simple_encode.EncodeFrame(&encode_frame_result);
    quantize_index_list.push_back(encode_frame_result.quantize_index);
    for (int j = 0; j < num_rows_4x4 * num_cols_4x4; ++j) {
      partition_info_list[i * num_units_4x4 + j] =
          encode_frame_result.partition_info[j];
    }
  }
  simple_encode.EndEncode();
  // The second encode.
  SimpleEncode simple_encode_2(width_, height_, frame_rate_num_,
                               frame_rate_den_, target_bitrate_, num_frames_,
                               target_level_, in_file_path_str_.c_str());
  simple_encode_2.ComputeFirstPassStats();
  const int num_coding_frames_2 = simple_encode_2.GetCodingFrameNum();
  simple_encode_2.StartEncode();
  for (int i = 0; i < num_coding_frames_2; ++i) {
    EncodeFrameResult encode_frame_result;
    simple_encode_2.EncodeFrameWithQuantizeIndex(&encode_frame_result,
                                                 quantize_index_list[i]);
    for (int j = 0; j < num_rows_4x4 * num_cols_4x4; ++j) {
      EXPECT_EQ(encode_frame_result.partition_info[j].row,
                partition_info_list[i * num_units_4x4 + j].row);
      EXPECT_EQ(encode_frame_result.partition_info[j].column,
                partition_info_list[i * num_units_4x4 + j].column);
      EXPECT_EQ(encode_frame_result.partition_info[j].row_start,
                partition_info_list[i * num_units_4x4 + j].row_start);
      EXPECT_EQ(encode_frame_result.partition_info[j].column_start,
                partition_info_list[i * num_units_4x4 + j].column_start);
      EXPECT_EQ(encode_frame_result.partition_info[j].width,
                partition_info_list[i * num_units_4x4 + j].width);
      EXPECT_EQ(encode_frame_result.partition_info[j].height,
                partition_info_list[i * num_units_4x4 + j].height);
    }
  }
  simple_encode_2.EndEncode();
}

// Encode with default VP9 decision first.
// Get QPs and arf locations from the first encode.
// Set external arfs and QPs for the second encode.
// Expect to get matched results.
TEST_F(SimpleEncodeTest, EncodeConsistencySetExternalGroupOfPicturesMap) {
  std::vector<int> quantize_index_list;
  std::vector<uint64_t> ref_sse_list;
  std::vector<double> ref_psnr_list;
  std::vector<size_t> ref_bit_size_list;
  std::vector<int> gop_map(num_frames_, 0);
  {
    // The first encode.
    SimpleEncode simple_encode(width_, height_, frame_rate_num_,
                               frame_rate_den_, target_bitrate_, num_frames_,
                               target_level_, in_file_path_str_.c_str());
    simple_encode.ComputeFirstPassStats();
    simple_encode.StartEncode();

    int coded_show_frame_count = 0;
    while (coded_show_frame_count < num_frames_) {
      const GroupOfPicture group_of_picture =
          simple_encode.ObserveGroupOfPicture();
      gop_map[coded_show_frame_count] |= kGopMapFlagStart;
      if (group_of_picture.use_alt_ref) {
        gop_map[coded_show_frame_count] |= kGopMapFlagUseAltRef;
      }
      const std::vector<EncodeFrameInfo> &encode_frame_list =
          group_of_picture.encode_frame_list;
      for (size_t group_index = 0; group_index < encode_frame_list.size();
           ++group_index) {
        EncodeFrameResult encode_frame_result;
        simple_encode.EncodeFrame(&encode_frame_result);
        quantize_index_list.push_back(encode_frame_result.quantize_index);
        ref_sse_list.push_back(encode_frame_result.sse);
        ref_psnr_list.push_back(encode_frame_result.psnr);
        ref_bit_size_list.push_back(encode_frame_result.coding_data_bit_size);
      }
      coded_show_frame_count += group_of_picture.show_frame_count;
    }
    simple_encode.EndEncode();
  }
  {
    // The second encode with quantize index got from the first encode.
    // The external arfs are the same as the first encode.
    SimpleEncode simple_encode(width_, height_, frame_rate_num_,
                               frame_rate_den_, target_bitrate_, num_frames_,
                               target_level_, in_file_path_str_.c_str());
    simple_encode.ComputeFirstPassStats();
    simple_encode.SetExternalGroupOfPicturesMap(gop_map.data(), gop_map.size());
    const int num_coding_frames = simple_encode.GetCodingFrameNum();
    EXPECT_EQ(static_cast<size_t>(num_coding_frames),
              quantize_index_list.size());
    simple_encode.StartEncode();
    for (int i = 0; i < num_coding_frames; ++i) {
      EncodeFrameResult encode_frame_result;
      simple_encode.EncodeFrameWithQuantizeIndex(&encode_frame_result,
                                                 quantize_index_list[i]);
      EXPECT_EQ(encode_frame_result.quantize_index, quantize_index_list[i]);
      EXPECT_EQ(encode_frame_result.sse, ref_sse_list[i]);
      EXPECT_DOUBLE_EQ(encode_frame_result.psnr, ref_psnr_list[i]);
      EXPECT_EQ(encode_frame_result.coding_data_bit_size, ref_bit_size_list[i]);
    }
    simple_encode.EndEncode();
  }
}

TEST_F(SimpleEncodeTest, SetExternalGroupOfPicturesMap) {
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();

  std::vector<int> gop_map(num_frames_, 0);

  // Should be the first gop group.
  gop_map[0] = 0;

  // Second gop group with an alt ref.
  gop_map[5] |= kGopMapFlagStart | kGopMapFlagUseAltRef;

  // Third gop group without an alt ref.
  gop_map[10] |= kGopMapFlagStart;

  // Last gop group.
  gop_map[14] |= kGopMapFlagStart | kGopMapFlagUseAltRef;

  simple_encode.SetExternalGroupOfPicturesMap(gop_map.data(), gop_map.size());

  std::vector<int> observed_gop_map =
      simple_encode.ObserveExternalGroupOfPicturesMap();

  // First gop group.
  // There is always a key frame at show_idx 0 and key frame should always be
  // the start of a gop. We expect ObserveExternalGroupOfPicturesMap() will
  // insert an extra gop start here.
  EXPECT_EQ(observed_gop_map[0], kGopMapFlagStart | kGopMapFlagUseAltRef);

  // Second gop group with an alt ref.
  EXPECT_EQ(observed_gop_map[5], kGopMapFlagStart | kGopMapFlagUseAltRef);

  // Third gop group without an alt ref.
  EXPECT_EQ(observed_gop_map[10], kGopMapFlagStart);

  // Last gop group. The last gop is not supposed to use an alt ref. We expect
  // ObserveExternalGroupOfPicturesMap() will remove the alt ref flag here.
  EXPECT_EQ(observed_gop_map[14], kGopMapFlagStart);

  int ref_gop_show_frame_count_list[4] = { 5, 5, 4, 3 };
  size_t ref_gop_coded_frame_count_list[4] = { 6, 6, 4, 3 };
  int gop_count = 0;

  simple_encode.StartEncode();
  int coded_show_frame_count = 0;
  while (coded_show_frame_count < num_frames_) {
    const GroupOfPicture group_of_picture =
        simple_encode.ObserveGroupOfPicture();
    const std::vector<EncodeFrameInfo> &encode_frame_list =
        group_of_picture.encode_frame_list;
    EXPECT_EQ(encode_frame_list.size(),
              ref_gop_coded_frame_count_list[gop_count]);
    EXPECT_EQ(group_of_picture.show_frame_count,
              ref_gop_show_frame_count_list[gop_count]);
    for (size_t group_index = 0; group_index < encode_frame_list.size();
         ++group_index) {
      EncodeFrameResult encode_frame_result;
      simple_encode.EncodeFrame(&encode_frame_result);
    }
    coded_show_frame_count += group_of_picture.show_frame_count;
    ++gop_count;
  }
  EXPECT_EQ(gop_count, 4);
  simple_encode.EndEncode();
}

TEST_F(SimpleEncodeTest, GetEncodeFrameInfo) {
  // Makes sure that the encode_frame_info obtained from GetEncodeFrameInfo()
  // matches the counterpart in encode_frame_result obtained from EncodeFrame()
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  simple_encode.ComputeFirstPassStats();
  const int num_coding_frames = simple_encode.GetCodingFrameNum();
  simple_encode.StartEncode();
  for (int i = 0; i < num_coding_frames; ++i) {
    EncodeFrameInfo encode_frame_info = simple_encode.GetNextEncodeFrameInfo();
    EncodeFrameResult encode_frame_result;
    simple_encode.EncodeFrame(&encode_frame_result);
    EXPECT_EQ(encode_frame_info.show_idx, encode_frame_result.show_idx);
    EXPECT_EQ(encode_frame_info.frame_type, encode_frame_result.frame_type);
  }
  simple_encode.EndEncode();
}

TEST_F(SimpleEncodeTest, GetFramePixelCount) {
  SimpleEncode simple_encode(width_, height_, frame_rate_num_, frame_rate_den_,
                             target_bitrate_, num_frames_, target_level_,
                             in_file_path_str_.c_str());
  EXPECT_EQ(simple_encode.GetFramePixelCount(),
            static_cast<uint64_t>(width_ * height_ * 3 / 2));
}

}  // namespace
}  // namespace vp9

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
