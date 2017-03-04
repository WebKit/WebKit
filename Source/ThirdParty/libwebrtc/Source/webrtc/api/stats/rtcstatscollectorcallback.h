/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_STATS_RTCSTATSCOLLECTORCALLBACK_H_
#define WEBRTC_API_STATS_RTCSTATSCOLLECTORCALLBACK_H_

#include "webrtc/api/stats/rtcstatsreport.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace webrtc {

class RTCStatsCollectorCallback : public virtual rtc::RefCountInterface {
 public:
  virtual ~RTCStatsCollectorCallback() {}

  virtual void OnStatsDelivered(
      const rtc::scoped_refptr<const RTCStatsReport>& report) = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_API_STATS_RTCSTATSCOLLECTORCALLBACK_H_
