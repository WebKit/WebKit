/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_external_media_impl.h"

#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/output_mixer.h"
#include "webrtc/voice_engine/transmit_mixer.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

VoEExternalMedia* VoEExternalMedia::GetInterface(VoiceEngine* voiceEngine) {
#ifndef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
  return NULL;
#else
  if (NULL == voiceEngine) {
    return NULL;
  }
  VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
  s->AddRef();
  return s;
#endif
}

#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API

VoEExternalMediaImpl::VoEExternalMediaImpl(voe::SharedData* shared)
    :
#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
      playout_delay_ms_(0),
#endif
      shared_(shared) {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(shared_->instance_id(), -1),
               "VoEExternalMediaImpl() - ctor");
}

VoEExternalMediaImpl::~VoEExternalMediaImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(shared_->instance_id(), -1),
               "~VoEExternalMediaImpl() - dtor");
}

int VoEExternalMediaImpl::RegisterExternalMediaProcessing(
    int channel,
    ProcessingTypes type,
    VoEMediaProcess& processObject) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(shared_->instance_id(), -1),
               "RegisterExternalMediaProcessing(channel=%d, type=%d, "
               "processObject=0x%x)",
               channel, type, &processObject);
  if (!shared_->statistics().Initialized()) {
    shared_->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  switch (type) {
    case kPlaybackPerChannel:
    case kRecordingPerChannel: {
      voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
      voe::Channel* channelPtr = ch.channel();
      if (channelPtr == NULL) {
        shared_->SetLastError(
            VE_CHANNEL_NOT_VALID, kTraceError,
            "RegisterExternalMediaProcessing() failed to locate "
            "channel");
        return -1;
      }
      return channelPtr->RegisterExternalMediaProcessing(type, processObject);
    }
    case kPlaybackAllChannelsMixed: {
      return shared_->output_mixer()->RegisterExternalMediaProcessing(
          processObject);
    }
    case kRecordingAllChannelsMixed:
    case kRecordingPreprocessing: {
      return shared_->transmit_mixer()->RegisterExternalMediaProcessing(
          &processObject, type);
    }
  }
  return -1;
}

int VoEExternalMediaImpl::DeRegisterExternalMediaProcessing(
    int channel,
    ProcessingTypes type) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(shared_->instance_id(), -1),
               "DeRegisterExternalMediaProcessing(channel=%d)", channel);
  if (!shared_->statistics().Initialized()) {
    shared_->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  switch (type) {
    case kPlaybackPerChannel:
    case kRecordingPerChannel: {
      voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
      voe::Channel* channelPtr = ch.channel();
      if (channelPtr == NULL) {
        shared_->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                              "RegisterExternalMediaProcessing() "
                              "failed to locate channel");
        return -1;
      }
      return channelPtr->DeRegisterExternalMediaProcessing(type);
    }
    case kPlaybackAllChannelsMixed: {
      return shared_->output_mixer()->DeRegisterExternalMediaProcessing();
    }
    case kRecordingAllChannelsMixed:
    case kRecordingPreprocessing: {
      return shared_->transmit_mixer()->DeRegisterExternalMediaProcessing(type);
    }
  }
  return -1;
}

int VoEExternalMediaImpl::GetAudioFrame(int channel, int desired_sample_rate_hz,
                                        AudioFrame* frame) {
  if (!shared_->statistics().Initialized()) {
    shared_->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    shared_->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "GetAudioFrame() failed to locate channel");
    return -1;
  }
  if (!channelPtr->ExternalMixing()) {
    shared_->SetLastError(VE_INVALID_OPERATION, kTraceError,
                          "GetAudioFrame() was called on channel that is not"
                          " externally mixed.");
    return -1;
  }
  if (!channelPtr->Playing()) {
    shared_->SetLastError(
        VE_INVALID_OPERATION, kTraceError,
        "GetAudioFrame() was called on channel that is not playing.");
    return -1;
  }
  if (desired_sample_rate_hz == -1) {
    shared_->SetLastError(VE_BAD_ARGUMENT, kTraceError,
                          "GetAudioFrame() was called with bad sample rate.");
    return -1;
  }
  frame->sample_rate_hz_ =
      desired_sample_rate_hz == 0 ? -1 : desired_sample_rate_hz;
  auto ret = channelPtr->GetAudioFrameWithMuted(channel, frame);
  if (ret == MixerParticipant::AudioFrameInfo::kMuted) {
    frame->Mute();
  }
  return ret == MixerParticipant::AudioFrameInfo::kError ? -1 : 0;
}

int VoEExternalMediaImpl::SetExternalMixing(int channel, bool enable) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice,
               VoEId(shared_->instance_id(), channel),
               "SetExternalMixing(channel=%d, enable=%d)", channel, enable);
  if (!shared_->statistics().Initialized()) {
    shared_->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = shared_->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    shared_->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "SetExternalMixing() failed to locate channel");
    return -1;
  }
  return channelPtr->SetExternalMixing(enable);
}

#endif  // WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API

}  // namespace webrtc
