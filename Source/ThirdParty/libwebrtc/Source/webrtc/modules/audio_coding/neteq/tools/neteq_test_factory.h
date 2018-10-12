/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_FACTORY_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_FACTORY_H_

#include <memory>
#include <string>

#include "modules/audio_coding/neteq/tools/neteq_test.h"

namespace webrtc {
namespace test {

class SsrcSwitchDetector;
class NetEqStatsGetter;
class NetEqStatsPlotter;

// Note that the NetEqTestFactory needs to be alive when the NetEqTest object is
// used for a simulation.
class NetEqTestFactory {
 public:
  NetEqTestFactory();
  ~NetEqTestFactory();
  void PrintCodecMap();
  std::unique_ptr<NetEqTest> InitializeTest(std::string input_filename,
                                            std::string output_filename);

 private:
  std::unique_ptr<AudioDecoder> replacement_decoder_;
  NetEqTest::ExtDecoderMap ext_codecs_;
  std::unique_ptr<SsrcSwitchDetector> ssrc_switch_detector_;
  std::unique_ptr<NetEqStatsPlotter> stats_plotter_;
};

}  // namespace test
}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_TEST_FACTORY_H_
