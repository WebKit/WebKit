/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_SHARED_DATA_H_
#define VOICE_ENGINE_SHARED_DATA_H_

#include <memory>

#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/thread_checker.h"
#include "voice_engine/channel_manager.h"

class ProcessThread;

namespace webrtc {
namespace voe {

class TransmitMixer;

class SharedData
{
public:
    // Public accessors.
    uint32_t instance_id() const { return _instanceId; }
    ChannelManager& channel_manager() { return _channelManager; }
    AudioDeviceModule* audio_device() { return _audioDevicePtr.get(); }
    void set_audio_device(
        const rtc::scoped_refptr<AudioDeviceModule>& audio_device);
    void set_audio_processing(AudioProcessing* audio_processing);
    TransmitMixer* transmit_mixer() { return _transmitMixerPtr; }
    rtc::CriticalSection* crit_sec() { return &_apiCritPtr; }
    ProcessThread* process_thread() { return _moduleProcessThreadPtr.get(); }
    rtc::TaskQueue* encoder_queue();

    int NumOfSendingChannels();
    int NumOfPlayingChannels();

protected:
 rtc::ThreadChecker construction_thread_;
 const uint32_t _instanceId;
 rtc::CriticalSection _apiCritPtr;
 ChannelManager _channelManager;
 rtc::scoped_refptr<AudioDeviceModule> _audioDevicePtr;
 TransmitMixer* _transmitMixerPtr;
 std::unique_ptr<ProcessThread> _moduleProcessThreadPtr;
 // |encoder_queue| is defined last to ensure all pending tasks are cancelled
 // and deleted before any other members.
 rtc::TaskQueue encoder_queue_ RTC_ACCESS_ON(construction_thread_);

 SharedData();
 virtual ~SharedData();
};

}  // namespace voe
}  // namespace webrtc
#endif // VOICE_ENGINE_SHARED_DATA_H_
