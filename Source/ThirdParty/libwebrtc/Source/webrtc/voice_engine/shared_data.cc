/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/shared_data.h"

#include "modules/audio_processing/include/audio_processing.h"
#include "voice_engine/channel.h"
#include "voice_engine/transmit_mixer.h"

namespace webrtc {

namespace voe {

static int32_t _gInstanceCounter = 0;

SharedData::SharedData()
    : _instanceId(++_gInstanceCounter),
      _channelManager(_gInstanceCounter),
      _audioDevicePtr(NULL),
      _moduleProcessThreadPtr(ProcessThread::Create("VoiceProcessThread")),
      encoder_queue_("AudioEncoderQueue") {
  if (TransmitMixer::Create(_transmitMixerPtr) == 0) {
    _transmitMixerPtr->SetEngineInformation(&_channelManager);
  }
}

SharedData::~SharedData()
{
    TransmitMixer::Destroy(_transmitMixerPtr);
    if (_audioDevicePtr) {
        _audioDevicePtr->Release();
    }
    _moduleProcessThreadPtr->Stop();
}

rtc::TaskQueue* SharedData::encoder_queue() {
  RTC_DCHECK_RUN_ON(&construction_thread_);
  return &encoder_queue_;
}

void SharedData::set_audio_device(
    const rtc::scoped_refptr<AudioDeviceModule>& audio_device) {
  _audioDevicePtr = audio_device;
}

void SharedData::set_audio_processing(AudioProcessing* audioproc) {
  _transmitMixerPtr->SetAudioProcessingModule(audioproc);
}

int SharedData::NumOfSendingChannels() {
  ChannelManager::Iterator it(&_channelManager);
  int sending_channels = 0;

  for (ChannelManager::Iterator it(&_channelManager); it.IsValid();
       it.Increment()) {
    if (it.GetChannel()->Sending())
      ++sending_channels;
  }

  return sending_channels;
}

int SharedData::NumOfPlayingChannels() {
  ChannelManager::Iterator it(&_channelManager);
  int playout_channels = 0;

  for (ChannelManager::Iterator it(&_channelManager); it.IsValid();
       it.Increment()) {
    if (it.GetChannel()->Playing())
      ++playout_channels;
  }

  return playout_channels;
}

}  // namespace voe

}  // namespace webrtc
