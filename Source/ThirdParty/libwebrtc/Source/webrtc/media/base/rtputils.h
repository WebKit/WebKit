/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_RTPUTILS_H_
#define WEBRTC_MEDIA_BASE_RTPUTILS_H_

#include "webrtc/base/byteorder.h"

namespace rtc {
struct PacketTimeUpdateParams;
}  // namespace rtc

namespace cricket {

const size_t kMinRtpPacketLen = 12;
const size_t kMaxRtpPacketLen = 2048;
const size_t kMinRtcpPacketLen = 4;

struct RtpHeader {
  int payload_type;
  int seq_num;
  uint32_t timestamp;
  uint32_t ssrc;
};

enum RtcpTypes {
  kRtcpTypeSR = 200,      // Sender report payload type.
  kRtcpTypeRR = 201,      // Receiver report payload type.
  kRtcpTypeSDES = 202,    // SDES payload type.
  kRtcpTypeBye = 203,     // BYE payload type.
  kRtcpTypeApp = 204,     // APP payload type.
  kRtcpTypeRTPFB = 205,   // Transport layer Feedback message payload type.
  kRtcpTypePSFB = 206,    // Payload-specific Feedback message payload type.
};

bool GetRtpPayloadType(const void* data, size_t len, int* value);
bool GetRtpSeqNum(const void* data, size_t len, int* value);
bool GetRtpTimestamp(const void* data, size_t len, uint32_t* value);
bool GetRtpSsrc(const void* data, size_t len, uint32_t* value);
bool GetRtpHeaderLen(const void* data, size_t len, size_t* value);
bool GetRtcpType(const void* data, size_t len, int* value);
bool GetRtcpSsrc(const void* data, size_t len, uint32_t* value);
bool GetRtpHeader(const void* data, size_t len, RtpHeader* header);

bool SetRtpSsrc(void* data, size_t len, uint32_t value);
// Assumes version 2, no padding, no extensions, no csrcs.
bool SetRtpHeader(void* data, size_t len, const RtpHeader& header);

bool IsRtpPacket(const void* data, size_t len);

// True if |payload type| is 0-127.
bool IsValidRtpPayloadType(int payload_type);

// True if |size| is appropriate for the indicated packet type.
bool IsValidRtpRtcpPacketSize(bool rtcp, size_t size);

// TODO(zstein): Consider using an enum instead of a bool to differentiate
// between RTP and RTCP.
// Returns "RTCP" or "RTP" according to |rtcp|.
const char* RtpRtcpStringLiteral(bool rtcp);

// Verifies that a packet has a valid RTP header.
bool ValidateRtpHeader(const uint8_t* rtp,
                       size_t length,
                       size_t* header_length);

// Helper method which updates the absolute send time extension if present.
bool UpdateRtpAbsSendTimeExtension(uint8_t* rtp,
                                   size_t length,
                                   int extension_id,
                                   uint64_t time_us);

// Applies specified |options| to the packet. It updates the absolute send time
// extension header if it is present present then updates HMAC.
bool ApplyPacketOptions(uint8_t* data,
                        size_t length,
                        const rtc::PacketTimeUpdateParams& packet_time_params,
                        uint64_t time_us);


}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_RTPUTILS_H_
