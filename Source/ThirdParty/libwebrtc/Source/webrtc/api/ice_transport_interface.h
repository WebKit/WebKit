/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_ICE_TRANSPORT_INTERFACE_H_
#define API_ICE_TRANSPORT_INTERFACE_H_

#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "rtc_base/ref_count.h"

namespace cricket {
class IceTransportInternal;
}  // namespace cricket

namespace webrtc {

// An ICE transport, as represented to the outside world.
// This object is refcounted, and is therefore alive until the
// last holder has released it.
class IceTransportInterface : public rtc::RefCountInterface {
 public:
  // Accessor for the internal representation of an ICE transport.
  // The returned object can only be safely used on the signalling thread.
  // TODO(crbug.com/907849): Add API calls for the functions that have to
  // be exposed to clients, and stop allowing access to the
  // cricket::IceTransportInternal API.
  virtual cricket::IceTransportInternal* internal() = 0;
};

}  // namespace webrtc
#endif  // API_ICE_TRANSPORT_INTERFACE_H_
