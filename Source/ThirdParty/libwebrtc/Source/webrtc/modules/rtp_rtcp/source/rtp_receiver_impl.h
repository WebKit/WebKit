/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_IMPL_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "absl/types/optional.h"
#include "modules/rtp_rtcp/include/rtp_receiver.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/contributing_sources.h"
#include "modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

class RtpReceiverImpl : public RtpReceiver {
 public:
  // Callbacks passed in here may not be NULL (use Null Object callbacks if you
  // want callbacks to do nothing). This class takes ownership of the media
  // receiver but nothing else.
  RtpReceiverImpl(Clock* clock,
                  RTPPayloadRegistry* rtp_payload_registry,
                  RTPReceiverStrategy* rtp_media_receiver);

  ~RtpReceiverImpl() override;

  int32_t RegisterReceivePayload(int payload_type,
                                 const SdpAudioFormat& audio_format) override;
  int32_t RegisterReceivePayload(const VideoCodec& video_codec) override;

  int32_t DeRegisterReceivePayload(const int8_t payload_type) override;

  bool IncomingRtpPacket(const RTPHeader& rtp_header,
                         const uint8_t* payload,
                         size_t payload_length,
                         PayloadUnion payload_specific) override;

  bool GetLatestTimestamps(uint32_t* timestamp,
                           int64_t* receive_time_ms) const override;

  uint32_t SSRC() const override;

  std::vector<RtpSource> GetSources() const override;

 private:
  void CheckSSRCChanged(const RTPHeader& rtp_header);

  void UpdateSources(const absl::optional<uint8_t>& ssrc_audio_level);
  void RemoveOutdatedSources(int64_t now_ms);

  Clock* clock_;
  rtc::CriticalSection critical_section_rtp_receiver_;

  RTPPayloadRegistry* const rtp_payload_registry_
      RTC_PT_GUARDED_BY(critical_section_rtp_receiver_);
  const std::unique_ptr<RTPReceiverStrategy> rtp_media_receiver_;

  // SSRCs.
  uint32_t ssrc_ RTC_GUARDED_BY(critical_section_rtp_receiver_);

  ContributingSources csrcs_ RTC_GUARDED_BY(critical_section_rtp_receiver_);

  // Sequence number and timestamps for the latest in-order packet.
  absl::optional<uint16_t> last_received_sequence_number_
      RTC_GUARDED_BY(critical_section_rtp_receiver_);
  uint32_t last_received_timestamp_
      RTC_GUARDED_BY(critical_section_rtp_receiver_);
  int64_t last_received_frame_time_ms_
      RTC_GUARDED_BY(critical_section_rtp_receiver_);

  // The RtpSource objects are sorted chronologically.
  std::vector<RtpSource> ssrc_sources_;
};
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_IMPL_H_
