/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/test/predictive_packet_manipulator.h"

#include <assert.h>
#include <stdio.h>

#include "webrtc/test/testsupport/packet_reader.h"

namespace webrtc {
namespace test {

PredictivePacketManipulator::PredictivePacketManipulator(
    PacketReader* packet_reader,
    const NetworkingConfig& config)
    : PacketManipulatorImpl(packet_reader, config, false) {}

PredictivePacketManipulator::~PredictivePacketManipulator() {}

void PredictivePacketManipulator::AddRandomResult(double result) {
  assert(result >= 0.0 && result <= 1.0);
  random_results_.push(result);
}

double PredictivePacketManipulator::RandomUniform() {
  if (random_results_.size() == 0u) {
    fprintf(stderr,
            "No more stored results, please make sure AddRandomResult()"
            "is called same amount of times you're going to invoke the "
            "RandomUniform() function, i.e. once per packet.\n");
    assert(false);
  }
  double result = random_results_.front();
  random_results_.pop();
  return result;
}

}  // namespace test
}  // namespace webrtc
