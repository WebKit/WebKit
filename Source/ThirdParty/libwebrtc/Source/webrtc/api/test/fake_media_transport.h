/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_FAKE_MEDIA_TRANSPORT_H_
#define API_TEST_FAKE_MEDIA_TRANSPORT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/test/fake_datagram_transport.h"
#include "api/transport/media/media_transport_interface.h"

namespace webrtc {

// Fake media transport factory creates fake media transport.
// Also creates fake datagram transport, since both media and datagram
// transports are created by |MediaTransportFactory|.
class FakeMediaTransportFactory : public MediaTransportFactory {
 public:
  explicit FakeMediaTransportFactory(
      const absl::optional<std::string>& transport_offer = "")
      : transport_offer_(transport_offer) {}
  ~FakeMediaTransportFactory() = default;

  std::string GetTransportName() const override { return "fake"; }

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::PacketTransportInternal* packet_transport,
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override {
    return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
  }

  RTCErrorOr<std::unique_ptr<MediaTransportInterface>> CreateMediaTransport(
      rtc::Thread* network_thread,
      const MediaTransportSettings& settings) override {
    return RTCError(RTCErrorType::UNSUPPORTED_OPERATION);
  }

  RTCErrorOr<std::unique_ptr<DatagramTransportInterface>>
  CreateDatagramTransport(rtc::Thread* network_thread,
                          const MediaTransportSettings& settings) override {
    return std::unique_ptr<DatagramTransportInterface>(
        new FakeDatagramTransport(settings, transport_offer_.value_or(""),
                                  transport_parameters_comparison_));
  }

  void set_transport_parameters_comparison(
      std::function<bool(absl::string_view, absl::string_view)> comparison) {
    transport_parameters_comparison_ = std::move(comparison);
  }

 private:
  const absl::optional<std::string> transport_offer_;
  std::function<bool(absl::string_view, absl::string_view)>
      transport_parameters_comparison_ =
          [](absl::string_view local, absl::string_view remote) {
            return local == remote;
          };
};

}  // namespace webrtc

#endif  // API_TEST_FAKE_MEDIA_TRANSPORT_H_
