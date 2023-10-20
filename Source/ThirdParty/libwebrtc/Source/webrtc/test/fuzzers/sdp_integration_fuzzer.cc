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
#include <stdlib.h>

#include "absl/strings/string_view.h"
#include "api/jsep.h"
#include "pc/test/integration_test_helpers.h"

namespace webrtc {

class FuzzerTest : public PeerConnectionIntegrationBaseTest {
 public:
  FuzzerTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}

  void RunNegotiateCycle(SdpType sdpType, absl::string_view message) {
    CreatePeerConnectionWrappers();
    // Note - we do not do test.ConnectFakeSignaling(); all signals
    // generated are discarded.

    auto srd_observer =
        rtc::make_ref_counted<FakeSetRemoteDescriptionObserver>();

    std::unique_ptr<SessionDescriptionInterface> sdp(
        CreateSessionDescription(sdpType, std::string(message)));
    caller()->pc()->SetRemoteDescription(std::move(sdp), srd_observer);
    // Wait a short time for observer to be called. Timeout is short
    // because the fuzzer should be trying many branches.
    EXPECT_TRUE_WAIT(srd_observer->called(), 100);

    // If set-remote-description was successful, try to answer.
    auto sld_observer =
        rtc::make_ref_counted<FakeSetLocalDescriptionObserver>();
    if (srd_observer->error().ok()) {
      caller()->pc()->SetLocalDescription(sld_observer);
      EXPECT_TRUE_WAIT(sld_observer->called(), 100);
    }
#if !defined(WEBRTC_WEBKIT_BUILD)
    // If there is an EXPECT failure, die here.
    RTC_CHECK(!HasFailure());
#endif // !defined(WEBRTC_WEBKIT_BUILD)
  }

  // This test isn't using the test definition macros, so we have to
  // define the TestBody() function, even though we don't need it.
  void TestBody() override {}
};

void FuzzOneInput(const uint8_t* data, size_t size) {
  uint8_t* newData = const_cast<uint8_t*>(data);
  size_t newSize = size;
  uint8_t type = 0;

  if (const char* var = getenv("SDP_TYPE")) {
    if (size > 16384) {
      return;
    }
    type = atoi(var);
  } else {
    if (size < 1 || size > 16385) {
      return;
    }
    type = data[0];
    newSize = size - 1;
    newData = reinterpret_cast<uint8_t*>(malloc(newSize));
    if (!newData)
      return;
    memcpy(newData, &data[1], newSize);
  }

  SdpType sdpType = SdpType::kOffer;
  switch (type % 4) {
    case 0: sdpType = SdpType::kOffer; break;
    case 1: sdpType = SdpType::kPrAnswer; break;
    case 2: sdpType = SdpType::kAnswer; break;
    case 3: sdpType = SdpType::kRollback; break;
  }

  FuzzerTest test;
  test.RunNegotiateCycle(
      sdpType,
      absl::string_view(reinterpret_cast<const char*>(newData), newSize));

  if (newData != data)
      free(newData);
}

}  // namespace webrtc
