/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_neteq_stats_impl.h"

#include "webrtc/modules/audio_coding/include/audio_coding_module.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

VoENetEqStats* VoENetEqStats::GetInterface(VoiceEngine* voiceEngine) {
  if (NULL == voiceEngine) {
    return NULL;
  }
  VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
  s->AddRef();
  return s;
}

VoENetEqStatsImpl::VoENetEqStatsImpl(voe::SharedData* shared)
    : _shared(shared) {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoENetEqStatsImpl::VoENetEqStatsImpl() - ctor");
}

VoENetEqStatsImpl::~VoENetEqStatsImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoENetEqStatsImpl::~VoENetEqStatsImpl() - dtor");
}

int VoENetEqStatsImpl::GetNetworkStatistics(int channel,
                                            NetworkStatistics& stats) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetNetworkStatistics() failed to locate channel");
    return -1;
  }

  return channelPtr->GetNetworkStatistics(stats);
}

int VoENetEqStatsImpl::GetDecodingCallStatistics(
    int channel, AudioDecodingCallStats* stats) const {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetDecodingCallStatistics() failed to locate "
                          "channel");
    return -1;
  }

  channelPtr->GetDecodingCallStatistics(stats);
  return 0;
}

}  // namespace webrtc
