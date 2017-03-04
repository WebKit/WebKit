/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_INCLUDE_MODULE_COMMON_TYPES_H_
#define WEBRTC_MODULES_INCLUDE_MODULE_COMMON_TYPES_H_

#include <assert.h>
#include <string.h>  // memcpy

#include <algorithm>
#include <limits>

#include "webrtc/api/video/video_rotation.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/deprecation.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8_globals.h"
#include "webrtc/modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264_globals.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct RTPAudioHeader {
  uint8_t numEnergy;                  // number of valid entries in arrOfEnergy
  uint8_t arrOfEnergy[kRtpCsrcSize];  // one energy byte (0-9) per channel
  bool isCNG;                         // is this CNG
  size_t channel;                     // number of channels 2 = stereo
};

union RTPVideoTypeHeader {
  RTPVideoHeaderVP8 VP8;
  RTPVideoHeaderVP9 VP9;
  RTPVideoHeaderH264 H264;
};

enum RtpVideoCodecTypes {
  kRtpVideoNone,
  kRtpVideoGeneric,
  kRtpVideoVp8,
  kRtpVideoVp9,
  kRtpVideoH264
};
// Since RTPVideoHeader is used as a member of a union, it can't have a
// non-trivial default constructor.
struct RTPVideoHeader {
  uint16_t width;  // size
  uint16_t height;
  VideoRotation rotation;

  PlayoutDelay playout_delay;

  union {
    bool is_first_packet_in_frame;
    RTC_DEPRECATED bool isFirstPacket;  // first packet in frame
  };
  uint8_t simulcastIdx;  // Index if the simulcast encoder creating
                         // this frame, 0 if not using simulcast.
  RtpVideoCodecTypes codec;
  RTPVideoTypeHeader codecHeader;
};
union RTPTypeHeader {
  RTPAudioHeader Audio;
  RTPVideoHeader Video;
};

struct WebRtcRTPHeader {
  RTPHeader header;
  FrameType frameType;
  RTPTypeHeader type;
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
        fragmentationPlType(NULL) {};

  ~RTPFragmentationHeader() {
    delete[] fragmentationOffset;
    delete[] fragmentationLength;
    delete[] fragmentationTimeDiff;
    delete[] fragmentationPlType;
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
        memcpy(fragmentationOffset, oldOffsets,
               sizeof(size_t) * oldVectorSize);
        delete[] oldOffsets;
      }
      // length
      {
        size_t* oldLengths = fragmentationLength;
        fragmentationLength = new size_t[size16];
        memset(fragmentationLength + oldVectorSize, 0,
               sizeof(size_t) * (size16 - oldVectorSize));
        memcpy(fragmentationLength, oldLengths,
               sizeof(size_t) * oldVectorSize);
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

// Types for the FEC packet masks. The type |kFecMaskRandom| is based on a
// random loss model. The type |kFecMaskBursty| is based on a bursty/consecutive
// loss model. The packet masks are defined in
// modules/rtp_rtcp/fec_private_tables_random(bursty).h
enum FecMaskType {
  kFecMaskRandom,
  kFecMaskBursty,
};

// Struct containing forward error correction settings.
struct FecProtectionParams {
  int fec_rate;
  int max_fec_frames;
  FecMaskType fec_mask_type;
};

// Interface used by the CallStats class to distribute call statistics.
// Callbacks will be triggered as soon as the class has been registered to a
// CallStats object using RegisterStatsObserver.
class CallStatsObserver {
 public:
  virtual void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) = 0;

  virtual ~CallStatsObserver() {}
};

/* This class holds up to 60 ms of super-wideband (32 kHz) stereo audio. It
 * allows for adding and subtracting frames while keeping track of the resulting
 * states.
 *
 * Notes
 * - The total number of samples in |data_| is
 *   samples_per_channel_ * num_channels_
 *
 * - Stereo data is interleaved starting with the left channel.
 *
 */
class AudioFrame {
 public:
  // Stereo, 32 kHz, 60 ms (2 * 32 * 60)
  enum : size_t {
    kMaxDataSizeSamples = 3840
  };

  enum VADActivity {
    kVadActive = 0,
    kVadPassive = 1,
    kVadUnknown = 2
  };
  enum SpeechType {
    kNormalSpeech = 0,
    kPLC = 1,
    kCNG = 2,
    kPLCCNG = 3,
    kUndefined = 4
  };

  AudioFrame();

  // Resets all members to their default state (except does not modify the
  // contents of |data_|).
  void Reset();

  void UpdateFrame(int id, uint32_t timestamp, const int16_t* data,
                   size_t samples_per_channel, int sample_rate_hz,
                   SpeechType speech_type, VADActivity vad_activity,
                   size_t num_channels = 1);

  void CopyFrom(const AudioFrame& src);

