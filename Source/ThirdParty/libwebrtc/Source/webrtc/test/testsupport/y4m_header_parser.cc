/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/y4m_header_parser.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/video/color_space.h"
#include "api/video/resolution.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_to_number.h"

namespace webrtc {
namespace test {
namespace {

std::optional<ColorSpace> ParseColorSpace(absl::string_view color_space_str) {
  // Common Y4M chroma types:
  // "420" / "420jpeg": 4:20 subsampling, centered chroma (JPEG / WebRTC
  // default). "420paldv": 4:20 subsampling, collocated horizontally and
  // vertically (PAL DV). "420mpeg2": 4:20 subsampling, collocated horizontally,
  // half vertical (MPEG-2 / H.264 / AV1 default). "422": 4:22 subsampling.
  // "444": 4:44 subsampling.
  // "mono": Monochrome.
  if (color_space_str == "420" || color_space_str == "420jpeg") {
    return ColorSpace(
        ColorSpace::PrimaryID::kBT709, ColorSpace::TransferID::kBT709,
        ColorSpace::MatrixID::kBT709, ColorSpace::RangeID::kLimited,
        ColorSpace::ChromaSiting::kHalf, ColorSpace::ChromaSiting::kHalf,
        /*hdr_metadata=*/nullptr);
  }
  if (color_space_str == "420mpeg2") {
    return ColorSpace(
        ColorSpace::PrimaryID::kBT709, ColorSpace::TransferID::kBT709,
        ColorSpace::MatrixID::kBT709, ColorSpace::RangeID::kLimited,
        ColorSpace::ChromaSiting::kCollocated, ColorSpace::ChromaSiting::kHalf,
        /*hdr_metadata=*/nullptr);
  }
  if (color_space_str == "420paldv") {
    return ColorSpace(
        ColorSpace::PrimaryID::kBT709, ColorSpace::TransferID::kBT709,
        ColorSpace::MatrixID::kBT709, ColorSpace::RangeID::kLimited,
        ColorSpace::ChromaSiting::kCollocated,
        ColorSpace::ChromaSiting::kCollocated,
        /*hdr_metadata=*/nullptr);
  }
  if (color_space_str == "422") {
    return ColorSpace(
        ColorSpace::PrimaryID::kBT709, ColorSpace::TransferID::kBT709,
        ColorSpace::MatrixID::kBT709, ColorSpace::RangeID::kLimited,
        ColorSpace::ChromaSiting::kCollocated,
        ColorSpace::ChromaSiting::kUnspecified,
        /*hdr_metadata=*/nullptr);
  }
  if (color_space_str == "444") {
    return ColorSpace(
        ColorSpace::PrimaryID::kBT709, ColorSpace::TransferID::kBT709,
        ColorSpace::MatrixID::kBT709, ColorSpace::RangeID::kLimited,
        ColorSpace::ChromaSiting::kUnspecified,
        ColorSpace::ChromaSiting::kUnspecified,
        /*hdr_metadata=*/nullptr);
  }
  if (color_space_str == "mono") {
    return ColorSpace(
        ColorSpace::PrimaryID::kBT709, ColorSpace::TransferID::kBT709,
        ColorSpace::MatrixID::kBT709, ColorSpace::RangeID::kLimited,
        ColorSpace::ChromaSiting::kUnspecified,
        ColorSpace::ChromaSiting::kUnspecified,
        /*hdr_metadata=*/nullptr);
  }
  RTC_LOG(LS_WARNING) << "Unknown or unsupported Y4M color space: "
                      << color_space_str;
  return std::nullopt;
}

bool ApplyHeaderToken(const Y4mHeaderToken& token, Y4mHeader& header) {
  switch (token.tag) {
    case 'W': {
      if (header.resolution.width > 0) {
        RTC_LOG(LS_WARNING) << "Duplicate Y4M width tag";
        return false;
      }
      std::optional<int> width = webrtc::StringToNumber<int>(token.value);
      if (!width.has_value() || *width <= 0) {
        RTC_LOG(LS_WARNING) << "Invalid Y4M width: " << token.value;
        return false;
      }
      header.resolution.width = *width;
      break;
    }
    case 'H': {
      if (header.resolution.height > 0) {
        RTC_LOG(LS_WARNING) << "Duplicate Y4M height tag";
        return false;
      }
      std::optional<int> height = webrtc::StringToNumber<int>(token.value);
      if (!height.has_value() || *height <= 0) {
        RTC_LOG(LS_WARNING) << "Invalid Y4M height: " << token.value;
        return false;
      }
      header.resolution.height = *height;
      break;
    }
    case 'F': {
      if (header.framerate.has_value()) {
        RTC_LOG(LS_WARNING) << "Duplicate Y4M framerate tag";
        return false;
      }
      size_t colon_pos = token.value.find(':');
      if (colon_pos == absl::string_view::npos) {
        RTC_LOG(LS_WARNING) << "Invalid Y4M framerate format: " << token.value;
        return false;
      }
      std::optional<int64_t> num =
          webrtc::StringToNumber<int64_t>(token.value.substr(0, colon_pos));
      std::optional<int64_t> den =
          webrtc::StringToNumber<int64_t>(token.value.substr(colon_pos + 1));
      if (!num.has_value() || !den.has_value() || *num <= 0 || *den <= 0) {
        RTC_LOG(LS_WARNING) << "Invalid Y4M framerate values: " << token.value;
        return false;
      }
      // Round to nearest millihertz for high-precision frequency
      // representation.
      int64_t millihertz = std::llround((1000.0 * *num) / *den);
      header.framerate = Frequency::MilliHertz(millihertz);
      break;
    }
    case 'C': {
      if (header.color_space.has_value()) {
        RTC_LOG(LS_WARNING) << "Duplicate Y4M color space tag";
        return false;
      }
      std::optional<ColorSpace> color_space = ParseColorSpace(token.value);
      if (!color_space.has_value()) {
        return false;
      }
      header.color_space = color_space;
      break;
    }
    case 'I':
    case 'A':
    case 'X':
    default:
      // Other valid Y4M tags (interlacing, aspect ratio, metadata comments)
      // or ignored parameters.
      break;
  }
  return true;
}

}  // namespace

std::optional<std::vector<Y4mHeaderToken>> TokenizeY4mHeader(
    absl::string_view header_content) {
  size_t newline_pos = header_content.find('\n');
  if (newline_pos == absl::string_view::npos) {
    RTC_LOG(LS_WARNING) << "Y4M header missing newline terminator";
    return std::nullopt;
  }
  if (newline_pos >= kMaxY4mHeaderSizeBytes) {
    RTC_LOG(LS_WARNING) << "Y4M header exceeds maximum supported size";
    return std::nullopt;
  }

  absl::string_view line = header_content.substr(0, newline_pos);
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }

