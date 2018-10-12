/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_TYPES_H_
#define COMMON_TYPES_H_

#include <stddef.h>
#include <string.h>
#include <string>
#include <vector>

#include "api/array_view.h"
// TODO(sprang): Remove this include when all usage includes it directly.
#include "api/video/video_bitrate_allocation.h"
#include "rtc_base/checks.h"
#include "rtc_base/deprecation.h"

#if defined(_MSC_VER)
// Disable "new behavior: elements of array will be default initialized"
// warning. Affects OverUseDetectorOptions.
#pragma warning(disable : 4351)
#endif

#define RTP_PAYLOAD_NAME_SIZE 32u

#if defined(WEBRTC_WIN) || defined(WIN32)
// Compares two strings without regard to case.
#define STR_CASE_CMP(s1, s2) ::_stricmp(s1, s2)
// Compares characters of two strings without regard to case.
#define STR_NCASE_CMP(s1, s2, n) ::_strnicmp(s1, s2, n)
#else
#define STR_CASE_CMP(s1, s2) ::strcasecmp(s1, s2)
#define STR_NCASE_CMP(s1, s2, n) ::strncasecmp(s1, s2, n)
#endif

namespace webrtc {

enum FrameType {
  kEmptyFrame = 0,
  kAudioFrameSpeech = 1,
  kAudioFrameCN = 2,
  kVideoFrameKey = 3,
  kVideoFrameDelta = 4,
};

// Statistics for an RTCP channel
struct RtcpStatistics {
  RtcpStatistics()
      : fraction_lost(0),
        packets_lost(0),
        extended_highest_sequence_number(0),
        jitter(0) {}

  uint8_t fraction_lost;
  int32_t packets_lost;  // Defined as a 24 bit signed integer in RTCP
  uint32_t extended_highest_sequence_number;
  uint32_t jitter;
};

class RtcpStatisticsCallback {
 public:
  virtual ~RtcpStatisticsCallback() {}

  virtual void StatisticsUpdated(const RtcpStatistics& statistics,
                                 uint32_t ssrc) = 0;
  virtual void CNameChanged(const char* cname, uint32_t ssrc) = 0;
};

// Statistics for RTCP packet types.
struct RtcpPacketTypeCounter {
  RtcpPacketTypeCounter()
      : first_packet_time_ms(-1),
        nack_packets(0),
        fir_packets(0),
        pli_packets(0),
        nack_requests(0),
        unique_nack_requests(0) {}

  void Add(const RtcpPacketTypeCounter& other) {
    nack_packets += other.nack_packets;
    fir_packets += other.fir_packets;
    pli_packets += other.pli_packets;
    nack_requests += other.nack_requests;
    unique_nack_requests += other.unique_nack_requests;
    if (other.first_packet_time_ms != -1 &&
        (other.first_packet_time_ms < first_packet_time_ms ||
         first_packet_time_ms == -1)) {
      // Use oldest time.
      first_packet_time_ms = other.first_packet_time_ms;
    }
  }

  void Subtract(const RtcpPacketTypeCounter& other) {
    nack_packets -= other.nack_packets;
    fir_packets -= other.fir_packets;
    pli_packets -= other.pli_packets;
    nack_requests -= other.nack_requests;
    unique_nack_requests -= other.unique_nack_requests;
    if (other.first_packet_time_ms != -1 &&
        (other.first_packet_time_ms > first_packet_time_ms ||
         first_packet_time_ms == -1)) {
      // Use youngest time.
      first_packet_time_ms = other.first_packet_time_ms;
    }
  }

  int64_t TimeSinceFirstPacketInMs(int64_t now_ms) const {
    return (first_packet_time_ms == -1) ? -1 : (now_ms - first_packet_time_ms);
  }

  int UniqueNackRequestsInPercent() const {
    if (nack_requests == 0) {
      return 0;
    }
    return static_cast<int>((unique_nack_requests * 100.0f / nack_requests) +
                            0.5f);
  }

  int64_t first_packet_time_ms;   // Time when first packet is sent/received.
  uint32_t nack_packets;          // Number of RTCP NACK packets.
  uint32_t fir_packets;           // Number of RTCP FIR packets.
  uint32_t pli_packets;           // Number of RTCP PLI packets.
  uint32_t nack_requests;         // Number of NACKed RTP packets.
  uint32_t unique_nack_requests;  // Number of unique NACKed RTP packets.
};

class RtcpPacketTypeCounterObserver {
 public:
  virtual ~RtcpPacketTypeCounterObserver() {}
  virtual void RtcpPacketTypesCounterUpdated(
      uint32_t ssrc,
      const RtcpPacketTypeCounter& packet_counter) = 0;
};

// Callback, used to notify an observer whenever new rates have been estimated.
class BitrateStatisticsObserver {
 public:
  virtual ~BitrateStatisticsObserver() {}

