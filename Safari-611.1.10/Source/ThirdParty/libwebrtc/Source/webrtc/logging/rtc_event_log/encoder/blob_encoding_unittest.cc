/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/blob_encoding.h"

#include <string>
#include <vector>

#include "logging/rtc_event_log/encoder/var_int.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

using CharT = std::string::value_type;

namespace webrtc {

namespace {

void TestEncodingAndDecoding(const std::vector<std::string>& blobs) {
  RTC_DCHECK(!blobs.empty());

  const std::string encoded = EncodeBlobs(blobs);
  ASSERT_FALSE(encoded.empty());

  const std::vector<absl::string_view> decoded =
      DecodeBlobs(encoded, blobs.size());

  ASSERT_EQ(decoded.size(), blobs.size());
  for (size_t i = 0; i < decoded.size(); ++i) {
    ASSERT_EQ(decoded[i], blobs[i]);
  }
}

void TestGracefulErrorHandling(absl::string_view encoded_blobs,
                               size_t num_of_blobs) {
  const std::vector<absl::string_view> decoded =
      DecodeBlobs(encoded_blobs, num_of_blobs);
  EXPECT_TRUE(decoded.empty());
}

}  // namespace

TEST(BlobEncoding, EmptyBlob) {
  TestEncodingAndDecoding({""});
}

TEST(BlobEncoding, SingleCharacterBlob) {
  TestEncodingAndDecoding({"a"});
}

TEST(BlobEncoding, LongBlob) {
  std::string blob = "";
  for (size_t i = 0; i < 100000; ++i) {
    blob += std::to_string(i + 1) + " Mississippi\n";
  }
  TestEncodingAndDecoding({blob});
}

TEST(BlobEncoding, BlobsOfVariousLengths) {
  constexpr size_t kJump = 0xf032d;  // Arbitrary.
  constexpr size_t kMax = 0xffffff;  // Arbitrary.

  std::string blob;
  blob.reserve(kMax);

  for (size_t i = 0; i < kMax; i += kJump) {
    blob.append(kJump, 'x');
    TestEncodingAndDecoding({blob});
  }
}

TEST(BlobEncoding, MultipleBlobs) {
  std::vector<std::string> blobs;
  for (size_t i = 0; i < 100000; ++i) {
    blobs.push_back(std::to_string(i + 1) + " Mississippi\n");
  }
  TestEncodingAndDecoding(blobs);
}

TEST(BlobEncoding, DecodeBlobsHandlesErrorsGracefullyEmptyInput) {
  TestGracefulErrorHandling("", 1);
}

TEST(BlobEncoding, DecodeBlobsHandlesErrorsGracefullyZeroBlobs) {
  const std::string encoded = EncodeBlobs({"a"});
  ASSERT_FALSE(encoded.empty());
  TestGracefulErrorHandling(encoded, 0);
}

TEST(BlobEncoding, DecodeBlobsHandlesErrorsGracefullyBlobLengthTooSmall) {
  std::string encoded = EncodeBlobs({"ab"});
  ASSERT_FALSE(encoded.empty());
  ASSERT_EQ(encoded[0], 0x02);
  encoded[0] = 0x01;
  TestGracefulErrorHandling(encoded, 1);
}

TEST(BlobEncoding, DecodeBlobsHandlesErrorsGracefullyBlobLengthTooLarge) {
  std::string encoded = EncodeBlobs({"a"});
  ASSERT_FALSE(encoded.empty());
  ASSERT_EQ(encoded[0], 0x01);
  encoded[0] = 0x02;
  TestGracefulErrorHandling(encoded, 1);
}

TEST(BlobEncoding,
     DecodeBlobsHandlesErrorsGracefullyNumberOfBlobsIncorrectlyHigh) {
  const std::vector<std::string> blobs = {"a", "b"};
  const std::string encoded = EncodeBlobs(blobs);
  // Test focus - two empty strings encoded, but DecodeBlobs() told way more
  // blobs are in the strings than could be expected.
  TestGracefulErrorHandling(encoded, 1000);

  // Test sanity - show that DecodeBlobs() would have worked if it got the
  // correct input.
  TestEncodingAndDecoding(blobs);
}

TEST(BlobEncoding, DecodeBlobsHandlesErrorsGracefullyDefectiveVarInt) {
  std::string defective_varint;
  for (size_t i = 0; i < kMaxVarIntLengthBytes; ++i) {
    ASSERT_LE(kMaxVarIntLengthBytes, 0xffu);
    defective_varint += static_cast<CharT>(static_cast<size_t>(0x80u) | i);
  }
  defective_varint += 0x01u;

  const std::string defective_encoded = defective_varint + "whatever";

  TestGracefulErrorHandling(defective_encoded, 1);
}

TEST(BlobEncoding, DecodeBlobsHandlesErrorsGracefullyLengthSumWrapAround) {
  std::string max_size_varint;
  for (size_t i = 0; i < kMaxVarIntLengthBytes - 1; ++i) {
    max_size_varint += 0xffu;
  }
  max_size_varint += 0x7fu;

  const std::string defective_encoded =
      max_size_varint + max_size_varint + "whatever";

  TestGracefulErrorHandling(defective_encoded, 2);
}

}  // namespace webrtc