  size_t pos = 0;
  while (pos < line.size() && line[pos] == ' ') {
    ++pos;
  }
  if (pos >= line.size()) {
    return std::nullopt;
  }

  size_t magic_end = line.find(' ', pos);
  if (magic_end == absl::string_view::npos) {
    magic_end = line.size();
  }
  absl::string_view magic = line.substr(pos, magic_end - pos);
  if (magic != "YUV4MPEG2") {
    RTC_LOG(LS_WARNING) << "Invalid Y4M magic: " << magic;
    return std::nullopt;
  }
  pos = magic_end;

  std::vector<Y4mHeaderToken> tokens;
  while (pos < line.size()) {
    while (pos < line.size() && line[pos] == ' ') {
      ++pos;
    }
    if (pos >= line.size()) {
      break;
    }
    size_t token_end = line.find(' ', pos);
    if (token_end == absl::string_view::npos) {
      token_end = line.size();
    }
    absl::string_view token_str = line.substr(pos, token_end - pos);
    pos = token_end;

    if (token_str.empty()) {
      continue;
    }
    tokens.push_back(
        Y4mHeaderToken{.tag = token_str.front(), .value = token_str.substr(1)});
  }

  return tokens;
}

std::optional<Y4mHeader> ParseY4mHeader(absl::string_view header_content) {
  size_t newline_pos = header_content.find('\n');
  if (newline_pos == absl::string_view::npos ||
      newline_pos >= kMaxY4mHeaderSizeBytes) {
    // Detailed warnings logged in TokenizeY4mHeader.
    return std::nullopt;
  }

  std::optional<std::vector<Y4mHeaderToken>> tokens =
      TokenizeY4mHeader(header_content);
  if (!tokens.has_value()) {
    return std::nullopt;
  }

  Y4mHeader header;
  header.header_size = DataSize::Bytes(newline_pos + 1);

  for (const Y4mHeaderToken& token : *tokens) {
    if (!ApplyHeaderToken(token, header)) {
      return std::nullopt;
    }
  }

  if (header.resolution.width <= 0 || header.resolution.height <= 0) {
    RTC_LOG(LS_WARNING) << "Y4M header missing valid width or height";
    return std::nullopt;
  }
  return header;
}

std::optional<Y4mHeader> ParseY4mHeader(FILE* file) {
  if (file == nullptr) {
    return std::nullopt;
  }

  std::string buffer;
  buffer.reserve(128);

  int c = 0;
  while (buffer.size() < kMaxY4mHeaderSizeBytes && (c = fgetc(file)) != EOF) {
    buffer.push_back(static_cast<char>(c));
    if (c == '\n') {
      break;
    }
  }

  if (buffer.empty() || buffer.back() != '\n') {
    return std::nullopt;
  }

  return ParseY4mHeader(buffer);
}

std::optional<Y4mHeader> ParseY4mHeaderFromFile(absl::string_view filepath) {
  std::string path_str(filepath);
  FILE* file = fopen(path_str.c_str(), "rb");
  if (file == nullptr) {
    RTC_LOG(LS_WARNING) << "Cannot open file: " << path_str;
    return std::nullopt;
  }

  std::optional<Y4mHeader> header = ParseY4mHeader(file);
  fclose(file);
  return header;
}

}  // namespace test
}  // namespace webrtc
