/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

constexpr uint8_t kDeprecatedFlags = 0x30;

// TODO(danilchap): Add fuzzer to test for various invalid inputs.

class RtpGenericFrameDescriptorExtensionTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<int> {
 public:
  RtpGenericFrameDescriptorExtensionTest() : version_(GetParam()) {}

  bool Parse(rtc::ArrayView<const uint8_t> data,
             RtpGenericFrameDescriptor* descriptor) const {
    switch (version_) {
      case 0:
        return RtpGenericFrameDescriptorExtension00::Parse(data, descriptor);
      case 1:
        return RtpGenericFrameDescriptorExtension01::Parse(data, descriptor);
    }
    RTC_NOTREACHED();
    return false;
  }

  size_t ValueSize(const RtpGenericFrameDescriptor& descriptor) const {
    switch (version_) {
      case 0:
        return RtpGenericFrameDescriptorExtension00::ValueSize(descriptor);
      case 1:
        return RtpGenericFrameDescriptorExtension01::ValueSize(descriptor);
    }
    RTC_NOTREACHED();
    return 0;
  }

  bool Write(rtc::ArrayView<uint8_t> data,
             const RtpGenericFrameDescriptor& descriptor) const {
    switch (version_) {
      case 0:
        return RtpGenericFrameDescriptorExtension00::Write(data, descriptor);
      case 1:
        return RtpGenericFrameDescriptorExtension01::Write(data, descriptor);
    }
    RTC_NOTREACHED();
    return false;
  }

