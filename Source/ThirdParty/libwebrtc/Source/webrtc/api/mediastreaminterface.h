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

#ifndef WEBRTC_API_MEDIASTREAMINTERFACE_H_
#define WEBRTC_API_MEDIASTREAMINTERFACE_H_

#include <string>
#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/optional.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/media/base/videosourceinterface.h"
#include "webrtc/video_frame.h"

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

// Base class for sources. A MediaStreamTrack have an underlying source that
// provide media. A source can be shared with multiple tracks.
class MediaSourceInterface : public rtc::RefCountInterface,
                             public NotifierInterface {
 public:
  enum SourceState {
    kInitializing,
    kLive,
    kEnded,
    kMuted
  };

  virtual SourceState state() const = 0;

  virtual bool remote() const = 0;

 protected:
  virtual ~MediaSourceInterface() {}
};

// Information about a track.
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
  virtual std::string id() const = 0;
  virtual bool enabled() const = 0;
  virtual TrackState state() const = 0;
  virtual bool set_enabled(bool enable) = 0;

 protected:
  virtual ~MediaStreamTrackInterface() {}
};

// VideoTrackSourceInterface is a reference counted source used for VideoTracks.
// The same source can be used in multiple VideoTracks.
class VideoTrackSourceInterface
    : public MediaSourceInterface,
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
  // explicitly setting suitable parameters for screencasts and dont' need this
  // implicit behavior.
  virtual bool is_screencast() const = 0;

  // Indicates that the encoder should denoise video before encoding it.
  // If it is not set, the default configuration is used which is different
  // depending on video codec.
  // TODO(perkj): Remove this once denoising is done by the source, and not by
  // the encoder.
  virtual rtc::Optional<bool> needs_denoising() const = 0;

  // Returns false if no stats are available, e.g, for a remote
  // source, or a source which has not seen its first frame yet.
  // Should avoid blocking.
  virtual bool GetStats(Stats* stats) = 0;

 protected:
  virtual ~VideoTrackSourceInterface() {}
};

class VideoTrackInterface
    : public MediaStreamTrackInterface,
      public rtc::VideoSourceInterface<VideoFrame> {
 public:
  // Register a video sink for this track.
  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override{};
  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override{};

  virtual VideoTrackSourceInterface* GetSource() const = 0;

 protected:
  virtual ~VideoTrackInterface() {}
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
// The same source can be used in multiple AudioTracks.
class AudioSourceInterface : public MediaSourceInterface {
 public:
  class AudioObserver {
   public:
    virtual void OnSetVolume(double volume) = 0;

   protected:
    virtual ~AudioObserver() {}
  };

  // TODO(xians): Makes all the interface pure virtual after Chrome has their
  // implementations.
  // Sets the volume to the source. |volume| is in  the range of [0, 10].
  // TODO(tommi): This method should be on the track and ideally volume should
  // be applied in the track in a way that does not affect clones of the track.
  virtual void SetVolume(double volume) {}

  // Registers/unregisters observer to the audio source.
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
  struct AudioProcessorStats {
    AudioProcessorStats() : typing_noise_detected(false),
                            echo_return_loss(0),
                            echo_return_loss_enhancement(0),
                            echo_delay_median_ms(0),
                            echo_delay_std_ms(0),
                            aec_quality_min(0.0),
                            residual_echo_likelihood(0.0f),
                            aec_divergent_filter_fraction(0.0) {}
    ~AudioProcessorStats() {}

    bool typing_noise_detected;
    int echo_return_loss;
    int echo_return_loss_enhancement;
    int echo_delay_median_ms;
    int echo_delay_std_ms;
    float aec_quality_min;
    float residual_echo_likelihood;
    float aec_divergent_filter_fraction;
  };

  // Get audio processor statistics.
  virtual void GetStats(AudioProcessorStats* stats) = 0;

 protected:
  virtual ~AudioProcessorInterface() {}
};

class AudioTrackInterface : public MediaStreamTrackInterface {
 public:
  // TODO(xians): Figure out if the following interface should be const or not.
  virtual AudioSourceInterface* GetSource() const =  0;

  // Add/Remove a sink that will receive the audio data from the track.
  virtual void AddSink(AudioTrackSinkInterface* sink) = 0;
  virtual void RemoveSink(AudioTrackSinkInterface* sink) = 0;

  // Get the signal level from the audio track.
  // Return true on success, otherwise false.
  // TODO(xians): Change the interface to int GetSignalLevel() and pure virtual
  // after Chrome has the correct implementation of the interface.
  virtual bool GetSignalLevel(int* level) { return false; }

  // Get the audio processor used by the audio track. Return NULL if the track
  // does not have any processor.
  // TODO(xians): Make the interface pure virtual.
  virtual rtc::scoped_refptr<AudioProcessorInterface>
      GetAudioProcessor() { return NULL; }

 protected:
  virtual ~AudioTrackInterface() {}
};

typedef std::vector<rtc::scoped_refptr<AudioTrackInterface> >
    AudioTrackVector;
typedef std::vector<rtc::scoped_refptr<VideoTrackInterface> >
    VideoTrackVector;

class MediaStreamInterface : public rtc::RefCountInterface,
                             public NotifierInterface {
 public:
  virtual std::string label() const = 0;

  virtual AudioTrackVector GetAudioTracks() = 0;
  virtual VideoTrackVector GetVideoTracks() = 0;
  virtual rtc::scoped_refptr<AudioTrackInterface>
      FindAudioTrack(const std::string& track_id) = 0;
  virtual rtc::scoped_refptr<VideoTrackInterface>
      FindVideoTrack(const std::string& track_id) = 0;

  virtual bool AddTrack(AudioTrackInterface* track) = 0;
  virtual bool AddTrack(VideoTrackInterface* track) = 0;
  virtual bool RemoveTrack(AudioTrackInterface* track) = 0;
  virtual bool RemoveTrack(VideoTrackInterface* track) = 0;

 protected:
  virtual ~MediaStreamInterface() {}
};

}  // namespace webrtc

#endif  // WEBRTC_API_MEDIASTREAMINTERFACE_H_
