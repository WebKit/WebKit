/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Callbacks for RTP and RTCP events such as modified SSRC or CSRC.
//  - SSRC handling.
//  - Transmission of RTCP sender reports.
//  - Obtaining RTCP data from incoming RTCP sender reports.
//  - RTP and RTCP statistics (jitter, packet loss, RTT etc.).
//  - Redundant Coding (RED)
//  - Writing RTP and RTCP packets to binary files for off-line analysis of
//    the call quality.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoERTP_RTCP* rtp_rtcp  = VoERTP_RTCP::GetInterface(voe);
//  base->Init();
//  int ch = base->CreateChannel();
//  ...
//  rtp_rtcp->SetLocalSSRC(ch, 12345);
//  ...
//  base->DeleteChannel(ch);
//  base->Terminate();
//  base->Release();
//  rtp_rtcp->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_H
#define WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_H

#include <vector>
#include "webrtc/common_types.h"

namespace webrtc {

class VoiceEngine;

// VoERTPObserver
class WEBRTC_DLLEXPORT VoERTPObserver {
 public:
  virtual void OnIncomingCSRCChanged(int channel,
                                     unsigned int CSRC,
                                     bool added) = 0;

  virtual void OnIncomingSSRCChanged(int channel, unsigned int SSRC) = 0;

 protected:
  virtual ~VoERTPObserver() {}
};

// CallStatistics
struct CallStatistics {
  unsigned short fractionLost;
  unsigned int cumulativeLost;
  unsigned int extendedMax;
  unsigned int jitterSamples;
  int64_t rttMs;
  size_t bytesSent;
  int packetsSent;
  size_t bytesReceived;
  int packetsReceived;
  // The capture ntp time (in local timebase) of the first played out audio
  // frame.
  int64_t capture_start_ntp_time_ms_;
};

// See section 6.4.1 in http://www.ietf.org/rfc/rfc3550.txt for details.
struct SenderInfo {
  uint32_t NTP_timestamp_high;
  uint32_t NTP_timestamp_low;
  uint32_t RTP_timestamp;
  uint32_t sender_packet_count;
  uint32_t sender_octet_count;
};

// See section 6.4.2 in http://www.ietf.org/rfc/rfc3550.txt for details.
struct ReportBlock {
  uint32_t sender_SSRC;  // SSRC of sender
  uint32_t source_SSRC;
  uint8_t fraction_lost;
  uint32_t cumulative_num_packets_lost;
  uint32_t extended_highest_sequence_number;
  uint32_t interarrival_jitter;
  uint32_t last_SR_timestamp;
  uint32_t delay_since_last_SR;
};

// VoERTP_RTCP
class WEBRTC_DLLEXPORT VoERTP_RTCP {
 public:
  // Factory for the VoERTP_RTCP sub-API. Increases an internal
  // reference counter if successful. Returns NULL if the API is not
  // supported or if construction fails.
  static VoERTP_RTCP* GetInterface(VoiceEngine* voiceEngine);

  // Releases the VoERTP_RTCP sub-API and decreases an internal
  // reference counter. Returns the new reference count. This value should
  // be zero for all sub-API:s before the VoiceEngine object can be safely
  // deleted.
  virtual int Release() = 0;

  // Sets the local RTP synchronization source identifier (SSRC) explicitly.
  virtual int SetLocalSSRC(int channel, unsigned int ssrc) = 0;

  // Gets the local RTP SSRC of a specified |channel|.
  virtual int GetLocalSSRC(int channel, unsigned int& ssrc) = 0;

  // Gets the SSRC of the incoming RTP packets.
  virtual int GetRemoteSSRC(int channel, unsigned int& ssrc) = 0;

  // Sets the status of rtp-audio-level-indication on a specific |channel|.
  virtual int SetSendAudioLevelIndicationStatus(int channel,
                                                bool enable,
                                                unsigned char id = 1) = 0;

  // Sets the RTCP status on a specific |channel|.
  virtual int SetRTCPStatus(int channel, bool enable) = 0;

  // Gets the RTCP status on a specific |channel|.
  virtual int GetRTCPStatus(int channel, bool& enabled) = 0;

  // Sets the canonical name (CNAME) parameter for RTCP reports on a
  // specific |channel|.
  virtual int SetRTCP_CNAME(int channel, const char cName[256]) = 0;

  // Gets the canonical name (CNAME) parameter for incoming RTCP reports
  // on a specific channel.
  virtual int GetRemoteRTCP_CNAME(int channel, char cName[256]) = 0;

  // Gets RTCP statistics for a specific |channel|.
  virtual int GetRTCPStatistics(int channel, CallStatistics& stats) = 0;

 protected:
  VoERTP_RTCP() {}
  virtual ~VoERTP_RTCP() {}
};

}  // namespace webrtc

#endif  // #ifndef WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_H
