/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKE_AUDIO_TRACK_H_
#define PC_TEST_FAKE_AUDIO_TRACK_H_

#include <string>

#include "api/make_ref_counted.h"
#include "api/media_stream_interface.h"
#include "api/media_stream_track.h"
#include "api/scoped_refptr.h"

namespace webrtc {

class FakeAudioProcessor : public AudioProcessorInterface {
 public:
  FakeAudioProcessor() = default;
  ~FakeAudioProcessor() override = default;

  AudioProcessorInterface::AudioProcessorStatistics GetStats(
      bool /*has_recv_streams*/) override {
    return return_stats ? stats
                        : AudioProcessorInterface::AudioProcessorStatistics();
  }

  AudioProcessorInterface::AudioProcessorStatistics stats;
  bool return_stats = true;
};

class FakeAudioTrack : public MediaStreamTrack<AudioTrackInterface> {
 public:
  static scoped_refptr<FakeAudioTrack> Create(
      const std::string& id,
      MediaStreamTrackInterface::TrackState state,
      scoped_refptr<FakeAudioProcessor> processor = nullptr) {
    auto audio_track = make_ref_counted<FakeAudioTrack>(id, processor);
    audio_track->set_state(state);
    return audio_track;
  }

  explicit FakeAudioTrack(const std::string& id,
                          scoped_refptr<FakeAudioProcessor> processor = nullptr)
      : MediaStreamTrack<AudioTrackInterface>(id),
        processor_(processor ? processor
                             : make_ref_counted<FakeAudioProcessor>()) {}

  std::string kind() const override {
    return MediaStreamTrackInterface::kAudioKind;
  }
  AudioSourceInterface* GetSource() const override { return nullptr; }
  void AddSink(AudioTrackSinkInterface* sink) override {}
  void RemoveSink(AudioTrackSinkInterface* sink) override {}
  bool GetSignalLevel(int* level) override {
    *level = 1;
    return true;
  }
  scoped_refptr<AudioProcessorInterface> GetAudioProcessor() override {
    return processor_;
  }

  void set_processor(scoped_refptr<AudioProcessorInterface> processor) {
    processor_ = processor;
  }

 private:
  scoped_refptr<AudioProcessorInterface> processor_;
};

}  // namespace webrtc

#endif  // PC_TEST_FAKE_AUDIO_TRACK_H_
