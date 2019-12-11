/* Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This is EXPERIMENTAL interface for media transport.
//
// The goal is to refactor WebRTC code so that audio and video frames
// are sent / received through the media transport interface. This will
// enable different media transport implementations, including QUIC-based
// media transport.

#ifndef API_TRANSPORT_MEDIA_VIDEO_TRANSPORT_H_
#define API_TRANSPORT_MEDIA_VIDEO_TRANSPORT_H_

#include <vector>

#include "api/video/encoded_image.h"

namespace webrtc {

// Represents encoded video frame, along with the codec information.
class MediaTransportEncodedVideoFrame final {
 public:
  MediaTransportEncodedVideoFrame(int64_t frame_id,
                                  std::vector<int64_t> referenced_frame_ids,
                                  int payload_type,
                                  const webrtc::EncodedImage& encoded_image);
  ~MediaTransportEncodedVideoFrame();
  MediaTransportEncodedVideoFrame(const MediaTransportEncodedVideoFrame&);
  MediaTransportEncodedVideoFrame& operator=(
      const MediaTransportEncodedVideoFrame& other);
  MediaTransportEncodedVideoFrame& operator=(
      MediaTransportEncodedVideoFrame&& other);
  MediaTransportEncodedVideoFrame(MediaTransportEncodedVideoFrame&&);

  int payload_type() const { return payload_type_; }
  const webrtc::EncodedImage& encoded_image() const { return encoded_image_; }

  int64_t frame_id() const { return frame_id_; }
  const std::vector<int64_t>& referenced_frame_ids() const {
    return referenced_frame_ids_;
  }

  // Hack to workaround lack of ownership of the EncodedImage buffer. If we
  // don't already own the underlying data, make a copy.
  void Retain() { encoded_image_.Retain(); }

 private:
  MediaTransportEncodedVideoFrame();

  int payload_type_;

  // The buffer is not always owned by the encoded image. On the sender it means
  // that it will need to make a copy using the Retain() method, if it wants to
  // deliver it asynchronously.
  webrtc::EncodedImage encoded_image_;

  // Frame id uniquely identifies a frame in a stream. It needs to be unique in
  // a given time window (i.e. technically unique identifier for the lifetime of
  // the connection is not needed, but you need to guarantee that remote side
  // got rid of the previous frame_id if you plan to reuse it).
  //
  // It is required by a remote jitter buffer, and is the same as
  // EncodedFrame::id::picture_id.
  //
  // This data must be opaque to the media transport, and media transport should
  // itself not make any assumptions about what it is and its uniqueness.
  int64_t frame_id_;

  // A single frame might depend on other frames. This is set of identifiers on
  // which the current frame depends.
  std::vector<int64_t> referenced_frame_ids_;
};

// Interface for receiving encoded video frames from MediaTransportInterface
// implementations.
class MediaTransportVideoSinkInterface {
 public:
  virtual ~MediaTransportVideoSinkInterface() = default;

  // Called when new encoded video frame is received.
  virtual void OnData(uint64_t channel_id,
                      MediaTransportEncodedVideoFrame frame) = 0;
};

// Interface for video sender to be notified of received key frame request.
class MediaTransportKeyFrameRequestCallback {
 public:
  virtual ~MediaTransportKeyFrameRequestCallback() = default;

  // Called when a key frame request is received on the transport.
  virtual void OnKeyFrameRequested(uint64_t channel_id) = 0;
};

}  // namespace webrtc
#endif  // API_TRANSPORT_MEDIA_VIDEO_TRANSPORT_H_
