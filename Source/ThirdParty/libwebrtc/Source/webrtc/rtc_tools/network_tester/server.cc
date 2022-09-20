/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/null_socket_server.h"
#include "rtc_tools/network_tester/test_controller.h"

int main(int /*argn*/, char* /*argv*/[]) {
  rtc::Thread main_thread(std::make_unique<rtc::NullSocketServer>());
  webrtc::TestController server(9090, 9090, "server_config.dat",
                                "server_packet_log.dat");
  while (!server.IsTestDone()) {
    // 100 ms is arbitrary chosen.
    main_thread.ProcessMessages(/*cms=*/100);
  }
  return 0;
}
