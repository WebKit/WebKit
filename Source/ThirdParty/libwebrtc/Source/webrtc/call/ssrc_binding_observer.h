/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_SSRC_BINDING_OBSERVER_H_
#define CALL_SSRC_BINDING_OBSERVER_H_

#include <string>

#include "rtc_base/basictypes.h"

namespace webrtc {

// With newer versions of SDP, SSRC is often not explicitly signaled and must
// be learned on the fly. This happens by correlating packet SSRCs with included
// RTP extension headers like MID and RSID, or by receiving information from
// RTCP messages.
// SsrcBindingObservers will be notified when a new binding is learned, which
// can happen during call setup and/or during the call.
class SsrcBindingObserver {
 public:
  virtual ~SsrcBindingObserver() = default;

  virtual void OnSsrcBoundToRsid(const std::string& rsid, uint32_t ssrc) {}

  virtual void OnSsrcBoundToMid(const std::string& mid, uint32_t ssrc) {}

  virtual void OnSsrcBoundToMidRsid(const std::string& mid,
                                    const std::string& rsid,
                                    uint32_t ssrc) {}

  virtual void OnSsrcBoundToPayloadType(uint8_t payload_type, uint32_t ssrc) {}
};

}  // namespace webrtc

#endif  // CALL_SSRC_BINDING_OBSERVER_H_
