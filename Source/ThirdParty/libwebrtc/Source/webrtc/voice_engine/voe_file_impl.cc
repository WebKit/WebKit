/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/voe_file_impl.h"

#include "webrtc/modules/media_file/media_file.h"
#include "webrtc/system_wrappers/include/file_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/output_mixer.h"
#include "webrtc/voice_engine/transmit_mixer.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {

VoEFile* VoEFile::GetInterface(VoiceEngine* voiceEngine) {
#ifndef WEBRTC_VOICE_ENGINE_FILE_API
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

#ifdef WEBRTC_VOICE_ENGINE_FILE_API

VoEFileImpl::VoEFileImpl(voe::SharedData* shared) : _shared(shared) {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoEFileImpl::VoEFileImpl() - ctor");
}

VoEFileImpl::~VoEFileImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "VoEFileImpl::~VoEFileImpl() - dtor");
}

int VoEFileImpl::StartPlayingFileLocally(int channel,
                                         const char fileNameUTF8[1024],
                                         bool loop,
                                         FileFormats format,
                                         float volumeScaling,
                                         int startPointMs,
                                         int stopPointMs) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StartPlayingFileLocally(channel=%d, fileNameUTF8[]=%s, "
               "loop=%d, format=%d, volumeScaling=%5.3f, startPointMs=%d,"
               " stopPointMs=%d)",
               channel, fileNameUTF8, loop, format, volumeScaling, startPointMs,
               stopPointMs);
  static_assert(1024 == FileWrapper::kMaxFileNameSize, "");
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "StartPlayingFileLocally() failed to locate channel");
    return -1;
  }

  return channelPtr->StartPlayingFileLocally(fileNameUTF8, loop, format,
                                             startPointMs, volumeScaling,
                                             stopPointMs, NULL);
}

int VoEFileImpl::StartPlayingFileLocally(int channel,
                                         InStream* stream,
                                         FileFormats format,
                                         float volumeScaling,
                                         int startPointMs,
                                         int stopPointMs) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StartPlayingFileLocally(channel=%d, stream, format=%d, "
               "volumeScaling=%5.3f, startPointMs=%d, stopPointMs=%d)",
               channel, format, volumeScaling, startPointMs, stopPointMs);

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "StartPlayingFileLocally() failed to locate channel");
    return -1;
  }

  return channelPtr->StartPlayingFileLocally(stream, format, startPointMs,
                                             volumeScaling, stopPointMs, NULL);
}

int VoEFileImpl::StopPlayingFileLocally(int channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StopPlayingFileLocally()");
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "StopPlayingFileLocally() failed to locate channel");
    return -1;
  }
  return channelPtr->StopPlayingFileLocally();
}

int VoEFileImpl::IsPlayingFileLocally(int channel) {
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
  voe::Channel* channelPtr = ch.channel();
  if (channelPtr == NULL) {
    _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                          "StopPlayingFileLocally() failed to locate channel");
    return -1;
  }
  return channelPtr->IsPlayingFileLocally();
}

int VoEFileImpl::StartPlayingFileAsMicrophone(int channel,
                                              const char fileNameUTF8[1024],
                                              bool loop,
                                              bool mixWithMicrophone,
                                              FileFormats format,
                                              float volumeScaling) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StartPlayingFileAsMicrophone(channel=%d, fileNameUTF8=%s, "
               "loop=%d, mixWithMicrophone=%d, format=%d, "
               "volumeScaling=%5.3f)",
               channel, fileNameUTF8, loop, mixWithMicrophone, format,
               volumeScaling);
  static_assert(1024 == FileWrapper::kMaxFileNameSize, "");
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  const uint32_t startPointMs(0);
  const uint32_t stopPointMs(0);

  if (channel == -1) {
    int res = _shared->transmit_mixer()->StartPlayingFileAsMicrophone(
        fileNameUTF8, loop, format, startPointMs, volumeScaling, stopPointMs,
        NULL);
    if (res) {
      WEBRTC_TRACE(
          kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
          "StartPlayingFileAsMicrophone() failed to start playing file");
      return (-1);
    } else {
      _shared->transmit_mixer()->SetMixWithMicStatus(mixWithMicrophone);
      return (0);
    }
  } else {
    // Add file after demultiplexing <=> affects one channel only
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(
          VE_CHANNEL_NOT_VALID, kTraceError,
          "StartPlayingFileAsMicrophone() failed to locate channel");
      return -1;
    }

    int res = channelPtr->StartPlayingFileAsMicrophone(
        fileNameUTF8, loop, format, startPointMs, volumeScaling, stopPointMs,
        NULL);
    if (res) {
      WEBRTC_TRACE(
          kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
          "StartPlayingFileAsMicrophone() failed to start playing file");
      return -1;
    } else {
      channelPtr->SetMixWithMicStatus(mixWithMicrophone);
      return 0;
    }
  }
}

