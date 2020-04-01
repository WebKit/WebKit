/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_FAKE_DATAGRAM_TRANSPORT_H_
#define API_TEST_FAKE_DATAGRAM_TRANSPORT_H_

#include <cstddef>
#include <string>

#include "api/transport/datagram_transport_interface.h"
#include "api/transport/media/media_transport_interface.h"

namespace webrtc {

// Maxmum size of datagrams sent by |FakeDatagramTransport|.
constexpr size_t kMaxFakeDatagramSize = 1000;

// Fake datagram transport.  Does not support making an actual connection
// or sending data.  Only used for tests that need to stub out a transport.
class FakeDatagramTransport : public DatagramTransportInterface {
 public:
  FakeDatagramTransport(
      const MediaTransportSettings& settings,
      std::string transport_parameters,
      const std::function<bool(absl::string_view, absl::string_view)>&
          are_parameters_compatible)
      : settings_(settings),
        transport_parameters_(transport_parameters),
        are_parameters_compatible_(are_parameters_compatible) {}

  ~FakeDatagramTransport() override { RTC_DCHECK(!state_callback_); }

  void Connect(rtc::PacketTransportInternal* packet_transport) override {
    packet_transport_ = packet_transport;
  }

  CongestionControlInterface* congestion_control() override {
    return nullptr;  // Datagram interface doesn't provide this yet.
  }

  void SetTransportStateCallback(
      MediaTransportStateCallback* callback) override {
    state_callback_ = callback;
  }

  RTCError SendDatagram(rtc::ArrayView<const uint8_t> data,
                        DatagramId datagram_id) override {
    return RTCError::OK();
  }

  size_t GetLargestDatagramSize() const override {
    return kMaxFakeDatagramSize;
  }

  void SetDatagramSink(DatagramSinkInterface* sink) override {}

  std::string GetTransportParameters() const override {
    if (settings_.remote_transport_parameters) {
      return *settings_.remote_transport_parameters;
    }
    return transport_parameters_;
  }

  RTCError SetRemoteTransportParameters(
      absl::string_view remote_parameters) override {
    if (are_parameters_compatible_(GetTransportParameters(),
                                   remote_parameters)) {
      return RTCError::OK();
    }
    return RTCError(RTCErrorType::UNSUPPORTED_PARAMETER,
                    "Incompatible remote transport parameters");
  }

  RTCError OpenChannel(int channel_id) override {
    return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
  }

  RTCError SendData(int channel_id,
                    const SendDataParams& params,
                    const rtc::CopyOnWriteBuffer& buffer) override {
    return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
  }

  RTCError CloseChannel(int channel_id) override {
    return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
  }

  void SetDataSink(DataChannelSink* /*sink*/) override {}

  bool IsReadyToSend() const override { return false; }

  rtc::PacketTransportInternal* packet_transport() { return packet_transport_; }

  void set_state(webrtc::MediaTransportState state) {
    if (state_callback_) {
      state_callback_->OnStateChanged(state);
    }
  }

  const MediaTransportSettings& settings() { return settings_; }

 private:
  const MediaTransportSettings settings_;
  const std::string transport_parameters_;
  const std::function<bool(absl::string_view, absl::string_view)>
      are_parameters_compatible_;

  rtc::PacketTransportInternal* packet_transport_ = nullptr;
  MediaTransportStateCallback* state_callback_ = nullptr;
};

}  // namespace webrtc

#endif  // API_TEST_FAKE_DATAGRAM_TRANSPORT_H_
