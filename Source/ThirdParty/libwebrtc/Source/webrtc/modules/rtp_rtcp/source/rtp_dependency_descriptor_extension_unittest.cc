/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "api/transport/rtp/dependency_descriptor.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Each;
using ::testing::ElementsAre;

TEST(RtpDependencyDescriptorExtensionTest, Writer3BytesForPerfectTemplate) {
  uint8_t buffer[3];
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.templates = {
      FrameDependencyTemplate().Dtis("SR").FrameDiffs({1}).ChainDiffs({2, 2})};
  DependencyDescriptor descriptor;
  descriptor.frame_dependencies = structure.templates[0];

  EXPECT_EQ(RtpDependencyDescriptorExtension::ValueSize(structure, descriptor),
            3u);
  EXPECT_TRUE(
      RtpDependencyDescriptorExtension::Write(buffer, structure, descriptor));
}

TEST(RtpDependencyDescriptorExtensionTest, WriteZeroInUnusedBits) {
  uint8_t buffer[32];
  std::memset(buffer, 0xff, sizeof(buffer));
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.templates = {
      FrameDependencyTemplate().Dtis("SR").FrameDiffs({1}).ChainDiffs({1, 1})};
  DependencyDescriptor descriptor;
  descriptor.frame_dependencies = structure.templates[0];
  descriptor.frame_dependencies.frame_diffs = {2};

  // To test unused bytes are zeroed, need a buffer large enough.
  size_t value_size =
      RtpDependencyDescriptorExtension::ValueSize(structure, descriptor);
  ASSERT_LT(value_size, sizeof(buffer));

  ASSERT_TRUE(
      RtpDependencyDescriptorExtension::Write(buffer, structure, descriptor));

  const uint8_t* unused_bytes = buffer + value_size;
  size_t num_unused_bytes = buffer + sizeof(buffer) - unused_bytes;
  // Check remaining bytes are zeroed.
  EXPECT_THAT(std::span(unused_bytes, num_unused_bytes), Each(0));
}

// In practice chain diff for inactive chain will grow uboundly because no
// frames are produced for it, that shouldn't block writing the extension.
TEST(RtpDependencyDescriptorExtensionTest,
     TemplateMatchingSkipsInactiveChains) {
  uint8_t buffer[3];
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.templates = {
      FrameDependencyTemplate().Dtis("SR").ChainDiffs({2, 2})};
  DependencyDescriptor descriptor;
  descriptor.frame_dependencies = structure.templates[0];

  // Set only 1st chain as active.
  std::bitset<32> active_chains = 0b01;
  descriptor.frame_dependencies.chain_diffs[1] = 1000;

  // Expect perfect template match since the only difference is for an inactive
  // chain. Pefect template match consumes 3 bytes.
  EXPECT_EQ(RtpDependencyDescriptorExtension::ValueSize(
                structure, active_chains, descriptor),
            3u);
  EXPECT_TRUE(RtpDependencyDescriptorExtension::Write(
      buffer, structure, active_chains, descriptor));
}

TEST(RtpDependencyDescriptorExtensionTest,
     AcceptsInvalidChainDiffForInactiveChainWhenChainsAreCustom) {
  uint8_t buffer[256];
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.templates = {
      FrameDependencyTemplate().Dtis("SR").ChainDiffs({2, 2})};
  DependencyDescriptor descriptor;
  descriptor.frame_dependencies = structure.templates[0];

  // Set only 1st chain as active.
  std::bitset<32> active_chains = 0b01;
  // Set chain_diff different to the template to make it custom.
  descriptor.frame_dependencies.chain_diffs[0] = 1;
  // Set chain diff for inactive chain beyound limit of 255 max chain diff.
  descriptor.frame_dependencies.chain_diffs[1] = 1000;

  // Because chains are custom, should use more than base 3 bytes.
  EXPECT_GT(RtpDependencyDescriptorExtension::ValueSize(
                structure, active_chains, descriptor),
            3u);
  EXPECT_TRUE(RtpDependencyDescriptorExtension::Write(
      buffer, structure, active_chains, descriptor));
}

TEST(RtpDependencyDescriptorExtensionTest, FailsToWriteInvalidDescriptor) {
  uint8_t buffer[256];
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.templates = {
      FrameDependencyTemplate().T(0).Dtis("SR").ChainDiffs({2, 2})};
  DependencyDescriptor descriptor;
  descriptor.frame_dependencies = structure.templates[0];
  descriptor.frame_dependencies.temporal_id = 1;

  EXPECT_EQ(
      RtpDependencyDescriptorExtension::ValueSize(structure, 0b11, descriptor),
      0u);
  EXPECT_FALSE(RtpDependencyDescriptorExtension::Write(buffer, structure, 0b11,
                                                       descriptor));
}

TEST(RtpDependencyDescriptorExtensionTest,
     FailsToWriteWhenNumberOfChainsMismatch) {
  uint8_t buffer[256];
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.templates = {
      FrameDependencyTemplate().T(0).Dtis("SR").ChainDiffs({2, 2})};
  DependencyDescriptor descriptor;
  descriptor.frame_dependencies = structure.templates[0];

  // Structure has 2 chains, but frame provide 1 chain diff,
  descriptor.frame_dependencies.chain_diffs = {2};

  EXPECT_EQ(
      RtpDependencyDescriptorExtension::ValueSize(structure, 0b11, descriptor),
      0u);
  EXPECT_FALSE(RtpDependencyDescriptorExtension::Write(buffer, structure, 0b11,
                                                       descriptor));
}