int VoEFileImpl::StartPlayingFileAsMicrophone(int channel,
                                              InStream* stream,
                                              bool mixWithMicrophone,
                                              FileFormats format,
                                              float volumeScaling) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StartPlayingFileAsMicrophone(channel=%d, stream,"
               " mixWithMicrophone=%d, format=%d, volumeScaling=%5.3f)",
               channel, mixWithMicrophone, format, volumeScaling);

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  const uint32_t startPointMs(0);
  const uint32_t stopPointMs(0);

  if (channel == -1) {
    int res = _shared->transmit_mixer()->StartPlayingFileAsMicrophone(
        stream, format, startPointMs, volumeScaling, stopPointMs, NULL);
    if (res) {
      WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
                   "StartPlayingFileAsMicrophone() failed to start "
                   "playing stream");
      return (-1);
    } else {
      _shared->transmit_mixer()->SetMixWithMicStatus(mixWithMicrophone);
      return (0);
    }
  } else {
    // Add file after demultiplexing <=> affects one channel only
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(
          VE_CHANNEL_NOT_VALID, kTraceError,
          "StartPlayingFileAsMicrophone() failed to locate channel");
      return -1;
    }

    int res = channelPtr->StartPlayingFileAsMicrophone(
        stream, format, startPointMs, volumeScaling, stopPointMs, NULL);
    if (res) {
      WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
                   "StartPlayingFileAsMicrophone() failed to start "
                   "playing stream");
      return -1;
    } else {
      channelPtr->SetMixWithMicStatus(mixWithMicrophone);
      return 0;
    }
  }
}

int VoEFileImpl::StopPlayingFileAsMicrophone(int channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StopPlayingFileAsMicrophone(channel=%d)", channel);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (channel == -1) {
    // Stop adding file before demultiplexing <=> affects all channels
    return _shared->transmit_mixer()->StopPlayingFileAsMicrophone();
  } else {
    // Stop adding file after demultiplexing <=> affects one channel only
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(
          VE_CHANNEL_NOT_VALID, kTraceError,
          "StopPlayingFileAsMicrophone() failed to locate channel");
      return -1;
    }
    return channelPtr->StopPlayingFileAsMicrophone();
  }
}

int VoEFileImpl::IsPlayingFileAsMicrophone(int channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "IsPlayingFileAsMicrophone(channel=%d)", channel);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (channel == -1) {
    return _shared->transmit_mixer()->IsPlayingFileAsMicrophone();
  } else {
    // Stop adding file after demultiplexing <=> affects one channel only
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(
          VE_CHANNEL_NOT_VALID, kTraceError,
          "IsPlayingFileAsMicrophone() failed to locate channel");
      return -1;
    }
    return channelPtr->IsPlayingFileAsMicrophone();
  }
}

int VoEFileImpl::StartRecordingPlayout(int channel,
                                       const char* fileNameUTF8,
                                       CodecInst* compression,
                                       int maxSizeBytes) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StartRecordingPlayout(channel=%d, fileNameUTF8=%s, "
               "compression, maxSizeBytes=%d)",
               channel, fileNameUTF8, maxSizeBytes);
  static_assert(1024 == FileWrapper::kMaxFileNameSize, "");

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (channel == -1) {
    return _shared->output_mixer()->StartRecordingPlayout(fileNameUTF8,
                                                          compression);
  } else {
    // Add file after demultiplexing <=> affects one channel only
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                            "StartRecordingPlayout() failed to locate channel");
      return -1;
    }
    return channelPtr->StartRecordingPlayout(fileNameUTF8, compression);
  }
}

