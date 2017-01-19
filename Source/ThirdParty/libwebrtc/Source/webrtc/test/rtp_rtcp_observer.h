/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_RTP_RTCP_OBSERVER_H_
#define WEBRTC_TEST_RTP_RTCP_OBSERVER_H_

#include <map>
#include <memory>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/test/constants.h"
#include "webrtc/test/direct_transport.h"
#include "webrtc/test/gtest.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_send_stream.h"

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

  virtual bool Wait() { return observation_complete_.Wait(timeout_ms_); }

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
  explicit RtpRtcpObserver(int event_timeout_ms)
      : observation_complete_(false, false),
        parser_(RtpHeaderParser::Create()),
        timeout_ms_(event_timeout_ms) {
    parser_->RegisterRtpHeaderExtension(kRtpExtensionTransmissionTimeOffset,
                                        kTOffsetExtensionId);
    parser_->RegisterRtpHeaderExtension(kRtpExtensionAbsoluteSendTime,
                                        kAbsSendTimeExtensionId);
    parser_->RegisterRtpHeaderExtension(kRtpExtensionTransportSequenceNumber,
                                        kTransportSequenceNumberExtensionId);
  }

  rtc::Event observation_complete_;
  const std::unique_ptr<RtpHeaderParser> parser_;

 private:
  const int timeout_ms_;
};

class PacketTransport : public test::DirectTransport {
 public:
  enum TransportType { kReceiver, kSender };

  PacketTransport(Call* send_call,
                  RtpRtcpObserver* observer,
                  TransportType transport_type,
                  const FakeNetworkPipe::Config& configuration)
      : test::DirectTransport(configuration, send_call),
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

#endif  // WEBRTC_TEST_RTP_RTCP_OBSERVER_H_
