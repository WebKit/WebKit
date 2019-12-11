/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/network_tester/test_controller.h"

#include <limits>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/thread.h"

namespace webrtc {

TestController::TestController(int min_port,
                               int max_port,
                               const std::string& config_file_path,
                               const std::string& log_file_path)
    : socket_factory_(rtc::ThreadManager::Instance()->WrapCurrentThread()),
      config_file_path_(config_file_path),
      packet_logger_(log_file_path),
      local_test_done_(false),
      remote_test_done_(false) {
  RTC_DCHECK_RUN_ON(&test_controller_thread_checker_);
  packet_sender_checker_.Detach();
  send_data_.fill(42);
  udp_socket_ =
      std::unique_ptr<rtc::AsyncPacketSocket>(socket_factory_.CreateUdpSocket(
          rtc::SocketAddress(rtc::GetAnyIP(AF_INET), 0), min_port, max_port));
  udp_socket_->SignalReadPacket.connect(this, &TestController::OnReadPacket);
}

void TestController::SendConnectTo(const std::string& hostname, int port) {
  RTC_DCHECK_RUN_ON(&test_controller_thread_checker_);
  remote_address_ = rtc::SocketAddress(hostname, port);
  NetworkTesterPacket packet;
  packet.set_type(NetworkTesterPacket::HAND_SHAKING);
  SendData(packet, absl::nullopt);
  rtc::CritScope scoped_lock(&local_test_done_lock_);
  local_test_done_ = false;
  remote_test_done_ = false;
}

void TestController::Run() {
  RTC_DCHECK_RUN_ON(&test_controller_thread_checker_);
  rtc::Thread::Current()->ProcessMessages(0);
}

void TestController::SendData(const NetworkTesterPacket& packet,
                              absl::optional<size_t> data_size) {
  // Can be call from packet_sender or from test_controller thread.
  size_t packet_size = packet.ByteSizeLong();
  send_data_[0] = packet_size;
  packet_size++;
  packet.SerializeToArray(&send_data_[1], std::numeric_limits<char>::max());
  if (data_size && *data_size > packet_size)
    packet_size = *data_size;
  udp_socket_->SendTo((const void*)send_data_.data(), packet_size,
                      remote_address_, rtc::PacketOptions());
}

void TestController::OnTestDone() {
  RTC_DCHECK_RUN_ON(&packet_sender_checker_);
  NetworkTesterPacket packet;
  packet.set_type(NetworkTesterPacket::TEST_DONE);
  SendData(packet, absl::nullopt);
  rtc::CritScope scoped_lock(&local_test_done_lock_);
  local_test_done_ = true;
}

bool TestController::IsTestDone() {
  RTC_DCHECK_RUN_ON(&test_controller_thread_checker_);
  rtc::CritScope scoped_lock(&local_test_done_lock_);
  return local_test_done_ && remote_test_done_;
}

void TestController::OnReadPacket(rtc::AsyncPacketSocket* socket,
                                  const char* data,
                                  size_t len,
                                  const rtc::SocketAddress& remote_addr,
                                  const int64_t& packet_time_us) {
  RTC_DCHECK_RUN_ON(&test_controller_thread_checker_);
  size_t packet_size = data[0];
  std::string receive_data(&data[1], packet_size);
  NetworkTesterPacket packet;
  packet.ParseFromString(receive_data);
  RTC_CHECK(packet.has_type());
  switch (packet.type()) {
    case NetworkTesterPacket::HAND_SHAKING: {
      NetworkTesterPacket packet;
      packet.set_type(NetworkTesterPacket::TEST_START);
      remote_address_ = remote_addr;
      SendData(packet, absl::nullopt);
      packet_sender_.reset(new PacketSender(this, config_file_path_));
      packet_sender_->StartSending();
      rtc::CritScope scoped_lock(&local_test_done_lock_);
      local_test_done_ = false;
      remote_test_done_ = false;
      break;
    }
    case NetworkTesterPacket::TEST_START: {
      packet_sender_.reset(new PacketSender(this, config_file_path_));
      packet_sender_->StartSending();
      rtc::CritScope scoped_lock(&local_test_done_lock_);
      local_test_done_ = false;
      remote_test_done_ = false;
      break;
    }
    case NetworkTesterPacket::TEST_DATA: {
      packet.set_arrival_timestamp(packet_time_us);
      packet.set_packet_size(len);
      packet_logger_.LogPacket(packet);
      break;
    }
    case NetworkTesterPacket::TEST_DONE: {
      remote_test_done_ = true;
      break;
    }
    default: {
      RTC_NOTREACHED();
    }
  }
}

}  // namespace webrtc
