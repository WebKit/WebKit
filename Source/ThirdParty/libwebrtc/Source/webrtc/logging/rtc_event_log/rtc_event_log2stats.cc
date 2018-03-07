/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <inttypes.h>
#include <stdio.h>

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "logging/rtc_event_log/rtc_event_log.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/logging.h"

// Files generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "logging/rtc_event_log/rtc_event_log.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace {

DEFINE_bool(help, false, "Prints this message.");

struct Stats {
  int count = 0;
  size_t total_size = 0;
};

// We are duplicating some parts of the parser here because we want to get
// access to raw protobuf events.
std::pair<uint64_t, bool> ParseVarInt(std::istream& stream) {
  uint64_t varint = 0;
  for (size_t bytes_read = 0; bytes_read < 10; ++bytes_read) {
    // The most significant bit of each byte is 0 if it is the last byte in
    // the varint and 1 otherwise. Thus, we take the 7 least significant bits
    // of each byte and shift them 7 bits for each byte read previously to get
    // the (unsigned) integer.
    int byte = stream.get();
    if (stream.eof()) {
      return std::make_pair(varint, false);
    }
    RTC_DCHECK_GE(byte, 0);
    RTC_DCHECK_LE(byte, 255);
    varint |= static_cast<uint64_t>(byte & 0x7F) << (7 * bytes_read);
    if ((byte & 0x80) == 0) {
      return std::make_pair(varint, true);
    }
  }
  return std::make_pair(varint, false);
}

bool ParseEvents(const std::string& filename,
                 std::vector<webrtc::rtclog::Event>* events) {
  std::ifstream stream(filename, std::ios_base::in | std::ios_base::binary);
  if (!stream.good() || !stream.is_open()) {
    RTC_LOG(LS_WARNING) << "Could not open file for reading.";
    return false;
  }

  const size_t kMaxEventSize = (1u << 16) - 1;
  std::vector<char> tmp_buffer(kMaxEventSize);
  uint64_t tag;
  uint64_t message_length;
  bool success;

  RTC_DCHECK(stream.good());

  while (1) {
    // Check whether we have reached end of file.
    stream.peek();
    if (stream.eof()) {
      return true;
    }

    // Read the next message tag. The tag number is defined as
    // (fieldnumber << 3) | wire_type. In our case, the field number is
    // supposed to be 1 and the wire type for an length-delimited field is 2.
    const uint64_t kExpectedTag = (1 << 3) | 2;
    std::tie(tag, success) = ParseVarInt(stream);
    if (!success) {
      RTC_LOG(LS_WARNING)
          << "Missing field tag from beginning of protobuf event.";
      return false;
    } else if (tag != kExpectedTag) {
      RTC_LOG(LS_WARNING)
          << "Unexpected field tag at beginning of protobuf event.";
      return false;
    }

    // Read the length field.
    std::tie(message_length, success) = ParseVarInt(stream);
    if (!success) {
      RTC_LOG(LS_WARNING) << "Missing message length after protobuf field tag.";
      return false;
    } else if (message_length > kMaxEventSize) {
      RTC_LOG(LS_WARNING) << "Protobuf message length is too large.";
      return false;
    }

    // Read the next protobuf event to a temporary char buffer.
    stream.read(tmp_buffer.data(), message_length);
    if (stream.gcount() != static_cast<int>(message_length)) {
      RTC_LOG(LS_WARNING) << "Failed to read protobuf message from file.";
      return false;
    }

    // Parse the protobuf event from the buffer.
    webrtc::rtclog::Event event;
    if (!event.ParseFromArray(tmp_buffer.data(), message_length)) {
      RTC_LOG(LS_WARNING) << "Failed to parse protobuf message.";
      return false;
    }
    events->push_back(event);
  }
}

