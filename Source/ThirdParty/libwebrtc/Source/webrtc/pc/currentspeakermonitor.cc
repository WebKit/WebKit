/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/currentspeakermonitor.h"

#include <vector>

#include "media/base/streamparams.h"
#include "pc/audiomonitor.h"
#include "rtc_base/logging.h"

namespace cricket {

namespace {
const int kMaxAudioLevel = 9;
// To avoid overswitching, we disable switching for a period of time after a
// switch is done.
const int kDefaultMinTimeBetweenSwitches = 1000;
}

CurrentSpeakerMonitor::CurrentSpeakerMonitor(
    AudioSourceContext* audio_source_context)
    : started_(false),
      audio_source_context_(audio_source_context),
      current_speaker_ssrc_(0),
      earliest_permitted_switch_time_(0),
      min_time_between_switches_(kDefaultMinTimeBetweenSwitches) {}

CurrentSpeakerMonitor::~CurrentSpeakerMonitor() {
  Stop();
}

void CurrentSpeakerMonitor::Start() {
  if (!started_) {
    audio_source_context_->SignalAudioMonitor.connect(
        this, &CurrentSpeakerMonitor::OnAudioMonitor);
    audio_source_context_->SignalMediaStreamsUpdate.connect(
        this, &CurrentSpeakerMonitor::OnMediaStreamsUpdate);
    audio_source_context_->SignalMediaStreamsReset.connect(
        this, &CurrentSpeakerMonitor::OnMediaStreamsReset);

    started_ = true;
  }
}

void CurrentSpeakerMonitor::Stop() {
  if (started_) {
    audio_source_context_->SignalAudioMonitor.disconnect(this);
    audio_source_context_->SignalMediaStreamsUpdate.disconnect(this);

    started_ = false;
    ssrc_to_speaking_state_map_.clear();
    current_speaker_ssrc_ = 0;
    earliest_permitted_switch_time_ = 0;
  }
}

void CurrentSpeakerMonitor::set_min_time_between_switches(
    int min_time_between_switches) {
  min_time_between_switches_ = min_time_between_switches;
}

void CurrentSpeakerMonitor::OnAudioMonitor(
    AudioSourceContext* audio_source_context, const AudioInfo& info) {
  std::map<uint32_t, int> active_ssrc_to_level_map;
  cricket::AudioInfo::StreamList::const_iterator stream_list_it;
  for (stream_list_it = info.active_streams.begin();
       stream_list_it != info.active_streams.end(); ++stream_list_it) {
    uint32_t ssrc = stream_list_it->first;
    active_ssrc_to_level_map[ssrc] = stream_list_it->second;

    // It's possible we haven't yet added this source to our map.  If so,
    // add it now with a "not speaking" state.
    if (ssrc_to_speaking_state_map_.find(ssrc) ==
        ssrc_to_speaking_state_map_.end()) {
      ssrc_to_speaking_state_map_[ssrc] = SS_NOT_SPEAKING;
    }
  }

  int max_level = 0;
  uint32_t loudest_speaker_ssrc = 0;

  // Update the speaking states of all participants based on the new audio
  // level information.  Also retain loudest speaker.
  std::map<uint32_t, SpeakingState>::iterator state_it;
  for (state_it = ssrc_to_speaking_state_map_.begin();
       state_it != ssrc_to_speaking_state_map_.end(); ++state_it) {
    bool is_previous_speaker = current_speaker_ssrc_ == state_it->first;

    // This uses a state machine in order to gradually identify
    // members as having started or stopped speaking. Matches the
    // algorithm used by the hangouts js code.

    std::map<uint32_t, int>::const_iterator level_it =
        active_ssrc_to_level_map.find(state_it->first);
    // Note that the stream map only contains streams with non-zero audio
    // levels.
    int level = (level_it != active_ssrc_to_level_map.end()) ?
        level_it->second : 0;
    switch (state_it->second) {
      case SS_NOT_SPEAKING:
        if (level > 0) {
          // Reset level because we don't think they're really speaking.
          level = 0;
          state_it->second = SS_MIGHT_BE_SPEAKING;
        } else {
          // State unchanged.
        }
        break;
      case SS_MIGHT_BE_SPEAKING:
        if (level > 0) {
          state_it->second = SS_SPEAKING;
        } else {
          state_it->second = SS_NOT_SPEAKING;
        }
        break;
      case SS_SPEAKING:
        if (level > 0) {
          // State unchanged.
        } else {
          state_it->second = SS_WAS_SPEAKING_RECENTLY1;
          if (is_previous_speaker) {
            // Assume this is an inter-word silence and assign him the highest
            // volume.
            level = kMaxAudioLevel;
          }
        }
        break;
      case SS_WAS_SPEAKING_RECENTLY1:
        if (level > 0) {
          state_it->second = SS_SPEAKING;
        } else {
          state_it->second = SS_WAS_SPEAKING_RECENTLY2;
          if (is_previous_speaker) {
            // Assume this is an inter-word silence and assign him the highest
            // volume.
            level = kMaxAudioLevel;
          }
        }
        break;
      case SS_WAS_SPEAKING_RECENTLY2:
        if (level > 0) {
          state_it->second = SS_SPEAKING;
        } else {
          state_it->second = SS_NOT_SPEAKING;
        }
        break;
    }

    if (level > max_level) {
      loudest_speaker_ssrc = state_it->first;
      max_level = level;
    } else if (level > 0 && level == max_level && is_previous_speaker) {
      // Favor continuity of loudest speakers if audio levels are equal.
      loudest_speaker_ssrc = state_it->first;
    }
  }

  // We avoid over-switching by disabling switching for a period of time after
  // a switch is done.
  int64_t now = rtc::TimeMillis();
  if (earliest_permitted_switch_time_ <= now &&
      current_speaker_ssrc_ != loudest_speaker_ssrc) {
    current_speaker_ssrc_ = loudest_speaker_ssrc;
    RTC_LOG(LS_INFO) << "Current speaker changed to " << current_speaker_ssrc_;
    earliest_permitted_switch_time_ = now + min_time_between_switches_;
    SignalUpdate(this, current_speaker_ssrc_);
  }
}

void CurrentSpeakerMonitor::OnMediaStreamsUpdate(
    AudioSourceContext* audio_source_context,
    const MediaStreams& added,
    const MediaStreams& removed) {
  if (audio_source_context == audio_source_context_) {
    // Update the speaking state map based on added and removed streams.
    for (std::vector<cricket::StreamParams>::const_iterator
           it = removed.audio().begin(); it != removed.audio().end(); ++it) {
      ssrc_to_speaking_state_map_.erase(it->first_ssrc());
    }

    for (std::vector<cricket::StreamParams>::const_iterator
           it = added.audio().begin(); it != added.audio().end(); ++it) {
      ssrc_to_speaking_state_map_[it->first_ssrc()] = SS_NOT_SPEAKING;
    }
  }
}

void CurrentSpeakerMonitor::OnMediaStreamsReset(
    AudioSourceContext* audio_source_context) {
  if (audio_source_context == audio_source_context_) {
    ssrc_to_speaking_state_map_.clear();
  }
}

}  // namespace cricket
