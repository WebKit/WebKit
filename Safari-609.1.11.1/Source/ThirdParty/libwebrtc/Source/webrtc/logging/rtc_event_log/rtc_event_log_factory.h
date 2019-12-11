/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_H_

#include <memory>

#include "logging/rtc_event_log/rtc_event_log_factory_interface.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

class RtcEventLogFactory : public RtcEventLogFactoryInterface {
 public:
  ~RtcEventLogFactory() override {}

  std::unique_ptr<RtcEventLog> CreateRtcEventLog(
      RtcEventLog::EncodingType encoding_type) override;

  std::unique_ptr<RtcEventLog> CreateRtcEventLog(
      RtcEventLog::EncodingType encoding_type,
      std::unique_ptr<rtc::TaskQueue> task_queue) override;
};

std::unique_ptr<RtcEventLogFactoryInterface> CreateRtcEventLogFactory();
}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_H_
