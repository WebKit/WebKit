/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_ENCODER_INTERFACE_H_
#define API_VIDEO_VIDEO_STREAM_ENCODER_INTERFACE_H_

#include <vector>

#include "api/rtpparameters.h"  // For DegradationPreference.
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_config.h"

namespace webrtc {

// TODO(nisse): Move full declaration to api/.
class VideoBitrateAllocationObserver;

// This interface represents a class responsible for creating and driving the
// encoder(s) for a single video stream. It is also responsible for adaptation
// decisions related to video quality, requesting reduced frame rate or
// resolution from the VideoSource when needed.
// TODO(bugs.webrtc.org/8830): This interface is under development. Changes
// under consideration include:
//
// 1. Taking out responsibility for adaptation decisions, instead only reporting
//    per-frame measurements to the decision maker.
//
// 2. Moving responsibility for simulcast and for software fallback into this
//    class.
class VideoStreamEncoderInterface : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  // Interface for receiving encoded video frames and notifications about
  // configuration changes.
  class EncoderSink : public EncodedImageCallback {
   public:
    virtual void OnEncoderConfigurationChanged(
        std::vector<VideoStream> streams,
        int min_transmit_bitrate_bps) = 0;
  };

  // Sets the source that will provide video frames to the VideoStreamEncoder's
  // OnFrame method. |degradation_preference| control whether or not resolution
  // or frame rate may be reduced. The VideoStreamEncoder registers itself with
  // |source|, and signals adaptation decisions to the source in the form of
  // VideoSinkWants.
  // TODO(nisse): When adaptation logic is extracted from this class,
  // it no longer needs to know the source.
  virtual void SetSource(
      rtc::VideoSourceInterface<VideoFrame>* source,
      const DegradationPreference& degradation_preference) = 0;

  // Sets the |sink| that gets the encoded frames. |rotation_applied| means
  // that the source must support rotation. Only set |rotation_applied| if the
  // remote side does not support the rotation extension.
  virtual void SetSink(EncoderSink* sink, bool rotation_applied) = 0;

  // Sets an initial bitrate, later overriden by OnBitrateUpdated. Mainly
  // affects the resolution of the initial key frame: If incoming frames are
  // larger than reasonable for the start bitrate, and scaling is enabled,
  // VideoStreamEncoder asks the source to scale down and drops a few initial
  // frames.
  // TODO(nisse): This is a poor interface, and mixes bandwidth estimation and
  // codec configuration in an undesired way. For the actual send bandwidth, we
  // should always be somewhat conservative, but we may nevertheless want to let
  // the application configure a more optimistic quality for the initial
  // resolution. Should be replaced by a construction time setting.
  virtual void SetStartBitrate(int start_bitrate_bps) = 0;

  // Request a key frame. Used for signalling from the remote receiver.
  virtual void SendKeyFrame() = 0;

  // Set the currently estimated network properties. A |bitrate_bps|
  // of zero pauses the encoder.
  virtual void OnBitrateUpdated(uint32_t bitrate_bps,
                                uint8_t fraction_lost,
                                int64_t round_trip_time_ms) = 0;

  // Register observer for the bitrate allocation between the temporal
  // and spatial layers.
  virtual void SetBitrateAllocationObserver(
      VideoBitrateAllocationObserver* bitrate_observer) = 0;

  // Creates and configures an encoder with the given |config|. The
  // |max_data_payload_length| is used to support single NAL unit
  // packetization for H.264.
  virtual void ConfigureEncoder(VideoEncoderConfig config,
                                size_t max_data_payload_length) = 0;

  // Permanently stop encoding. After this method has returned, it is
  // guaranteed that no encoded frames will be delivered to the sink.
  virtual void Stop() = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_STREAM_ENCODER_INTERFACE_H_
