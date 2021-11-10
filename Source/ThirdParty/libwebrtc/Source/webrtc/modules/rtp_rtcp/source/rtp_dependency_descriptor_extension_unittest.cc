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

#include "api/array_view.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"

#include "test/gmock.h"

namespace webrtc {
namespace {

using ::testing::Each;

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
  EXPECT_THAT(rtc::MakeArrayView(unused_bytes, num_unused_bytes), Each(0));
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

}  // namespace
}  // namespace webrtc