 protected:
  const int version_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         RtpGenericFrameDescriptorExtensionTest,
                         ::testing::Values(0, 1));

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseFirstPacketOfIndependenSubFrame) {
  const int kTemporalLayer = 5;
  constexpr uint8_t kRaw[] = {0x80 | kTemporalLayer, 0x49, 0x12, 0x34};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));

  EXPECT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_FALSE(descriptor.LastPacketInSubFrame());

  const absl::optional<bool> discardable = descriptor.Discardable();
  if (version_ == 0) {
    ASSERT_FALSE(discardable.has_value());
  } else {
    ASSERT_TRUE(discardable.has_value());
    EXPECT_FALSE(discardable.value());
  }

  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), IsEmpty());
  EXPECT_EQ(descriptor.TemporalLayer(), kTemporalLayer);
  EXPECT_EQ(descriptor.SpatialLayersBitmask(), 0x49);
  EXPECT_EQ(descriptor.FrameId(), 0x3412);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteFirstPacketOfIndependenSubFrame) {
  const int kTemporalLayer = 5;
  uint8_t kRaw[] = {0x80 | kTemporalLayer, 0x49, 0x12, 0x34};
  if (version_ == 0) {
    kRaw[0] |= kDeprecatedFlags;
  }
  RtpGenericFrameDescriptor descriptor;

  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.SetTemporalLayer(kTemporalLayer);
  descriptor.SetSpatialLayersBitmask(0x49);
  descriptor.SetFrameId(0x3412);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));

  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseLastPacketOfSubFrame) {
  constexpr uint8_t kRaw[] = {0x40};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));

  EXPECT_FALSE(descriptor.FirstPacketInSubFrame());

  const absl::optional<bool> discardable = descriptor.Discardable();
  if (version_ == 0) {
    ASSERT_FALSE(discardable.has_value());
  } else {
    ASSERT_TRUE(discardable.has_value());
    EXPECT_FALSE(discardable.value());
  }

  EXPECT_TRUE(descriptor.LastPacketInSubFrame());
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteLastPacketOfSubFrame) {
  uint8_t kRaw[] = {0x40};
  if (version_ == 0) {
    kRaw[0] |= kDeprecatedFlags;
  }

  RtpGenericFrameDescriptor descriptor;
  descriptor.SetLastPacketInSubFrame(true);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseDiscardable) {
  if (version_ == 0) {
    return;
  }

  constexpr uint8_t kRaw[] = {0x10};
  RtpGenericFrameDescriptor descriptor;
  ASSERT_TRUE(Parse(kRaw, &descriptor));
  const absl::optional<bool> discardable = descriptor.Discardable();
  ASSERT_TRUE(discardable.has_value());
  EXPECT_TRUE(discardable.value());
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteDiscardable) {
  if (version_ == 0) {
    return;
  }

  constexpr uint8_t kRaw[] = {0x10};
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetDiscardable(true);
  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMinShortFrameDependencies) {
  constexpr uint16_t kDiff = 1;
  constexpr uint8_t kRaw[] = {0x88, 0x01, 0x00, 0x00, 0x04};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMinShortFrameDependencies) {
  constexpr uint16_t kDiff = 1;
  uint8_t kRaw[] = {0x88, 0x01, 0x00, 0x00, 0x04};
  if (version_ == 0) {
    kRaw[0] |= kDeprecatedFlags;
  }
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMaxShortFrameDependencies) {
  constexpr uint16_t kDiff = 0x3f;
  constexpr uint8_t kRaw[] = {0xb8, 0x01, 0x00, 0x00, 0xfc};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMaxShortFrameDependencies) {
  constexpr uint16_t kDiff = 0x3f;
  uint8_t kRaw[] = {0x88, 0x01, 0x00, 0x00, 0xfc};
  if (version_ == 0) {
    kRaw[0] |= kDeprecatedFlags;
  }
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMinLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x40;
  constexpr uint8_t kRaw[] = {0xb8, 0x01, 0x00, 0x00, 0x02, 0x01};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMinLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x40;
  uint8_t kRaw[] = {0x88, 0x01, 0x00, 0x00, 0x02, 0x01};
  if (version_ == 0) {
    kRaw[0] |= kDeprecatedFlags;
  }
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseLongFrameDependenciesAsBigEndian) {
  constexpr uint16_t kDiff = 0x7654 >> 2;
  constexpr uint8_t kRaw[] = {0xb8, 0x01, 0x00, 0x00, 0x54 | 0x02, 0x76};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteLongFrameDependenciesAsBigEndian) {
  constexpr uint16_t kDiff = 0x7654 >> 2;
  uint8_t kRaw[] = {0x88, 0x01, 0x00, 0x00, 0x54 | 0x02, 0x76};
  if (version_ == 0) {
    kRaw[0] |= kDeprecatedFlags;
  }
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseMaxLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x3fff;
  constexpr uint8_t kRaw[] = {0xb8, 0x01, 0x00, 0x00, 0xfe, 0xff};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteMaxLongFrameDependencies) {
  constexpr uint16_t kDiff = 0x3fff;
  uint8_t kRaw[] = {0x88, 0x01, 0x00, 0x00, 0xfe, 0xff};
  if (version_ == 0) {
    kRaw[0] |= kDeprecatedFlags;
  }
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, ParseTwoFrameDependencies) {
  constexpr uint16_t kDiff1 = 9;
  constexpr uint16_t kDiff2 = 15;
  constexpr uint8_t kRaw[] = {
      0xb8, 0x01, 0x00, 0x00, (kDiff1 << 2) | 0x01, kDiff2 << 2};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  ASSERT_TRUE(descriptor.FirstPacketInSubFrame());
  EXPECT_THAT(descriptor.FrameDependenciesDiffs(), ElementsAre(kDiff1, kDiff2));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest, WriteTwoFrameDependencies) {
  constexpr uint16_t kDiff1 = 9;
  constexpr uint16_t kDiff2 = 15;
  uint8_t kRaw[] = {0x88, 0x01, 0x00, 0x00, (kDiff1 << 2) | 0x01, kDiff2 << 2};
  if (version_ == 0) {
    kRaw[0] |= kDeprecatedFlags;
  }
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.AddFrameDependencyDiff(kDiff1);
  descriptor.AddFrameDependencyDiff(kDiff2);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       ParseResolutionOnIndependentFrame) {
  constexpr int kWidth = 0x2468;
  constexpr int kHeight = 0x6543;
  constexpr uint8_t kRaw[] = {0xb0, 0x01, 0x00, 0x00, 0x24, 0x68, 0x65, 0x43};
  RtpGenericFrameDescriptor descriptor;

  ASSERT_TRUE(Parse(kRaw, &descriptor));
  EXPECT_EQ(descriptor.Width(), kWidth);
  EXPECT_EQ(descriptor.Height(), kHeight);
}

TEST_P(RtpGenericFrameDescriptorExtensionTest,
       WriteResolutionOnIndependentFrame) {
  constexpr int kWidth = 0x2468;
  constexpr int kHeight = 0x6543;
  uint8_t kRaw[] = {0x80, 0x01, 0x00, 0x00, 0x24, 0x68, 0x65, 0x43};
  if (version_ == 0) {
    kRaw[0] |= kDeprecatedFlags;
  }
  RtpGenericFrameDescriptor descriptor;
  descriptor.SetFirstPacketInSubFrame(true);
  descriptor.SetResolution(kWidth, kHeight);

  ASSERT_EQ(ValueSize(descriptor), sizeof(kRaw));
  uint8_t buffer[sizeof(kRaw)];
  EXPECT_TRUE(Write(buffer, descriptor));
  EXPECT_THAT(buffer, ElementsAreArray(kRaw));
}
}  // namespace
}  // namespace webrtc
