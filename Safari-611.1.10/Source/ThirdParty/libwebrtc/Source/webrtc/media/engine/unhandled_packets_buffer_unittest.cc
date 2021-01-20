/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/unhandled_packets_buffer.h"

#include <memory>

#include "absl/memory/memory.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;

namespace {

rtc::CopyOnWriteBuffer Create(int n) {
  return rtc::CopyOnWriteBuffer(std::to_string(n));
}

constexpr int64_t kPacketTimeUs = 122;

}  // namespace

namespace cricket {

TEST(UnhandledPacketsBuffer, NoPackets) {
  UnhandledPacketsBuffer buff;
  buff.AddPacket(2, kPacketTimeUs, Create(3));

  std::vector<uint32_t> ssrcs = {3};
  std::vector<rtc::CopyOnWriteBuffer> packets;
  buff.BackfillPackets(ssrcs, [&packets](uint32_t ssrc, int64_t packet_time_us,
                                         rtc::CopyOnWriteBuffer packet) {
    packets.push_back(packet);
  });
  EXPECT_EQ(0u, packets.size());
}

TEST(UnhandledPacketsBuffer, OnePacket) {
  UnhandledPacketsBuffer buff;
  buff.AddPacket(2, kPacketTimeUs, Create(3));

  std::vector<uint32_t> ssrcs = {2};
  std::vector<rtc::CopyOnWriteBuffer> packets;
  buff.BackfillPackets(ssrcs, [&packets](uint32_t ssrc, int64_t packet_time_us,
                                         rtc::CopyOnWriteBuffer packet) {
    packets.push_back(packet);
  });
  ASSERT_EQ(1u, packets.size());
  EXPECT_EQ(Create(3), packets[0]);
}

TEST(UnhandledPacketsBuffer, TwoPacketsTwoSsrcs) {
  UnhandledPacketsBuffer buff;
  buff.AddPacket(2, kPacketTimeUs, Create(3));
  buff.AddPacket(3, kPacketTimeUs, Create(4));

  std::vector<uint32_t> ssrcs = {2, 3};
  std::vector<rtc::CopyOnWriteBuffer> packets;
  buff.BackfillPackets(ssrcs, [&packets](uint32_t ssrc, int64_t packet_time_us,
                                         rtc::CopyOnWriteBuffer packet) {
    packets.push_back(packet);
  });
  ASSERT_EQ(2u, packets.size());
  EXPECT_EQ(Create(3), packets[0]);
  EXPECT_EQ(Create(4), packets[1]);
}

TEST(UnhandledPacketsBuffer, TwoPacketsTwoSsrcsOneMatch) {
  UnhandledPacketsBuffer buff;
  buff.AddPacket(2, kPacketTimeUs, Create(3));
  buff.AddPacket(3, kPacketTimeUs, Create(4));

  std::vector<uint32_t> ssrcs = {3};
  buff.BackfillPackets(ssrcs, [](uint32_t ssrc, int64_t packet_time_us,
                                 rtc::CopyOnWriteBuffer packet) {
    EXPECT_EQ(ssrc, 3u);
    EXPECT_EQ(Create(4), packet);
  });

  std::vector<uint32_t> ssrcs_again = {2};
  buff.BackfillPackets(ssrcs, [](uint32_t ssrc, int64_t packet_time_us,
                                 rtc::CopyOnWriteBuffer packet) {
    EXPECT_EQ(ssrc, 2u);
    EXPECT_EQ(Create(3), packet);
  });
}

TEST(UnhandledPacketsBuffer, Full) {
  const size_t cnt = UnhandledPacketsBuffer::kMaxStashedPackets;
  UnhandledPacketsBuffer buff;
  for (size_t i = 0; i < cnt; i++) {
    buff.AddPacket(2, kPacketTimeUs, Create(i));
  }

  std::vector<uint32_t> ssrcs = {2};
  std::vector<rtc::CopyOnWriteBuffer> packets;
  buff.BackfillPackets(ssrcs, [&packets](uint32_t ssrc, int64_t packet_time_us,
                                         rtc::CopyOnWriteBuffer packet) {
    packets.push_back(packet);
  });
  ASSERT_EQ(cnt, packets.size());
  for (size_t i = 0; i < cnt; i++) {
    EXPECT_EQ(Create(i), packets[i]);
  }

  // Add a packet after backfill and check that it comes back.
  buff.AddPacket(23, kPacketTimeUs, Create(1001));
  buff.BackfillPackets(ssrcs, [](uint32_t ssrc, int64_t packet_time_us,
                                 rtc::CopyOnWriteBuffer packet) {
    EXPECT_EQ(ssrc, 23u);
    EXPECT_EQ(Create(1001), packet);
  });
}

TEST(UnhandledPacketsBuffer, Wrap) {
  UnhandledPacketsBuffer buff;
  size_t cnt = UnhandledPacketsBuffer::kMaxStashedPackets + 10;
  for (size_t i = 0; i < cnt; i++) {
    buff.AddPacket(2, kPacketTimeUs, Create(i));
  }

  std::vector<uint32_t> ssrcs = {2};
  std::vector<rtc::CopyOnWriteBuffer> packets;
  buff.BackfillPackets(ssrcs, [&packets](uint32_t ssrc, int64_t packet_time_us,
                                         rtc::CopyOnWriteBuffer packet) {
    packets.push_back(packet);
  });
  for (size_t i = 0; i < packets.size(); i++) {
    EXPECT_EQ(Create(i + 10), packets[i]);
  }

  // Add a packet after backfill and check that it comes back.
  buff.AddPacket(23, kPacketTimeUs, Create(1001));
  buff.BackfillPackets(ssrcs, [](uint32_t ssrc, int64_t packet_time_us,
                                 rtc::CopyOnWriteBuffer packet) {
    EXPECT_EQ(ssrc, 23u);
    EXPECT_EQ(Create(1001), packet);
  });
}

TEST(UnhandledPacketsBuffer, Interleaved) {
  UnhandledPacketsBuffer buff;
  buff.AddPacket(2, kPacketTimeUs, Create(2));
  buff.AddPacket(3, kPacketTimeUs, Create(3));

  std::vector<uint32_t> ssrcs = {2};
  buff.BackfillPackets(ssrcs, [](uint32_t ssrc, int64_t packet_time_us,
                                 rtc::CopyOnWriteBuffer packet) {
    EXPECT_EQ(ssrc, 2u);
    EXPECT_EQ(Create(2), packet);
  });

  buff.AddPacket(4, kPacketTimeUs, Create(4));

  ssrcs = {3};
  buff.BackfillPackets(ssrcs, [](uint32_t ssrc, int64_t packet_time_us,
                                 rtc::CopyOnWriteBuffer packet) {
    EXPECT_EQ(ssrc, 3u);
    EXPECT_EQ(Create(3), packet);
  });
}

}  // namespace cricket
