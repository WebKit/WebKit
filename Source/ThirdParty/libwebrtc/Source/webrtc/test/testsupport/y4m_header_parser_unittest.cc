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

#include <stdio.h>

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/video/color_space.h"
#include "api/video/resolution.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;

TEST(Y4mHeaderParserTest, TokenizeValidHeader) {
  const std::string header_str = "YUV4MPEG2 W1920 H1080 F30:1 C420 Ip A1:1\n";
  std::optional<std::vector<Y4mHeaderToken>> tokens =
      TokenizeY4mHeader(header_str);

  ASSERT_TRUE(tokens.has_value());
  EXPECT_THAT(*tokens, ElementsAre(Y4mHeaderToken{.tag = 'W', .value = "1920"},
                                   Y4mHeaderToken{.tag = 'H', .value = "1080"},
                                   Y4mHeaderToken{.tag = 'F', .value = "30:1"},
                                   Y4mHeaderToken{.tag = 'C', .value = "420"},
                                   Y4mHeaderToken{.tag = 'I', .value = "p"},
                                   Y4mHeaderToken{.tag = 'A', .value = "1:1"}));
}

TEST(Y4mHeaderParserTest, TokenizeEmptyTagsReturnsEmptyTokensList) {
  const std::string header_str = "YUV4MPEG2\n";
  std::optional<std::vector<Y4mHeaderToken>> tokens =
      TokenizeY4mHeader(header_str);

  ASSERT_TRUE(tokens.has_value());
  EXPECT_TRUE(tokens->empty());
}

TEST(Y4mHeaderParserTest, RejectsDuplicateWidth) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H480 W1280 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsDuplicateHeight) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H480 H720 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsDuplicateFramerate) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H480 F30:1 F60:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsDuplicateColorSpace) {
  EXPECT_FALSE(
      ParseY4mHeader("YUV4MPEG2 W640 H480 C420 C420jpeg\n").has_value());
}

TEST(Y4mHeaderParserTest, TokenizeInvalidMagicReturnsNullopt) {
  EXPECT_FALSE(TokenizeY4mHeader("NOT_Y4M W10 H10\n").has_value());
}

TEST(Y4mHeaderParserTest, MinimalHeaderWithoutFps) {
  const std::string header_str = "YUV4MPEG2 W1 H1\n";
  std::optional<Y4mHeader> header = ParseY4mHeader(header_str);

  ASSERT_TRUE(header.has_value());
  EXPECT_THAT(*header, AllOf(Field(&Y4mHeader::resolution,
                                   Eq(Resolution{.width = 1, .height = 1})),
                             Field(&Y4mHeader::header_size,
                                   Eq(DataSize::Bytes(header_str.size()))),
                             Field(&Y4mHeader::framerate, Eq(std::nullopt)),
                             Field(&Y4mHeader::color_space, Eq(std::nullopt))));
}

TEST(Y4mHeaderParserTest, MinimalHeaderWithFps) {
  const std::string header_str = "YUV4MPEG2 W2 H2 F2:1\n";
  std::optional<Y4mHeader> header = ParseY4mHeader(header_str);

  ASSERT_TRUE(header.has_value());
  EXPECT_THAT(*header,
              AllOf(Field(&Y4mHeader::resolution,
                          Eq(Resolution{.width = 2, .height = 2})),
                    Field(&Y4mHeader::header_size,
                          Eq(DataSize::Bytes(header_str.size()))),
                    Field(&Y4mHeader::framerate, Eq(Frequency::Hertz(2)))));
}

TEST(Y4mHeaderParserTest, ArbitraryTagOrderingAndRecognizedColorSpace) {
  // Colour space (C420) comes before width and height, interlace and aspect
  // ratio tags included.
  const std::string header_str = "YUV4MPEG2 C420 W640 H360 Ip F30:1 A1:1\n";
  std::optional<Y4mHeader> header = ParseY4mHeader(header_str);

  ASSERT_TRUE(header.has_value());
  EXPECT_THAT(*header,
              AllOf(Field(&Y4mHeader::resolution,
                          Eq(Resolution{.width = 640, .height = 360})),
                    Field(&Y4mHeader::header_size,
                          Eq(DataSize::Bytes(header_str.size()))),
                    Field(&Y4mHeader::framerate, Eq(Frequency::Hertz(30)))));
  ASSERT_TRUE(header->color_space.has_value());
  EXPECT_EQ(header->color_space->chroma_siting_horizontal(),
            ColorSpace::ChromaSiting::kHalf);
  EXPECT_EQ(header->color_space->chroma_siting_vertical(),
            ColorSpace::ChromaSiting::kHalf);
}

