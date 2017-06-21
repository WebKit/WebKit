/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_H_
#define WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_H_

#include <memory>

#include "webrtc/logging/rtc_event_log/rtc_event_log_factory_interface.h"

namespace webrtc {

class RtcEventLogFactory : public RtcEventLogFactoryInterface {
 public:
  ~RtcEventLogFactory() override {}

  std::unique_ptr<RtcEventLog> CreateRtcEventLog() override;
};

std::unique_ptr<RtcEventLogFactoryInterface> CreateRtcEventLogFactory();
}  // namespace webrtc

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_H_
