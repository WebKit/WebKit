/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>

#include "pc/test/integration_test_helpers.h"

namespace webrtc {

class FuzzerTest : public PeerConnectionIntegrationBaseTest {
 public:
  FuzzerTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}

  void TestBody() override {}
};

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 16384) {
    return;
  }
  std::string message(reinterpret_cast<const char*>(data), size);

  FuzzerTest test;
  test.CreatePeerConnectionWrappers();
  // Note - we do not do test.ConnectFakeSignaling(); all signals
  // generated are discarded.

  auto srd_observer =
      rtc::make_ref_counted<MockSetSessionDescriptionObserver>();

  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp(
      CreateSessionDescription("offer", message, &error));
  // Note: This form of SRD takes ownership of the description.
  test.caller()->pc()->SetRemoteDescription(srd_observer, sdp.release());
  // Wait a short time for observer to be called. Timeout is short
  // because the fuzzer should be trying many branches.
  EXPECT_TRUE_WAIT(srd_observer->called(), 100);

  // If set-remote-description was successful, try to answer.
  auto sld_observer =
      rtc::make_ref_counted<MockSetSessionDescriptionObserver>();
  if (srd_observer->result()) {
    test.caller()->pc()->SetLocalDescription(sld_observer.get());
    EXPECT_TRUE_WAIT(sld_observer->called(), 100);
  }
}

}  // namespace webrtc
