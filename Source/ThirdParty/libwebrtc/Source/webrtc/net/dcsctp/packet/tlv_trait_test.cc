/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/packet/tlv_trait.h"

#include <vector>

#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::ElementsAre;
using ::testing::SizeIs;

struct OneByteTypeConfig {
  static constexpr int kTypeSizeInBytes = 1;
  static constexpr int kType = 0x49;
  static constexpr size_t kHeaderSize = 12;
  static constexpr int kVariableLengthAlignment = 4;
};

class OneByteChunk : public TLVTrait<OneByteTypeConfig> {
 public:
  static constexpr size_t kVariableSize = 4;

  void SerializeTo(std::vector<uint8_t>& out) {
    BoundedByteWriter<OneByteTypeConfig::kHeaderSize> writer =
        AllocateTLV(out, kVariableSize);
    writer.Store32<4>(0x01020304);
    writer.Store16<8>(0x0506);
    writer.Store16<10>(0x0708);

    uint8_t variable_data[kVariableSize] = {0xDE, 0xAD, 0xBE, 0xEF};
    writer.CopyToVariableData(rtc::ArrayView<const uint8_t>(variable_data));
  }

  static absl::optional<BoundedByteReader<OneByteTypeConfig::kHeaderSize>>
  Parse(rtc::ArrayView<const uint8_t> data) {
    return ParseTLV(data);
  }
};

TEST(TlvDataTest, CanWriteOneByteTypeTlvs) {
  std::vector<uint8_t> out;
  OneByteChunk().SerializeTo(out);

  EXPECT_THAT(out, SizeIs(OneByteTypeConfig::kHeaderSize +
                          OneByteChunk::kVariableSize));
  EXPECT_THAT(out, ElementsAre(0x49, 0x00, 0x00, 0x10, 0x01, 0x02, 0x03, 0x04,
                               0x05, 0x06, 0x07, 0x08, 0xDE, 0xAD, 0xBE, 0xEF));
}

TEST(TlvDataTest, CanReadOneByteTypeTlvs) {
  uint8_t data[] = {0x49, 0x00, 0x00, 0x10, 0x01, 0x02, 0x03, 0x04,
                    0x05, 0x06, 0x07, 0x08, 0xDE, 0xAD, 0xBE, 0xEF};

  absl::optional<BoundedByteReader<OneByteTypeConfig::kHeaderSize>> reader =
      OneByteChunk::Parse(data);
  ASSERT_TRUE(reader.has_value());
  EXPECT_EQ(reader->Load32<4>(), 0x01020304U);
  EXPECT_EQ(reader->Load16<8>(), 0x0506U);
  EXPECT_EQ(reader->Load16<10>(), 0x0708U);
  EXPECT_THAT(reader->variable_data(), ElementsAre(0xDE, 0xAD, 0xBE, 0xEF));
}

struct TwoByteTypeConfig {
  static constexpr int kTypeSizeInBytes = 2;
  static constexpr int kType = 31337;
  static constexpr size_t kHeaderSize = 8;
  static constexpr int kVariableLengthAlignment = 2;
};

class TwoByteChunk : public TLVTrait<TwoByteTypeConfig> {
 public:
  static constexpr size_t kVariableSize = 8;

  void SerializeTo(std::vector<uint8_t>& out) {
    BoundedByteWriter<TwoByteTypeConfig::kHeaderSize> writer =
        AllocateTLV(out, kVariableSize);
    writer.Store32<4>(0x01020304U);

    uint8_t variable_data[] = {0x05, 0x06, 0x07, 0x08, 0xDE, 0xAD, 0xBE, 0xEF};
    writer.CopyToVariableData(rtc::ArrayView<const uint8_t>(variable_data));
  }

  static absl::optional<BoundedByteReader<TwoByteTypeConfig::kHeaderSize>>
  Parse(rtc::ArrayView<const uint8_t> data) {
    return ParseTLV(data);
  }
};

TEST(TlvDataTest, CanWriteTwoByteTypeTlvs) {
  std::vector<uint8_t> out;

  TwoByteChunk().SerializeTo(out);

  EXPECT_THAT(out, SizeIs(TwoByteTypeConfig::kHeaderSize +
                          TwoByteChunk::kVariableSize));
  EXPECT_THAT(out, ElementsAre(0x7A, 0x69, 0x00, 0x10, 0x01, 0x02, 0x03, 0x04,
                               0x05, 0x06, 0x07, 0x08, 0xDE, 0xAD, 0xBE, 0xEF));
}

TEST(TlvDataTest, CanReadTwoByteTypeTlvs) {
  uint8_t data[] = {0x7A, 0x69, 0x00, 0x10, 0x01, 0x02, 0x03, 0x04,
                    0x05, 0x06, 0x07, 0x08, 0xDE, 0xAD, 0xBE, 0xEF};

  absl::optional<BoundedByteReader<TwoByteTypeConfig::kHeaderSize>> reader =
      TwoByteChunk::Parse(data);
  EXPECT_TRUE(reader.has_value());
  EXPECT_EQ(reader->Load32<4>(), 0x01020304U);
  EXPECT_THAT(reader->variable_data(),
              ElementsAre(0x05, 0x06, 0x07, 0x08, 0xDE, 0xAD, 0xBE, 0xEF));
}

TEST(TlvDataTest, CanHandleInvalidLengthSmallerThanFixedSize) {
  // Has 'length=6', which is below the kHeaderSize of 8.
  uint8_t data[] = {0x7A, 0x69, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04};

  EXPECT_FALSE(TwoByteChunk::Parse(data).has_value());
}

}  // namespace
}  // namespace dcsctp