  // These methods are deprecated. Use the functions in
  // webrtc/audio/utility instead. These methods will exists for a
  // short period of time until webrtc clients have updated. See
  // webrtc:6548 for details.
  RTC_DEPRECATED void Mute();
  RTC_DEPRECATED AudioFrame& operator>>=(const int rhs);
  RTC_DEPRECATED AudioFrame& operator+=(const AudioFrame& rhs);

  int id_;
  // RTP timestamp of the first sample in the AudioFrame.
  uint32_t timestamp_ = 0;
  // Time since the first frame in milliseconds.
  // -1 represents an uninitialized value.
  int64_t elapsed_time_ms_ = -1;
  // NTP time of the estimated capture time in local timebase in milliseconds.
  // -1 represents an uninitialized value.
  int64_t ntp_time_ms_ = -1;
  int16_t data_[kMaxDataSizeSamples];
  size_t samples_per_channel_ = 0;
  int sample_rate_hz_ = 0;
  size_t num_channels_ = 0;
  SpeechType speech_type_ = kUndefined;
  VADActivity vad_activity_ = kVadUnknown;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioFrame);
};

// TODO(henrik.lundin) Can we remove the call to data_()?
// See https://bugs.chromium.org/p/webrtc/issues/detail?id=5647.
inline AudioFrame::AudioFrame()
    : data_() {
}

inline void AudioFrame::Reset() {
  id_ = -1;
  // TODO(wu): Zero is a valid value for |timestamp_|. We should initialize
  // to an invalid value, or add a new member to indicate invalidity.
  timestamp_ = 0;
  elapsed_time_ms_ = -1;
  ntp_time_ms_ = -1;
  samples_per_channel_ = 0;
  sample_rate_hz_ = 0;
  num_channels_ = 0;
  speech_type_ = kUndefined;
  vad_activity_ = kVadUnknown;
}

inline void AudioFrame::UpdateFrame(int id,
                                    uint32_t timestamp,
                                    const int16_t* data,
                                    size_t samples_per_channel,
                                    int sample_rate_hz,
                                    SpeechType speech_type,
                                    VADActivity vad_activity,
                                    size_t num_channels) {
  id_ = id;
  timestamp_ = timestamp;
  samples_per_channel_ = samples_per_channel;
  sample_rate_hz_ = sample_rate_hz;
  speech_type_ = speech_type;
  vad_activity_ = vad_activity;
  num_channels_ = num_channels;

  const size_t length = samples_per_channel * num_channels;
  assert(length <= kMaxDataSizeSamples);
  if (data != NULL) {
    memcpy(data_, data, sizeof(int16_t) * length);
  } else {
    memset(data_, 0, sizeof(int16_t) * length);
  }
}

inline void AudioFrame::CopyFrom(const AudioFrame& src) {
  if (this == &src) return;

  id_ = src.id_;
  timestamp_ = src.timestamp_;
  elapsed_time_ms_ = src.elapsed_time_ms_;
  ntp_time_ms_ = src.ntp_time_ms_;
  samples_per_channel_ = src.samples_per_channel_;
  sample_rate_hz_ = src.sample_rate_hz_;
  speech_type_ = src.speech_type_;
  vad_activity_ = src.vad_activity_;
  num_channels_ = src.num_channels_;

  const size_t length = samples_per_channel_ * num_channels_;
  assert(length <= kMaxDataSizeSamples);
  memcpy(data_, src.data_, sizeof(int16_t) * length);
}

inline void AudioFrame::Mute() {
  memset(data_, 0, samples_per_channel_ * num_channels_ * sizeof(int16_t));
}

inline AudioFrame& AudioFrame::operator>>=(const int rhs) {
  assert((num_channels_ > 0) && (num_channels_ < 3));
  if ((num_channels_ > 2) || (num_channels_ < 1)) return *this;

  for (size_t i = 0; i < samples_per_channel_ * num_channels_; i++) {
    data_[i] = static_cast<int16_t>(data_[i] >> rhs);
  }
  return *this;
}

inline AudioFrame& AudioFrame::operator+=(const AudioFrame& rhs) {
  // Sanity check
  assert((num_channels_ > 0) && (num_channels_ < 3));
  if ((num_channels_ > 2) || (num_channels_ < 1)) return *this;
  if (num_channels_ != rhs.num_channels_) return *this;

  bool noPrevData = false;
  if (samples_per_channel_ != rhs.samples_per_channel_) {
    if (samples_per_channel_ == 0) {
      // special case we have no data to start with
      samples_per_channel_ = rhs.samples_per_channel_;
      noPrevData = true;
    } else {
      return *this;
    }
  }

  if ((vad_activity_ == kVadActive) || rhs.vad_activity_ == kVadActive) {
    vad_activity_ = kVadActive;
  } else if (vad_activity_ == kVadUnknown || rhs.vad_activity_ == kVadUnknown) {
    vad_activity_ = kVadUnknown;
  }

  if (speech_type_ != rhs.speech_type_) speech_type_ = kUndefined;

  if (noPrevData) {
    memcpy(data_, rhs.data_,
           sizeof(int16_t) * rhs.samples_per_channel_ * num_channels_);
  } else {
    // IMPROVEMENT this can be done very fast in assembly
    for (size_t i = 0; i < samples_per_channel_ * num_channels_; i++) {
      int32_t wrap_guard =
          static_cast<int32_t>(data_[i]) + static_cast<int32_t>(rhs.data_[i]);
      data_[i] = rtc::saturated_cast<int16_t>(wrap_guard);
    }
  }
  return *this;
}

