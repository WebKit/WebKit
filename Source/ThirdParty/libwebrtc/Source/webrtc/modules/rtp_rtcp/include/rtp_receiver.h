/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_RECEIVER_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_RECEIVER_H_

#include <vector>

#include "api/rtpreceiverinterface.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class RTPPayloadRegistry;
class VideoCodec;

class TelephoneEventHandler {
 public:
  virtual ~TelephoneEventHandler() {}

  // The following three methods implement the TelephoneEventHandler interface.
  // Forward DTMFs to decoder for playout.
  virtual void SetTelephoneEventForwardToDecoder(bool forward_to_decoder) = 0;

  // Is forwarding of outband telephone events turned on/off?
  virtual bool TelephoneEventForwardToDecoder() const = 0;

  // Is TelephoneEvent configured with payload type payload_type
  virtual bool TelephoneEventPayloadType(const int8_t payload_type) const = 0;
};

class RtpReceiver {
 public:
  // Creates a video-enabled RTP receiver.
  static RtpReceiver* CreateVideoReceiver(
      Clock* clock,
      RtpData* incoming_payload_callback,
      RtpFeedback* incoming_messages_callback,
      RTPPayloadRegistry* rtp_payload_registry);

  // Creates an audio-enabled RTP receiver.
  static RtpReceiver* CreateAudioReceiver(
      Clock* clock,
      RtpData* incoming_payload_callback,
      RtpFeedback* incoming_messages_callback,
      RTPPayloadRegistry* rtp_payload_registry);

  virtual ~RtpReceiver() {}

  // Returns a TelephoneEventHandler if available.
  virtual TelephoneEventHandler* GetTelephoneEventHandler() = 0;

  // Registers a receive payload in the payload registry and notifies the media
  // receiver strategy.
  virtual int32_t RegisterReceivePayload(
      int payload_type,
      const SdpAudioFormat& audio_format) = 0;

  // Deprecated version of the above.
  int32_t RegisterReceivePayload(const CodecInst& audio_codec);

  // Registers a receive payload in the payload registry.
  virtual int32_t RegisterReceivePayload(const VideoCodec& video_codec) = 0;

  // De-registers |payload_type| from the payload registry.
  virtual int32_t DeRegisterReceivePayload(const int8_t payload_type) = 0;

  // Parses the media specific parts of an RTP packet and updates the receiver
  // state. This for instance means that any changes in SSRC and payload type is
  // detected and acted upon.
  virtual bool IncomingRtpPacket(const RTPHeader& rtp_header,
                                 const uint8_t* payload,
                                 size_t payload_length,
                                 PayloadUnion payload_specific) = 0;
  // TODO(nisse): Deprecated version, delete as soon as downstream
  // applications are updated.
  bool IncomingRtpPacket(const RTPHeader& rtp_header,
                         const uint8_t* payload,
                         size_t payload_length,
                         PayloadUnion payload_specific,
                         bool in_order /* Ignored */) {
    return IncomingRtpPacket(rtp_header, payload, payload_length,
                             payload_specific);
  }

  // Gets the RTP timestamp and the corresponding monotonic system
  // time for the most recent in-order packet. Returns true on
  // success, false if no packet has been received.
  virtual bool GetLatestTimestamps(uint32_t* timestamp,
                                   int64_t* receive_time_ms) const = 0;

  // Returns the remote SSRC of the currently received RTP stream.
  virtual uint32_t SSRC() const = 0;

  // Returns the current remote CSRCs.
  virtual int32_t CSRCs(uint32_t array_of_csrc[kRtpCsrcSize]) const = 0;

  // Returns the current energy of the RTP stream received.
  virtual int32_t Energy(uint8_t array_of_energy[kRtpCsrcSize]) const = 0;

  virtual std::vector<RtpSource> GetSources() const = 0;
};
}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_RTP_RECEIVER_H_
