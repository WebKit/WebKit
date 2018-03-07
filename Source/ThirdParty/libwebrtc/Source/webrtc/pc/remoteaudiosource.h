/*
 *  Copyright 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_REMOTEAUDIOSOURCE_H_
#define PC_REMOTEAUDIOSOURCE_H_

#include <list>
#include <string>

#include "api/call/audio_sink.h"
#include "api/notifier.h"
#include "pc/channel.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/messagehandler.h"

namespace rtc {
struct Message;
class Thread;
}  // namespace rtc

namespace webrtc {

// This class implements the audio source used by the remote audio track.
class RemoteAudioSource : public Notifier<AudioSourceInterface>,
                          rtc::MessageHandler {
 public:
  // Creates an instance of RemoteAudioSource.
  static rtc::scoped_refptr<RemoteAudioSource> Create(
      uint32_t ssrc,
      cricket::VoiceChannel* channel);

  // MediaSourceInterface implementation.
  MediaSourceInterface::SourceState state() const override;
  bool remote() const override;

  void AddSink(AudioTrackSinkInterface* sink) override;
  void RemoveSink(AudioTrackSinkInterface* sink) override;

 protected:
  RemoteAudioSource();
  ~RemoteAudioSource() override;

  // Post construction initialize where we can do things like save a reference
  // to ourselves (need to be fully constructed).
  void Initialize(uint32_t ssrc, cricket::VoiceChannel* channel);

 private:
  typedef std::list<AudioObserver*> AudioObserverList;

  // AudioSourceInterface implementation.
  void SetVolume(double volume) override;
  void RegisterAudioObserver(AudioObserver* observer) override;
  void UnregisterAudioObserver(AudioObserver* observer) override;

  class Sink;
  void OnData(const AudioSinkInterface::Data& audio);
  void OnAudioChannelGone();

  void OnMessage(rtc::Message* msg) override;

  AudioObserverList audio_observers_;
  rtc::CriticalSection sink_lock_;
  std::list<AudioTrackSinkInterface*> sinks_;
  rtc::Thread* const main_thread_;
  SourceState state_;
};

}  // namespace webrtc

#endif  // PC_REMOTEAUDIOSOURCE_H_
