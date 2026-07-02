/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKE_VIDEO_TRACK_H_
#define PC_TEST_FAKE_VIDEO_TRACK_H_

#include <string>
#include <utility>

#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_stream_track.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"

namespace webrtc {

class FakeVideoTrack : public MediaStreamTrack<VideoTrackInterface> {
 public:
  static scoped_refptr<FakeVideoTrack> Create(
      const std::string& id,
      MediaStreamTrackInterface::TrackState state,
      scoped_refptr<VideoTrackSourceInterface> source) {
    auto video_track = make_ref_counted<FakeVideoTrack>(id, std::move(source));
    video_track->set_state(state);
    return video_track;
  }

  FakeVideoTrack(const std::string& id,
                 scoped_refptr<VideoTrackSourceInterface> source)
      : MediaStreamTrack<VideoTrackInterface>(id), source_(source) {}

  std::string kind() const override {
    return MediaStreamTrackInterface::kVideoKind;
  }

  void AddOrUpdateSink(VideoSinkInterface<VideoFrame>* sink,
                       const VideoSinkWants& wants) override {}
  void RemoveSink(VideoSinkInterface<VideoFrame>* sink) override {}

  VideoTrackSourceInterface* GetSource() const override {
    return source_.get();
  }

 private:
  scoped_refptr<VideoTrackSourceInterface> source_;
};

}  // namespace webrtc

#endif  // PC_TEST_FAKE_VIDEO_TRACK_H_
