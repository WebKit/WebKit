/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/qp_parser_for_test.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "absl/strings/string_view.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_codec.h"
#include "modules/video_coding/utility/av1_qp_parser.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

Av1QpParser::Settings CreateAv1Settings(bool use_average_qp) {
  Av1QpParser::Settings settings;
  settings.use_average_qp = use_average_qp;
  return settings;
}

}  // namespace

QpParserForTest::QpParserForTest(bool use_average_qp)
    : QpParserForTest(CreateAv1Settings(use_average_qp)) {}

QpParserForTest::QpParserForTest(Av1QpParser::Settings av1_settings)
    : av1_parser_(Av1QpParser::Create(av1_settings)) {}

QpParserForTest::~QpParserForTest() = default;

std::optional<uint32_t> QpParserForTest::Parse(
    VideoCodecType codec_type,
    size_t spatial_idx,
    std::span<const uint8_t> frame_data,
    int operating_point) {
  if (codec_type != kVideoCodecAV1) {
    return non_av1_parsers_.Parse(codec_type, spatial_idx, frame_data.data(),
                                  frame_data.size());
  }

  RTC_DCHECK_EQ(codec_type, kVideoCodecAV1);
  return av1_parser_->Parse(frame_data, operating_point);
}

std::optional<uint32_t> QpParserForTest::Parse(
    absl::string_view codec_name,
    size_t spatial_idx,
    std::span<const uint8_t> frame_data,
    int operating_point) {
  return Parse(PayloadStringToCodecType(codec_name), spatial_idx, frame_data,
               operating_point);
}

}  // namespace webrtc