// TODO(terelius): Should this be placed in some utility file instead?
std::string EventTypeToString(webrtc::rtclog::Event::EventType event_type) {
  switch (event_type) {
    case webrtc::rtclog::Event::UNKNOWN_EVENT:
      return "UNKNOWN_EVENT";
    case webrtc::rtclog::Event::LOG_START:
      return "LOG_START";
    case webrtc::rtclog::Event::LOG_END:
      return "LOG_END";
    case webrtc::rtclog::Event::RTP_EVENT:
      return "RTP_EVENT";
    case webrtc::rtclog::Event::RTCP_EVENT:
      return "RTCP_EVENT";
    case webrtc::rtclog::Event::AUDIO_PLAYOUT_EVENT:
      return "AUDIO_PLAYOUT_EVENT";
    case webrtc::rtclog::Event::LOSS_BASED_BWE_UPDATE:
      return "LOSS_BASED_BWE_UPDATE";
    case webrtc::rtclog::Event::DELAY_BASED_BWE_UPDATE:
      return "DELAY_BASED_BWE_UPDATE";
    case webrtc::rtclog::Event::VIDEO_RECEIVER_CONFIG_EVENT:
      return "VIDEO_RECV_CONFIG";
    case webrtc::rtclog::Event::VIDEO_SENDER_CONFIG_EVENT:
      return "VIDEO_SEND_CONFIG";
    case webrtc::rtclog::Event::AUDIO_RECEIVER_CONFIG_EVENT:
      return "AUDIO_RECV_CONFIG";
    case webrtc::rtclog::Event::AUDIO_SENDER_CONFIG_EVENT:
      return "AUDIO_SEND_CONFIG";
    case webrtc::rtclog::Event::AUDIO_NETWORK_ADAPTATION_EVENT:
      return "AUDIO_NETWORK_ADAPTATION";
    case webrtc::rtclog::Event::BWE_PROBE_CLUSTER_CREATED_EVENT:
      return "BWE_PROBE_CREATED";
    case webrtc::rtclog::Event::BWE_PROBE_RESULT_EVENT:
      return "BWE_PROBE_RESULT";
  }
  RTC_NOTREACHED();
  return "UNKNOWN_EVENT";
}

}  // namespace

// This utility will print basic information about each packet to stdout.
// Note that parser will assert if the protobuf event is missing some required
// fields and we attempt to access them. We don't handle this at the moment.
int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage =
      "Tool for file usage statistics from an RtcEventLog.\n"
      "Run " +
      program_name +
      " --help for usage.\n"
      "Example usage:\n" +
      program_name + " input.rel\n";
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true) ||
      FLAG_help || argc != 2) {
    std::cout << usage;
    if (FLAG_help) {
      rtc::FlagList::Print(nullptr, false);
      return 0;
    }
    return 1;
  }
  std::string file_name = argv[1];

  std::vector<webrtc::rtclog::Event> events;
  if (!ParseEvents(file_name, &events)) {
    RTC_LOG(LS_ERROR) << "Failed to parse event log.";
    return -1;
  }

  // Get file size
  FILE* file = fopen(file_name.c_str(), "rb");
  fseek(file, 0L, SEEK_END);
  int64_t file_size = ftell(file);
  fclose(file);

  // We are deliberately using low level protobuf functions to get the stats
  // since the convenience functions in the parser would CHECK that the events
  // are well formed.
  std::map<webrtc::rtclog::Event::EventType, Stats> stats;
  int malformed_events = 0;
  size_t malformed_event_size = 0;
  size_t accumulated_event_size = 0;
  for (const webrtc::rtclog::Event& event : events) {
    size_t serialized_size = event.ByteSize();
    // When the event is written on the disk, it is part of an EventStream
    // object. The event stream will prepend a 1 byte field number/wire type,
    // and a varint encoding (base 128) of the event length.
    serialized_size =
        1 + (1 + (serialized_size > 127) + (serialized_size > 16383)) +
        serialized_size;

    if (event.has_type() && event.has_timestamp_us()) {
      stats[event.type()].count++;
      stats[event.type()].total_size += serialized_size;
    } else {
      // The event is missing the type or the timestamp field.
      malformed_events++;
      malformed_event_size += serialized_size;
    }
    accumulated_event_size += serialized_size;
  }

  printf("Type                  \tCount\tTotal size\tAverage size\tPercent\n");
  printf(
      "-----------------------------------------------------------------------"
      "\n");
  for (const auto it : stats) {
    printf("%-22s\t%5d\t%10zu\t%12.2lf\t%7.2lf\n",
           EventTypeToString(it.first).c_str(), it.second.count,
           it.second.total_size,
           static_cast<double>(it.second.total_size) / it.second.count,
           static_cast<double>(it.second.total_size) / file_size * 100);
  }
  if (malformed_events != 0) {
    printf("%-22s\t%5d\t%10zu\t%12.2lf\t%7.2lf\n", "MALFORMED",
           malformed_events, malformed_event_size,
           static_cast<double>(malformed_event_size) / malformed_events,
           static_cast<double>(malformed_event_size) / file_size * 100);
  }
  if (file_size - accumulated_event_size != 0) {
    printf("WARNING: %" PRId64 " bytes not accounted for\n",
           file_size - accumulated_event_size);
  }

  return 0;
}
