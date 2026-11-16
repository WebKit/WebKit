/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/test/network_emulation/ecn_marking_counter.h"

#include "api/transport/ecn_marking.h"

namespace webrtc {

void EcnMarkingCounter::Add(EcnMarking ecn) {
  switch (ecn) {
    case EcnMarking::kNotEct:
      ++not_ect_;
      break;
    case EcnMarking::kEct0:
      ++ect_0_;
      break;
    case EcnMarking::kEct1:
      ++ect_1_;
      break;
    case EcnMarking::kCe:
      ++ce_;
      break;
  }
}

EcnMarkingCounter& EcnMarkingCounter::operator+=(
    const EcnMarkingCounter& counter) {
  not_ect_ += counter.not_ect();
  ect_0_ += counter.ect_0();
  ect_1_ += counter.ect_1();
  ce_ += counter.ce();
  return *this;
}

}  // namespace webrtc
