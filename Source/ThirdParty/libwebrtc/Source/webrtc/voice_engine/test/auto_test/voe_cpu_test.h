/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_CPU_TEST_H
#define WEBRTC_VOICE_ENGINE_VOE_CPU_TEST_H

#include "webrtc/voice_engine/test/auto_test/voe_standard_test.h"

namespace voetest {

class VoETestManager;

class VoECpuTest {
 public:
  VoECpuTest(VoETestManager& mgr);
  ~VoECpuTest() {}
  int DoTest();
 private:
  VoETestManager& _mgr;
};

}  // namespace voetest

#endif // WEBRTC_VOICE_ENGINE_VOE_CPU_TEST_H
