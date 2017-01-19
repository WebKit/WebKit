/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - RTP header modification (time stamp and sequence number fields).
//  - Playout delay tuning to synchronize the voice with video.
//  - Playout delay monitoring.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoEVideoSync* vsync  = VoEVideoSync::GetInterface(voe);
//  base->Init();
//  ...
//  int buffer_ms(0);
//  vsync->GetPlayoutBufferSize(buffer_ms);
//  ...
//  base->Terminate();
//  base->Release();
//  vsync->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_VIDEO_SYNC_H
#define WEBRTC_VOICE_ENGINE_VOE_VIDEO_SYNC_H

#include "webrtc/common_types.h"

namespace webrtc {

class RtpReceiver;
class RtpRtcp;
class VoiceEngine;

class WEBRTC_DLLEXPORT VoEVideoSync {
 public:
  // Factory for the VoEVideoSync sub-API. Increases an internal
  // reference counter if successful. Returns NULL if the API is not
  // supported or if construction fails.
  static VoEVideoSync* GetInterface(VoiceEngine* voiceEngine);

  // Releases the VoEVideoSync sub-API and decreases an internal
  // reference counter. Returns the new reference count. This value should
  // be zero for all sub-API:s before the VoiceEngine object can be safely
  // deleted.
  virtual int Release() = 0;

  // Gets the current sound card buffer size (playout delay).
  virtual int GetPlayoutBufferSize(int& buffer_ms) = 0;

  // Sets a minimum target delay for the jitter buffer. This delay is
  // maintained by the jitter buffer, unless channel condition (jitter in
  // inter-arrival times) dictates a higher required delay. The overall
  // jitter buffer delay is max of |delay_ms| and the latency that NetEq
  // computes based on inter-arrival times and its playout mode.
  virtual int SetMinimumPlayoutDelay(int channel, int delay_ms) = 0;

  // Gets the |jitter_buffer_delay_ms| (including the algorithmic delay), and
  // the |playout_buffer_delay_ms| for a specified |channel|.
  virtual int GetDelayEstimate(int channel,
                               int* jitter_buffer_delay_ms,
                               int* playout_buffer_delay_ms) = 0;

  // Returns the least required jitter buffer delay. This is computed by the
  // the jitter buffer based on the inter-arrival time of RTP packets and
  // playout mode. NetEq maintains this latency unless a higher value is
  // requested by calling SetMinimumPlayoutDelay().
  virtual int GetLeastRequiredDelayMs(int channel) const = 0;

  // Manual initialization of the RTP timestamp.
  virtual int SetInitTimestamp(int channel, unsigned int timestamp) = 0;

  // Manual initialization of the RTP sequence number.
  virtual int SetInitSequenceNumber(int channel, short sequenceNumber) = 0;

  // Get the received RTP timestamp
  virtual int GetPlayoutTimestamp(int channel, unsigned int& timestamp) = 0;

  virtual int GetRtpRtcp(int channel,
                         RtpRtcp** rtpRtcpModule,
                         RtpReceiver** rtp_receiver) = 0;

 protected:
  VoEVideoSync() {}
  virtual ~VoEVideoSync() {}
};

}  // namespace webrtc

#endif  // #ifndef WEBRTC_VOICE_ENGINE_VOE_VIDEO_SYNC_H
