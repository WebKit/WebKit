/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/active_decode_targets_helper.h"

#include <vector>

#include "absl/types/optional.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr std::bitset<32> kAll = ~uint32_t{0};
}  // namespace

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsNulloptOnKeyFrameWhenAllDecodeTargetsAreActive) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  ActiveDecodeTargetsHelper helper;
  int chain_diffs[] = {0};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b11,
                 /*is_keyframe=*/true, /*frame_id=*/1, chain_diffs);

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsNulloptOnKeyFrameWhenAllDecodeTargetsAreActiveAfterDeltaFrame) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  ActiveDecodeTargetsHelper helper;
  int chain_diffs_key[] = {0};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b11,
                 /*is_keyframe=*/true, /*frame_id=*/1, chain_diffs_key);
  int chain_diffs_delta[] = {1};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/false, /*frame_id=*/2, chain_diffs_delta);

  ASSERT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b01u);
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b11,
                 /*is_keyframe=*/true, /*frame_id=*/3, chain_diffs_key);

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsBitmaskOnKeyFrameWhenSomeDecodeTargetsAreInactive) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  ActiveDecodeTargetsHelper helper;
  int chain_diffs[] = {0};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/true, /*frame_id=*/1, chain_diffs);

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b01u);
}

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsBitmaskOnKeyFrameWhenSomeDecodeTargetsAreInactiveAfterDeltaFrame) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  ActiveDecodeTargetsHelper helper;
  int chain_diffs_key[] = {0};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/true, /*frame_id=*/1, chain_diffs_key);
  int chain_diffs_delta[] = {1};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/false, /*frame_id=*/2, chain_diffs_delta);

  ASSERT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/true, /*frame_id=*/3, chain_diffs_key);

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b01u);
}

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsNulloptWhenActiveDecodeTargetsAreUnused) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  ActiveDecodeTargetsHelper helper;
  int chain_diffs[] = {0};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kAll,
                 /*is_keyframe=*/true, /*frame_id=*/1, chain_diffs);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);

  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kAll,
                 /*is_keyframe=*/false, /*frame_id=*/2, chain_diffs);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsNulloptOnDeltaFrameAfterSentOnKeyFrame) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  ActiveDecodeTargetsHelper helper;
  int chain_diffs_key[] = {0};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/true, /*frame_id=*/1, chain_diffs_key);
  int chain_diffs_delta[] = {1};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/false, /*frame_id=*/2, chain_diffs_delta);

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

TEST(ActiveDecodeTargetsHelperTest, ReturnsNewBitmaskOnDeltaFrame) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  ActiveDecodeTargetsHelper helper;
  int chain_diffs_key[] = {0};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b11,
                 /*is_keyframe=*/true, /*frame_id=*/1, chain_diffs_key);
  ASSERT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
  int chain_diffs_delta[] = {1};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/false, /*frame_id=*/2, chain_diffs_delta);

  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b01u);
}

TEST(ActiveDecodeTargetsHelperTest,
     ReturnsBitmaskWhenAllDecodeTargetsReactivatedOnDeltaFrame) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 0};
  ActiveDecodeTargetsHelper helper;
  int chain_diffs_key[] = {0};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/true, /*frame_id=*/1, chain_diffs_key);
  ASSERT_NE(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
  int chain_diffs_delta[] = {1};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b01,
                 /*is_keyframe=*/false, /*frame_id=*/2, chain_diffs_delta);
  ASSERT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);

  // Reactive all the decode targets
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kAll,
                 /*is_keyframe=*/false, /*frame_id=*/3, chain_diffs_delta);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b11u);
}

