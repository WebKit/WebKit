/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_CONFIG_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_CONFIG_H_

// Configuration file for RTP utilities (RTPSender, RTPReceiver ...)
namespace webrtc {
enum { NACK_BYTECOUNT_SIZE = 60 };  // size of our NACK history
// A sanity for the NACK list parsing at the send-side.
enum { kSendSideNackListSizeSanity = 20000 };
enum { kDefaultMaxReorderingThreshold = 50 };  // In sequence numbers.
enum { kRtcpMaxNackFields = 253 };

enum { RTCP_INTERVAL_VIDEO_MS = 1000 };
enum { RTCP_INTERVAL_AUDIO_MS = 5000 };
enum { RTCP_SEND_BEFORE_KEY_FRAME_MS = 100 };
enum { RTCP_MAX_REPORT_BLOCKS = 31 };  // RFC 3550 page 37
enum {
  kRtcpAppCode_DATA_SIZE = 32 * 4
};  // multiple of 4, this is not a limitation of the size
enum { RTCP_RPSI_DATA_SIZE = 30 };
enum { RTCP_NUMBER_OF_SR = 60 };

enum { MAX_NUMBER_OF_TEMPORAL_ID = 8 };              // RFC
enum { MAX_NUMBER_OF_DEPENDENCY_QUALITY_ID = 128 };  // RFC
enum { MAX_NUMBER_OF_REMB_FEEDBACK_SSRCS = 255 };

enum { BW_HISTORY_SIZE = 35 };

#define MIN_AUDIO_BW_MANAGEMENT_BITRATE 6
#define MIN_VIDEO_BW_MANAGEMENT_BITRATE 30

enum { RTP_MAX_BURST_SLEEP_TIME = 500 };
enum { RTP_AUDIO_LEVEL_UNIQUE_ID = 0xbede };
enum { RTP_MAX_PACKETS_PER_FRAME = 512 };  // must be multiple of 32
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_CONFIG_H_
