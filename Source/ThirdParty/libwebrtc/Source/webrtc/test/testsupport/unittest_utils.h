/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_UNITTEST_UTILS_H_
#define TEST_TESTSUPPORT_UNITTEST_UTILS_H_

namespace webrtc {
namespace test {

const size_t kPacketSizeInBytes = 1500;
const size_t kPacketDataLength = kPacketSizeInBytes * 2 + 1;
const int kPacketDataNumberOfPackets = 3;

// A base test fixture for packet related tests. Contains
// two full prepared packets with 1s, 2s in their data and a third packet with
// a single 3 in it (size=1).
// A packet data structure is also available, that contains these three packets
// in order.
class PacketRelatedTest: public testing::Test {
 protected:
  // Tree packet byte arrays with data used for verification:
  uint8_t packet1_[kPacketSizeInBytes];
  uint8_t packet2_[kPacketSizeInBytes];
  uint8_t packet3_[1];
  // Construct a data structure containing these packets
  uint8_t packet_data_[kPacketDataLength];
  uint8_t* packet_data_pointer_;

  PacketRelatedTest() {
    packet_data_pointer_ = packet_data_;

    memset(packet1_, 1, kPacketSizeInBytes);
    memset(packet2_, 2, kPacketSizeInBytes);
    memset(packet3_, 3, 1);
    // Fill the packet_data:
    memcpy(packet_data_pointer_, packet1_, kPacketSizeInBytes);
    memcpy(packet_data_pointer_ + kPacketSizeInBytes, packet2_,
           kPacketSizeInBytes);
    memcpy(packet_data_pointer_ + kPacketSizeInBytes * 2, packet3_, 1);
  }
  virtual ~PacketRelatedTest() {}
  void SetUp() {}
  void TearDown() {}
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_UNITTEST_UTILS_H_
