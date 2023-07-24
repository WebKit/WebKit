/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/optional_blob_encoding.h"

#include <string>
#include <vector>

#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace webrtc {
namespace {

class BitBuilder {
 public:
  BitBuilder& Bit(uint8_t bit) {
    if (total_bits_ % 8 == 0) {
      bits_.push_back(0);
    }
    bits_[total_bits_ / 8] |= bit << (7 - (total_bits_ % 8));
    ++total_bits_;
    return *this;
  }

  BitBuilder& Bytes(const std::vector<uint8_t>& bytes) {
    for (uint8_t byte : bytes) {
      for (int i = 1; i <= 8; ++i) {
        uint8_t bit = (byte >> (8 - i)) & 1;
        Bit(bit);
      }
    }
    return *this;
  }

  BitBuilder& ByteAlign() {
    while (total_bits_ % 8 > 0) {
      Bit(0);
    }
    return *this;
  }

  std::string AsString() { return std::string(bits_.begin(), bits_.end()); }

 private:
  std::vector<uint8_t> bits_;
  uint64_t total_bits_ = 0;
};

TEST(OptionalBlobEncoding, AllBlobsPresent) {
  std::string encoded = EncodeOptionalBlobs({"a", "b", "c"});
  std::string expected = BitBuilder()
                             .Bit(1)
                             .ByteAlign()
                             .Bytes({0x01, 'a'})
                             .Bytes({0x01, 'b'})
                             .Bytes({0x01, 'c'})
                             .AsString();
  EXPECT_EQ(encoded, expected);
}

TEST(OptionalBlobEncoding, SomeBlobsPresent) {
  std::string encoded = EncodeOptionalBlobs({"a", absl::nullopt, "c"});
  std::string expected = BitBuilder()
                             .Bit(0)
                             .Bit(1)
                             .Bit(0)
                             .Bit(1)
                             .ByteAlign()
                             .Bytes({0x01, 'a'})
                             .Bytes({0x01, 'c'})
                             .AsString();
  EXPECT_EQ(encoded, expected);
}

TEST(OptionalBlobEncoding, NoBlobsPresent) {
  std::string encoded =
      EncodeOptionalBlobs({absl::nullopt, absl::nullopt, absl::nullopt});
  EXPECT_THAT(encoded, IsEmpty());
}

TEST(OptionalBlobEncoding, EmptyBlobsPresent) {
  std::string encoded = EncodeOptionalBlobs({absl::nullopt, "", absl::nullopt});
  std::string expected = BitBuilder()
                             .Bit(0)
                             .Bit(0)
                             .Bit(1)
                             .Bit(0)
                             .ByteAlign()
                             .Bytes({0x0})
                             .AsString();
  EXPECT_EQ(encoded, expected);
}

TEST(OptionalBlobEncoding, ZeroBlobs) {
  std::string encoded = EncodeOptionalBlobs({});
  EXPECT_EQ(encoded, std::string());
}

TEST(OptionalBlobEncoding, LongBlobs) {
  std::string medium_string(100, 'a');
  std::string long_string(200, 'b');
  std::string encoded = EncodeOptionalBlobs({medium_string, long_string});
  std::string expected =
      BitBuilder()
          .Bit(1)
          .ByteAlign()
          .Bytes({0x64})
          .Bytes({medium_string.begin(), medium_string.end()})
          .Bytes({0xC8, 0x01})
          .Bytes({long_string.begin(), long_string.end()})
          .AsString();
  EXPECT_EQ(encoded, expected);
}

TEST(OptionalBlobDecoding, AllBlobsPresent) {
  std::string encoded = BitBuilder()
                            .Bit(1)
                            .ByteAlign()
                            .Bytes({0x01, 'a'})
                            .Bytes({0x01, 'b'})
                            .Bytes({0x01, 'c'})
                            .AsString();
  auto decoded = DecodeOptionalBlobs(encoded, 3);
  EXPECT_THAT(decoded, ElementsAre("a", "b", "c"));
}

TEST(OptionalBlobDecoding, SomeBlobsPresent) {
  std::string encoded = BitBuilder()
                            .Bit(0)
                            .Bit(1)
                            .Bit(0)
                            .Bit(1)
                            .ByteAlign()
                            .Bytes({0x01, 'a'})
                            .Bytes({0x01, 'c'})
                            .AsString();
  auto decoded = DecodeOptionalBlobs(encoded, 3);
  EXPECT_THAT(decoded, ElementsAre("a", absl::nullopt, "c"));
}

TEST(OptionalBlobDecoding, NoBlobsPresent) {
  auto decoded = DecodeOptionalBlobs("", 3);
  EXPECT_THAT(decoded,
              ElementsAre(absl::nullopt, absl::nullopt, absl::nullopt));
}

TEST(OptionalBlobDecoding, EmptyBlobsPresent) {
  std::string encoded = BitBuilder()
                            .Bit(0)
                            .Bit(0)
                            .Bit(1)
                            .Bit(0)
                            .ByteAlign()
                            .Bytes({0x0})
                            .AsString();
  auto decoded = DecodeOptionalBlobs(encoded, 3);
  EXPECT_THAT(decoded, ElementsAre(absl::nullopt, "", absl::nullopt));
}

TEST(OptionalBlobDecoding, ZeroBlobs) {
  std::string encoded;
  auto decoded = DecodeOptionalBlobs(encoded, 0);
  EXPECT_THAT(decoded, IsEmpty());
}

TEST(OptionalBlobDecoding, LongBlobs) {
  std::string medium_string(100, 'a');
  std::string long_string(200, 'b');
  std::string encoded = BitBuilder()
                            .Bit(1)
                            .ByteAlign()
                            .Bytes({0x64})
                            .Bytes({medium_string.begin(), medium_string.end()})
                            .Bytes({0xC8, 0x01})
                            .Bytes({long_string.begin(), long_string.end()})
                            .AsString();
  auto decoded = DecodeOptionalBlobs(encoded, 2);
  EXPECT_THAT(decoded, ElementsAre(medium_string, long_string));
}

TEST(OptionalBlobDecoding, TooShortEncodedBlobLength) {
  std::string encoded =
      BitBuilder().Bit(1).ByteAlign().Bytes({0x01, 'a', 'b'}).AsString();
  auto decoded = DecodeOptionalBlobs(encoded, 1);
  EXPECT_THAT(decoded, IsEmpty());
}

TEST(OptionalBlobDecoding, TooLongEncodedBlobLength) {
  std::string encoded =
      BitBuilder().Bit(1).ByteAlign().Bytes({0x03, 'a', 'b'}).AsString();
  auto decoded = DecodeOptionalBlobs(encoded, 1);
  EXPECT_THAT(decoded, IsEmpty());
}

TEST(OptionalBlobDecoding, TooLongEncodedBufferLength) {
  std::string encoded = BitBuilder().Bytes({0x00, 0x00, 0x00}).AsString();
  auto decoded = DecodeOptionalBlobs(encoded, 8);
  EXPECT_THAT(decoded, IsEmpty());
}

}  // namespace
}  // namespace webrtc
