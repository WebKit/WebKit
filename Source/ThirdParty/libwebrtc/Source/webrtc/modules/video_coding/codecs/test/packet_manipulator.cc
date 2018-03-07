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

#include <stdio.h>

#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"

namespace webrtc {
namespace test {

PacketManipulatorImpl::PacketManipulatorImpl(PacketReader* packet_reader,
                                             const NetworkingConfig& config,
                                             bool verbose)
    : packet_reader_(packet_reader),
      config_(config),
      verbose_(verbose),
      active_burst_packets_(0),
      random_seed_(1) {
  RTC_DCHECK(packet_reader);
}

int PacketManipulatorImpl::ManipulatePackets(
    webrtc::EncodedImage* encoded_image) {
  int nbr_packets_dropped = 0;
  // There's no need to build a copy of the image data since viewing an
  // EncodedImage object, setting the length to a new lower value represents
  // that everything is dropped after that position in the byte array.
  // EncodedImage._size is the allocated bytes.
  // EncodedImage._length is how many that are filled with data.
  int new_length = 0;
  packet_reader_->InitializeReading(encoded_image->_buffer,
                                    encoded_image->_length,
                                    config_.packet_size_in_bytes);
  uint8_t* packet = nullptr;
  int nbr_bytes_to_read;
  // keep track of if we've lost any packets, since then we shall loose
  // the remains of the current frame:
  bool packet_loss_has_occurred = false;
  while ((nbr_bytes_to_read = packet_reader_->NextPacket(&packet)) > 0) {
    // Check if we're currently in a packet loss burst that is not completed:
    if (active_burst_packets_ > 0) {
      active_burst_packets_--;
      nbr_packets_dropped++;
    } else if (RandomUniform() < config_.packet_loss_probability ||
               packet_loss_has_occurred) {
      packet_loss_has_occurred = true;
      nbr_packets_dropped++;
      if (config_.packet_loss_mode == kBurst) {
        // Initiate a new burst
        active_burst_packets_ = config_.packet_loss_burst_length - 1;
      }
    } else {
      new_length += nbr_bytes_to_read;
    }
  }
  encoded_image->_length = new_length;
  if (nbr_packets_dropped > 0) {
    // Must set completeFrame to false to inform the decoder about this:
    encoded_image->_completeFrame = false;
    if (verbose_) {
      printf("Dropped %d packets for frame %d (frame length: %" PRIuS ")\n",
             nbr_packets_dropped, encoded_image->_timeStamp,
             encoded_image->_length);
    }
  }
  return nbr_packets_dropped;
}

void PacketManipulatorImpl::InitializeRandomSeed(unsigned int seed) {
  random_seed_ = seed;
}

inline double PacketManipulatorImpl::RandomUniform() {
  // Use the previous result as new seed before each rand() call. Doing this
  // it doesn't matter if other threads are calling rand() since we'll always
  // get the same behavior as long as we're using a fixed initial seed.
  critsect_.Enter();
  srand(random_seed_);
  random_seed_ = rand();  // NOLINT (rand_r instead of rand)
  critsect_.Leave();
  return (random_seed_ + 1.0) / (RAND_MAX + 1.0);
}

}  // namespace test
}  // namespace webrtc
