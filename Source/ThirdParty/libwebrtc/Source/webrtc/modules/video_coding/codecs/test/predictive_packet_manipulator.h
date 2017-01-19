/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_PREDICTIVE_PACKET_MANIPULATOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_PREDICTIVE_PACKET_MANIPULATOR_H_

#include <queue>

#include "webrtc/modules/video_coding/codecs/test/packet_manipulator.h"
#include "webrtc/test/testsupport/packet_reader.h"

namespace webrtc {
namespace test {

// Predictive packet manipulator that allows for setup of the result of
// the random invocations.
class PredictivePacketManipulator : public PacketManipulatorImpl {
 public:
  PredictivePacketManipulator(PacketReader* packet_reader,
                              const NetworkingConfig& config);
  virtual ~PredictivePacketManipulator();
  // Adds a result. You must add at least the same number of results as the
  // expected calls to the RandomUniform method. The results are added to a
  // FIFO queue so they will be returned in the same order they were added.
  // Result parameter must be 0.0 to 1.0.
  void AddRandomResult(double result);

 protected:
  // Returns a uniformly distributed random value between 0.0 and 1.0
  double RandomUniform() override;

 private:
  std::queue<double> random_results_;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_TEST_PREDICTIVE_PACKET_MANIPULATOR_H_
