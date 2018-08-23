/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_

#include <set>

#include "modules/rtp_rtcp/include/rtp_receiver.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "rtc_base/onetimeevent.h"

namespace webrtc {

// Handles audio RTP packets. This class is thread-safe.
class RTPReceiverAudio : public RTPReceiverStrategy {
 public:
  explicit RTPReceiverAudio(RtpData* data_callback);
  ~RTPReceiverAudio() override;

  int32_t ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                         const PayloadUnion& specific_payload,
                         const uint8_t* packet,
                         size_t payload_length,
                         int64_t timestamp_ms) override;

 private:
  int32_t ParseAudioCodecSpecific(WebRtcRTPHeader* rtp_header,
                                  const uint8_t* payload_data,
                                  size_t payload_length,
                                  const AudioPayload& audio_specific);

  ThreadUnsafeOneTimeEvent first_packet_received_;
};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_
