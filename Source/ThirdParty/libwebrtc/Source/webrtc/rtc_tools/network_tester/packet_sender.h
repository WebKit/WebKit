/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_NETWORK_TESTER_PACKET_SENDER_H_
#define RTC_TOOLS_NETWORK_TESTER_PACKET_SENDER_H_

#include <memory>
#include <string>

#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_factory.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/task_queue.h"

#ifdef WEBRTC_NETWORK_TESTER_PROTO
RTC_PUSH_IGNORING_WUNDEF()
#include "rtc_tools/network_tester/network_tester_packet.pb.h"
RTC_POP_IGNORING_WUNDEF()
using webrtc::network_tester::packet::NetworkTesterPacket;
#else
class NetworkTesterPacket;
#endif  // WEBRTC_NETWORK_TESTER_PROTO

namespace webrtc {

class TestController;

class PacketSender {
 public:
  PacketSender(TestController* test_controller,
               const std::string& config_file_path);
  ~PacketSender();

  void StartSending();
  void StopSending();
  bool IsSending() const;

  void SendPacket();

  int64_t GetSendIntervalMs() const;
  void UpdateTestSetting(size_t packet_size, int64_t send_interval_ms);

 private:
  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_queue_checker_;
  size_t packet_size_ RTC_GUARDED_BY(worker_queue_checker_);
  int64_t send_interval_ms_ RTC_GUARDED_BY(worker_queue_checker_);
  int64_t sequence_number_ RTC_GUARDED_BY(worker_queue_checker_);
  bool sending_ RTC_GUARDED_BY(worker_queue_checker_);
  const std::string config_file_path_;
  TestController* const test_controller_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  rtc::TaskQueue worker_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(PacketSender);
};

}  // namespace webrtc

#endif  // RTC_TOOLS_NETWORK_TESTER_PACKET_SENDER_H_