int VoEFileImpl::StartRecordingPlayout(int channel,
                                       OutStream* stream,
                                       CodecInst* compression) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StartRecordingPlayout(channel=%d, stream, compression)",
               channel);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (channel == -1) {
    return _shared->output_mixer()->StartRecordingPlayout(stream, compression);
  } else {
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                            "StartRecordingPlayout() failed to locate channel");
      return -1;
    }
    return channelPtr->StartRecordingPlayout(stream, compression);
  }
}

int VoEFileImpl::StopRecordingPlayout(int channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StopRecordingPlayout(channel=%d)", channel);
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (channel == -1) {
    return _shared->output_mixer()->StopRecordingPlayout();
  } else {
    voe::ChannelOwner ch = _shared->channel_manager().GetChannel(channel);
    voe::Channel* channelPtr = ch.channel();
    if (channelPtr == NULL) {
      _shared->SetLastError(VE_CHANNEL_NOT_VALID, kTraceError,
                            "StopRecordingPlayout() failed to locate channel");
      return -1;
    }
    return channelPtr->StopRecordingPlayout();
  }
}

int VoEFileImpl::StartRecordingMicrophone(const char* fileNameUTF8,
                                          CodecInst* compression,
                                          int maxSizeBytes) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StartRecordingMicrophone(fileNameUTF8=%s, compression, "
               "maxSizeBytes=%d)",
               fileNameUTF8, maxSizeBytes);
  static_assert(1024 == FileWrapper::kMaxFileNameSize, "");

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (_shared->transmit_mixer()->StartRecordingMicrophone(fileNameUTF8,
                                                          compression)) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartRecordingMicrophone() failed to start recording");
    return -1;
  }
  if (!_shared->audio_device()->Recording()) {
    if (_shared->audio_device()->InitRecording() != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
                   "StartRecordingMicrophone() failed to initialize recording");
      return -1;
    }
    if (_shared->audio_device()->StartRecording() != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
                   "StartRecordingMicrophone() failed to start recording");
      return -1;
    }
  }
  return 0;
}

int VoEFileImpl::StartRecordingMicrophone(OutStream* stream,
                                          CodecInst* compression) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StartRecordingMicrophone(stream, compression)");

  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }
  if (_shared->transmit_mixer()->StartRecordingMicrophone(stream,
                                                          compression) == -1) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StartRecordingMicrophone() failed to start recording");
    return -1;
  }
  if (!_shared->audio_device()->Recording()) {
    if (_shared->audio_device()->InitRecording() != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
                   "StartRecordingMicrophone() failed to initialize recording");
      return -1;
    }
    if (_shared->audio_device()->StartRecording() != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
                   "StartRecordingMicrophone() failed to start recording");
      return -1;
    }
  }
  return 0;
}

int VoEFileImpl::StopRecordingMicrophone() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVoice, VoEId(_shared->instance_id(), -1),
               "StopRecordingMicrophone()");
  if (!_shared->statistics().Initialized()) {
    _shared->SetLastError(VE_NOT_INITED, kTraceError);
    return -1;
  }

  int err = 0;

  // TODO(xians): consider removing Start/StopRecording() in
  // Start/StopRecordingMicrophone() if no channel is recording.
  if (_shared->NumOfSendingChannels() == 0 &&
      _shared->audio_device()->Recording()) {
    // Stop audio-device recording if no channel is recording
    if (_shared->audio_device()->StopRecording() != 0) {
      _shared->SetLastError(
          VE_CANNOT_STOP_RECORDING, kTraceError,
          "StopRecordingMicrophone() failed to stop recording");
      err = -1;
    }
  }

  if (_shared->transmit_mixer()->StopRecordingMicrophone() != 0) {
    WEBRTC_TRACE(kTraceError, kTraceVoice, VoEId(_shared->instance_id(), -1),
                 "StopRecordingMicrophone() failed to stop recording to mixer");
    err = -1;
  }

  return err;
}

#endif  // #ifdef WEBRTC_VOICE_ENGINE_FILE_API

}  // namespace webrtc
