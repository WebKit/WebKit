/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_H_
#define WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_H_

#include <string>
#include <vector>

#include "webrtc/base/ignore_wundef.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

// Files generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace webrtc {

enum class MediaType;

class ParsedRtcEventLog {
  friend class RtcEventLogTestHelper;

 public:
  enum EventType {
    UNKNOWN_EVENT = 0,
    LOG_START = 1,
    LOG_END = 2,
    RTP_EVENT = 3,
    RTCP_EVENT = 4,
    AUDIO_PLAYOUT_EVENT = 5,
    LOSS_BASED_BWE_UPDATE = 6,
    DELAY_BASED_BWE_UPDATE = 7,
    VIDEO_RECEIVER_CONFIG_EVENT = 8,
    VIDEO_SENDER_CONFIG_EVENT = 9,
    AUDIO_RECEIVER_CONFIG_EVENT = 10,
    AUDIO_SENDER_CONFIG_EVENT = 11,
    AUDIO_NETWORK_ADAPTATION_EVENT = 16,
    BWE_PROBE_CLUSTER_CREATED_EVENT = 17,
    BWE_PROBE_RESULT_EVENT = 18
  };

  // Reads an RtcEventLog file and returns true if parsing was successful.
  bool ParseFile(const std::string& file_name);

  // Reads an RtcEventLog from a string and returns true if successful.
  bool ParseString(const std::string& s);

  // Reads an RtcEventLog from an istream and returns true if successful.
  bool ParseStream(std::istream& stream);

  // Returns the number of events in an EventStream.
  size_t GetNumberOfEvents() const;

  // Reads the arrival timestamp (in microseconds) from a rtclog::Event.
  int64_t GetTimestamp(size_t index) const;

  // Reads the event type of the rtclog::Event at |index|.
  EventType GetEventType(size_t index) const;

  // Reads the header, direction, media type, header length and packet length
  // from the RTP event at |index|, and stores the values in the corresponding
  // output parameters. Each output parameter can be set to nullptr if that
  // value isn't needed.
  // NB: The header must have space for at least IP_PACKET_SIZE bytes.
  void GetRtpHeader(size_t index,
                    PacketDirection* incoming,
                    MediaType* media_type,
                    uint8_t* header,
                    size_t* header_length,
                    size_t* total_length) const;

  // Reads packet, direction, media type and packet length from the RTCP event
  // at |index|, and stores the values in the corresponding output parameters.
  // Each output parameter can be set to nullptr if that value isn't needed.
  // NB: The packet must have space for at least IP_PACKET_SIZE bytes.
  void GetRtcpPacket(size_t index,
                     PacketDirection* incoming,
                     MediaType* media_type,
                     uint8_t* packet,
                     size_t* length) const;

  // Reads a config event to a (non-NULL) VideoReceiveStream::Config struct.
  // Only the fields that are stored in the protobuf will be written.
  void GetVideoReceiveConfig(size_t index,
                             VideoReceiveStream::Config* config) const;

  // Reads a config event to a (non-NULL) VideoSendStream::Config struct.
  // Only the fields that are stored in the protobuf will be written.
  void GetVideoSendConfig(size_t index, VideoSendStream::Config* config) const;

  // Reads a config event to a (non-NULL) AudioReceiveStream::Config struct.
  // Only the fields that are stored in the protobuf will be written.
  void GetAudioReceiveConfig(size_t index,
                             AudioReceiveStream::Config* config) const;

  // Reads a config event to a (non-NULL) AudioSendStream::Config struct.
  // Only the fields that are stored in the protobuf will be written.
  void GetAudioSendConfig(size_t index, AudioSendStream::Config* config) const;

  // Reads the SSRC from the audio playout event at |index|. The SSRC is stored
  // in the output parameter ssrc. The output parameter can be set to nullptr
  // and in that case the function only asserts that the event is well formed.
  void GetAudioPlayout(size_t index, uint32_t* ssrc) const;

  // Reads bitrate, fraction loss (as defined in RFC 1889) and total number of
  // expected packets from the loss based BWE event at |index| and stores the
  // values in
  // the corresponding output parameters. Each output parameter can be set to
  // nullptr if that
  // value isn't needed.
  void GetLossBasedBweUpdate(size_t index,
                             int32_t* bitrate_bps,
                             uint8_t* fraction_loss,
                             int32_t* total_packets) const;

  // Reads bitrate and detector_state from the delay based BWE event at |index|
  // and stores the values in the corresponding output parameters. Each output
  // parameter can be set to nullptr if that
  // value isn't needed.
  void GetDelayBasedBweUpdate(size_t index,
                              int32_t* bitrate_bps,
                              BandwidthUsage* detector_state) const;

  // Reads a audio network adaptation event to a (non-NULL)
  // AudioNetworkAdaptor::EncoderRuntimeConfig struct. Only the fields that are
  // stored in the protobuf will be written.
  void GetAudioNetworkAdaptation(
      size_t index,
      AudioNetworkAdaptor::EncoderRuntimeConfig* config) const;

 private:
  std::vector<rtclog::Event> events_;
};

}  // namespace webrtc

#endif  // WEBRTC_LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_H_
