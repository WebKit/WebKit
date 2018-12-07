/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/run_loop.h"

#include <assert.h>

#include <Windows.h>
#include <conio.h>
#include <stdio.h>

namespace webrtc {
namespace test {

void PressEnterToContinue(SingleThreadedTaskQueueForTesting &task_queue) {
  puts(">> Press ENTER to continue...");

  while (!_kbhit() || _getch() != '\r') {
    // Drive the message loop for the thread running the task_queue
    task_queue.SendTask([&]() {
      MSG msg;
      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    });
  }
}
}  // namespace test
}  // namespace webrtc