TEST(Y4mHeaderParserTest, FractionalFramerate) {
  const std::string header_str = "YUV4MPEG2 W1920 H1080 F30000:1001 C420\n";
  std::optional<Y4mHeader> header = ParseY4mHeader(header_str);

  ASSERT_TRUE(header.has_value());
  // 30000 / 1001 = 29.97002997... Hz = 29970 mHz.
  EXPECT_THAT(
      *header,
      AllOf(Field(&Y4mHeader::resolution,
                  Eq(Resolution{.width = 1920, .height = 1080})),
            Field(&Y4mHeader::framerate, Eq(Frequency::MilliHertz(29970)))));
  // Frequency::hertz() rounds to the nearest integer, so 29.97 Hz rounds to 30.
  EXPECT_EQ(header->framerate->hertz(), 30);
}

TEST(Y4mHeaderParserTest, ColorSpace420JpegRecognized) {
  std::optional<Y4mHeader> header =
      ParseY4mHeader("YUV4MPEG2 W640 H360 C420jpeg\n");
  ASSERT_TRUE(header.has_value());
  ASSERT_TRUE(header->color_space.has_value());
  EXPECT_EQ(header->color_space->chroma_siting_horizontal(),
            ColorSpace::ChromaSiting::kHalf);
  EXPECT_EQ(header->color_space->chroma_siting_vertical(),
            ColorSpace::ChromaSiting::kHalf);
}

TEST(Y4mHeaderParserTest, ColorSpace420Mpeg2Recognized) {
  std::optional<Y4mHeader> header =
      ParseY4mHeader("YUV4MPEG2 W640 H360 C420mpeg2\n");
  ASSERT_TRUE(header.has_value());
  ASSERT_TRUE(header->color_space.has_value());
  EXPECT_EQ(header->color_space->chroma_siting_horizontal(),
            ColorSpace::ChromaSiting::kCollocated);
  EXPECT_EQ(header->color_space->chroma_siting_vertical(),
            ColorSpace::ChromaSiting::kHalf);
}

TEST(Y4mHeaderParserTest, ColorSpace420PalDvRecognized) {
  std::optional<Y4mHeader> header =
      ParseY4mHeader("YUV4MPEG2 W640 H360 C420paldv\n");
  ASSERT_TRUE(header.has_value());
  ASSERT_TRUE(header->color_space.has_value());
  EXPECT_EQ(header->color_space->chroma_siting_horizontal(),
            ColorSpace::ChromaSiting::kCollocated);
  EXPECT_EQ(header->color_space->chroma_siting_vertical(),
            ColorSpace::ChromaSiting::kCollocated);
}

TEST(Y4mHeaderParserTest, RejectsUnknownColorSpaceString) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H360 Cunknown\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsUnsupportedColorSpaceString) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H360 Cbad_space\n").has_value());
}

TEST(Y4mHeaderParserTest, HandlesWindowsCrlf) {
  const std::string header_str = "YUV4MPEG2 W320 H240 F15:1\r\n";
  std::optional<Y4mHeader> header = ParseY4mHeader(header_str);

  ASSERT_TRUE(header.has_value());
  EXPECT_THAT(*header,
              AllOf(Field(&Y4mHeader::resolution,
                          Eq(Resolution{.width = 320, .height = 240})),
                    Field(&Y4mHeader::header_size,
                          Eq(DataSize::Bytes(header_str.size()))),
                    Field(&Y4mHeader::framerate, Eq(Frequency::Hertz(15)))));
}

TEST(Y4mHeaderParserTest, HandlesExtraWhitespace) {
  const std::string header_str = "YUV4MPEG2   W800   H600   F60:1  \n";
  std::optional<Y4mHeader> header = ParseY4mHeader(header_str);

  ASSERT_TRUE(header.has_value());
  EXPECT_THAT(*header,
              AllOf(Field(&Y4mHeader::resolution,
                          Eq(Resolution{.width = 800, .height = 600})),
                    Field(&Y4mHeader::header_size,
                          Eq(DataSize::Bytes(header_str.size()))),
                    Field(&Y4mHeader::framerate, Eq(Frequency::Hertz(60)))));
}

TEST(Y4mHeaderParserTest, HandlesCommentsAndMetadata) {
  const std::string header_str =
      "YUV4MPEG2 W1280 H720 F25:1 Xsource=cam1 Xscene=room\n";
  std::optional<Y4mHeader> header = ParseY4mHeader(header_str);

  ASSERT_TRUE(header.has_value());
  EXPECT_THAT(*header,
              AllOf(Field(&Y4mHeader::resolution,
                          Eq(Resolution{.width = 1280, .height = 720})),
                    Field(&Y4mHeader::header_size,
                          Eq(DataSize::Bytes(header_str.size()))),
                    Field(&Y4mHeader::framerate, Eq(Frequency::Hertz(25)))));
}

