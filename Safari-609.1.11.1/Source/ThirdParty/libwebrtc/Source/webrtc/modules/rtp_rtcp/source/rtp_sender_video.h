/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_

#include <map>
#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/rtp_rtcp/include/flexfec_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/playout_delay_oracle.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "modules/rtp_rtcp/source/rtp_sequence_number_map.h"
#include "modules/rtp_rtcp/source/ulpfec_generator.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/one_time_event.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class FrameEncryptorInterface;
class RtpPacketizer;
class RtpPacketToSend;

// kConditionallyRetransmitHigherLayers allows retransmission of video frames
// in higher layers if either the last frame in that layer was too far back in
// time, or if we estimate that a new frame will be available in a lower layer
// in a shorter time than it would take to request and receive a retransmission.
enum RetransmissionMode : uint8_t {
  kRetransmitOff = 0x0,
  kRetransmitBaseLayer = 0x2,
  kRetransmitHigherLayers = 0x4,
  kConditionallyRetransmitHigherLayers = 0x8,
};

class RTPSenderVideo {
 public:
  static constexpr int64_t kTLRateWindowSizeMs = 2500;

  RTPSenderVideo(Clock* clock,
                 RTPSender* rtpSender,
                 FlexfecSender* flexfec_sender,
                 PlayoutDelayOracle* playout_delay_oracle,
                 FrameEncryptorInterface* frame_encryptor,
                 bool require_frame_encryption,
                 bool need_rtp_packet_infos,
                 const WebRtcKeyValueConfig& field_trials);
  virtual ~RTPSenderVideo();

  // expected_retransmission_time_ms.has_value() -> retransmission allowed.
  bool SendVideo(VideoFrameType frame_type,
                 int8_t payload_type,
                 uint32_t capture_timestamp,
                 int64_t capture_time_ms,
                 const uint8_t* payload_data,
                 size_t payload_size,
                 const RTPFragmentationHeader* fragmentation,
                 const RTPVideoHeader* video_header,
                 absl::optional<int64_t> expected_retransmission_time_ms);

  void RegisterPayloadType(int8_t payload_type,
                           absl::string_view payload_name,
                           bool raw_payload);

  // Set RED and ULPFEC payload types. A payload type of -1 means that the
  // corresponding feature is turned off. Note that we DO NOT support enabling
  // ULPFEC without enabling RED, and RED is only ever used when ULPFEC is
  // enabled.
  void SetUlpfecConfig(int red_payload_type, int ulpfec_payload_type);

  // FlexFEC/ULPFEC.
  // Set FEC rates, max frames before FEC is sent, and type of FEC masks.
  // Returns false on failure.
  void SetFecParameters(const FecProtectionParams& delta_params,
                        const FecProtectionParams& key_params);

  // FlexFEC.
  absl::optional<uint32_t> FlexfecSsrc() const;

  uint32_t VideoBitrateSent() const;
  uint32_t FecOverheadRate() const;

  // Returns the current packetization overhead rate, in bps. Note that this is
  // the payload overhead, eg the VP8 payload headers, not the RTP headers
  // or extension/
  uint32_t PacketizationOverheadBps() const;

  // For each sequence number in |sequence_number|, recall the last RTP packet
  // which bore it - its timestamp and whether it was the first and/or last
  // packet in that frame. If all of the given sequence numbers could be
  // recalled, return a vector with all of them (in corresponding order).
  // If any could not be recalled, return an empty vector.
  std::vector<RtpSequenceNumberMap::Info> GetSentRtpPacketInfos(
      rtc::ArrayView<const uint16_t> sequence_numbers) const;

 protected:
  static uint8_t GetTemporalId(const RTPVideoHeader& header);
  bool AllowRetransmission(uint8_t temporal_id,
                           int32_t retransmission_settings,
                           int64_t expected_retransmission_time_ms);

 private:
  struct TemporalLayerStats {
    TemporalLayerStats()
        : frame_rate_fp1000s(kTLRateWindowSizeMs, 1000 * 1000),
          last_frame_time_ms(0) {}
    // Frame rate, in frames per 1000 seconds. This essentially turns the fps
    // value into a fixed point value with three decimals. Improves precision at
    // low frame rates.
    RateStatistics frame_rate_fp1000s;
    int64_t last_frame_time_ms;
  };

