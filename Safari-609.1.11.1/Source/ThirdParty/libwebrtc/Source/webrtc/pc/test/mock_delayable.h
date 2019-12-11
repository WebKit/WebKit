/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_MOCK_DELAYABLE_H_
#define PC_TEST_MOCK_DELAYABLE_H_

#include <stdint.h>

#include "absl/types/optional.h"
#include "media/base/delayable.h"
#include "test/gmock.h"

namespace webrtc {

class MockDelayable : public cricket::Delayable {
 public:
  MOCK_METHOD2(SetBaseMinimumPlayoutDelayMs, bool(uint32_t ssrc, int delay_ms));
  MOCK_CONST_METHOD1(GetBaseMinimumPlayoutDelayMs,
                     absl::optional<int>(uint32_t ssrc));
};

}  // namespace webrtc

#endif  // PC_TEST_MOCK_DELAYABLE_H_
