/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/common_types.h"

#include <string.h>

namespace webrtc {

StreamDataCounters::StreamDataCounters() : first_packet_time_ms(-1) {}

RTPHeaderExtension::RTPHeaderExtension()
    : hasTransmissionTimeOffset(false),
      transmissionTimeOffset(0),
      hasAbsoluteSendTime(false),
      absoluteSendTime(0),
      hasTransportSequenceNumber(false),
      transportSequenceNumber(0),
      hasAudioLevel(false),
      voiceActivity(false),
      audioLevel(0),
      hasVideoRotation(false),
      videoRotation(kVideoRotation_0) {}

RTPHeader::RTPHeader()
    : markerBit(false),
      payloadType(0),
      sequenceNumber(0),
      timestamp(0),
      ssrc(0),
      numCSRCs(0),
      arrOfCSRCs(),
      paddingLength(0),
      headerLength(0),
      payload_type_frequency(0),
      extension() {}

VideoCodec::VideoCodec()
    : codecType(kVideoCodecUnknown),
      plName(),
      plType(0),
      width(0),
      height(0),
      startBitrate(0),
      maxBitrate(0),
      minBitrate(0),
      targetBitrate(0),
      maxFramerate(0),
      qpMax(0),
      numberOfSimulcastStreams(0),
      simulcastStream(),
      spatialLayers(),
      mode(kRealtimeVideo),
      codecSpecific() {}

VideoCodecVP8* VideoCodec::VP8() {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP8);
  return &codecSpecific.VP8;
}

const VideoCodecVP8& VideoCodec::VP8() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP8);
  return codecSpecific.VP8;
}

VideoCodecVP9* VideoCodec::VP9() {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP9);
  return &codecSpecific.VP9;
}

const VideoCodecVP9& VideoCodec::VP9() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecVP9);
  return codecSpecific.VP9;
}

VideoCodecH264* VideoCodec::H264() {
  RTC_DCHECK_EQ(codecType, kVideoCodecH264);
  return &codecSpecific.H264;
}

const VideoCodecH264& VideoCodec::H264() const {
  RTC_DCHECK_EQ(codecType, kVideoCodecH264);
  return codecSpecific.H264;
}

}  // namespace webrtc
