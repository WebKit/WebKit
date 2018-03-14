/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_H_

#include <map>
#include <string>
#include <utility>  // pair
#include <vector>

#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "logging/rtc_event_log/events/rtc_event_probe_result_failure.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/ignore_wundef.h"

// Files generated at build-time by the protobuf compiler.
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/logging/rtc_event_log/rtc_event_log.pb.h"
#else
#include "logging/rtc_event_log/rtc_event_log.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()

namespace webrtc {

enum class BandwidthUsage;
enum class MediaType;

struct AudioEncoderRuntimeConfig;

class ParsedRtcEventLog {
  friend class RtcEventLogTestHelper;

 public:
  struct BweProbeClusterCreatedEvent {
    uint64_t timestamp;
    uint32_t id;
    uint64_t bitrate_bps;
    uint32_t min_packets;
    uint32_t min_bytes;
  };

  struct BweProbeResultEvent {
    uint64_t timestamp;
    uint32_t id;
    rtc::Optional<uint64_t> bitrate_bps;
    rtc::Optional<ProbeFailureReason> failure_reason;
  };

  struct BweDelayBasedUpdate {
    uint64_t timestamp;
    int32_t bitrate_bps;
    BandwidthUsage detector_state;
  };

  struct AlrStateEvent {
    uint64_t timestamp;
    bool in_alr;
  };

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
    BWE_PROBE_RESULT_EVENT = 18,
    ALR_STATE_EVENT = 19
  };

  enum class MediaType { ANY, AUDIO, VIDEO, DATA };

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

  // Reads the header, direction, header length and packet length from the RTP
  // event at |index|, and stores the values in the corresponding output
  // parameters. Each output parameter can be set to nullptr if that value
  // isn't needed.
  // NB: The header must have space for at least IP_PACKET_SIZE bytes.
  // Returns: a pointer to a header extensions map acquired from parsing
  // corresponding Audio/Video Sender/Receiver config events.
  // Warning: if the same SSRC is reused by both video and audio streams during
  // call, extensions maps may be incorrect (the last one would be returned).
  webrtc::RtpHeaderExtensionMap* GetRtpHeader(size_t index,
                                              PacketDirection* incoming,
                                              uint8_t* header,
                                              size_t* header_length,
                                              size_t* total_length,
                                              int* probe_cluster_id) const;

  // Reads packet, direction and packet length from the RTCP event at |index|,
  // and stores the values in the corresponding output parameters.
  // Each output parameter can be set to nullptr if that value isn't needed.
  // NB: The packet must have space for at least IP_PACKET_SIZE bytes.
  void GetRtcpPacket(size_t index,
                     PacketDirection* incoming,
                     uint8_t* packet,
                     size_t* length) const;

  // Reads a video receive config event to a StreamConfig struct.
  // Only the fields that are stored in the protobuf will be written.
  rtclog::StreamConfig GetVideoReceiveConfig(size_t index) const;

  // Reads a video send config event to a StreamConfig struct. If the proto
  // contains multiple SSRCs and RTX SSRCs (this used to be the case for
  // simulcast streams) then we return one StreamConfig per SSRC,RTX_SSRC pair.
  // Only the fields that are stored in the protobuf will be written.
  std::vector<rtclog::StreamConfig> GetVideoSendConfig(size_t index) const;

  // Reads a audio receive config event to a StreamConfig struct.
  // Only the fields that are stored in the protobuf will be written.
  rtclog::StreamConfig GetAudioReceiveConfig(size_t index) const;

  // Reads a config event to a StreamConfig struct.
  // Only the fields that are stored in the protobuf will be written.
  rtclog::StreamConfig GetAudioSendConfig(size_t index) const;

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
  BweDelayBasedUpdate GetDelayBasedBweUpdate(size_t index) const;

  // Reads a audio network adaptation event to a (non-NULL)
  // AudioEncoderRuntimeConfig struct. Only the fields that are
  // stored in the protobuf will be written.
  void GetAudioNetworkAdaptation(size_t index,
                                 AudioEncoderRuntimeConfig* config) const;

  BweProbeClusterCreatedEvent GetBweProbeClusterCreated(size_t index) const;

  BweProbeResultEvent GetBweProbeResult(size_t index) const;

  MediaType GetMediaType(uint32_t ssrc, PacketDirection direction) const;

  AlrStateEvent GetAlrState(size_t index) const;

 private:
  rtclog::StreamConfig GetVideoReceiveConfig(const rtclog::Event& event) const;
  std::vector<rtclog::StreamConfig> GetVideoSendConfig(
      const rtclog::Event& event) const;
  rtclog::StreamConfig GetAudioReceiveConfig(const rtclog::Event& event) const;
  rtclog::StreamConfig GetAudioSendConfig(const rtclog::Event& event) const;

  std::vector<rtclog::Event> events_;

  struct Stream {
    Stream(uint32_t ssrc,
           MediaType media_type,
           webrtc::PacketDirection direction,
           webrtc::RtpHeaderExtensionMap map)
        : ssrc(ssrc),
          media_type(media_type),
          direction(direction),
          rtp_extensions_map(map) {}
    uint32_t ssrc;
    MediaType media_type;
    webrtc::PacketDirection direction;
    webrtc::RtpHeaderExtensionMap rtp_extensions_map;
  };

  // All configured streams found in the event log.
  std::vector<Stream> streams_;

  // To find configured extensions map for given stream, what are needed to
  // parse a header.
  typedef std::pair<uint32_t, webrtc::PacketDirection> StreamId;
  std::map<StreamId, webrtc::RtpHeaderExtensionMap*> rtp_extensions_maps_;
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_LOG_PARSER_H_
