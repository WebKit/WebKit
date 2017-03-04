/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H
#define WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H

#include <stdio.h>
#include <string>

#include "gflags/gflags.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_common.h"

#if defined(WEBRTC_ANDROID)
extern char mobileLogMsg[640];
#endif

DECLARE_bool(include_timing_dependent_tests);

namespace voetest {

class SubAPIManager {
 public:
  SubAPIManager()
    : _base(true),
      _codec(false),
      _file(false),
      _hardware(false),
      _netEqStats(false),
      _network(false),
      _rtp_rtcp(false),
      _volumeControl(false),
      _apm(false) {
      _codec = true;
      _file = true;
      _hardware = true;
      _netEqStats = true;
      _network = true;
      _rtp_rtcp = true;
      _volumeControl = true;
      _apm = true;
  }

  void DisplayStatus() const;

 private:
  bool _base, _codec;
  bool _file, _hardware;
  bool _netEqStats, _network, _rtp_rtcp, _volumeControl, _apm;
};

}  // namespace voetest

#endif // WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H
