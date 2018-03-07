/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKESCTPTRANSPORT_H_
#define PC_TEST_FAKESCTPTRANSPORT_H_

#include <memory>

#include "media/sctp/sctptransportinternal.h"

// Used for tests in this file to verify that PeerConnection responds to signals
// from the SctpTransport correctly, and calls Start with the correct
// local/remote ports.
class FakeSctpTransport : public cricket::SctpTransportInternal {
 public:
  void SetTransportChannel(rtc::PacketTransportInternal* channel) override {}
  bool Start(int local_port, int remote_port) override {
    local_port_.emplace(local_port);
    remote_port_.emplace(remote_port);
    return true;
  }
  bool OpenStream(int sid) override { return true; }
  bool ResetStream(int sid) override { return true; }
  bool SendData(const cricket::SendDataParams& params,
                const rtc::CopyOnWriteBuffer& payload,
                cricket::SendDataResult* result = nullptr) override {
    return true;
  }
  bool ReadyToSendData() override { return true; }
  void set_debug_name_for_testing(const char* debug_name) override {}

  int local_port() const { return *local_port_; }
  int remote_port() const { return *remote_port_; }

 private:
  rtc::Optional<int> local_port_;
  rtc::Optional<int> remote_port_;
};

class FakeSctpTransportFactory : public cricket::SctpTransportInternalFactory {
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

#endif  // PC_TEST_FAKESCTPTRANSPORT_H_
