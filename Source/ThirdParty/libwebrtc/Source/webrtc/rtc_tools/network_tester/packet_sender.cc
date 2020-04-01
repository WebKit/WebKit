/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/network_tester/packet_sender.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/time_utils.h"
#include "rtc_tools/network_tester/config_reader.h"
#include "rtc_tools/network_tester/test_controller.h"

namespace webrtc {

namespace {

class SendPacketTask : public QueuedTask {
 public:
  explicit SendPacketTask(PacketSender* packet_sender)
      : target_time_ms_(rtc::TimeMillis()), packet_sender_(packet_sender) {}

 private:
  bool Run() override {
    if (packet_sender_->IsSending()) {
      packet_sender_->SendPacket();
      target_time_ms_ += packet_sender_->GetSendIntervalMs();
      int64_t delay_ms = std::max(static_cast<int64_t>(0),
                                  target_time_ms_ - rtc::TimeMillis());
      TaskQueueBase::Current()->PostDelayedTask(
          std::unique_ptr<QueuedTask>(this), delay_ms);
      return false;
    } else {
      return true;
    }
  }
  int64_t target_time_ms_;
  PacketSender* const packet_sender_;
};

class UpdateTestSettingTask : public QueuedTask {
 public:
  UpdateTestSettingTask(PacketSender* packet_sender,
                        std::unique_ptr<ConfigReader> config_reader)
      : packet_sender_(packet_sender),
        config_reader_(std::move(config_reader)) {}

 private:
  bool Run() override {
    auto config = config_reader_->GetNextConfig();
    if (config) {
      packet_sender_->UpdateTestSetting((*config).packet_size,
                                        (*config).packet_send_interval_ms);
      TaskQueueBase::Current()->PostDelayedTask(
          std::unique_ptr<QueuedTask>(this), (*config).execution_time_ms);
      return false;
    } else {
      packet_sender_->StopSending();
      return true;
    }
  }
  PacketSender* const packet_sender_;
  const std::unique_ptr<ConfigReader> config_reader_;
};

}  // namespace

PacketSender::PacketSender(TestController* test_controller,
                           const std::string& config_file_path)
    : packet_size_(0),
      send_interval_ms_(0),
      sequence_number_(0),
      sending_(false),
      config_file_path_(config_file_path),
      test_controller_(test_controller),
      task_queue_factory_(CreateDefaultTaskQueueFactory()),
      worker_queue_(task_queue_factory_->CreateTaskQueue(
          "Packet Sender",
          TaskQueueFactory::Priority::HIGH)) {}

PacketSender::~PacketSender() = default;

void PacketSender::StartSending() {
  worker_queue_checker_.Detach();
  worker_queue_.PostTask([this]() {
    RTC_DCHECK_RUN_ON(&worker_queue_checker_);
    sending_ = true;
  });
  worker_queue_.PostTask(std::make_unique<UpdateTestSettingTask>(
      this, std::make_unique<ConfigReader>(config_file_path_)));
  worker_queue_.PostTask(std::make_unique<SendPacketTask>(this));
}

void PacketSender::StopSending() {
  RTC_DCHECK_RUN_ON(&worker_queue_checker_);
  sending_ = false;
  test_controller_->OnTestDone();
}

bool PacketSender::IsSending() const {
  RTC_DCHECK_RUN_ON(&worker_queue_checker_);
  return sending_;
}

void PacketSender::SendPacket() {
  RTC_DCHECK_RUN_ON(&worker_queue_checker_);
  NetworkTesterPacket packet;
  packet.set_type(NetworkTesterPacket::TEST_DATA);
  packet.set_sequence_number(sequence_number_++);
  packet.set_send_timestamp(rtc::TimeMicros());
  test_controller_->SendData(packet, packet_size_);
}

int64_t PacketSender::GetSendIntervalMs() const {
  RTC_DCHECK_RUN_ON(&worker_queue_checker_);
  return send_interval_ms_;
}

void PacketSender::UpdateTestSetting(size_t packet_size,
                                     int64_t send_interval_ms) {
  RTC_DCHECK_RUN_ON(&worker_queue_checker_);
  send_interval_ms_ = send_interval_ms;
  packet_size_ = packet_size;
}

}  // namespace webrtc