  virtual void Notify(uint32_t total_bitrate_bps,
                      uint32_t retransmit_bitrate_bps,
                      uint32_t ssrc) = 0;
};

struct FrameCounts {
  FrameCounts() : key_frames(0), delta_frames(0) {}
  int key_frames;
  int delta_frames;
};

// Callback, used to notify an observer whenever frame counts have been updated.
class FrameCountObserver {
 public:
  virtual ~FrameCountObserver() {}
  virtual void FrameCountUpdated(const FrameCounts& frame_counts,
                                 uint32_t ssrc) = 0;
};

// Callback, used to notify an observer whenever the send-side delay is updated.
class SendSideDelayObserver {
 public:
  virtual ~SendSideDelayObserver() {}
  virtual void SendSideDelayUpdated(int avg_delay_ms,
                                    int max_delay_ms,
                                    uint32_t ssrc) = 0;
};

// Callback, used to notify an observer whenever a packet is sent to the
// transport.
// TODO(asapersson): This class will remove the need for SendSideDelayObserver.
// Remove SendSideDelayObserver once possible.
class SendPacketObserver {
 public:
  virtual ~SendPacketObserver() {}
  virtual void OnSendPacket(uint16_t packet_id,
                            int64_t capture_time_ms,
                            uint32_t ssrc) = 0;
};

// Callback, used to notify an observer when the overhead per packet
// has changed.
class OverheadObserver {
 public:
  virtual ~OverheadObserver() = default;
  virtual void OnOverheadChanged(size_t overhead_bytes_per_packet) = 0;
};

// ==================================================================
// Voice specific types
// ==================================================================

// Each codec supported can be described by this structure.
struct CodecInst {
  int pltype;
  char plname[RTP_PAYLOAD_NAME_SIZE];
  int plfreq;
  int pacsize;
  size_t channels;
  int rate;  // bits/sec unlike {start,min,max}Bitrate elsewhere in this file!

  bool operator==(const CodecInst& other) const {
    return pltype == other.pltype &&
           (STR_CASE_CMP(plname, other.plname) == 0) &&
           plfreq == other.plfreq && pacsize == other.pacsize &&
           channels == other.channels && rate == other.rate;
  }

  bool operator!=(const CodecInst& other) const { return !(*this == other); }
};

// RTP
enum { kRtpCsrcSize = 15 };  // RFC 3550 page 13

// NETEQ statistics.
struct NetworkStatistics {
  // current jitter buffer size in ms
  uint16_t currentBufferSize;
  // preferred (optimal) buffer size in ms
  uint16_t preferredBufferSize;
  // adding extra delay due to "peaky jitter"
  bool jitterPeaksFound;
  // Stats below correspond to similarly-named fields in the WebRTC stats spec.
  // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats
  uint64_t totalSamplesReceived;
  uint64_t concealedSamples;
  uint64_t concealmentEvents;
  uint64_t jitterBufferDelayMs;
  // Stats below DO NOT correspond directly to anything in the WebRTC stats
  // Loss rate (network + late); fraction between 0 and 1, scaled to Q14.
  uint16_t currentPacketLossRate;
  // Late loss rate; fraction between 0 and 1, scaled to Q14.
  union {
    RTC_DEPRECATED uint16_t currentDiscardRate;
  };
  // fraction (of original stream) of synthesized audio inserted through
  // expansion (in Q14)
  uint16_t currentExpandRate;
  // fraction (of original stream) of synthesized speech inserted through
  // expansion (in Q14)
  uint16_t currentSpeechExpandRate;
  // fraction of synthesized speech inserted through pre-emptive expansion
  // (in Q14)
  uint16_t currentPreemptiveRate;
  // fraction of data removed through acceleration (in Q14)
  uint16_t currentAccelerateRate;
  // fraction of data coming from secondary decoding (in Q14)
  uint16_t currentSecondaryDecodedRate;
  // Fraction of secondary data, including FEC and RED, that is discarded (in
  // Q14). Discarding of secondary data can be caused by the reception of the
  // primary data, obsoleting the secondary data. It can also be caused by early
  // or late arrival of secondary data.
  uint16_t currentSecondaryDiscardedRate;
  // clock-drift in parts-per-million (negative or positive)
  int32_t clockDriftPPM;
  // average packet waiting time in the jitter buffer (ms)
  int meanWaitingTimeMs;
  // median packet waiting time in the jitter buffer (ms)
  int medianWaitingTimeMs;
  // min packet waiting time in the jitter buffer (ms)
  int minWaitingTimeMs;
  // max packet waiting time in the jitter buffer (ms)
  int maxWaitingTimeMs;
  // added samples in off mode due to packet loss
  size_t addedSamples;
};

// Statistics for calls to AudioCodingModule::PlayoutData10Ms().
struct AudioDecodingCallStats {
  AudioDecodingCallStats()
      : calls_to_silence_generator(0),
        calls_to_neteq(0),
        decoded_normal(0),
        decoded_plc(0),
        decoded_cng(0),
        decoded_plc_cng(0),
        decoded_muted_output(0) {}