  size_t CalculateFecPacketOverhead() const RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void SendVideoPacket(std::unique_ptr<RtpPacketToSend> packet);

  void SendVideoPacketAsRedMaybeWithUlpfec(
      std::unique_ptr<RtpPacketToSend> media_packet,
      bool protect_media_packet);

  // TODO(brandtr): Remove the FlexFEC functions when FlexfecSender has been
  // moved to PacedSender.
  void SendVideoPacketWithFlexfec(std::unique_ptr<RtpPacketToSend> media_packet,
                                  bool protect_media_packet);

  bool LogAndSendToNetwork(std::unique_ptr<RtpPacketToSend> packet);

  bool red_enabled() const RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    return red_payload_type_ >= 0;
  }

  bool ulpfec_enabled() const RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    return ulpfec_payload_type_ >= 0;
  }

  bool flexfec_enabled() const { return flexfec_sender_ != nullptr; }

  bool UpdateConditionalRetransmit(uint8_t temporal_id,
                                   int64_t expected_retransmission_time_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stats_crit_);

  RTPSender* const rtp_sender_;
  Clock* const clock_;

  // Maps payload type to codec type, for packetization.
  // TODO(nisse): Set on construction, to avoid lock.
  rtc::CriticalSection payload_type_crit_;
  std::map<int8_t, absl::optional<VideoCodecType>> payload_type_map_
      RTC_GUARDED_BY(payload_type_crit_);

  // Should never be held when calling out of this class.
  rtc::CriticalSection crit_;

  int32_t retransmission_settings_ RTC_GUARDED_BY(crit_);
  VideoRotation last_rotation_ RTC_GUARDED_BY(crit_);
  absl::optional<ColorSpace> last_color_space_ RTC_GUARDED_BY(crit_);
  bool transmit_color_space_next_frame_ RTC_GUARDED_BY(crit_);
  // Tracks the current request for playout delay limits from application
  // and decides whether the current RTP frame should include the playout
  // delay extension on header.
  PlayoutDelayOracle* const playout_delay_oracle_;

  // Maps sent packets' sequence numbers to a tuple consisting of:
  // 1. The timestamp, without the randomizing offset mandated by the RFC.
  // 2. Whether the packet was the first in its frame.
  // 3. Whether the packet was the last in its frame.
  const std::unique_ptr<RtpSequenceNumberMap> rtp_sequence_number_map_
      RTC_PT_GUARDED_BY(crit_);

  // RED/ULPFEC.
  int red_payload_type_ RTC_GUARDED_BY(crit_);
  int ulpfec_payload_type_ RTC_GUARDED_BY(crit_);
  UlpfecGenerator ulpfec_generator_ RTC_GUARDED_BY(crit_);

  // FlexFEC.
  FlexfecSender* const flexfec_sender_;

  // FEC parameters, applicable to either ULPFEC or FlexFEC.
  FecProtectionParams delta_fec_params_ RTC_GUARDED_BY(crit_);
  FecProtectionParams key_fec_params_ RTC_GUARDED_BY(crit_);

  rtc::CriticalSection stats_crit_;
  // Bitrate used for FEC payload, RED headers, RTP headers for FEC packets
  // and any padding overhead.
  RateStatistics fec_bitrate_ RTC_GUARDED_BY(stats_crit_);
  // Bitrate used for video payload and RTP headers.
  RateStatistics video_bitrate_ RTC_GUARDED_BY(stats_crit_);
  RateStatistics packetization_overhead_bitrate_ RTC_GUARDED_BY(stats_crit_);

  std::map<int, TemporalLayerStats> frame_stats_by_temporal_layer_
      RTC_GUARDED_BY(stats_crit_);

  OneTimeEvent first_frame_sent_;

  // E2EE Custom Video Frame Encryptor (optional)
  FrameEncryptorInterface* const frame_encryptor_ = nullptr;
  // If set to true will require all outgoing frames to pass through an
  // initialized frame_encryptor_ before being sent out of the network.
  // Otherwise these payloads will be dropped.
  bool require_frame_encryption_;
  // Set to true if the generic descriptor should be authenticated.
  const bool generic_descriptor_auth_experiment_;

  const bool exclude_transport_sequence_number_from_fec_experiment_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