TEST(ActiveDecodeTargetsHelperTest, ReturnsNulloptAfterSentOnAllActiveChains) {
  // Active decode targets (0 and 1) are protected by chains 1 and 2.
  const std::bitset<32> kSome = 0b011;
  constexpr int kDecodeTargetProtectedByChain[] = {2, 1, 0};

  ActiveDecodeTargetsHelper helper;
  int chain_diffs_key[] = {0, 0, 0};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b111,
                 /*is_keyframe=*/true,
                 /*frame_id=*/0, chain_diffs_key);
  ASSERT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);

  int chain_diffs_delta1[] = {1, 1, 1};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false,
                 /*frame_id=*/1, chain_diffs_delta1);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b011u);

  int chain_diffs_delta2[] = {2, 2, 1};  // Previous frame was part of chain#2
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false,
                 /*frame_id=*/2, chain_diffs_delta2);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b011u);

  // active_decode_targets_bitmask was send on chains 1 and 2. It was never sent
  // on chain 0, but chain 0 only protects inactive decode target#2
  int chain_diffs_delta3[] = {3, 1, 2};  // Previous frame was part of chain#1
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/kSome,
                 /*is_keyframe=*/false,
                 /*frame_id=*/3, chain_diffs_delta3);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

TEST(ActiveDecodeTargetsHelperTest, ReturnsBitmaskWhenChanged) {
  constexpr int kDecodeTargetProtectedByChain[] = {0, 1, 1};

  ActiveDecodeTargetsHelper helper;
  int chain_diffs_key[] = {0, 0};
  helper.OnFrame(kDecodeTargetProtectedByChain, /*active_decode_targets=*/0b111,
                 /*is_keyframe=*/true,
                 /*frame_id=*/0, chain_diffs_key);
  int chain_diffs_delta1[] = {1, 1};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b011,
                 /*is_keyframe=*/false,
                 /*frame_id=*/1, chain_diffs_delta1);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b011u);

  int chain_diffs_delta2[] = {1, 2};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b101,
                 /*is_keyframe=*/false,
                 /*frame_id=*/2, chain_diffs_delta2);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b101u);

  // active_decode_target_bitmask was send on chain0, but it was an old one.
  int chain_diffs_delta3[] = {2, 1};
  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b101,
                 /*is_keyframe=*/false,
                 /*frame_id=*/3, chain_diffs_delta3);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), 0b101u);
}

TEST(ActiveDecodeTargetsHelperTest, ReturnsNulloptWhenChainsAreNotUsed) {
  const rtc::ArrayView<const int> kDecodeTargetProtectedByChain;
  const rtc::ArrayView<const int> kNoChainDiffs;

  ActiveDecodeTargetsHelper helper;
  helper.OnFrame(kDecodeTargetProtectedByChain, /*active_decode_targets=*/kAll,
                 /*is_keyframe=*/true,
                 /*frame_id=*/0, kNoChainDiffs);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);

  helper.OnFrame(kDecodeTargetProtectedByChain,
                 /*active_decode_targets=*/0b101,
                 /*is_keyframe=*/false,
                 /*frame_id=*/1, kNoChainDiffs);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
}

TEST(ActiveDecodeTargetsHelperTest, Supports32DecodeTargets) {
  std::bitset<32> some;
  std::vector<int> decode_target_protected_by_chain(32);
  for (int i = 0; i < 32; ++i) {
    decode_target_protected_by_chain[i] = i;
    some[i] = i % 2 == 0;
  }

  ActiveDecodeTargetsHelper helper;
  std::vector<int> chain_diffs_key(32, 0);
  helper.OnFrame(decode_target_protected_by_chain,
                 /*active_decode_targets=*/some,
                 /*is_keyframe=*/true,
                 /*frame_id=*/1, chain_diffs_key);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), some.to_ulong());
  std::vector<int> chain_diffs_delta(32, 1);
  helper.OnFrame(decode_target_protected_by_chain,
                 /*active_decode_targets=*/some,
                 /*is_keyframe=*/false,
                 /*frame_id=*/2, chain_diffs_delta);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), absl::nullopt);
  helper.OnFrame(decode_target_protected_by_chain,
                 /*active_decode_targets=*/kAll,
                 /*is_keyframe=*/false,
                 /*frame_id=*/2, chain_diffs_delta);
  EXPECT_EQ(helper.ActiveDecodeTargetsBitmask(), kAll.to_ulong());
}

}  // namespace webrtc
