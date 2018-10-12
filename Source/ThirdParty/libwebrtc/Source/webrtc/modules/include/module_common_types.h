/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_INCLUDE_MODULE_COMMON_TYPES_H_
#define MODULES_INCLUDE_MODULE_COMMON_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include "api/rtp_headers.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types_public.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

struct WebRtcRTPHeader {
  RTPVideoHeader& video_header() { return video; }
  const RTPVideoHeader& video_header() const { return video; }
  RTPVideoHeader video;

  RTPHeader header;
  FrameType frameType;
  // NTP time of the capture time in local timebase in milliseconds.
  int64_t ntp_time_ms;
};

class RTPFragmentationHeader {
 public:
  RTPFragmentationHeader();
  RTPFragmentationHeader(const RTPFragmentationHeader&) = delete;
  RTPFragmentationHeader(RTPFragmentationHeader&& other);
  RTPFragmentationHeader& operator=(const RTPFragmentationHeader& other) =
      delete;
  RTPFragmentationHeader& operator=(RTPFragmentationHeader&& other);
  ~RTPFragmentationHeader();

  friend void swap(RTPFragmentationHeader& a, RTPFragmentationHeader& b);

  void CopyFrom(const RTPFragmentationHeader& src);
  void VerifyAndAllocateFragmentationHeader(size_t size) { Resize(size); }

  void Resize(size_t size);
  size_t Size() const { return fragmentationVectorSize; }

  size_t Offset(size_t index) const { return fragmentationOffset[index]; }
  size_t Length(size_t index) const { return fragmentationLength[index]; }
  uint16_t TimeDiff(size_t index) const { return fragmentationTimeDiff[index]; }
  int PayloadType(size_t index) const { return fragmentationPlType[index]; }

  // TODO(danilchap): Move all members to private section,
  // simplify by replacing 4 raw arrays with single std::vector<Fragment>
  uint16_t fragmentationVectorSize;  // Number of fragmentations
  size_t* fragmentationOffset;       // Offset of pointer to data for each
                                     // fragmentation
  size_t* fragmentationLength;       // Data size for each fragmentation
  uint16_t* fragmentationTimeDiff;   // Timestamp difference relative "now" for
                                     // each fragmentation
  uint8_t* fragmentationPlType;      // Payload type of each fragmentation
};

// Interface used by the CallStats class to distribute call statistics.
// Callbacks will be triggered as soon as the class has been registered to a
// CallStats object using RegisterStatsObserver.
class CallStatsObserver {
 public:
  virtual void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) = 0;

  virtual ~CallStatsObserver() {}
};

// Interface used by NackModule and JitterBuffer.
class NackSender {
 public:
  virtual void SendNack(const std::vector<uint16_t>& sequence_numbers) = 0;

 protected:
  virtual ~NackSender() {}
};

// Interface used by NackModule and JitterBuffer.
class KeyFrameRequestSender {
 public:
  virtual void RequestKeyFrame() = 0;

 protected:
  virtual ~KeyFrameRequestSender() {}
};

// Used to indicate if a received packet contain a complete NALU (or equivalent)
enum VCMNaluCompleteness {
  kNaluUnset = 0,     // Packet has not been filled.
  kNaluComplete = 1,  // Packet can be decoded as is.
  kNaluStart,         // Packet contain beginning of NALU
  kNaluIncomplete,    // Packet is not beginning or end of NALU
  kNaluEnd,           // Packet is the end of a NALU
};
}  // namespace webrtc

#endif  // MODULES_INCLUDE_MODULE_COMMON_TYPES_H_
