/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/frame_analyzer/video_temporal_aligner.h"

#include <cstddef>

#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "rtc_tools/video_file_reader.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {

class VideoTemporalAlignerTest : public ::testing::Test {
 protected:
  void SetUp() {
    reference_video =
        OpenYuvFile(ResourcePath("foreman_128x96", "yuv"), 128, 96);
    ASSERT_TRUE(reference_video);
  }

  rtc::scoped_refptr<Video> reference_video;
};

TEST_F(VideoTemporalAlignerTest, FindMatchingFrameIndicesEmpty) {
  rtc::scoped_refptr<Video> empty_test_video =
      ReorderVideo(reference_video, std::vector<size_t>());

  const std::vector<size_t> matched_indices =
      FindMatchingFrameIndices(reference_video, empty_test_video);

  EXPECT_TRUE(matched_indices.empty());
}

TEST_F(VideoTemporalAlignerTest, FindMatchingFrameIndicesIdentity) {
  const std::vector<size_t> indices =
      FindMatchingFrameIndices(reference_video, reference_video);

  EXPECT_EQ(indices.size(), reference_video->number_of_frames());
  for (size_t i = 0; i < indices.size(); ++i)
    EXPECT_EQ(i, indices[i]);
}

TEST_F(VideoTemporalAlignerTest, FindMatchingFrameIndicesDuplicateFrames) {
  const std::vector<size_t> indices = {2, 2, 2, 2};

  // Generate a test video based on this sequence.
  rtc::scoped_refptr<Video> test_video = ReorderVideo(reference_video, indices);

  const std::vector<size_t> matched_indices =
      FindMatchingFrameIndices(reference_video, test_video);

  EXPECT_EQ(indices, matched_indices);
}

TEST_F(VideoTemporalAlignerTest, FindMatchingFrameIndicesLoopAround) {
  std::vector<size_t> indices;
  for (size_t i = 0; i < reference_video->number_of_frames() * 2; ++i)
    indices.push_back(i % reference_video->number_of_frames());

  // Generate a test video based on this sequence.
  rtc::scoped_refptr<Video> test_video = ReorderVideo(reference_video, indices);

  const std::vector<size_t> matched_indices =
      FindMatchingFrameIndices(reference_video, test_video);

  for (size_t i = 0; i < matched_indices.size(); ++i)
    EXPECT_EQ(i, matched_indices[i]);
}

TEST_F(VideoTemporalAlignerTest, FindMatchingFrameIndicesStressTest) {
  std::vector<size_t> indices;
  // Arbitrary start index.
  const size_t start_index = 12345;
  // Generate some generic sequence of frames.
  indices.push_back(start_index % reference_video->number_of_frames());
  indices.push_back((start_index + 1) % reference_video->number_of_frames());
  indices.push_back((start_index + 2) % reference_video->number_of_frames());
  indices.push_back((start_index + 5) % reference_video->number_of_frames());
  indices.push_back((start_index + 10) % reference_video->number_of_frames());
  indices.push_back((start_index + 20) % reference_video->number_of_frames());
  indices.push_back((start_index + 20) % reference_video->number_of_frames());
  indices.push_back((start_index + 22) % reference_video->number_of_frames());
  indices.push_back((start_index + 32) % reference_video->number_of_frames());

  // Generate a test video based on this sequence.
  rtc::scoped_refptr<Video> test_video = ReorderVideo(reference_video, indices);

  const std::vector<size_t> matched_indices =
      FindMatchingFrameIndices(reference_video, test_video);

  EXPECT_EQ(indices, matched_indices);
}

TEST_F(VideoTemporalAlignerTest, GenerateAlignedReferenceVideo) {
  // Arbitrary start index.
  const size_t start_index = 12345;
  std::vector<size_t> indices;
  const size_t frame_step = 10;
  for (size_t i = 0; i < reference_video->number_of_frames() / frame_step;
       ++i) {
    indices.push_back((start_index + i * frame_step) %
                      reference_video->number_of_frames());
  }

  // Generate a test video based on this sequence.
  rtc::scoped_refptr<Video> test_video = ReorderVideo(reference_video, indices);

  rtc::scoped_refptr<Video> aligned_reference_video =
      GenerateAlignedReferenceVideo(reference_video, test_video);

  // Assume perfect match, i.e. ssim == 1, for all frames.
  for (size_t i = 0; i < test_video->number_of_frames(); ++i) {
    EXPECT_EQ(1.0, Ssim(test_video->GetFrame(i),
                        aligned_reference_video->GetFrame(i)));
  }
}

}  // namespace test
}  // namespace webrtc
