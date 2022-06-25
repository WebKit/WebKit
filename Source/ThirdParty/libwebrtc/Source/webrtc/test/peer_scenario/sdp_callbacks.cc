/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/peer_scenario/sdp_callbacks.h"

#include <utility>

namespace webrtc {
namespace test {

webrtc_sdp_obs_impl::SdpSetObserversInterface* SdpSetObserver(
    std::function<void()> callback) {
  class SdpSetObserver : public webrtc_sdp_obs_impl::SdpSetObserversInterface {
   public:
    explicit SdpSetObserver(std::function<void()> callback)
        : callback_(std::move(callback)) {}
    void OnSuccess() override { callback_(); }
    void OnFailure(RTCError error) override {
      RTC_NOTREACHED() << error.message();
    }
    void OnSetRemoteDescriptionComplete(RTCError error) override {
      RTC_CHECK(error.ok()) << error.message();
      callback_();
    }
    std::function<void()> callback_;
  };
  return new rtc::RefCountedObject<SdpSetObserver>(std::move(callback));
}

CreateSessionDescriptionObserver* SdpCreateObserver(
    std::function<void(SessionDescriptionInterface*)> callback) {
  class SdpCreateObserver : public CreateSessionDescriptionObserver {
   public:
    explicit SdpCreateObserver(decltype(callback) callback)
        : callback_(std::move(callback)) {}
    void OnSuccess(SessionDescriptionInterface* desc) override {
      callback_(desc);
    }
    void OnFailure(RTCError error) override {
      RTC_NOTREACHED() << error.message();
    }
    decltype(callback) callback_;
  };
  return new rtc::RefCountedObject<SdpCreateObserver>(std::move(callback));
}

}  // namespace test
}  // namespace webrtc
