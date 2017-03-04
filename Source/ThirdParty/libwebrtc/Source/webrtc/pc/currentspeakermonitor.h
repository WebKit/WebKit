/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// CurrentSpeakerMonitor monitors the audio levels for a session and determines
// which participant is currently speaking.

#ifndef WEBRTC_PC_CURRENTSPEAKERMONITOR_H_
#define WEBRTC_PC_CURRENTSPEAKERMONITOR_H_

#include <stdint.h>

#include <map>

#include "webrtc/base/sigslot.h"

namespace cricket {

struct AudioInfo;
struct MediaStreams;

class AudioSourceContext {
 public:
  sigslot::signal2<AudioSourceContext*, const cricket::AudioInfo&>
      SignalAudioMonitor;
  sigslot::signal1<AudioSourceContext*> SignalMediaStreamsReset;
  sigslot::signal3<AudioSourceContext*,
                   const cricket::MediaStreams&,
                   const cricket::MediaStreams&> SignalMediaStreamsUpdate;
};

// CurrentSpeakerMonitor can be used to monitor the audio-levels from
// many audio-sources and report on changes in the loudest audio-source.
// Its a generic type and relies on an AudioSourceContext which is aware of
// the audio-sources. AudioSourceContext needs to provide two signals namely
// SignalAudioInfoMonitor - provides audio info of the all current speakers.
// SignalMediaSourcesUpdated - provides updates when a speaker leaves or joins.
// Note that the AudioSourceContext's audio monitor must be started
// before this is started.
// It's recommended that the audio monitor be started with a 100 ms period.
class CurrentSpeakerMonitor : public sigslot::has_slots<> {
 public:
  explicit CurrentSpeakerMonitor(AudioSourceContext* audio_source_context);
  ~CurrentSpeakerMonitor();

  void Start();
  void Stop();

  // Used by tests.  Note that the actual minimum time between switches
  // enforced by the monitor will be the given value plus or minus the
  // resolution of the system clock.
  void set_min_time_between_switches(int min_time_between_switches);

  // This is fired when the current speaker changes, and provides his audio
  // SSRC.  This only fires after the audio monitor on the underlying
  // AudioSourceContext has been started.
  sigslot::signal2<CurrentSpeakerMonitor*, uint32_t> SignalUpdate;

 private:
  void OnAudioMonitor(AudioSourceContext* audio_source_context,
                      const AudioInfo& info);
  void OnMediaStreamsUpdate(AudioSourceContext* audio_source_context,
                            const MediaStreams& added,
                            const MediaStreams& removed);
  void OnMediaStreamsReset(AudioSourceContext* audio_source_context);

  // These are states that a participant will pass through so that we gradually
  // recognize that they have started and stopped speaking.  This avoids
  // "twitchiness".
  enum SpeakingState {
    SS_NOT_SPEAKING,
    SS_MIGHT_BE_SPEAKING,
    SS_SPEAKING,
    SS_WAS_SPEAKING_RECENTLY1,
    SS_WAS_SPEAKING_RECENTLY2
  };

  bool started_;
  AudioSourceContext* audio_source_context_;
  std::map<uint32_t, SpeakingState> ssrc_to_speaking_state_map_;
  uint32_t current_speaker_ssrc_;
  // To prevent overswitching, switching is disabled for some time after a
  // switch is made.  This gives us the earliest time a switch is permitted.
  int64_t earliest_permitted_switch_time_;
  int min_time_between_switches_;
};

}  // namespace cricket

#endif  // WEBRTC_PC_CURRENTSPEAKERMONITOR_H_
