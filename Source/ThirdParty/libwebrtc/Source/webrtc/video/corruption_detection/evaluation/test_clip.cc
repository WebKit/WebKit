/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/evaluation/test_clip.h"

#include <string>

#include "absl/strings/string_view.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/checks.h"
#include "test/testsupport/file_utils.h"
#include "video/corruption_detection/evaluation/utils.h"

namespace webrtc {

TestClip TestClip::CreateYuvClip(absl::string_view filename,
                                 int width,
                                 int height,
                                 int framerate,
                                 VideoCodecMode codec_mode) {
  // First assume that the filename does not contain the extension.
  std::string clip_path = test::ResourcePath(filename, "yuv");
  if (!test::FileExists(clip_path)) {
    // Second assume that the filename contains a full path to the video.
    RTC_CHECK(test::FileExists(filename)) << "Could not find clip " << filename;
    clip_path = std::string(filename);
  }
  return TestClip(clip_path, width, height, framerate, codec_mode,
                  /*is_yuv=*/true);
}

TestClip TestClip::CreateY4mClip(absl::string_view filename,
                                 VideoCodecMode codec_mode) {
  // First assume that the filename does not contain the extension.
  std::string clip_path = test::ResourcePath(filename, "y4m");
  if (!test::FileExists(clip_path)) {
    // Second assume that the filename contains a full path to the video.
    RTC_CHECK(test::FileExists(filename)) << "Could not find clip " << filename;
    clip_path = std::string(filename);
  }
  const Y4mMetadata y4m_metadata = ReadMetadataFromY4mHeader(clip_path);
  return TestClip(clip_path, y4m_metadata.width, y4m_metadata.height,
                  y4m_metadata.framerate, codec_mode,
                  /*is_yuv=*/false);
}

}  // namespace webrtc
