/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_SFRAME_DESCRIPTOR_H_
#define MODULES_RTP_RTCP_SOURCE_SFRAME_DESCRIPTOR_H_

namespace webrtc {

// Encryption granularity signaled by the T bit in the SFrame descriptor.
// See draft-ietf-avtcore-rtp-sframe §4.
enum class SframeEncryptionLevel {
  // T=0: per-frame encryption (raw). The entire frame is encrypted as one
  // SFrame ciphertext.
  kFrame,
  // T=1: per-packet encryption. Each RTP packet carries its own SFrame
  // ciphertext.
  kPacket,
};

// SFrame payload descriptor byte (first byte of each SFrame RTP payload).
// See draft-ietf-avtcore-rtp-sframe §4.
//
//   0 1 2 3 4 5 6 7
//  +-+-+-+-+-+-+-+-+
//  |S|E|T|x x x x x|
//  +-+-+-+-+-+-+-+-+
//
// S: Start bit — 1 if this is the first fragment of the SFrame frame.
// E: End bit   — 1 if this is the last fragment of the SFrame frame.
// T: Type bit  — 0 for raw (per-frame SFrame), 1 for packetized (per-packet).
// x: Reserved, must be 0.
struct SFrameDescriptor {
  bool start = false;
  bool end = false;
  SframeEncryptionLevel encryption_level =
      SframeEncryptionLevel::kFrame;  // T bit
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_SFRAME_DESCRIPTOR_H_
