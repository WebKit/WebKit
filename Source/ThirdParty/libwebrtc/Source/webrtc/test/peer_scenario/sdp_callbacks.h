/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_PEER_SCENARIO_SDP_CALLBACKS_H_
#define TEST_PEER_SCENARIO_SDP_CALLBACKS_H_

#include "api/peer_connection_interface.h"

// Helpers to allow usage of std::function/lambdas to observe SDP operation in
// the peer conenction API. As they only have handlers for sucess, failures will
// cause a crash.

namespace webrtc {
namespace test {
namespace webrtc_sdp_obs_impl {
class SdpSetObserversInterface : public SetSessionDescriptionObserver,
                                 public SetRemoteDescriptionObserverInterface {
};
}  // namespace webrtc_sdp_obs_impl

// Implementation of both SetSessionDescriptionObserver and
// SetRemoteDescriptionObserverInterface for use with SDP set operations. This
// return a raw owning pointer as it's only intended to be used as input to
// PeerConnection API which will take ownership.
webrtc_sdp_obs_impl::SdpSetObserversInterface* SdpSetObserver(
    std::function<void()> callback);

// Implementation of CreateSessionDescriptionObserver for use with SDP create
// operations. This return a raw owning pointer as it's only intended to be used
// as input to PeerConnection API which will take ownership.
CreateSessionDescriptionObserver* SdpCreateObserver(
    std::function<void(SessionDescriptionInterface*)> callback);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PEER_SCENARIO_SDP_CALLBACKS_H_
