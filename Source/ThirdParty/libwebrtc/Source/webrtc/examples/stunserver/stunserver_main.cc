/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_POSIX)
#include <errno.h>
#endif  // WEBRTC_POSIX

#include <iostream>

#include "p2p/base/stunserver.h"
#include "rtc_base/thread.h"

using cricket::StunServer;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "usage: stunserver address" << std::endl;
    return 1;
  }

  rtc::SocketAddress server_addr;
  if (!server_addr.FromString(argv[1])) {
    std::cerr << "Unable to parse IP address: " << argv[1];
    return 1;
  }

  rtc::Thread *pthMain = rtc::Thread::Current();

  rtc::AsyncUDPSocket* server_socket =
      rtc::AsyncUDPSocket::Create(pthMain->socketserver(), server_addr);
  if (!server_socket) {
    std::cerr << "Failed to create a UDP socket" << std::endl;
    return 1;
  }

  StunServer* server = new StunServer(server_socket);

  std::cout << "Listening at " << server_addr.ToString() << std::endl;

  pthMain->Run();

  delete server;
  return 0;
}
