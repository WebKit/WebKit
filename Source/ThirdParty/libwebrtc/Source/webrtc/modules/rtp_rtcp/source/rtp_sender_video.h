/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_

#include <list>
#include <memory>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/onetimeevent.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/rate_statistics.h"
#include "webrtc/base/sequenced_task_checker.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/flexfec_sender.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/ulpfec_generator.h"
#include "webrtc/modules/rtp_rtcp/source/video_codec_information.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class RtpPacketToSend;

class RTPSenderVideo {
 public:
  RTPSenderVideo(Clock* clock,
                 RTPSender* rtpSender,
                 FlexfecSender* flexfec_sender);
  virtual ~RTPSenderVideo();

  virtual RtpVideoCodecTypes VideoCodecType() const;

  size_t FecPacketOverhead() const {
    rtc::CritScope cs(&crit_);
    return CalculateFecPacketOverhead();
  }

  static RtpUtility::Payload* CreateVideoPayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      int8_t payload_type);

  bool SendVideo(RtpVideoCodecTypes video_type,
                 FrameType frame_type,
                 int8_t payload_type,
                 uint32_t capture_timestamp,
                 int64_t capture_time_ms,
                 const uint8_t* payload_data,
                 size_t payload_size,
                 const RTPFragmentationHeader* fragmentation,
                 const RTPVideoHeader* video_header);

  void SetVideoCodecType(RtpVideoCodecTypes type);

  // ULPFEC.
  void SetUlpfecConfig(int red_payload_type, int ulpfec_payload_type);
  void GetUlpfecConfig(int* red_payload_type, int* ulpfec_payload_type) const;

  // FlexFEC/ULPFEC.
  void SetFecParameters(const FecProtectionParams& delta_params,
                        const FecProtectionParams& key_params);

  // FlexFEC.
  rtc::Optional<uint32_t> FlexfecSsrc() const;

  uint32_t VideoBitrateSent() const;
  uint32_t FecOverheadRate() const;

  int SelectiveRetransmissions() const;
  void SetSelectiveRetransmissions(uint8_t settings);

 private:
  size_t CalculateFecPacketOverhead() const EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void SendVideoPacket(std::unique_ptr<RtpPacketToSend> packet,
                       StorageType storage);

  void SendVideoPacketAsRedMaybeWithUlpfec(
      std::unique_ptr<RtpPacketToSend> media_packet,
      StorageType media_packet_storage,
      bool protect_media_packet);

  // TODO(brandtr): Remove the FlexFEC functions when FlexfecSender has been
  // moved to PacedSender.
  void SendVideoPacketWithFlexfec(std::unique_ptr<RtpPacketToSend> media_packet,
                                  StorageType media_packet_storage,
                                  bool protect_media_packet);

  bool red_enabled() const EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    return red_payload_type_ >= 0;
  }

  bool ulpfec_enabled() const EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    return ulpfec_payload_type_ >= 0;
  }

  bool flexfec_enabled() const { return flexfec_sender_ != nullptr; }

  RTPSender* const rtp_sender_;
  Clock* const clock_;

  // Should never be held when calling out of this class.
  rtc::CriticalSection crit_;

  RtpVideoCodecTypes video_type_;
  int32_t retransmission_settings_ GUARDED_BY(crit_);
  VideoRotation last_rotation_ GUARDED_BY(crit_);

  // RED/ULPFEC.
  int red_payload_type_ GUARDED_BY(crit_);
  int ulpfec_payload_type_ GUARDED_BY(crit_);
  UlpfecGenerator ulpfec_generator_ GUARDED_BY(crit_);

  // FlexFEC.
  FlexfecSender* const flexfec_sender_;

  // FEC parameters, applicable to either ULPFEC or FlexFEC.
  FecProtectionParams delta_fec_params_ GUARDED_BY(crit_);
  FecProtectionParams key_fec_params_ GUARDED_BY(crit_);

  rtc::CriticalSection stats_crit_;
  // Bitrate used for FEC payload, RED headers, RTP headers for FEC packets
  // and any padding overhead.
  RateStatistics fec_bitrate_ GUARDED_BY(stats_crit_);
  // Bitrate used for video payload and RTP headers.
  RateStatistics video_bitrate_ GUARDED_BY(stats_crit_);
  OneTimeEvent first_frame_sent_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
