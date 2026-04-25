/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_CREATE_TIME_CONTROLLER_H_
#define API_TEST_CREATE_TIME_CONTROLLER_H_

#include <memory>

#include "api/test/time_controller.h"

namespace webrtc {

// Creates a time controller that runs in simulated time.
std::unique_ptr<TimeController> CreateSimulatedTimeController();

}  // namespace webrtc

#endif  // API_TEST_CREATE_TIME_CONTROLLER_H_
