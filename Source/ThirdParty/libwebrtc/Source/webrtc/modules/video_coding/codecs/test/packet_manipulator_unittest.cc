/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/packet_manipulator.h"

#include <queue>

#include "modules/video_coding/include/video_codec_interface.h"
#include "test/gtest.h"
#include "test/testsupport/unittest_utils.h"
#include "typedefs.h"  // NOLINT(build/include)

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

}  // namespace test
}  // namespace webrtc
