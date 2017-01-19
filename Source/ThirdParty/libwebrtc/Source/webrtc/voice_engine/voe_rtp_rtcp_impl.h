/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_IMPL_H

#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoERTP_RTCPImpl : public VoERTP_RTCP {
 public:
  // RTCP
  int SetRTCPStatus(int channel, bool enable) override;

  int GetRTCPStatus(int channel, bool& enabled) override;

  int SetRTCP_CNAME(int channel, const char cName[256]) override;

  int GetRemoteRTCP_CNAME(int channel, char cName[256]) override;

  int GetRemoteRTCPData(int channel,
                        unsigned int& NTPHigh,
                        unsigned int& NTPLow,
                        unsigned int& timestamp,
                        unsigned int& playoutTimestamp,
                        unsigned int* jitter = NULL,
                        unsigned short* fractionLost = NULL) override;

  // SSRC
  int SetLocalSSRC(int channel, unsigned int ssrc) override;

  int GetLocalSSRC(int channel, unsigned int& ssrc) override;

  int GetRemoteSSRC(int channel, unsigned int& ssrc) override;

  // RTP Header Extension for Client-to-Mixer Audio Level Indication
  int SetSendAudioLevelIndicationStatus(int channel,
                                        bool enable,
                                        unsigned char id) override;
  int SetReceiveAudioLevelIndicationStatus(int channel,
                                           bool enable,
                                           unsigned char id) override;

  // Statistics
  int GetRTPStatistics(int channel,
                       unsigned int& averageJitterMs,
                       unsigned int& maxJitterMs,
                       unsigned int& discardedPackets) override;

  int GetRTCPStatistics(int channel, CallStatistics& stats) override;

  int GetRemoteRTCPReportBlocks(
      int channel,
      std::vector<ReportBlock>* report_blocks) override;

  // NACK
  int SetNACKStatus(int channel, bool enable, int maxNoPackets) override;

 protected:
  VoERTP_RTCPImpl(voe::SharedData* shared);
  ~VoERTP_RTCPImpl() override;

 private:
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_IMPL_H
