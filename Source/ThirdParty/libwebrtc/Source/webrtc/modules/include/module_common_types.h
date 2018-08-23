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

#include <assert.h>
#include <string.h>  // memcpy

#include <algorithm>
#include <limits>

#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "api/transport/network_types.h"
#include "api/video/video_rotation.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types_public.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/deprecation.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

// TODO(nisse): Deprecated, use webrtc::VideoCodecType instead.
using RtpVideoCodecTypes = VideoCodecType;

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
  RTPFragmentationHeader()
      : fragmentationVectorSize(0),
        fragmentationOffset(NULL),
        fragmentationLength(NULL),
        fragmentationTimeDiff(NULL),
        fragmentationPlType(NULL) {}

  RTPFragmentationHeader(RTPFragmentationHeader&& other)
      : RTPFragmentationHeader() {
    std::swap(*this, other);
  }

  ~RTPFragmentationHeader() {
    delete[] fragmentationOffset;
    delete[] fragmentationLength;
    delete[] fragmentationTimeDiff;
    delete[] fragmentationPlType;
  }

  void operator=(RTPFragmentationHeader&& other) { std::swap(*this, other); }

  friend void swap(RTPFragmentationHeader& a, RTPFragmentationHeader& b) {
    using std::swap;
    swap(a.fragmentationVectorSize, b.fragmentationVectorSize);
    swap(a.fragmentationOffset, b.fragmentationOffset);
    swap(a.fragmentationLength, b.fragmentationLength);
    swap(a.fragmentationTimeDiff, b.fragmentationTimeDiff);
    swap(a.fragmentationPlType, b.fragmentationPlType);
  }

  void CopyFrom(const RTPFragmentationHeader& src) {
    if (this == &src) {
      return;
    }

    if (src.fragmentationVectorSize != fragmentationVectorSize) {
      // new size of vectors

      // delete old
      delete[] fragmentationOffset;
      fragmentationOffset = NULL;
      delete[] fragmentationLength;
      fragmentationLength = NULL;
      delete[] fragmentationTimeDiff;
      fragmentationTimeDiff = NULL;
      delete[] fragmentationPlType;
      fragmentationPlType = NULL;

      if (src.fragmentationVectorSize > 0) {
        // allocate new
        if (src.fragmentationOffset) {
          fragmentationOffset = new size_t[src.fragmentationVectorSize];
        }
        if (src.fragmentationLength) {
          fragmentationLength = new size_t[src.fragmentationVectorSize];
        }
        if (src.fragmentationTimeDiff) {
          fragmentationTimeDiff = new uint16_t[src.fragmentationVectorSize];
        }
        if (src.fragmentationPlType) {
          fragmentationPlType = new uint8_t[src.fragmentationVectorSize];
        }
      }
      // set new size
      fragmentationVectorSize = src.fragmentationVectorSize;
    }

    if (src.fragmentationVectorSize > 0) {
      // copy values
      if (src.fragmentationOffset) {
        memcpy(fragmentationOffset, src.fragmentationOffset,
               src.fragmentationVectorSize * sizeof(size_t));
      }
      if (src.fragmentationLength) {
        memcpy(fragmentationLength, src.fragmentationLength,
               src.fragmentationVectorSize * sizeof(size_t));
      }
      if (src.fragmentationTimeDiff) {
        memcpy(fragmentationTimeDiff, src.fragmentationTimeDiff,
               src.fragmentationVectorSize * sizeof(uint16_t));
      }
      if (src.fragmentationPlType) {
        memcpy(fragmentationPlType, src.fragmentationPlType,
               src.fragmentationVectorSize * sizeof(uint8_t));
      }
    }
  }

  void VerifyAndAllocateFragmentationHeader(const size_t size) {
    assert(size <= std::numeric_limits<uint16_t>::max());
    const uint16_t size16 = static_cast<uint16_t>(size);
    if (fragmentationVectorSize < size16) {
      uint16_t oldVectorSize = fragmentationVectorSize;
      {
        // offset
        size_t* oldOffsets = fragmentationOffset;
        fragmentationOffset = new size_t[size16];
        memset(fragmentationOffset + oldVectorSize, 0,
               sizeof(size_t) * (size16 - oldVectorSize));
        // copy old values
        memcpy(fragmentationOffset, oldOffsets, sizeof(size_t) * oldVectorSize);
        delete[] oldOffsets;
      }
      // length
      {
        size_t* oldLengths = fragmentationLength;
        fragmentationLength = new size_t[size16];
        memset(fragmentationLength + oldVectorSize, 0,
               sizeof(size_t) * (size16 - oldVectorSize));
        memcpy(fragmentationLength, oldLengths, sizeof(size_t) * oldVectorSize);
        delete[] oldLengths;
      }
      // time diff
      {
        uint16_t* oldTimeDiffs = fragmentationTimeDiff;
        fragmentationTimeDiff = new uint16_t[size16];
        memset(fragmentationTimeDiff + oldVectorSize, 0,
               sizeof(uint16_t) * (size16 - oldVectorSize));
        memcpy(fragmentationTimeDiff, oldTimeDiffs,
               sizeof(uint16_t) * oldVectorSize);
        delete[] oldTimeDiffs;
      }
      // payload type
      {
        uint8_t* oldTimePlTypes = fragmentationPlType;
        fragmentationPlType = new uint8_t[size16];
        memset(fragmentationPlType + oldVectorSize, 0,
               sizeof(uint8_t) * (size16 - oldVectorSize));
        memcpy(fragmentationPlType, oldTimePlTypes,
               sizeof(uint8_t) * oldVectorSize);
        delete[] oldTimePlTypes;
      }
      fragmentationVectorSize = size16;
    }
  }

  uint16_t fragmentationVectorSize;  // Number of fragmentations
  size_t* fragmentationOffset;       // Offset of pointer to data for each
                                     // fragmentation
  size_t* fragmentationLength;       // Data size for each fragmentation
  uint16_t* fragmentationTimeDiff;   // Timestamp difference relative "now" for
                                     // each fragmentation
  uint8_t* fragmentationPlType;      // Payload type of each fragmentation

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(RTPFragmentationHeader);
};

struct RTCPVoIPMetric {
  // RFC 3611 4.7
  uint8_t lossRate;
  uint8_t discardRate;
  uint8_t burstDensity;
  uint8_t gapDensity;
  uint16_t burstDuration;
  uint16_t gapDuration;
  uint16_t roundTripDelay;
  uint16_t endSystemDelay;
  uint8_t signalLevel;
  uint8_t noiseLevel;
  uint8_t RERL;
  uint8_t Gmin;
  uint8_t Rfactor;
  uint8_t extRfactor;
  uint8_t MOSLQ;
  uint8_t MOSCQ;
  uint8_t RXconfig;
  uint16_t JBnominal;
  uint16_t JBmax;
  uint16_t JBabsMax;
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
