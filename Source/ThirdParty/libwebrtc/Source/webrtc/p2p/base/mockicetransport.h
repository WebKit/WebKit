/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_MOCKICETRANSPORT_H_
#define WEBRTC_P2P_BASE_MOCKICETRANSPORT_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/gunit.h"
#include "webrtc/p2p/base/icetransportinternal.h"
#include "webrtc/test/gmock.h"

using testing::_;
using testing::Return;

namespace cricket {

// Used in Chromium/remoting/protocol/channel_socket_adapter_unittest.cc
class MockIceTransport : public IceTransportInternal {
 public:
  MockIceTransport() {
    SignalReadyToSend(this);
    SignalWritableState(this);
  }

  MOCK_METHOD4(SendPacket,
               int(const char* data,
                   size_t len,
                   const rtc::PacketOptions& options,
                   int flags));
  MOCK_METHOD2(SetOption, int(rtc::Socket::Option opt, int value));
  MOCK_METHOD0(GetError, int());
  MOCK_CONST_METHOD0(GetIceRole, cricket::IceRole());
  MOCK_METHOD1(GetStats, bool(cricket::ConnectionInfos* infos));
  MOCK_CONST_METHOD0(IsDtlsActive, bool());
  MOCK_CONST_METHOD1(GetSslRole, bool(rtc::SSLRole* role));

  IceTransportState GetState() const override {
    return IceTransportState::STATE_INIT;
  }
  const std::string& transport_name() const override { return transport_name_; }
  int component() const override { return 0; }
  void SetIceRole(IceRole role) override {}
  void SetIceTiebreaker(uint64_t tiebreaker) override {}
  // The ufrag and pwd in |ice_params| must be set
  // before candidate gathering can start.
  void SetIceParameters(const IceParameters& ice_params) override {}
  void SetRemoteIceParameters(const IceParameters& ice_params) override {}
  void SetRemoteIceMode(IceMode mode) override {}
  void SetIceConfig(const IceConfig& config) override {}
  rtc::Optional<int> GetRttEstimate() override {
    return rtc::Optional<int>();
  }
  void MaybeStartGathering() override {}
  void SetMetricsObserver(webrtc::MetricsObserverInterface* observer) override {
  }
  void AddRemoteCandidate(const Candidate& candidate) override {}
  void RemoveRemoteCandidate(const Candidate& candidate) override {}
  IceGatheringState gathering_state() const override {
    return IceGatheringState::kIceGatheringComplete;
  }

  bool receiving() const override { return true; }
  bool writable() const override { return true; }

 private:
  std::string transport_name_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_MOCKICETRANSPORT_H_