inline bool IsNewerSequenceNumber(uint16_t sequence_number,
                                  uint16_t prev_sequence_number) {
  // Distinguish between elements that are exactly 0x8000 apart.
  // If s1>s2 and |s1-s2| = 0x8000: IsNewer(s1,s2)=true, IsNewer(s2,s1)=false
  // rather than having IsNewer(s1,s2) = IsNewer(s2,s1) = false.
  if (static_cast<uint16_t>(sequence_number - prev_sequence_number) == 0x8000) {
    return sequence_number > prev_sequence_number;
  }
  return sequence_number != prev_sequence_number &&
         static_cast<uint16_t>(sequence_number - prev_sequence_number) < 0x8000;
}

inline bool IsNewerTimestamp(uint32_t timestamp, uint32_t prev_timestamp) {
  // Distinguish between elements that are exactly 0x80000000 apart.
  // If t1>t2 and |t1-t2| = 0x80000000: IsNewer(t1,t2)=true,
  // IsNewer(t2,t1)=false
  // rather than having IsNewer(t1,t2) = IsNewer(t2,t1) = false.
  if (static_cast<uint32_t>(timestamp - prev_timestamp) == 0x80000000) {
    return timestamp > prev_timestamp;
  }
  return timestamp != prev_timestamp &&
         static_cast<uint32_t>(timestamp - prev_timestamp) < 0x80000000;
}

inline uint16_t LatestSequenceNumber(uint16_t sequence_number1,
                                     uint16_t sequence_number2) {
  return IsNewerSequenceNumber(sequence_number1, sequence_number2)
             ? sequence_number1
             : sequence_number2;
}

inline uint32_t LatestTimestamp(uint32_t timestamp1, uint32_t timestamp2) {
  return IsNewerTimestamp(timestamp1, timestamp2) ? timestamp1 : timestamp2;
}

// Utility class to unwrap a sequence number to a larger type, for easier
// handling large ranges. Note that sequence numbers will never be unwrapped
// to a negative value.
class SequenceNumberUnwrapper {
 public:
  SequenceNumberUnwrapper() : last_seq_(-1) {}

  // Get the unwrapped sequence, but don't update the internal state.
  int64_t UnwrapWithoutUpdate(uint16_t sequence_number) {
    if (last_seq_ == -1)
      return sequence_number;

    uint16_t cropped_last = static_cast<uint16_t>(last_seq_);
    int64_t delta = sequence_number - cropped_last;
    if (IsNewerSequenceNumber(sequence_number, cropped_last)) {
      if (delta < 0)
        delta += (1 << 16);  // Wrap forwards.
    } else if (delta > 0 && (last_seq_ + delta - (1 << 16)) >= 0) {
      // If sequence_number is older but delta is positive, this is a backwards
      // wrap-around. However, don't wrap backwards past 0 (unwrapped).
      delta -= (1 << 16);
    }

    return last_seq_ + delta;
  }

  // Only update the internal state to the specified last (unwrapped) sequence.
  void UpdateLast(int64_t last_sequence) { last_seq_ = last_sequence; }

  // Unwrap the sequence number and update the internal state.
  int64_t Unwrap(uint16_t sequence_number) {
    int64_t unwrapped = UnwrapWithoutUpdate(sequence_number);
    UpdateLast(unwrapped);
    return unwrapped;
  }

 private:
  int64_t last_seq_;
};

struct PacedPacketInfo {
  PacedPacketInfo() {}
  PacedPacketInfo(int probe_cluster_id,
                  int probe_cluster_min_probes,
                  int probe_cluster_min_bytes)
      : probe_cluster_id(probe_cluster_id),
        probe_cluster_min_probes(probe_cluster_min_probes),
        probe_cluster_min_bytes(probe_cluster_min_bytes) {}

  bool operator==(const PacedPacketInfo& rhs) const {
    return send_bitrate_bps == rhs.send_bitrate_bps &&
           probe_cluster_id == rhs.probe_cluster_id &&
           probe_cluster_min_probes == rhs.probe_cluster_min_probes &&
           probe_cluster_min_bytes == rhs.probe_cluster_min_bytes;
  }

  static constexpr int kNotAProbe = -1;
  int send_bitrate_bps = -1;
  int probe_cluster_id = kNotAProbe;
  int probe_cluster_min_probes = -1;
  int probe_cluster_min_bytes = -1;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_INCLUDE_MODULE_COMMON_TYPES_H_
