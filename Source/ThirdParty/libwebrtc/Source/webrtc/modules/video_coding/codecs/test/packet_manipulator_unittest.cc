/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/test/packet_manipulator.h"

#include <queue>

#include "webrtc/modules/video_coding/codecs/test/predictive_packet_manipulator.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/unittest_utils.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

const double kNeverDropProbability = 0.0;
const double kAlwaysDropProbability = 1.0;
const int kBurstLength = 1;

class PacketManipulatorTest : public PacketRelatedTest {
 protected:
  PacketReader packet_reader_;
  EncodedImage image_;
  NetworkingConfig drop_config_;
  NetworkingConfig no_drop_config_;

  PacketManipulatorTest() {
    image_._buffer = packet_data_;
    image_._length = kPacketDataLength;
    image_._size = kPacketDataLength;

    drop_config_.packet_size_in_bytes = kPacketSizeInBytes;
    drop_config_.packet_loss_probability = kAlwaysDropProbability;
    drop_config_.packet_loss_burst_length = kBurstLength;
    drop_config_.packet_loss_mode = kUniform;

    no_drop_config_.packet_size_in_bytes = kPacketSizeInBytes;
    no_drop_config_.packet_loss_probability = kNeverDropProbability;
    no_drop_config_.packet_loss_burst_length = kBurstLength;
    no_drop_config_.packet_loss_mode = kUniform;
  }

  virtual ~PacketManipulatorTest() {}

  void SetUp() { PacketRelatedTest::SetUp(); }

  void TearDown() { PacketRelatedTest::TearDown(); }

  void VerifyPacketLoss(int expected_nbr_packets_dropped,
                        int actual_nbr_packets_dropped,
                        size_t expected_packet_data_length,
                        uint8_t* expected_packet_data,
                        const EncodedImage& actual_image) {
    EXPECT_EQ(expected_nbr_packets_dropped, actual_nbr_packets_dropped);
    EXPECT_EQ(expected_packet_data_length, image_._length);
    EXPECT_EQ(0, memcmp(expected_packet_data, actual_image._buffer,
                        expected_packet_data_length));
  }
};

TEST_F(PacketManipulatorTest, Constructor) {
  PacketManipulatorImpl manipulator(&packet_reader_, no_drop_config_, false);
}

TEST_F(PacketManipulatorTest, DropNone) {
  PacketManipulatorImpl manipulator(&packet_reader_, no_drop_config_, false);
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);
  VerifyPacketLoss(0, nbr_packets_dropped, kPacketDataLength, packet_data_,
                   image_);
}

TEST_F(PacketManipulatorTest, UniformDropNoneSmallFrame) {
  size_t data_length = 400;  // smaller than the packet size
  image_._length = data_length;
  PacketManipulatorImpl manipulator(&packet_reader_, no_drop_config_, false);
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);

  VerifyPacketLoss(0, nbr_packets_dropped, data_length, packet_data_, image_);
}

TEST_F(PacketManipulatorTest, UniformDropAll) {
  PacketManipulatorImpl manipulator(&packet_reader_, drop_config_, false);
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);
  VerifyPacketLoss(kPacketDataNumberOfPackets, nbr_packets_dropped, 0,
                   packet_data_, image_);
}

// Use our customized test class to make the second packet being lost
TEST_F(PacketManipulatorTest, UniformDropSinglePacket) {
  drop_config_.packet_loss_probability = 0.5;
  PredictivePacketManipulator manipulator(&packet_reader_, drop_config_);
  manipulator.AddRandomResult(1.0);
  manipulator.AddRandomResult(0.3);  // less than 0.5 will cause packet loss
  manipulator.AddRandomResult(1.0);

  // Execute the test target method:
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);

  // Since we setup the predictive packet manipulator, it will throw away the
  // second packet. The third packet is also lost because when we have lost one,
  // the remains shall also be discarded (in the current implementation).
  VerifyPacketLoss(2, nbr_packets_dropped, kPacketSizeInBytes, packet1_,
                   image_);
}

// Use our customized test class to make the second packet being lost
TEST_F(PacketManipulatorTest, BurstDropNinePackets) {
  // Create a longer packet data structure (10 packets)
  const int kNbrPackets = 10;
  const size_t kDataLength = kPacketSizeInBytes * kNbrPackets;
  uint8_t data[kDataLength];
  uint8_t* data_pointer = data;
  // Fill with 0s, 1s and so on to be able to easily verify which were dropped:
  for (int i = 0; i < kNbrPackets; ++i) {
    memset(data_pointer + i * kPacketSizeInBytes, i, kPacketSizeInBytes);
  }
  // Overwrite the defaults from the test fixture:
  image_._buffer = data;
  image_._length = kDataLength;
  image_._size = kDataLength;

  drop_config_.packet_loss_probability = 0.5;
  drop_config_.packet_loss_burst_length = 5;
  drop_config_.packet_loss_mode = kBurst;
  PredictivePacketManipulator manipulator(&packet_reader_, drop_config_);
  manipulator.AddRandomResult(1.0);
  manipulator.AddRandomResult(0.3);  // less than 0.5 will cause packet loss
  for (int i = 0; i < kNbrPackets - 2; ++i) {
    manipulator.AddRandomResult(1.0);
  }

  // Execute the test target method:
  int nbr_packets_dropped = manipulator.ManipulatePackets(&image_);

  // Should discard every packet after the first one.
  VerifyPacketLoss(9, nbr_packets_dropped, kPacketSizeInBytes, data, image_);
}

}  // namespace test
}  // namespace webrtc
