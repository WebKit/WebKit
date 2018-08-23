/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>  // NOLINT
#include <memory>

#include "p2p/base/relayserver.h"
#include "rtc_base/thread.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: relayserver internal-address external-address"
              << std::endl;
    return 1;
  }

  rtc::SocketAddress int_addr;
  if (!int_addr.FromString(argv[1])) {
    std::cerr << "Unable to parse IP address: " << argv[1];
    return 1;
  }

  rtc::SocketAddress ext_addr;
  if (!ext_addr.FromString(argv[2])) {
    std::cerr << "Unable to parse IP address: " << argv[2];
    return 1;
  }

  rtc::Thread* pthMain = rtc::Thread::Current();

  std::unique_ptr<rtc::AsyncUDPSocket> int_socket(
      rtc::AsyncUDPSocket::Create(pthMain->socketserver(), int_addr));
  if (!int_socket) {
    std::cerr << "Failed to create a UDP socket bound at" << int_addr.ToString()
              << std::endl;
    return 1;
  }

  std::unique_ptr<rtc::AsyncUDPSocket> ext_socket(
      rtc::AsyncUDPSocket::Create(pthMain->socketserver(), ext_addr));
  if (!ext_socket) {
    std::cerr << "Failed to create a UDP socket bound at" << ext_addr.ToString()
              << std::endl;
    return 1;
  }

  cricket::RelayServer server(pthMain);
  server.AddInternalSocket(int_socket.get());
  server.AddExternalSocket(ext_socket.get());

  std::cout << "Listening internally at " << int_addr.ToString() << std::endl;
  std::cout << "Listening externally at " << ext_addr.ToString() << std::endl;

  pthMain->Run();
  return 0;
}
