/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_RTP_RTCP_OBSERVER_H_
#define TEST_RTP_RTCP_OBSERVER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "api/test/simulated_network.h"
#include "call/simulated_packet_receiver.h"
#include "call/video_send_stream.h"
#include "rtc_base/event.h"
#include "system_wrappers/include/field_trial.h"
#include "test/direct_transport.h"
#include "test/gtest.h"
#include "test/rtp_header_parser.h"

namespace {
const int kShortTimeoutMs = 500;
}

namespace webrtc {
namespace test {

class PacketTransport;

class RtpRtcpObserver {
 public:
  enum Action {
    SEND_PACKET,
    DROP_PACKET,
  };

  virtual ~RtpRtcpObserver() {}

  virtual bool Wait() {
    if (field_trial::IsEnabled("WebRTC-QuickPerfTest")) {
      observation_complete_.Wait(kShortTimeoutMs);
      return true;
    }
    return observation_complete_.Wait(timeout_ms_);
  }

  virtual Action OnSendRtp(const uint8_t* packet, size_t length) {
    return SEND_PACKET;
  }

  virtual Action OnSendRtcp(const uint8_t* packet, size_t length) {
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtp(const uint8_t* packet, size_t length) {
    return SEND_PACKET;
  }

  virtual Action OnReceiveRtcp(const uint8_t* packet, size_t length) {
    return SEND_PACKET;
  }

 protected:
  RtpRtcpObserver() : RtpRtcpObserver(TimeDelta::Zero()) {}
  explicit RtpRtcpObserver(TimeDelta event_timeout)
      : timeout_ms_(event_timeout.ms()) {}

  rtc::Event observation_complete_;

 private:
  const int timeout_ms_;
};

class PacketTransport : public test::DirectTransport {
 public:
  enum TransportType { kReceiver, kSender };

  PacketTransport(TaskQueueBase* task_queue,
                  Call* send_call,
                  RtpRtcpObserver* observer,
                  TransportType transport_type,
                  const std::map<uint8_t, MediaType>& payload_type_map,
                  std::unique_ptr<SimulatedPacketReceiverInterface> nw_pipe)
      : test::DirectTransport(task_queue,
                              std::move(nw_pipe),
                              send_call,
                              payload_type_map),
        observer_(observer),
        transport_type_(transport_type) {}

 private:
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override {
    EXPECT_FALSE(RtpHeaderParser::IsRtcp(packet, length));
    RtpRtcpObserver::Action action;
    {
      if (transport_type_ == kSender) {
        action = observer_->OnSendRtp(packet, length);
      } else {
        action = observer_->OnReceiveRtp(packet, length);
      }
    }
    switch (action) {
      case RtpRtcpObserver::DROP_PACKET:
        // Drop packet silently.
        return true;
      case RtpRtcpObserver::SEND_PACKET:
        return test::DirectTransport::SendRtp(packet, length, options);
    }
    return true;  // Will never happen, makes compiler happy.
  }

  bool SendRtcp(const uint8_t* packet, size_t length) override {
    EXPECT_TRUE(RtpHeaderParser::IsRtcp(packet, length));
    RtpRtcpObserver::Action action;
    {
      if (transport_type_ == kSender) {
        action = observer_->OnSendRtcp(packet, length);
      } else {
        action = observer_->OnReceiveRtcp(packet, length);
      }
    }
    switch (action) {
      case RtpRtcpObserver::DROP_PACKET:
        // Drop packet silently.
        return true;
      case RtpRtcpObserver::SEND_PACKET:
        return test::DirectTransport::SendRtcp(packet, length);
    }
    return true;  // Will never happen, makes compiler happy.
  }

  RtpRtcpObserver* const observer_;
  TransportType transport_type_;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_RTP_RTCP_OBSERVER_H_
