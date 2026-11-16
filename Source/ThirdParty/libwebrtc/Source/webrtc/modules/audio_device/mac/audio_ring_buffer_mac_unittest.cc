/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/mac/audio_ring_buffer_mac.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>
#include <vector>

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(AudioRingBufferMacTest, EmptyBuffer) {
  AudioRingBufferMac<int16_t> ring_buffer(100);
  EXPECT_EQ(ring_buffer.capacity(), 100u);
  EXPECT_EQ(ring_buffer.AvailableToRead(), 0u);
  EXPECT_EQ(ring_buffer.AvailableToWrite(), 100u);

  int16_t out[10] = {0};
  EXPECT_EQ(ring_buffer.Read(out), 0u);
}

TEST(AudioRingBufferMacTest, WriteAndRead) {
  AudioRingBufferMac<int16_t> ring_buffer(10);
  const std::vector<int16_t> in = {1, 2, 3, 4, 5};

  EXPECT_EQ(ring_buffer.Write(in), 5u);
  EXPECT_EQ(ring_buffer.AvailableToRead(), 5u);
  EXPECT_EQ(ring_buffer.AvailableToWrite(), 5u);

  std::vector<int16_t> out(5, 0);
  EXPECT_EQ(ring_buffer.Read(out), 5u);
  EXPECT_EQ(in, out);
  EXPECT_EQ(ring_buffer.AvailableToRead(), 0u);
  EXPECT_EQ(ring_buffer.AvailableToWrite(), 10u);
}

TEST(AudioRingBufferMacTest, BufferFull) {
  AudioRingBufferMac<int16_t> ring_buffer(4);
  const std::vector<int16_t> in = {10, 20, 30, 40, 50};

  // Writing 5 elements to a buffer of capacity 4 should write 4.
  EXPECT_EQ(ring_buffer.Write(in), 4u);
  EXPECT_EQ(ring_buffer.AvailableToRead(), 4u);
  EXPECT_EQ(ring_buffer.AvailableToWrite(), 0u);

  // Subsequent write when full should return 0.
  std::vector<int16_t> extra = {99};
  EXPECT_EQ(ring_buffer.Write(extra), 0u);

  std::vector<int16_t> out(4);
  EXPECT_EQ(ring_buffer.Read(out), 4u);
  EXPECT_EQ(out, (std::vector<int16_t>{10, 20, 30, 40}));
}

TEST(AudioRingBufferMacTest, WrapAround) {
  AudioRingBufferMac<int16_t> ring_buffer(5);

  // Fill and consume partial buffer to shift write+read index.
  const std::vector<int16_t> init = {1, 2, 3};
  EXPECT_EQ(ring_buffer.Write(init), 3u);
  std::vector<int16_t> temp(3);
  EXPECT_EQ(ring_buffer.Read(temp), 3u);
  EXPECT_EQ(ring_buffer.AvailableToRead(), 0u);
  EXPECT_EQ(ring_buffer.AvailableToWrite(), 5u);

  // Now write 4 elements: this will wrap around the internal physical boundary.
  const std::vector<int16_t> in = {10, 20, 30, 40};
  EXPECT_EQ(ring_buffer.Write(in), 4u);
  EXPECT_EQ(ring_buffer.AvailableToRead(), 4u);

  std::vector<int16_t> out(4);
  EXPECT_EQ(ring_buffer.Read(out), 4u);
  EXPECT_EQ(in, out);
}

TEST(AudioRingBufferMacTest, Clear) {
  AudioRingBufferMac<float> ring_buffer(8);
  const std::vector<float> in = {1.0f, 2.0f, 3.0f, 4.0f};
  EXPECT_EQ(ring_buffer.Write(in), 4u);
  EXPECT_EQ(ring_buffer.AvailableToRead(), 4u);

  ring_buffer.Clear();
  EXPECT_EQ(ring_buffer.AvailableToRead(), 0u);
  EXPECT_EQ(ring_buffer.AvailableToWrite(), 8u);
}

TEST(AudioRingBufferMacTest, MultiThreadedSpsc) {
  constexpr size_t kCapacity = 64;
  constexpr size_t kTotalElements = 50000;
  AudioRingBufferMac<int32_t> ring_buffer(kCapacity);

  std::thread producer([&]() {
    for (int32_t i = 0; i < static_cast<int32_t>(kTotalElements);) {
      if (ring_buffer.Write(std::span<const int32_t>(&i, 1)) == 1) {
        ++i;
      } else {
        std::this_thread::yield();
      }
    }
  });

  std::vector<int32_t> received;
  received.reserve(kTotalElements);
  std::thread consumer([&]() {
    while (received.size() < kTotalElements) {
      int32_t val = 0;
      if (ring_buffer.Read(std::span<int32_t>(&val, 1)) == 1) {
        received.push_back(val);
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  ASSERT_EQ(received.size(), kTotalElements);
  for (size_t i = 0; i < kTotalElements; ++i) {
    EXPECT_EQ(received[i], static_cast<int32_t>(i));
  }
}

}  // namespace
}  // namespace webrtc
