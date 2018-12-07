/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
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

#include "api/media_transport_interface.h"

#include <cstdint>
#include <utility>

namespace webrtc {

MediaTransportSettings::MediaTransportSettings() = default;
MediaTransportSettings::MediaTransportSettings(const MediaTransportSettings&) =
    default;
MediaTransportSettings& MediaTransportSettings::operator=(
    const MediaTransportSettings&) = default;
MediaTransportSettings::~MediaTransportSettings() = default;

MediaTransportEncodedAudioFrame::~MediaTransportEncodedAudioFrame() {}

MediaTransportEncodedAudioFrame::MediaTransportEncodedAudioFrame(
    int sampling_rate_hz,
    int starting_sample_index,
    int samples_per_channel,
    int sequence_number,
    FrameType frame_type,
    uint8_t payload_type,
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

MediaTransportEncodedVideoFrame::~MediaTransportEncodedVideoFrame() {}

MediaTransportEncodedVideoFrame::MediaTransportEncodedVideoFrame(
    int64_t frame_id,
    std::vector<int64_t> referenced_frame_ids,
    VideoCodecType codec_type,
    const webrtc::EncodedImage& encoded_image)
    : codec_type_(codec_type),
      encoded_image_(encoded_image),
      frame_id_(frame_id),
      referenced_frame_ids_(std::move(referenced_frame_ids)) {}

MediaTransportEncodedVideoFrame& MediaTransportEncodedVideoFrame::operator=(
    const MediaTransportEncodedVideoFrame&) = default;

MediaTransportEncodedVideoFrame& MediaTransportEncodedVideoFrame::operator=(
    MediaTransportEncodedVideoFrame&&) = default;

MediaTransportEncodedVideoFrame::MediaTransportEncodedVideoFrame(
    const MediaTransportEncodedVideoFrame&) = default;

MediaTransportEncodedVideoFrame::MediaTransportEncodedVideoFrame(
    MediaTransportEncodedVideoFrame&&) = default;

SendDataParams::SendDataParams() = default;

RTCErrorOr<std::unique_ptr<MediaTransportInterface>>
MediaTransportFactory::CreateMediaTransport(
    rtc::PacketTransportInternal* packet_transport,
    rtc::Thread* network_thread,
    bool is_caller) {
  MediaTransportSettings settings;
  settings.is_caller = is_caller;
  return CreateMediaTransport(packet_transport, network_thread, settings);
}

RTCErrorOr<std::unique_ptr<MediaTransportInterface>>
MediaTransportFactory::CreateMediaTransport(
    rtc::PacketTransportInternal* packet_transport,
    rtc::Thread* network_thread,
    const MediaTransportSettings& settings) {
  return std::unique_ptr<MediaTransportInterface>(nullptr);
}

absl::optional<TargetTransferRate>
MediaTransportInterface::GetLatestTargetTransferRate() {
  return absl::nullopt;
}

void MediaTransportInterface::SetNetworkChangeCallback(
    MediaTransportNetworkChangeCallback* callback) {}

void MediaTransportInterface::RemoveTargetTransferRateObserver(
    webrtc::TargetTransferRateObserver* observer) {}

void MediaTransportInterface::AddTargetTransferRateObserver(
    webrtc::TargetTransferRateObserver* observer) {}

size_t MediaTransportInterface::GetAudioPacketOverhead() const {
  return 0;
}

}  // namespace webrtc
