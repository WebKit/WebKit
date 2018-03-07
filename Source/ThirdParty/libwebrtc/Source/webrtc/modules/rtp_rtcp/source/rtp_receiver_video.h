/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/onetimeevent.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class RTPReceiverVideo : public RTPReceiverStrategy {
 public:
  explicit RTPReceiverVideo(RtpData* data_callback);

  virtual ~RTPReceiverVideo();

  int32_t ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                         const PayloadUnion& specific_payload,
                         bool is_red,
                         const uint8_t* packet,
                         size_t packet_length,
                         int64_t timestamp) override;

  TelephoneEventHandler* GetTelephoneEventHandler() override { return NULL; }

  RTPAliveType ProcessDeadOrAlive(uint16_t last_payload_length) const override;

  bool ShouldReportCsrcChanges(uint8_t payload_type) const override;

  int32_t OnNewPayloadTypeCreated(int payload_type,
                                  const SdpAudioFormat& audio_format) override;

  int32_t InvokeOnInitializeDecoder(
      RtpFeedback* callback,
      int8_t payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const PayloadUnion& specific_payload) const override;

  void SetPacketOverHead(uint16_t packet_over_head);

 private:
  OneTimeEvent first_packet_received_;
};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
