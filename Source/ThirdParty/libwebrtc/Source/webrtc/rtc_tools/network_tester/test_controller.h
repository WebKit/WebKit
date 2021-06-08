/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_NETWORK_TESTER_TEST_CONTROLLER_H_
#define RTC_TOOLS_NETWORK_TESTER_TEST_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_tools/network_tester/packet_logger.h"
#include "rtc_tools/network_tester/packet_sender.h"

#ifdef WEBRTC_NETWORK_TESTER_PROTO
RTC_PUSH_IGNORING_WUNDEF()
#include "rtc_tools/network_tester/network_tester_packet.pb.h"
RTC_POP_IGNORING_WUNDEF()
using webrtc::network_tester::packet::NetworkTesterPacket;
#else
class NetworkTesterPacket;
#endif  // WEBRTC_NETWORK_TESTER_PROTO

namespace webrtc {

constexpr size_t kEthernetMtu = 1500;

class TestController : public sigslot::has_slots<> {
 public:
  TestController(int min_port,
                 int max_port,
                 const std::string& config_file_path,
                 const std::string& log_file_path);

  void Run();

  void SendConnectTo(const std::string& hostname, int port);

  void SendData(const NetworkTesterPacket& packet,
                absl::optional<size_t> data_size);

  void OnTestDone();

  bool IsTestDone();

 private:
  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data,
                    size_t len,
                    const rtc::SocketAddress& remote_addr,
                    const int64_t& packet_time_us);
  SequenceChecker test_controller_thread_checker_;
  SequenceChecker packet_sender_checker_;
  rtc::BasicPacketSocketFactory socket_factory_;
  const std::string config_file_path_;
  PacketLogger packet_logger_;
  Mutex local_test_done_lock_;
  bool local_test_done_ RTC_GUARDED_BY(local_test_done_lock_);
  bool remote_test_done_;
  std::array<char, kEthernetMtu> send_data_;
  std::unique_ptr<rtc::AsyncPacketSocket> udp_socket_;
  rtc::SocketAddress remote_address_;
  std::unique_ptr<PacketSender> packet_sender_;

  RTC_DISALLOW_COPY_AND_ASSIGN(TestController);
};

}  // namespace webrtc

#endif  // RTC_TOOLS_NETWORK_TESTER_TEST_CONTROLLER_H_