TEST(Y4mHeaderParserTest, MaximumHeaderSize) {
  std::string header_str = "YUV4MPEG2 W1280 H720 F30:1 X";
  // Pad with comment data up to exactly kMaxY4mHeaderSizeBytes.
  header_str.append(kMaxY4mHeaderSizeBytes - header_str.size() - 1, 'c');
  header_str.push_back('\n');
  ASSERT_EQ(header_str.size(), kMaxY4mHeaderSizeBytes);

  std::optional<Y4mHeader> header = ParseY4mHeader(header_str);
  ASSERT_TRUE(header.has_value());
  EXPECT_THAT(*header,
              AllOf(Field(&Y4mHeader::resolution,
                          Eq(Resolution{.width = 1280, .height = 720})),
                    Field(&Y4mHeader::header_size,
                          Eq(DataSize::Bytes(kMaxY4mHeaderSizeBytes)))));
}

TEST(Y4mHeaderParserTest, RejectsHeaderExceedingMaximumSize) {
  std::string header_str = "YUV4MPEG2 W1280 H720 F30:1 X";
  header_str.append(kMaxY4mHeaderSizeBytes, 'c');
  header_str.push_back('\n');

  EXPECT_FALSE(ParseY4mHeader(header_str).has_value());
}

TEST(Y4mHeaderParserTest, RejectsMissingNewline) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H480 F30:1").has_value());
}

TEST(Y4mHeaderParserTest, RejectsInvalidMagic) {
  EXPECT_FALSE(ParseY4mHeader("NOTY4M W640 H480 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsMissingHeight) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsMissingWidth) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 H480 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsMissingBothDimensions) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsZeroWidth) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W0 H480 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsNegativeWidth) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W-640 H480 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsZeroHeight) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H0 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsNegativeHeight) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H-480 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsNonNumericWidth) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 Wbad H480 F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsNonNumericHeight) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 Hbad F30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsFramerateMissingColon) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W1920 H1080 F30 C420\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsFramerateTrailingColon) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W1920 H1080 F30: C420\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsFramerateLeadingColon) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W1920 H1080 F:1 C420\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsFramerateNonParsableNumbers) {
  EXPECT_FALSE(
      ParseY4mHeader("YUV4MPEG2 W1920 H1080 Ffoo:bar C420\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsFramerateNonParsableNumerator) {
  EXPECT_FALSE(
      ParseY4mHeader("YUV4MPEG2 W1920 H1080 Ffoo:1 C420\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsFramerateNonParsableDenominator) {
  EXPECT_FALSE(
      ParseY4mHeader("YUV4MPEG2 W1920 H1080 F30:bar C420\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsZeroFramerateNumerator) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H480 F0:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsNegativeFramerateNumerator) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H480 F-30:1\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsZeroFramerateDenominator) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H480 F30:0\n").has_value());
}

TEST(Y4mHeaderParserTest, RejectsNegativeFramerateDenominator) {
  EXPECT_FALSE(ParseY4mHeader("YUV4MPEG2 W640 H480 F30:-1\n").has_value());
}

TEST(Y4mHeaderParserTest, ParseFromFileMinimalFrame) {
  std::string filepath = TempFilename(OutputPath(), "minimal_test.y4m");
  FILE* file = fopen(filepath.c_str(), "wb");
  ASSERT_TRUE(file != nullptr);

  // Minimal 1x1 Y4M frame (1.5 bytes rounded up to 6 bytes in I420).
  const std::string content =
      "YUV4MPEG2 W1 H1 F1:1\n"
      "FRAME\n"
      "YUV123";
  fwrite(content.data(), 1, content.size(), file);
  fclose(file);

  std::optional<Y4mHeader> header = ParseY4mHeaderFromFile(filepath);
  ASSERT_TRUE(header.has_value());
  EXPECT_THAT(*header,
              AllOf(Field(&Y4mHeader::resolution,
                          Eq(Resolution{.width = 1, .height = 1})),
                    Field(&Y4mHeader::framerate, Eq(Frequency::Hertz(1)))));

  remove(filepath.c_str());
}

TEST(Y4mHeaderParserTest, ParseFromFileMaxHeader) {
  std::string filepath = TempFilename(OutputPath(), "max_header_test.y4m");
  FILE* file = fopen(filepath.c_str(), "wb");
  ASSERT_TRUE(file != nullptr);

  std::string content = "YUV4MPEG2 W4 H4 F10:1 X";
  content.append(kMaxY4mHeaderSizeBytes - content.size() - 1, 'x');
  content.push_back('\n');
  content.append("FRAME\n012345678901234567890123");
  fwrite(content.data(), 1, content.size(), file);
  fclose(file);

  std::optional<Y4mHeader> header = ParseY4mHeaderFromFile(filepath);
  ASSERT_TRUE(header.has_value());
  EXPECT_THAT(*header,
              AllOf(Field(&Y4mHeader::resolution,
                          Eq(Resolution{.width = 4, .height = 4})),
                    Field(&Y4mHeader::header_size,
                          Eq(DataSize::Bytes(kMaxY4mHeaderSizeBytes)))));

  remove(filepath.c_str());
}

TEST(Y4mHeaderParserTest, ParseFromNonExistentFileFails) {
  EXPECT_FALSE(ParseY4mHeaderFromFile("/non/existent/file.y4m").has_value());
}

}  // namespace test
}  // namespace webrtc
