/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_
#define CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_frame.h"
#include "call/adaptation/encoder_settings.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/video_source_restrictions.h"

namespace webrtc {

// The listener is responsible for carrying out the reconfiguration of the video
// source such that the VideoSourceRestrictions are fulfilled.
class ResourceAdaptationProcessorListener {
 public:
  virtual ~ResourceAdaptationProcessorListener();

  virtual void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions) = 0;
};

// Responsible for reconfiguring encoded streams based on resource consumption,
// such as scaling down resolution or frame rate when CPU is overused. This
// interface is meant to be injectable into VideoStreamEncoder.
class ResourceAdaptationProcessorInterface {
 public:
  virtual ~ResourceAdaptationProcessorInterface();

  virtual void StartResourceAdaptation(
      ResourceAdaptationProcessorListener* adaptation_listener) = 0;
  virtual void StopResourceAdaptation() = 0;

  // The resource must out-live the module.
  virtual void AddResource(Resource* resource) = 0;

  // The following methods are callable whether or not adaption is started.

  // Informs the module whether we have input video. By default, the module must
  // assume the value is false.
  virtual void SetHasInputVideo(bool has_input_video) = 0;
  virtual void SetDegradationPreference(
      DegradationPreference degradation_preference) = 0;
  virtual void SetEncoderSettings(EncoderSettings encoder_settings) = 0;
  // TODO(bugs.webrtc.org/11222): This function shouldn't be needed, start
  // bitrates should be apart of the constructor ideally. See the comment on
  // VideoStreamEncoderInterface::SetStartBitrate.
  virtual void SetStartBitrate(DataRate start_bitrate) = 0;
  virtual void SetTargetBitrate(DataRate target_bitrate) = 0;
  // The encoder rates are the target encoder bitrate distributed across spatial
  // and temporal layers. This may be different than target bitrate depending on
  // encoder configuration, e.g. if we can encode at desired quality in less
  // than the allowed target bitrate or if the encoder has not been initialized
  // yet.
  virtual void SetEncoderRates(
      const VideoEncoder::RateControlParameters& encoder_rates) = 0;

  // The following methods correspond to the pipeline that a frame goes through.
  // Note that if the encoder is parallelized, multiple frames may be processed
  // in parallel and methods may be invoked in unexpected orders.
  //
  // The implementation must not retain VideoFrames. Doing so may keep video
  // frame buffers alive - this may even stall encoding.
  // TODO(hbos): Can we replace VideoFrame with a different struct, maybe width
  // and height is enough, and some sort of way to identify it at each step?

  // 1. A frame is delivered to the encoder, e.g. from the camera. Next up: it
  // may get dropped or it may get encoded, see OnFrameDroppedDueToSize() and
  // OnEncodeStarted().
  virtual void OnFrame(const VideoFrame& frame) = 0;
  // 2.i) An input frame was dropped because its resolution is too big (e.g. for
  // the target bitrate). This frame will not continue through the rest of the
  // pipeline. The module should adapt down in resolution to avoid subsequent
  // frames getting dropped for the same reason.
  // TODO(hbos): If we take frame rate into account perhaps it would be valid to
  // adapt down in frame rate as well.
  virtual void OnFrameDroppedDueToSize() = 0;
  // 2.ii) If the frame will not be dropped due to size then signal that it may
  // get encoded. However the frame is not guaranteed to be encoded right away
  // or ever (for example if encoding is paused).
  // TODO(eshr): Try replace OnMaybeEncodeFrame and merge behaviour into
  // EncodeStarted.
  // TODO(eshr): Try to merge OnFrame, OnFrameDroppedDueToSize, and
  // OnMaybeEncode frame into one method.
  virtual void OnMaybeEncodeFrame() = 0;
  // 2.iii) An input frame is about to be encoded. It may have been cropped and
  // have different dimensions than what was observed at OnFrame(). Next
  // up: encoding completes or fails, see OnEncodeCompleted(). There is
  // currently no signal for encode failure.
  virtual void OnEncodeStarted(const VideoFrame& cropped_frame,
                               int64_t time_when_first_seen_us) = 0;
  // 3.i) The frame has successfully completed encoding. Next up: The encoded
  // frame is dropped or packetized and sent over the network. There is
  // currently no signal what happens beyond this point.
  virtual void OnEncodeCompleted(const EncodedImage& encoded_image,
                                 int64_t time_sent_in_us,
                                 absl::optional<int> encode_duration_us) = 0;
  // A frame was dropped at any point in the pipeline. This may come from
  // the encoder, or elsewhere, like a frame dropper or frame size check.
  virtual void OnFrameDropped(EncodedImageCallback::DropReason reason) = 0;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_INTERFACE_H_