  int calls_to_silence_generator;  // Number of calls where silence generated,
                                   // and NetEq was disengaged from decoding.
  int calls_to_neteq;              // Number of calls to NetEq.
  int decoded_normal;  // Number of calls where audio RTP packet decoded.
  int decoded_plc;     // Number of calls resulted in PLC.
  int decoded_cng;  // Number of calls where comfort noise generated due to DTX.
  int decoded_plc_cng;       // Number of calls resulted where PLC faded to CNG.
  int decoded_muted_output;  // Number of calls returning a muted state output.
};

// ==================================================================
// Video specific types
// ==================================================================

// TODO(nisse): Delete, and switch to fourcc values everywhere?
// Supported video types.
enum class VideoType {
  kUnknown,
  kI420,
  kIYUV,
  kRGB24,
  kABGR,
  kARGB,
  kARGB4444,
  kRGB565,
  kARGB1555,
  kYUY2,
  kYV12,
  kUYVY,
  kMJPEG,
  kNV21,
  kNV12,
  kBGRA,
};

// TODO(magjed): Move this and other H264 related classes out to their own file.
namespace H264 {

enum Profile {
  kProfileConstrainedBaseline,
  kProfileBaseline,
  kProfileMain,
  kProfileConstrainedHigh,
  kProfileHigh,
};

}  // namespace H264

// Video codec types
enum VideoCodecType {
  // There are various memset(..., 0, ...) calls in the code that rely on
  // kVideoCodecGeneric being zero.
  kVideoCodecGeneric = 0,
  kVideoCodecVP8,
  kVideoCodecVP9,
  kVideoCodecH264,
  kVideoCodecI420,
  kVideoCodecMultiplex,
};

struct SpatialLayer {
  bool operator==(const SpatialLayer& other) const;
  bool operator!=(const SpatialLayer& other) const { return !(*this == other); }

  unsigned short width;
  unsigned short height;
  float maxFramerate;  // fps.
  unsigned char numberOfTemporalLayers;
  unsigned int maxBitrate;     // kilobits/sec.
  unsigned int targetBitrate;  // kilobits/sec.
  unsigned int minBitrate;     // kilobits/sec.
  unsigned int qpMax;          // minimum quality
  bool active;                 // encoded and sent.
};

// Simulcast is when the same stream is encoded multiple times with different
// settings such as resolution.
typedef SpatialLayer SimulcastStream;

// TODO(sprang): Remove this when downstream projects have been updated.
using BitrateAllocation = VideoBitrateAllocation;

// Bandwidth over-use detector options.  These are used to drive
// experimentation with bandwidth estimation parameters.
// See modules/remote_bitrate_estimator/overuse_detector.h
// TODO(terelius): This is only used in overuse_estimator.cc, and only in the
// default constructed state. Can we move the relevant variables into that
// class and delete this? See also disabled warning at line 27
struct OverUseDetectorOptions {
  OverUseDetectorOptions()
      : initial_slope(8.0 / 512.0),
        initial_offset(0),
        initial_e(),
        initial_process_noise(),
        initial_avg_noise(0.0),
        initial_var_noise(50) {
    initial_e[0][0] = 100;
    initial_e[1][1] = 1e-1;
    initial_e[0][1] = initial_e[1][0] = 0;
    initial_process_noise[0] = 1e-13;
    initial_process_noise[1] = 1e-3;
  }
  double initial_slope;
  double initial_offset;
  double initial_e[2][2];
  double initial_process_noise[2];
  double initial_avg_noise;
  double initial_var_noise;
};

// Minimum and maximum playout delay values from capture to render.
// These are best effort values.
//
// A value < 0 indicates no change from previous valid value.
//
// min = max = 0 indicates that the receiver should try and render
// frame as soon as possible.
//
// min = x, max = y indicates that the receiver is free to adapt
// in the range (x, y) based on network jitter.
//
// Note: Given that this gets embedded in a union, it is up-to the owner to
// initialize these values.
struct PlayoutDelay {
  int min_ms;
  int max_ms;
};

}  // namespace webrtc

#endif  // COMMON_TYPES_H_
