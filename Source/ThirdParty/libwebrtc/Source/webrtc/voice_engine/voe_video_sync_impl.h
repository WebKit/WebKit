/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_VIDEO_SYNC_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_VIDEO_SYNC_IMPL_H

#include "webrtc/voice_engine/include/voe_video_sync.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoEVideoSyncImpl : public VoEVideoSync {
 public:
  int GetPlayoutBufferSize(int& bufferMs) override;

  int SetMinimumPlayoutDelay(int channel, int delayMs) override;

  int GetDelayEstimate(int channel,
                       int* jitter_buffer_delay_ms,
                       int* playout_buffer_delay_ms) override;

  int GetLeastRequiredDelayMs(int channel) const override;

  int SetInitTimestamp(int channel, unsigned int timestamp) override;

  int SetInitSequenceNumber(int channel, short sequenceNumber) override;

  int GetPlayoutTimestamp(int channel, unsigned int& timestamp) override;

  int GetRtpRtcp(int channel,
                 RtpRtcp** rtpRtcpModule,
                 RtpReceiver** rtp_receiver) override;

 protected:
  VoEVideoSyncImpl(voe::SharedData* shared);
  ~VoEVideoSyncImpl() override;

 private:
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_VIDEO_SYNC_IMPL_H
