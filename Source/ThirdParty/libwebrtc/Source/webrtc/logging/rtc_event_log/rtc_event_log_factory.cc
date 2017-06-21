/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/logging/rtc_event_log/rtc_event_log_factory.h"

#include "webrtc/logging/rtc_event_log/rtc_event_log.h"

namespace webrtc {

std::unique_ptr<RtcEventLog> RtcEventLogFactory::CreateRtcEventLog() {
  return RtcEventLog::Create();
}

std::unique_ptr<RtcEventLogFactoryInterface> CreateRtcEventLogFactory() {
  return std::unique_ptr<RtcEventLogFactoryInterface>(new RtcEventLogFactory());
}

}  // namespace webrtc
