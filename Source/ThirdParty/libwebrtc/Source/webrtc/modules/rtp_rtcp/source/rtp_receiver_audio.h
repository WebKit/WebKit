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
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

// Handles audio RTP packets. This class is thread-safe.
class RTPReceiverAudio : public RTPReceiverStrategy,
                         public TelephoneEventHandler {
 public:
  explicit RTPReceiverAudio(RtpData* data_callback);
  virtual ~RTPReceiverAudio() {}

  // The following three methods implement the TelephoneEventHandler interface.
  // Forward DTMFs to decoder for playout.
  void SetTelephoneEventForwardToDecoder(bool forward_to_decoder) override;

  // Is forwarding of outband telephone events turned on/off?
  bool TelephoneEventForwardToDecoder() const override;

  // Is TelephoneEvent configured with |payload_type|.
  bool TelephoneEventPayloadType(const int8_t payload_type) const override;

  TelephoneEventHandler* GetTelephoneEventHandler() override { return this; }

  // Returns true if CNG is configured with |payload_type|.
  bool CNGPayloadType(const int8_t payload_type);

  int32_t ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                         const PayloadUnion& specific_payload,
                         bool is_red,
                         const uint8_t* packet,
                         size_t payload_length,
                         int64_t timestamp_ms) override;

  RTPAliveType ProcessDeadOrAlive(uint16_t last_payload_length) const override;

  bool ShouldReportCsrcChanges(uint8_t payload_type) const override;

  int32_t OnNewPayloadTypeCreated(int payload_type,
                                  const SdpAudioFormat& audio_format) override;

  int32_t InvokeOnInitializeDecoder(
      RtpFeedback* callback,
      int8_t payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const PayloadUnion& specific_payload) const override;

  // We need to look out for special payload types here and sometimes reset
  // statistics. In addition we sometimes need to tweak the frequency.
  void CheckPayloadChanged(int8_t payload_type,
                           PayloadUnion* specific_payload,
                           bool* should_discard_changes) override;

  int Energy(uint8_t array_of_energy[kRtpCsrcSize]) const override;

 private:
  int32_t ParseAudioCodecSpecific(WebRtcRTPHeader* rtp_header,
                                  const uint8_t* payload_data,
                                  size_t payload_length,
                                  const AudioPayload& audio_specific,
                                  bool is_red);

  bool telephone_event_forward_to_decoder_;
  int8_t telephone_event_payload_type_;
  std::set<uint8_t> telephone_event_reported_;

  int8_t cng_nb_payload_type_;
  int8_t cng_wb_payload_type_;
  int8_t cng_swb_payload_type_;
  int8_t cng_fb_payload_type_;

  uint8_t num_energy_;
  uint8_t current_remote_energy_[kRtpCsrcSize];

  ThreadUnsafeOneTimeEvent first_packet_received_;
};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_
