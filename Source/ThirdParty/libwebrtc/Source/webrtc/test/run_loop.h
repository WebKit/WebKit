/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_RUN_LOOP_H_
#define TEST_RUN_LOOP_H_

#include "api/task_queue/task_queue_base.h"

namespace webrtc {
namespace test {

// Blocks until the user presses enter.
void PressEnterToContinue(TaskQueueBase* task_queue);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_RUN_LOOP_H_
