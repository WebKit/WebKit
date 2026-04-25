/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_end_log.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_field_encoding.h"
#include "logging/rtc_event_log/events/rtc_event_field_encoding_parser.h"
#include "logging/rtc_event_log/events/rtc_event_log_parse_status.h"

namespace webrtc {

RtcEventEndLog::RtcEventEndLog(Timestamp timestamp)
    : RtcEvent(timestamp.us()) {}

RtcEventEndLog::~RtcEventEndLog() = default;

std::string RtcEventEndLog::Encode(ArrayView<const RtcEvent*> batch) {
  EventEncoder encoder(event_params_, batch);
  return encoder.AsString();
}

RtcEventLogParseStatus RtcEventEndLog::Parse(
    absl::string_view encoded_bytes,
    bool batched,
    std::vector<LoggedStopEvent>& output) {
  EventParser parser;
  auto status = parser.Initialize(encoded_bytes, batched);
  if (!status.ok())
    return status;

  ArrayView<LoggedStopEvent> output_batch =
      ExtendLoggedBatch(output, parser.NumEventsInBatch());

  constexpr FieldParameters timestamp_params{
      .name = "timestamp_ms",
      .field_id = FieldParameters::kTimestampField,
      .field_type = FieldType::kVarInt,
      .value_width = 64};
  RtcEventLogParseStatusOr<ArrayView<uint64_t>> result =
      parser.ParseNumericField(timestamp_params);
  if (!result.ok())
    return result.status();
  status = PopulateRtcEventTimestamp(result.value(),
                                     &LoggedStopEvent::timestamp, output_batch);
  if (!status.ok())
    return status;

  return RtcEventLogParseStatus::Success();
}

}  // namespace webrtc
