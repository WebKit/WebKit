/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains interfaces for MediaStream, MediaTrack and MediaSource.
// These interfaces are used for implementing MediaStream and MediaTrack as
// defined in http://dev.w3.org/2011/webrtc/editor/webrtc.html#stream-api. These
// interfaces must be used only with PeerConnection. PeerConnectionManager
// interface provides the factory methods to create MediaStream and MediaTracks.

#ifndef API_MEDIASTREAMINTERFACE_H_
#define API_MEDIASTREAMINTERFACE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/video_frame.h"
// TODO(zhihuang): Remove unrelated headers once downstream applications stop
// relying on them; they were previously transitively included by
// mediachannel.h, which is no longer a dependency of this file.
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "rtc_base/ratetracker.h"
#include "rtc_base/refcount.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"
#include "rtc_base/timeutils.h"

namespace webrtc {

// Generic observer interface.
class ObserverInterface {
 public:
  virtual void OnChanged() = 0;

 protected:
  virtual ~ObserverInterface() {}
};

class NotifierInterface {
 public:
  virtual void RegisterObserver(ObserverInterface* observer) = 0;
  virtual void UnregisterObserver(ObserverInterface* observer) = 0;

  virtual ~NotifierInterface() {}
};

// Base class for sources. A MediaStreamTrack has an underlying source that
// provides media. A source can be shared by multiple tracks.
class MediaSourceInterface : public rtc::RefCountInterface,
                             public NotifierInterface {
 public:
  enum SourceState { kInitializing, kLive, kEnded, kMuted };

  virtual SourceState state() const = 0;

  virtual bool remote() const = 0;

 protected:
  ~MediaSourceInterface() override = default;
};

// C++ version of MediaStreamTrack.
// See: https://www.w3.org/TR/mediacapture-streams/#mediastreamtrack
class MediaStreamTrackInterface : public rtc::RefCountInterface,
                                  public NotifierInterface {
 public:
  enum TrackState {
    kLive,
    kEnded,
  };

  static const char kAudioKind[];
  static const char kVideoKind[];

  // The kind() method must return kAudioKind only if the object is a
  // subclass of AudioTrackInterface, and kVideoKind only if the
  // object is a subclass of VideoTrackInterface. It is typically used
  // to protect a static_cast<> to the corresponding subclass.
  virtual std::string kind() const = 0;

  // Track identifier.
  virtual std::string id() const = 0;

  // A disabled track will produce silence (if audio) or black frames (if
  // video). Can be disabled and re-enabled.
  virtual bool enabled() const = 0;
  virtual bool set_enabled(bool enable) = 0;

  // Live or ended. A track will never be live again after becoming ended.
  virtual TrackState state() const = 0;

 protected:
  ~MediaStreamTrackInterface() override = default;
};

// VideoTrackSourceInterface is a reference counted source used for
// VideoTracks. The same source can be used by multiple VideoTracks.
// VideoTrackSourceInterface is designed to be invoked on the signaling thread
// except for rtc::VideoSourceInterface<VideoFrame> methods that will be invoked
// on the worker thread via a VideoTrack. A custom implementation of a source
// can inherit AdaptedVideoTrackSource instead of directly implementing this
// interface.
class VideoTrackSourceInterface : public MediaSourceInterface,
                                  public rtc::VideoSourceInterface<VideoFrame> {
 public:
  struct Stats {
    // Original size of captured frame, before video adaptation.
    int input_width;
    int input_height;
  };

  // Indicates that parameters suitable for screencasts should be automatically
  // applied to RtpSenders.
  // TODO(perkj): Remove these once all known applications have moved to
  // explicitly setting suitable parameters for screencasts and don't need this
  // implicit behavior.
  virtual bool is_screencast() const = 0;

  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  // TODO(perkj): Remove this once denoising is done by the source, and not by
  // the encoder.
  virtual absl::optional<bool> needs_denoising() const = 0;

  // Returns false if no stats are available, e.g, for a remote source, or a
  // source which has not seen its first frame yet.
  //
  // Implementation should avoid blocking.
  virtual bool GetStats(Stats* stats) = 0;

 protected:
  ~VideoTrackSourceInterface() override = default;
};

// VideoTrackInterface is designed to be invoked on the signaling thread except
// for rtc::VideoSourceInterface<VideoFrame> methods that must be invoked
// on the worker thread.
// PeerConnectionFactory::CreateVideoTrack can be used for creating a VideoTrack
// that ensures thread safety and that all methods are called on the right
// thread.
class VideoTrackInterface : public MediaStreamTrackInterface,
                            public rtc::VideoSourceInterface<VideoFrame> {
 public:
  // Video track content hint, used to override the source is_screencast
  // property.
  // See https://crbug.com/653531 and https://w3c.github.io/mst-content-hint.
  enum class ContentHint { kNone, kFluid, kDetailed, kText };

  // Register a video sink for this track. Used to connect the track to the
  // underlying video engine.
  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {}
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override {}

  virtual VideoTrackSourceInterface* GetSource() const = 0;

  virtual ContentHint content_hint() const;
  virtual void set_content_hint(ContentHint hint) {}

 protected:
  ~VideoTrackInterface() override = default;
};

// Interface for receiving audio data from a AudioTrack.
class AudioTrackSinkInterface {
 public:
  virtual void OnData(const void* audio_data,
                      int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames) = 0;

