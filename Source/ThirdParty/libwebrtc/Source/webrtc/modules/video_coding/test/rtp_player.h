/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_RTP_PLAYER_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_RTP_PLAYER_H_

#include <string>
#include <vector>

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"

namespace webrtc {
class Clock;

namespace rtpplayer {

class PayloadCodecTuple {
 public:
  PayloadCodecTuple(uint8_t payload_type,
                    const std::string& codec_name,
                    VideoCodecType codec_type)
      : name_(codec_name),
        payload_type_(payload_type),
        codec_type_(codec_type) {}

  const std::string& name() const { return name_; }
  uint8_t payload_type() const { return payload_type_; }
  VideoCodecType codec_type() const { return codec_type_; }

 private:
  std::string name_;
  uint8_t payload_type_;
  VideoCodecType codec_type_;
};

typedef std::vector<PayloadCodecTuple> PayloadTypes;
typedef std::vector<PayloadCodecTuple>::const_iterator PayloadTypesIterator;

// Implemented by RtpPlayer and given to client as a means to retrieve
// information about a specific RTP stream.
class RtpStreamInterface {
 public:
  virtual ~RtpStreamInterface() {}

  // Ask for missing packets to be resent.
  virtual void ResendPackets(const uint16_t* sequence_numbers,
                             uint16_t length) = 0;

  virtual uint32_t ssrc() const = 0;
  virtual const PayloadTypes& payload_types() const = 0;
};

// Implemented by a sink. Wraps RtpData because its d-tor is protected.
class PayloadSinkInterface : public RtpData {
 public:
  virtual ~PayloadSinkInterface() {}
};

// Implemented to provide a sink for RTP data, such as hooking up a VCM to
// the incoming RTP stream.
class PayloadSinkFactoryInterface {
 public:
  virtual ~PayloadSinkFactoryInterface() {}

  // Return NULL if failed to create sink. 'stream' is guaranteed to be
  // around for as long as the RtpData. The returned object is owned by
  // the caller (RtpPlayer).
  virtual PayloadSinkInterface* Create(RtpStreamInterface* stream) = 0;
};

// The client's view of an RtpPlayer.
class RtpPlayerInterface {
 public:
  virtual ~RtpPlayerInterface() {}

  virtual int NextPacket(int64_t timeNow) = 0;
  virtual uint32_t TimeUntilNextPacket() const = 0;
  virtual void Print() const = 0;
};

RtpPlayerInterface* Create(const std::string& inputFilename,
                           PayloadSinkFactoryInterface* payloadSinkFactory,
                           Clock* clock,
                           const PayloadTypes& payload_types,
                           float lossRate,
                           int64_t rttMs,
                           bool reordering);

}  // namespace rtpplayer
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_TEST_RTP_PLAYER_H_
