/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_SET_LOCAL_DESCRIPTION_OBSERVER_INTERFACE_H_
#define API_SET_LOCAL_DESCRIPTION_OBSERVER_INTERFACE_H_

#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/ref_count.h"
#include "api/rtc_error.h"

namespace webrtc {

// OnSetLocalDescriptionComplete() invokes as soon as
// PeerConnectionInterface::SetLocalDescription() operation completes, allowing
// the observer to examine the effects of the operation without delay.
class SetLocalDescriptionObserverInterface : public RefCountInterface {
 public:
  // On success, `error.ok()` is true.
  virtual void OnSetLocalDescriptionComplete(RTCError error) = 0;

  // An embedding that delivers the observer result asynchronously may defer
  // invoking `callback` until the result has reached its API consumer. The
  // callback must be invoked exactly once on the operation's sequence.
  virtual void OnOperationComplete(absl::AnyInvocable<void() &&> callback) {
    std::move(callback)();
  }
};

}  // namespace webrtc

#endif  // API_SET_LOCAL_DESCRIPTION_OBSERVER_INTERFACE_H_