 protected:
  virtual ~AudioTrackSinkInterface() {}
};

// AudioSourceInterface is a reference counted source used for AudioTracks.
// The same source can be used by multiple AudioTracks.
class AudioSourceInterface : public MediaSourceInterface {
 public:
  class AudioObserver {
   public:
    virtual void OnSetVolume(double volume) = 0;

   protected:
    virtual ~AudioObserver() {}
  };

  // TODO(deadbeef): Makes all the interfaces pure virtual after they're
  // implemented in chromium.

  // Sets the volume of the source. |volume| is in  the range of [0, 10].
  // TODO(tommi): This method should be on the track and ideally volume should
  // be applied in the track in a way that does not affect clones of the track.
  virtual void SetVolume(double volume) {}

  // Registers/unregisters observers to the audio source.
  virtual void RegisterAudioObserver(AudioObserver* observer) {}
  virtual void UnregisterAudioObserver(AudioObserver* observer) {}

  // TODO(tommi): Make pure virtual.
  virtual void AddSink(AudioTrackSinkInterface* sink) {}
  virtual void RemoveSink(AudioTrackSinkInterface* sink) {}
};

// Interface of the audio processor used by the audio track to collect
// statistics.
class AudioProcessorInterface : public rtc::RefCountInterface {
 public:
  // Deprecated, use AudioProcessorStatistics instead.
  // TODO(ivoc): Remove this when all implementations have switched to the new
  //             GetStats function. See b/67926135.
  struct AudioProcessorStats {
    AudioProcessorStats()
        : typing_noise_detected(false),
          echo_return_loss(0),
          echo_return_loss_enhancement(0),
          echo_delay_median_ms(0),
          echo_delay_std_ms(0),
          residual_echo_likelihood(0.0f),
          residual_echo_likelihood_recent_max(0.0f),
          aec_divergent_filter_fraction(0.0) {}
    ~AudioProcessorStats() {}

    bool typing_noise_detected;
    int echo_return_loss;
    int echo_return_loss_enhancement;
    int echo_delay_median_ms;
    int echo_delay_std_ms;
    float residual_echo_likelihood;
    float residual_echo_likelihood_recent_max;
    float aec_divergent_filter_fraction;
  };
  // This struct maintains the optionality of the stats, and will replace the
  // regular stats struct when all users have been updated.
  struct AudioProcessorStatistics {
    bool typing_noise_detected = false;
    AudioProcessingStats apm_statistics;
  };

  // Get audio processor statistics.
  virtual void GetStats(AudioProcessorStats* stats);

  // Get audio processor statistics. The |has_remote_tracks| argument should be
  // set if there are active remote tracks (this would usually be true during
  // a call). If there are no remote tracks some of the stats will not be set by
  // the AudioProcessor, because they only make sense if there is at least one
  // remote track.
  // TODO(ivoc): Make pure virtual when all implementions are updated.
  virtual AudioProcessorStatistics GetStats(bool has_remote_tracks);

 protected:
  ~AudioProcessorInterface() override = default;
};

class AudioTrackInterface : public MediaStreamTrackInterface {
 public:
  // TODO(deadbeef): Figure out if the following interface should be const or
  // not.
  virtual AudioSourceInterface* GetSource() const = 0;

  // Add/Remove a sink that will receive the audio data from the track.
  virtual void AddSink(AudioTrackSinkInterface* sink) = 0;
  virtual void RemoveSink(AudioTrackSinkInterface* sink) = 0;

  // Get the signal level from the audio track.
  // Return true on success, otherwise false.
  // TODO(deadbeef): Change the interface to int GetSignalLevel() and pure
  // virtual after it's implemented in chromium.
  virtual bool GetSignalLevel(int* level);

  // Get the audio processor used by the audio track. Return null if the track
  // does not have any processor.
  // TODO(deadbeef): Make the interface pure virtual.
  virtual rtc::scoped_refptr<AudioProcessorInterface> GetAudioProcessor();

 protected:
  ~AudioTrackInterface() override = default;
};

typedef std::vector<rtc::scoped_refptr<AudioTrackInterface> > AudioTrackVector;
typedef std::vector<rtc::scoped_refptr<VideoTrackInterface> > VideoTrackVector;

// C++ version of https://www.w3.org/TR/mediacapture-streams/#mediastream.
//
// A major difference is that remote audio/video tracks (received by a
// PeerConnection/RtpReceiver) are not synchronized simply by adding them to
// the same stream; a session description with the correct "a=msid" attributes
// must be pushed down.
//
// Thus, this interface acts as simply a container for tracks.
class MediaStreamInterface : public rtc::RefCountInterface,
                             public NotifierInterface {
 public:
  virtual std::string id() const = 0;

  virtual AudioTrackVector GetAudioTracks() = 0;
  virtual VideoTrackVector GetVideoTracks() = 0;
  virtual rtc::scoped_refptr<AudioTrackInterface> FindAudioTrack(
      const std::string& track_id) = 0;
  virtual rtc::scoped_refptr<VideoTrackInterface> FindVideoTrack(
      const std::string& track_id) = 0;

  virtual bool AddTrack(AudioTrackInterface* track) = 0;
  virtual bool AddTrack(VideoTrackInterface* track) = 0;
  virtual bool RemoveTrack(AudioTrackInterface* track) = 0;
  virtual bool RemoveTrack(VideoTrackInterface* track) = 0;

 protected:
  ~MediaStreamInterface() override = default;
};

}  // namespace webrtc

#endif  // API_MEDIASTREAMINTERFACE_H_
