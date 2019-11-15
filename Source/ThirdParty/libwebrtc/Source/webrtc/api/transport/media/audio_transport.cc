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

#include "api/transport/media/audio_transport.h"

#include <utility>

namespace webrtc {

MediaTransportEncodedAudioFrame::~MediaTransportEncodedAudioFrame() {}

MediaTransportEncodedAudioFrame::MediaTransportEncodedAudioFrame(
    int sampling_rate_hz,
    int starting_sample_index,
    int samples_per_channel,
    int sequence_number,
    FrameType frame_type,
    int payload_type,
    std::vector<uint8_t> encoded_data)
    : sampling_rate_hz_(sampling_rate_hz),
      starting_sample_index_(starting_sample_index),
      samples_per_channel_(samples_per_channel),
      sequence_number_(sequence_number),
      frame_type_(frame_type),
      payload_type_(payload_type),
      encoded_data_(std::move(encoded_data)) {}

MediaTransportEncodedAudioFrame& MediaTransportEncodedAudioFrame::operator=(
    const MediaTransportEncodedAudioFrame&) = default;

MediaTransportEncodedAudioFrame& MediaTransportEncodedAudioFrame::operator=(
    MediaTransportEncodedAudioFrame&&) = default;

MediaTransportEncodedAudioFrame::MediaTransportEncodedAudioFrame(
    const MediaTransportEncodedAudioFrame&) = default;

MediaTransportEncodedAudioFrame::MediaTransportEncodedAudioFrame(
    MediaTransportEncodedAudioFrame&&) = default;

}  // namespace webrtc
