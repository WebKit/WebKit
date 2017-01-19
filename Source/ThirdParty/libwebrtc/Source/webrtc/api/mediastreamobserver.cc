/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/mediastreamobserver.h"

#include <algorithm>

namespace webrtc {

MediaStreamObserver::MediaStreamObserver(MediaStreamInterface* stream)
    : stream_(stream),
      cached_audio_tracks_(stream->GetAudioTracks()),
      cached_video_tracks_(stream->GetVideoTracks()) {
  stream_->RegisterObserver(this);
}

MediaStreamObserver::~MediaStreamObserver() {
  stream_->UnregisterObserver(this);
}

void MediaStreamObserver::OnChanged() {
  AudioTrackVector new_audio_tracks = stream_->GetAudioTracks();
  VideoTrackVector new_video_tracks = stream_->GetVideoTracks();

  // Find removed audio tracks.
  for (const auto& cached_track : cached_audio_tracks_) {
    auto it = std::find_if(
        new_audio_tracks.begin(), new_audio_tracks.end(),
        [cached_track](const AudioTrackVector::value_type& new_track) {
          return new_track->id().compare(cached_track->id()) == 0;
        });
    if (it == new_audio_tracks.end()) {
      SignalAudioTrackRemoved(cached_track.get(), stream_);
    }
  }

  // Find added audio tracks.
  for (const auto& new_track : new_audio_tracks) {
    auto it = std::find_if(
        cached_audio_tracks_.begin(), cached_audio_tracks_.end(),
        [new_track](const AudioTrackVector::value_type& cached_track) {
          return new_track->id().compare(cached_track->id()) == 0;
        });
    if (it == cached_audio_tracks_.end()) {
      SignalAudioTrackAdded(new_track.get(), stream_);
    }
  }

  // Find removed video tracks.
  for (const auto& cached_track : cached_video_tracks_) {
    auto it = std::find_if(
        new_video_tracks.begin(), new_video_tracks.end(),
        [cached_track](const VideoTrackVector::value_type& new_track) {
          return new_track->id().compare(cached_track->id()) == 0;
        });
    if (it == new_video_tracks.end()) {
      SignalVideoTrackRemoved(cached_track.get(), stream_);
    }
  }

  // Find added video tracks.
  for (const auto& new_track : new_video_tracks) {
    auto it = std::find_if(
        cached_video_tracks_.begin(), cached_video_tracks_.end(),
        [new_track](const VideoTrackVector::value_type& cached_track) {
          return new_track->id().compare(cached_track->id()) == 0;
        });
    if (it == cached_video_tracks_.end()) {
      SignalVideoTrackAdded(new_track.get(), stream_);
    }
  }

  cached_audio_tracks_ = new_audio_tracks;
  cached_video_tracks_ = new_video_tracks;
}

}  // namespace webrtc
