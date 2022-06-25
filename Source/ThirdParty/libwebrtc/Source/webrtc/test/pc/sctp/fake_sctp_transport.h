/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_SCTP_FAKE_SCTP_TRANSPORT_H_
#define TEST_PC_SCTP_FAKE_SCTP_TRANSPORT_H_

#include <memory>

#include "media/sctp/sctp_transport_internal.h"

// Used for tests in this file to verify that PeerConnection responds to signals
// from the SctpTransport correctly, and calls Start with the correct
// local/remote ports.
class FakeSctpTransport : public cricket::SctpTransportInternal {
 public:
  void SetDtlsTransport(rtc::PacketTransportInternal* transport) override {}
  bool Start(int local_port, int remote_port, int max_message_size) override {
    local_port_.emplace(local_port);
    remote_port_.emplace(remote_port);
    max_message_size_ = max_message_size;
    return true;
  }
  bool OpenStream(int sid) override { return true; }
  bool ResetStream(int sid) override { return true; }
  bool SendData(int sid,
                const webrtc::SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                cricket::SendDataResult* result = nullptr) override {
    return true;
  }
  bool ReadyToSendData() override { return true; }
  void set_debug_name_for_testing(const char* debug_name) override {}

  int max_message_size() const { return max_message_size_; }
  absl::optional<int> max_outbound_streams() const { return absl::nullopt; }
  absl::optional<int> max_inbound_streams() const { return absl::nullopt; }
  int local_port() const {
    RTC_DCHECK(local_port_);
    return *local_port_;
  }
  int remote_port() const {
    RTC_DCHECK(remote_port_);
    return *remote_port_;
  }

 private:
  absl::optional<int> local_port_;
  absl::optional<int> remote_port_;
  int max_message_size_;
};

class FakeSctpTransportFactory : public webrtc::SctpTransportFactoryInterface {
 public:
  std::unique_ptr<cricket::SctpTransportInternal> CreateSctpTransport(
      rtc::PacketTransportInternal*) override {
    last_fake_sctp_transport_ = new FakeSctpTransport();
    return std::unique_ptr<cricket::SctpTransportInternal>(
        last_fake_sctp_transport_);
  }

  FakeSctpTransport* last_fake_sctp_transport() {
    return last_fake_sctp_transport_;
  }

 private:
  FakeSctpTransport* last_fake_sctp_transport_ = nullptr;
};

#endif  // TEST_PC_SCTP_FAKE_SCTP_TRANSPORT_H_
