/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/test/run_loop.h"

#include <assert.h>

#include <conio.h>
#include <stdio.h>
#include <Windows.h>

namespace webrtc {
namespace test {

void PressEnterToContinue() {
  puts(">> Press ENTER to continue...");

  MSG msg;
  BOOL ret;
  while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
    assert(ret != -1);
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}
}  // namespace test
}  // namespace webrtc
