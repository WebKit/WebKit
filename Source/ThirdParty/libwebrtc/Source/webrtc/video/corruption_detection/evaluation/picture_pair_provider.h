/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_PICTURE_PAIR_PROVIDER_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_PICTURE_PAIR_PROVIDER_H_

#include <optional>

#include "api/units/data_rate.h"
#include "api/video/video_frame.h"
#include "test/testsupport/file_utils.h"
#include "video/corruption_detection/evaluation/test_clip.h"

namespace webrtc {

struct OriginalCompressedPicturePair {
  const VideoFrame original_image;
  // The corresponding compressed image, obtained through encoding and decoding
  // with the QP value = `frame_average_qp`.
  const VideoFrame compressed_image;
  const int frame_average_qp = 0;
};

// Opens and reads one frame at a time from a raw video. Encodes and decodes
// this frame (to obtain a compressed frame) based on the provided bitrate.
// The original and compressed frame is returned with the `GetNextPicturePair()`
// method together with the corresponding average QP value for that frame.
class PicturePairProvider {
 public:
  PicturePairProvider() = default;
  virtual ~PicturePairProvider() = default;

  // Configures the provider such that `GetNextPicturePair()` can provide an
  // original and compressed frame.
  // Inputs:
  //   `clip` indicates the test clip's path, codec_mode, resolution and
  //   framerate.
  //   `bitrate` the maximum bitrate allowed for encoding a raw video.
  virtual bool Configure(const TestClip& clip, DataRate bitrate) = 0;

  // Encodes and decodes the next frame based on the parameters given in the
  // `Configure()`.
  // Returns:
  //    `OriginalCompressedPicturePair` structure, with the original and
  //    compressed frames and the mean QP of the frame in focus.
  virtual std::optional<OriginalCompressedPicturePair> GetNextPicturePair() = 0;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_PICTURE_PAIR_PROVIDER_H_
