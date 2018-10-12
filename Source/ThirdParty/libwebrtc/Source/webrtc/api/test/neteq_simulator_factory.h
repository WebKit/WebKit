/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETEQ_SIMULATOR_FACTORY_H_
#define API_TEST_NETEQ_SIMULATOR_FACTORY_H_

#include <memory>

#include "api/test/neteq_simulator.h"

namespace webrtc {
namespace test {

class NetEqTestFactory;

class NetEqSimulatorFactory {
 public:
  NetEqSimulatorFactory();
  ~NetEqSimulatorFactory();
  // This function takes the same arguments as the neteq_rtpplay utility.
  std::unique_ptr<NetEqSimulator> CreateSimulator(int argc, char* argv[]);

 private:
  std::unique_ptr<NetEqTestFactory> factory_;
};

}  // namespace test
}  // namespace webrtc

#endif  // API_TEST_NETEQ_SIMULATOR_FACTORY_H_
