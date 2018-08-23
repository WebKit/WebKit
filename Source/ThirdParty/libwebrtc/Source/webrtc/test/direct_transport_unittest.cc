/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/direct_transport.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
TEST(DemuxerTest, Demuxing) {
  constexpr uint8_t kVideoPayloadType = 100;
  constexpr uint8_t kAudioPayloadType = 101;
  constexpr size_t kPacketSize = 10;
  Demuxer demuxer({{kVideoPayloadType, MediaType::VIDEO},
                   {kAudioPayloadType, MediaType::AUDIO}});

  uint8_t data[kPacketSize];
  memset(data, 0, kPacketSize);
  data[1] = kVideoPayloadType;
  EXPECT_EQ(demuxer.GetMediaType(data, kPacketSize), MediaType::VIDEO);
  data[1] = kAudioPayloadType;
  EXPECT_EQ(demuxer.GetMediaType(data, kPacketSize), MediaType::AUDIO);
}
}  // namespace test
}  // namespace webrtc
