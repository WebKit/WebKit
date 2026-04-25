/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_utils.h"

#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "api/array_view.h"
#include "test/gtest.h"

namespace webrtc {

std::vector<uint8_t> ToVector(ArrayView<const uint8_t> array) {
  return std::vector<uint8_t>(array.begin(), array.end());
}

TEST(PacketStash, Add) {
  PacketStash stash;
  std::vector<uint8_t> packet = {
      0x2f, 0x5b, 0x4c, 0x00, 0x23, 0x47, 0xab, 0xe7, 0x90, 0x96,
      0xc0, 0xac, 0x2f, 0x25, 0x40, 0x35, 0x35, 0xa3, 0x81, 0x50,
      0x0c, 0x38, 0x0a, 0xf6, 0xd4, 0xd5, 0x7d, 0xbe, 0x9a, 0xa3,
      0xcb, 0xcb, 0x67, 0xb0, 0x77, 0x79, 0x8b, 0x48, 0x60, 0xf8,
  };

  stash.Add(packet);
  EXPECT_EQ(stash.size(), 1);
  EXPECT_EQ(ToVector(stash.GetNext()), packet);

  stash.Add(packet);
  EXPECT_EQ(stash.size(), 2);
  EXPECT_EQ(ToVector(stash.GetNext()), packet);
  EXPECT_EQ(ToVector(stash.GetNext()), packet);
}

TEST(PacketStash, AddIfUnique) {
  PacketStash stash;
  std::vector<uint8_t> packet1 = {
      0x2f, 0x5b, 0x4c, 0x00, 0x23, 0x47, 0xab, 0xe7, 0x90, 0x96,
      0xc0, 0xac, 0x2f, 0x25, 0x40, 0x35, 0x35, 0xa3, 0x81, 0x50,
      0x0c, 0x38, 0x0a, 0xf6, 0xd4, 0xd5, 0x7d, 0xbe, 0x9a, 0xa3,
      0xcb, 0xcb, 0x67, 0xb0, 0x77, 0x79, 0x8b, 0x48, 0x60, 0xf8,
  };

  std::vector<uint8_t> packet2 = {
      0x16, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x00,
      0xac, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  stash.AddIfUnique(packet1);
  EXPECT_EQ(stash.size(), 1);
  EXPECT_EQ(ToVector(stash.GetNext()), packet1);

  stash.AddIfUnique(packet1);
  EXPECT_EQ(stash.size(), 1);
  EXPECT_EQ(ToVector(stash.GetNext()), packet1);

  stash.AddIfUnique(packet2);
  EXPECT_EQ(stash.size(), 2);
  EXPECT_EQ(ToVector(stash.GetNext()), packet1);
  EXPECT_EQ(ToVector(stash.GetNext()), packet2);

  stash.AddIfUnique(packet2);
  EXPECT_EQ(stash.size(), 2);
}

TEST(PacketStash, Prune) {
  PacketStash stash;
  std::vector<uint8_t> packet1 = {
      0x2f, 0x5b, 0x4c, 0x00, 0x23, 0x47, 0xab, 0xe7, 0x90, 0x96,
      0xc0, 0xac, 0x2f, 0x25, 0x40, 0x35, 0x35, 0xa3, 0x81, 0x50,
      0x0c, 0x38, 0x0a, 0xf6, 0xd4, 0xd5, 0x7d, 0xbe, 0x9a, 0xa3,
      0xcb, 0xcb, 0x67, 0xb0, 0x77, 0x79, 0x8b, 0x48, 0x60, 0xf8,
  };

  std::vector<uint8_t> packet2 = {
      0x16, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x00,
      0xac, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  stash.AddIfUnique(packet1);
  stash.AddIfUnique(packet2);
  EXPECT_EQ(stash.size(), 2);
  EXPECT_EQ(ToVector(stash.GetNext()), packet1);
  EXPECT_EQ(ToVector(stash.GetNext()), packet2);

  absl::flat_hash_set<uint32_t> remove;
  remove.insert(PacketStash::Hash(packet1));
  stash.Prune(remove);

  EXPECT_EQ(stash.size(), 1);
  EXPECT_EQ(ToVector(stash.GetNext()), packet2);
}

TEST(PacketStash, PruneSize) {
  PacketStash stash;
  std::vector<uint8_t> packet1 = {
      0x2f, 0x5b, 0x4c, 0x00, 0x23, 0x47, 0xab, 0xe7, 0x90, 0x96,
      0xc0, 0xac, 0x2f, 0x25, 0x40, 0x35, 0x35, 0xa3, 0x81, 0x50,
      0x0c, 0x38, 0x0a, 0xf6, 0xd4, 0xd5, 0x7d, 0xbe, 0x9a, 0xa3,
      0xcb, 0xcb, 0x67, 0xb0, 0x77, 0x79, 0x8b, 0x48, 0x60, 0xf8,
  };

  std::vector<uint8_t> packet2 = {
      0x16, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x0c, 0x0e, 0x00, 0x00, 0x00, 0x00,
      0xac, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  std::vector<uint8_t> packet3 = {0x3};
  std::vector<uint8_t> packet4 = {0x4};
  std::vector<uint8_t> packet5 = {0x5};
  std::vector<uint8_t> packet6 = {0x6};

  stash.AddIfUnique(packet1);
  stash.AddIfUnique(packet2);
  stash.AddIfUnique(packet3);
  stash.AddIfUnique(packet4);
  stash.AddIfUnique(packet5);
  stash.AddIfUnique(packet6);
  EXPECT_EQ(stash.size(), 6);
  EXPECT_EQ(ToVector(stash.GetNext()), packet1);
  EXPECT_EQ(ToVector(stash.GetNext()), packet2);
  EXPECT_EQ(ToVector(stash.GetNext()), packet3);
  EXPECT_EQ(ToVector(stash.GetNext()), packet4);
  EXPECT_EQ(ToVector(stash.GetNext()), packet5);
  EXPECT_EQ(ToVector(stash.GetNext()), packet6);

  // Should be NOP.
  stash.Prune(/* max_size= */ 6);
  EXPECT_EQ(ToVector(stash.GetNext()), packet1);
  EXPECT_EQ(ToVector(stash.GetNext()), packet2);
  EXPECT_EQ(ToVector(stash.GetNext()), packet3);
  EXPECT_EQ(ToVector(stash.GetNext()), packet4);
  EXPECT_EQ(ToVector(stash.GetNext()), packet5);
  EXPECT_EQ(ToVector(stash.GetNext()), packet6);

  // Move "cursor" forward.
  EXPECT_EQ(ToVector(stash.GetNext()), packet1);
  stash.Prune(/* max_size= */ 4);
  EXPECT_EQ(stash.size(), 4);
  EXPECT_EQ(ToVector(stash.GetNext()), packet3);
  EXPECT_EQ(ToVector(stash.GetNext()), packet4);
  EXPECT_EQ(ToVector(stash.GetNext()), packet5);
  EXPECT_EQ(ToVector(stash.GetNext()), packet6);
}

TEST(PacketStash, PruneSome) {
  PacketStash stash;
  std::vector<uint8_t> packet1 = {1};
  std::vector<uint8_t> packet2 = {2};
  std::vector<uint8_t> packet3 = {3};

  stash.AddIfUnique(packet1);
  stash.AddIfUnique(packet2);
  stash.AddIfUnique(packet3);
  EXPECT_EQ(stash.size(), 3);

  EXPECT_EQ(ToVector(stash.GetNext()), packet1);
  EXPECT_EQ(ToVector(stash.GetNext()), packet2);
  EXPECT_EQ(ToVector(stash.GetNext()), packet3);
  EXPECT_EQ(ToVector(stash.GetNext()), packet1);

  absl::flat_hash_set<uint32_t> remove;
  remove.insert(PacketStash::Hash(packet1));
  remove.insert(PacketStash::Hash(packet2));
  stash.Prune(remove);

  EXPECT_EQ(ToVector(stash.GetNext()), packet3);
}

}  // namespace webrtc
