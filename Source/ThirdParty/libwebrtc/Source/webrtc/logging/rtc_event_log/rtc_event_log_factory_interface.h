/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_INTERFACE_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_INTERFACE_H_

#include <memory>

#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/task_queue.h"

namespace webrtc {

// This interface exists to allow webrtc to be optionally built without
// RtcEventLog support. A PeerConnectionFactory is constructed with an
// RtcEventLogFactoryInterface, which may or may not be null.
class RtcEventLogFactoryInterface {
 public:
  virtual ~RtcEventLogFactoryInterface() {}

  virtual std::unique_ptr<RtcEventLog> CreateRtcEventLog(
      RtcEventLog::EncodingType encoding_type) = 0;

  virtual std::unique_ptr<RtcEventLog> CreateRtcEventLog(
      RtcEventLog::EncodingType encoding_type,
      std::unique_ptr<rtc::TaskQueue> task_queue) = 0;
};

std::unique_ptr<RtcEventLogFactoryInterface> CreateRtcEventLogFactory();

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_FACTORY_INTERFACE_H_
