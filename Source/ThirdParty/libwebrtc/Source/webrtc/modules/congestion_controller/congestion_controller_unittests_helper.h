/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_CONGESTION_CONTROLLER_UNITTESTS_HELPER_H_
#define MODULES_CONGESTION_CONTROLLER_CONGESTION_CONTROLLER_UNITTESTS_HELPER_H_

#include <vector>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {
void ComparePacketFeedbackVectors(const std::vector<PacketFeedback>& truth,
                                  const std::vector<PacketFeedback>& input);
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_CONGESTION_CONTROLLER_UNITTESTS_HELPER_H_
