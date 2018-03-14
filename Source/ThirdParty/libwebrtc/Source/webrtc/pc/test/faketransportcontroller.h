/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKETRANSPORTCONTROLLER_H_
#define PC_TEST_FAKETRANSPORTCONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "p2p/base/fakedtlstransport.h"
#include "p2p/base/fakeicetransport.h"
#include "pc/transportcontroller.h"
#include "rtc_base/bind.h"
#include "rtc_base/sslfingerprint.h"
#include "rtc_base/thread.h"

namespace cricket {

// Fake TransportController class, which can be passed into a WebRtcSession
// object for test purposes. Can be connected to other FakeTransportControllers
// via Connect().
//
// This fake is unusual in that for the most part, it's implemented with the
// real TransportController code, but with fake TransportChannels underneath.
class FakeTransportController : public TransportController {
 public:
  FakeTransportController()
      : TransportController(rtc::Thread::Current(),
                            rtc::Thread::Current(),
                            nullptr,
                            /*redetermine_role_on_ice_restart=*/true,
                            rtc::CryptoOptions()) {}

  explicit FakeTransportController(bool redetermine_role_on_ice_restart)
      : TransportController(rtc::Thread::Current(),
                            rtc::Thread::Current(),
                            nullptr,
                            redetermine_role_on_ice_restart,
                            rtc::CryptoOptions()) {}

  explicit FakeTransportController(IceRole role)
      : TransportController(rtc::Thread::Current(),
                            rtc::Thread::Current(),
                            nullptr,
                            /*redetermine_role_on_ice_restart=*/true,
                            rtc::CryptoOptions()) {
    SetIceRole(role);
  }

  explicit FakeTransportController(rtc::Thread* network_thread)
      : TransportController(rtc::Thread::Current(),
                            network_thread,
                            nullptr,
                            /*redetermine_role_on_ice_restart=*/true,
                            rtc::CryptoOptions()) {}

  FakeTransportController(rtc::Thread* network_thread, IceRole role)
      : TransportController(rtc::Thread::Current(),
                            network_thread,
                            nullptr,
                            /*redetermine_role_on_ice_restart=*/true,
                            rtc::CryptoOptions()) {
    SetIceRole(role);
  }

  FakeDtlsTransport* GetFakeDtlsTransport_n(const std::string& transport_name,
                                            int component) {
    return static_cast<FakeDtlsTransport*>(
        get_channel_for_testing(transport_name, component));
  }

  // Simulate the exchange of transport descriptions, and the gathering and
  // exchange of ICE candidates.
  void Connect(FakeTransportController* dest) {
    for (const std::string& transport_name : transport_names_for_testing()) {
      std::unique_ptr<rtc::SSLFingerprint> local_fingerprint;
      std::unique_ptr<rtc::SSLFingerprint> remote_fingerprint;
      if (certificate_for_testing()) {
        local_fingerprint.reset(rtc::SSLFingerprint::CreateFromCertificate(
            certificate_for_testing()));
      }
      if (dest->certificate_for_testing()) {
        remote_fingerprint.reset(rtc::SSLFingerprint::CreateFromCertificate(
            dest->certificate_for_testing()));
      }
      TransportDescription local_desc(
          std::vector<std::string>(),
          rtc::CreateRandomString(cricket::ICE_UFRAG_LENGTH),
          rtc::CreateRandomString(cricket::ICE_PWD_LENGTH),
          cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_NONE,
          local_fingerprint.get());
      TransportDescription remote_desc(
          std::vector<std::string>(),
          rtc::CreateRandomString(cricket::ICE_UFRAG_LENGTH),
          rtc::CreateRandomString(cricket::ICE_PWD_LENGTH),
          cricket::ICEMODE_FULL, cricket::CONNECTIONROLE_NONE,
          remote_fingerprint.get());
      std::string err;
      SetLocalTransportDescription(transport_name, local_desc,
                                   webrtc::SdpType::kOffer, &err);
      dest->SetRemoteTransportDescription(transport_name, local_desc,
                                          webrtc::SdpType::kOffer, &err);
      dest->SetLocalTransportDescription(transport_name, remote_desc,
                                         webrtc::SdpType::kAnswer, &err);
      SetRemoteTransportDescription(transport_name, remote_desc,
                                    webrtc::SdpType::kAnswer, &err);
    }
    MaybeStartGathering();
    dest->MaybeStartGathering();
    network_thread()->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&FakeTransportController::SetChannelDestinations_n, this,
                  dest));
  }

  void DestroyRtcpTransport(const std::string& transport_name) {
    DestroyDtlsTransport_n(transport_name,
                           cricket::ICE_CANDIDATE_COMPONENT_RTCP);
  }

 protected:
  IceTransportInternal* CreateIceTransportChannel_n(
      const std::string& transport_name,
      int component) override {
    return new FakeIceTransport(transport_name, component);
  }

  DtlsTransportInternal* CreateDtlsTransportChannel_n(
      const std::string& transport_name,
      int component,
      IceTransportInternal* ice) override {
    return new FakeDtlsTransport(static_cast<FakeIceTransport*>(ice));
  }

 private:
  void SetChannelDestinations_n(FakeTransportController* dest) {
    for (DtlsTransportInternal* tc : channels_for_testing()) {
      FakeDtlsTransport* local = static_cast<FakeDtlsTransport*>(tc);
      FakeDtlsTransport* remote = dest->GetFakeDtlsTransport_n(
          local->transport_name(), local->component());
      if (remote) {
        bool asymmetric = false;
        local->SetDestination(remote, asymmetric);
      }
    }
  }
};

}  // namespace cricket

#endif  // PC_TEST_FAKETRANSPORTCONTROLLER_H_
