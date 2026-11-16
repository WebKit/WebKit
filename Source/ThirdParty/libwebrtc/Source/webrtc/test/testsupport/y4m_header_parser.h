/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_Y4M_HEADER_PARSER_H_
#define TEST_TESTSUPPORT_Y4M_HEADER_PARSER_H_

#include <cstddef>
#include <cstdio>
#include <optional>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/video/color_space.h"
#include "api/video/resolution.h"

namespace webrtc {
namespace test {

struct Y4mHeader {
  Resolution resolution = {.width = 0, .height = 0};
  // Nominal framerate if specified in the header.
  std::optional<Frequency> framerate;
  // ColorSpace if specified in the header and recognized.
  std::optional<ColorSpace> color_space;
  // Total size of the file header in bytes, including the terminating '\n'.
  DataSize header_size = DataSize::Zero();
};

struct Y4mHeaderToken {
  char tag = '\0';
  absl::string_view value;

  bool operator==(const Y4mHeaderToken& other) const = default;
};

// Maximum allowed header size in bytes. Headers exceeding this without a
// newline are rejected to avoid unbounded memory consumption.
inline constexpr size_t kMaxY4mHeaderSizeBytes = 2048;

// Extracts tokens from a Y4M header string line up to '\n'.
// Validates the magic prefix ("YUV4MPEG2") and splits the header into
// {tag, value} tokens. Returns std::nullopt if the line is not a valid Y4M
// header.
std::optional<std::vector<Y4mHeaderToken>> TokenizeY4mHeader(
    absl::string_view header_content);

// Parses Y4M header from an in-memory buffer containing the header line up to
// '\n'. Uses bounds-checked string_view parsing without unsafe pointer
// arithmetic. Returns std::nullopt if the buffer does not contain a valid Y4M
// header.
std::optional<Y4mHeader> ParseY4mHeader(absl::string_view header_content);

// Reads from `file` up to `kMaxY4mHeaderSizeBytes` or until newline and parses
// the Y4M header. Advances the file stream position past the header on success.
// Returns std::nullopt if reading fails or header is invalid.
std::optional<Y4mHeader> ParseY4mHeader(FILE* file);

// Opens `filepath`, reads and parses the Y4M header, and closes the file.
// Returns std::nullopt if the file cannot be opened or header is invalid.
std::optional<Y4mHeader> ParseY4mHeaderFromFile(absl::string_view filepath);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_Y4M_HEADER_PARSER_H_
