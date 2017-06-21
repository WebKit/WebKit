/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_PACKET_MANIPULATOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_PACKET_MANIPULATOR_H_

#include <stdlib.h>

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/test/testsupport/packet_reader.h"

namespace webrtc {
namespace test {

// Which mode the packet loss shall be performed according to.
enum PacketLossMode {
  // Drops packets with a configured probability independently for each packet
  kUniform,
  // Drops packets similar to uniform but when a packet is being dropped,
  // the number of lost packets in a row is equal to the configured burst
  // length.
  kBurst
};
// Returns a string representation of the enum value.
const char* PacketLossModeToStr(PacketLossMode e);

// Contains configurations related to networking and simulation of
// scenarios caused by network interference.
struct NetworkingConfig {
  NetworkingConfig()
      : packet_size_in_bytes(1500),
        max_payload_size_in_bytes(1440),
        packet_loss_mode(kUniform),
        packet_loss_probability(0.0),
        packet_loss_burst_length(1) {}

  // Packet size in bytes. Default: 1500 bytes.
  size_t packet_size_in_bytes;

  // Encoder specific setting of maximum size in bytes of each payload.
  // Default: 1440 bytes.
  size_t max_payload_size_in_bytes;

  // Packet loss mode. Two different packet loss models are supported:
  // uniform or burst. This setting has no effect unless
  // packet_loss_probability is >0.
  // Default: uniform.
  PacketLossMode packet_loss_mode;

  // Packet loss probability. A value between 0.0 and 1.0 that defines the
  // probability of a packet being lost. 0.1 means 10% and so on.
  // Default: 0 (no loss).
  double packet_loss_probability;

  // Packet loss burst length. Defines how many packets will be lost in a burst
  // when a packet has been decided to be lost. Must be >=1. Default: 1.
  int packet_loss_burst_length;
};

// Class for simulating packet loss on the encoded frame data.
// When a packet loss has occurred in a frame, the remaining data in that
// frame is lost (even if burst length is only a single packet).
// TODO(kjellander): Support discarding only individual packets in the frame
// when CL 172001 has been submitted. This also requires a correct
// fragmentation header to be passed to the decoder.
//
// To get a repeatable packet drop pattern, re-initialize the random seed
// using InitializeRandomSeed before each test run.
class PacketManipulator {
 public:
  virtual ~PacketManipulator() {}

  // Manipulates the data of the encoded_image to simulate parts being lost
  // during transport.
  // If packets are dropped from frame data, the completedFrame field will be
  // set to false.
  // Returns the number of packets being dropped.
  virtual int ManipulatePackets(webrtc::EncodedImage* encoded_image) = 0;
};

class PacketManipulatorImpl : public PacketManipulator {
 public:
  PacketManipulatorImpl(PacketReader* packet_reader,
                        const NetworkingConfig& config,
                        bool verbose);
  ~PacketManipulatorImpl() = default;
  int ManipulatePackets(webrtc::EncodedImage* encoded_image) override;
  virtual void InitializeRandomSeed(unsigned int seed);

 protected:
  // Returns a uniformly distributed random value between 0.0 and 1.0
  virtual double RandomUniform();

 private:
  PacketReader* packet_reader_;
  const NetworkingConfig& config_;
  // Used to simulate a burst over several frames.
  int active_burst_packets_;
  rtc::CriticalSection critsect_;
  unsigned int random_seed_;
  bool verbose_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_PACKET_MANIPULATOR_H_
