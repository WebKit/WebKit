/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/packet_reader.h"

#include <assert.h>
#include <stdio.h>
#include <algorithm>

namespace webrtc {
namespace test {

PacketReader::PacketReader()
    : initialized_(false) {}

PacketReader::~PacketReader() {}

void PacketReader::InitializeReading(uint8_t* data,
                                     size_t data_length_in_bytes,
                                     size_t packet_size_in_bytes) {
  assert(data);
  assert(packet_size_in_bytes > 0);
  data_ = data;
  data_length_ = data_length_in_bytes;
  packet_size_ = packet_size_in_bytes;
  currentIndex_ = 0;
  initialized_ = true;
}

int PacketReader::NextPacket(uint8_t** packet_pointer) {
  if (!initialized_) {
    fprintf(stderr, "Attempting to use uninitialized PacketReader!\n");
    return -1;
  }
  *packet_pointer = data_ + currentIndex_;
  size_t old_index = currentIndex_;
  currentIndex_ = std::min(currentIndex_ + packet_size_, data_length_);
  return static_cast<int>(currentIndex_ - old_index);
}

}  // namespace test
}  // namespace webrtc