TEST(RtpDependencyDescriptorExtensionTest,
     FailsToWriteWhenNumberOfDecodeTargetsMismatch) {
  uint8_t buffer[256];
  FrameDependencyStructure structure;
  structure.num_decode_targets = 2;
  structure.num_chains = 2;
  structure.templates = {
      FrameDependencyTemplate().T(0).Dtis("SR").ChainDiffs({2, 2})};
  DependencyDescriptor descriptor;
  descriptor.frame_dependencies = structure.templates[0];

  // Structure has 2 decode targets, but frame provide 1 indication,
  descriptor.frame_dependencies.decode_target_indications = {
      DecodeTargetIndication::kSwitch};

  EXPECT_EQ(
      RtpDependencyDescriptorExtension::ValueSize(structure, 0b11, descriptor),
      0u);
  EXPECT_FALSE(RtpDependencyDescriptorExtension::Write(buffer, structure, 0b11,
                                                       descriptor));
}

// The largest frame diff the dependency descriptor can represent is 4096.
TEST(RtpDependencyDescriptorExtensionTest, FailsToWriteTooLargeFrameDiff) {
  uint8_t buffer[256];
  FrameDependencyStructure structure;
  structure.num_decode_targets = 1;
  structure.templates = {FrameDependencyTemplate().Dtis("S")};
  DependencyDescriptor descriptor;
  descriptor.frame_dependencies = structure.templates[0];
  descriptor.frame_dependencies.frame_diffs = {5275};

  EXPECT_EQ(RtpDependencyDescriptorExtension::ValueSize(structure, descriptor),
            0u);
  EXPECT_FALSE(
      RtpDependencyDescriptorExtension::Write(buffer, structure, descriptor));
}

TEST(RtpDependencyDescriptorExtensionTest, FailsToWriteNonPositiveFrameDiff) {
  uint8_t buffer[256];
  FrameDependencyStructure structure;
  structure.num_decode_targets = 1;
  structure.templates = {FrameDependencyTemplate().Dtis("S")};
  for (int fdiff : {0, -1}) {
    SCOPED_TRACE(fdiff);
    DependencyDescriptor descriptor;
    descriptor.frame_dependencies = structure.templates[0];
    descriptor.frame_dependencies.frame_diffs = {fdiff};

    EXPECT_EQ(
        RtpDependencyDescriptorExtension::ValueSize(structure, descriptor), 0u);
    EXPECT_FALSE(
        RtpDependencyDescriptorExtension::Write(buffer, structure, descriptor));
  }
}

// Chain diffs of active chains must be in the range [0, 255].
TEST(RtpDependencyDescriptorExtensionTest,
     FailsToWriteInvalidChainDiffForActiveChain) {
  uint8_t buffer[256];
  FrameDependencyStructure structure;
  structure.num_decode_targets = 1;
  structure.num_chains = 1;
  structure.decode_target_protected_by_chain = {0};
  structure.templates = {FrameDependencyTemplate().Dtis("S").ChainDiffs({1})};
  for (int chain_diff : {-1, 256}) {
    SCOPED_TRACE(chain_diff);
    DependencyDescriptor descriptor;
    descriptor.frame_dependencies = structure.templates[0];
    descriptor.frame_dependencies.chain_diffs = {chain_diff};

    EXPECT_EQ(
        RtpDependencyDescriptorExtension::ValueSize(structure, descriptor), 0u);
    EXPECT_FALSE(
        RtpDependencyDescriptorExtension::Write(buffer, structure, descriptor));
  }
}

TEST(RtpDependencyDescriptorExtensionTest, RoundTripsFrameDiffsUpToLimit) {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 1;
  structure.templates = {FrameDependencyTemplate().Dtis("S")};
  // Boundaries of the 4, 8 and 12 bit representations of a frame diff.
  for (int fdiff : {1, 16, 17, 256, 257, 4096}) {
    SCOPED_TRACE(fdiff);
    DependencyDescriptor descriptor;
    descriptor.frame_dependencies = structure.templates[0];
    descriptor.frame_dependencies.frame_diffs = {fdiff};
    uint8_t buffer[16];
    size_t value_size =
        RtpDependencyDescriptorExtension::ValueSize(structure, descriptor);
    ASSERT_GT(value_size, 0u);
    ASSERT_LE(value_size, sizeof(buffer));
    std::span<uint8_t> data = std::span(buffer).first(value_size);

    ASSERT_TRUE(
        RtpDependencyDescriptorExtension::Write(data, structure, descriptor));
    DependencyDescriptor parsed;
    ASSERT_TRUE(
        RtpDependencyDescriptorExtension::Parse(data, &structure, &parsed));
    EXPECT_THAT(parsed.frame_dependencies.frame_diffs, ElementsAre(fdiff));
  }
}

TEST(RtpDependencyDescriptorExtensionTest, RoundTripsChainDiffUpToLimit) {
  FrameDependencyStructure structure;
  structure.num_decode_targets = 1;
  structure.num_chains = 1;
  structure.decode_target_protected_by_chain = {0};
  structure.templates = {FrameDependencyTemplate().Dtis("S").ChainDiffs({1})};
  DependencyDescriptor descriptor;
  descriptor.frame_dependencies = structure.templates[0];
  descriptor.frame_dependencies.chain_diffs = {255};
  uint8_t buffer[16];
  size_t value_size =
      RtpDependencyDescriptorExtension::ValueSize(structure, descriptor);
  ASSERT_GT(value_size, 0u);
  ASSERT_LE(value_size, sizeof(buffer));
  std::span<uint8_t> data = std::span(buffer).first(value_size);

  ASSERT_TRUE(
      RtpDependencyDescriptorExtension::Write(data, structure, descriptor));
  DependencyDescriptor parsed;
  ASSERT_TRUE(
      RtpDependencyDescriptorExtension::Parse(data, &structure, &parsed));
  EXPECT_THAT(parsed.frame_dependencies.chain_diffs, ElementsAre(255));
}

}  // namespace
}  // namespace webrtc
