/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
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

#include "api/transport/media/video_transport.h"

#include <utility>

namespace webrtc {

MediaTransportEncodedVideoFrame::MediaTransportEncodedVideoFrame() = default;

MediaTransportEncodedVideoFrame::~MediaTransportEncodedVideoFrame() = default;

MediaTransportEncodedVideoFrame::MediaTransportEncodedVideoFrame(
    int64_t frame_id,
    std::vector<int64_t> referenced_frame_ids,
    int payload_type,
    const webrtc::EncodedImage& encoded_image)
    : payload_type_(payload_type),
      encoded_image_(encoded_image),
      frame_id_(frame_id),
      referenced_frame_ids_(std::move(referenced_frame_ids)) {}

MediaTransportEncodedVideoFrame& MediaTransportEncodedVideoFrame::operator=(
    const MediaTransportEncodedVideoFrame&) = default;

MediaTransportEncodedVideoFrame& MediaTransportEncodedVideoFrame::operator=(
    MediaTransportEncodedVideoFrame&&) = default;

MediaTransportEncodedVideoFrame::MediaTransportEncodedVideoFrame(
    const MediaTransportEncodedVideoFrame& o)
    : MediaTransportEncodedVideoFrame() {
  *this = o;
}

MediaTransportEncodedVideoFrame::MediaTransportEncodedVideoFrame(
    MediaTransportEncodedVideoFrame&& o)
    : MediaTransportEncodedVideoFrame() {
  *this = std::move(o);
}

}  // namespace webrtc
