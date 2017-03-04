/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_STRATEGY_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_STRATEGY_H_

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct CodecInst;

class TelephoneEventHandler;

// This strategy deals with media-specific RTP packet processing.
// This class is not thread-safe and must be protected by its caller.
class RTPReceiverStrategy {
 public:
  static RTPReceiverStrategy* CreateVideoStrategy(RtpData* data_callback);
  static RTPReceiverStrategy* CreateAudioStrategy(RtpData* data_callback);

  virtual ~RTPReceiverStrategy() {}

  // Parses the RTP packet and calls the data callback with the payload data.
  // Implementations are encouraged to use the provided packet buffer and RTP
  // header as arguments to the callback; implementations are also allowed to
  // make changes in the data as necessary. The specific_payload argument
  // provides audio or video-specific data. The is_first_packet argument is true
  // if this packet is either the first packet ever or the first in its frame.
  virtual int32_t ParseRtpPacket(WebRtcRTPHeader* rtp_header,
                                 const PayloadUnion& specific_payload,
                                 bool is_red,
                                 const uint8_t* payload,
                                 size_t payload_length,
                                 int64_t timestamp_ms,
                                 bool is_first_packet) = 0;

  virtual TelephoneEventHandler* GetTelephoneEventHandler() = 0;

  // Computes the current dead-or-alive state.
  virtual RTPAliveType ProcessDeadOrAlive(
      uint16_t last_payload_length) const = 0;

  // Returns true if we should report CSRC changes for this payload type.
  // TODO(phoglund): should move out of here along with other payload stuff.
  virtual bool ShouldReportCsrcChanges(uint8_t payload_type) const = 0;

  // Notifies the strategy that we have created a new non-RED audio payload type
  // in the payload registry.
  virtual int32_t OnNewPayloadTypeCreated(const CodecInst& audio_codec) = 0;

  // Invokes the OnInitializeDecoder callback in a media-specific way.
  virtual int32_t InvokeOnInitializeDecoder(
      RtpFeedback* callback,
      int8_t payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const PayloadUnion& specific_payload) const = 0;

  // Checks if the payload type has changed, and returns whether we should
  // reset statistics and/or discard this packet.
  virtual void CheckPayloadChanged(int8_t payload_type,
                                   PayloadUnion* specific_payload,
                                   bool* should_discard_changes);

  virtual int Energy(uint8_t array_of_energy[kRtpCsrcSize]) const;

  // Stores / retrieves the last media specific payload for later reference.
  void GetLastMediaSpecificPayload(PayloadUnion* payload) const;
  void SetLastMediaSpecificPayload(const PayloadUnion& payload);

 protected:
  // The data callback is where we should send received payload data.
  // See ParseRtpPacket. This class does not claim ownership of the callback.
  // Implementations must NOT hold any critical sections while calling the
  // callback.
  //
  // Note: Implementations may call the callback for other reasons than calls
  // to ParseRtpPacket, for instance if the implementation somehow recovers a
  // packet.
  explicit RTPReceiverStrategy(RtpData* data_callback);

  rtc::CriticalSection crit_sect_;
  PayloadUnion last_payload_;
  RtpData* data_callback_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_STRATEGY_H_
