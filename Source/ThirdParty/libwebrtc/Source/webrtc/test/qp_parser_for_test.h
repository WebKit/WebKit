/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_QP_PARSER_FOR_TEST_H_
#define TEST_QP_PARSER_FOR_TEST_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "absl/strings/string_view.h"
#include "api/video/video_codec_type.h"
#include "modules/video_coding/utility/av1_qp_parser.h"
#include "modules/video_coding/utility/qp_parser.h"

namespace webrtc {

// This class is a test-only wrapper around `Av1QpParser` and `QpParser`.
// It is used to parse the QP value for any supported codec: AV1, VP8, VP9,
// H264, and H265.
class QpParserForTest {
 public:
  explicit QpParserForTest(bool use_average_qp = true);
  explicit QpParserForTest(Av1QpParser::Settings av1_settings);
  ~QpParserForTest();

  std::optional<uint32_t> Parse(VideoCodecType codec_type,
                                size_t spatial_idx,
                                std::span<const uint8_t> frame_data,
                                int operating_point = 0);

  std::optional<uint32_t> Parse(absl::string_view codec_name,
                                size_t spatial_idx,
                                std::span<const uint8_t> frame_data,
                                int operating_point = 0);

 private:
  std::unique_ptr<Av1QpParser> av1_parser_;
  QpParser non_av1_parsers_;
};

}  // namespace webrtc

#endif  // TEST_QP_PARSER_FOR_TEST_H_
