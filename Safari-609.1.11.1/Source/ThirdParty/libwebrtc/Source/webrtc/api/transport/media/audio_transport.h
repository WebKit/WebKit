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

#ifndef API_TRANSPORT_MEDIA_AUDIO_TRANSPORT_H_
#define API_TRANSPORT_MEDIA_AUDIO_TRANSPORT_H_

#include <vector>

#include "api/array_view.h"

namespace webrtc {

// Represents encoded audio frame in any encoding (type of encoding is opaque).
// To avoid copying of encoded data use move semantics when passing by value.
class MediaTransportEncodedAudioFrame final {
 public:
  enum class FrameType {
    // Normal audio frame (equivalent to webrtc::kAudioFrameSpeech).
    kSpeech,

    // DTX frame (equivalent to webrtc::kAudioFrameCN).
    kDiscontinuousTransmission,
    // TODO(nisse): Mis-spelled version, update users, then delete.
    kDiscountinuousTransmission = kDiscontinuousTransmission,
  };

  MediaTransportEncodedAudioFrame(
      // Audio sampling rate, for example 48000.
      int sampling_rate_hz,

      // Starting sample index of the frame, i.e. how many audio samples were
      // before this frame since the beginning of the call or beginning of time
      // in one channel (the starting point should not matter for NetEq). In
      // WebRTC it is used as a timestamp of the frame.
      // TODO(sukhanov): Starting_sample_index is currently adjusted on the
      // receiver side in RTP path. Non-RTP implementations should preserve it.
      // For NetEq initial offset should not matter so we should consider fixing
      // RTP path.
      int starting_sample_index,

      // Number of audio samples in audio frame in 1 channel.
      int samples_per_channel,

      // Sequence number of the frame in the order sent, it is currently
      // required by NetEq, but we can fix NetEq, because starting_sample_index
      // should be enough.
      int sequence_number,

      // If audio frame is a speech or discontinued transmission.
      FrameType frame_type,

      // Opaque payload type. In RTP codepath payload type is stored in RTP
      // header. In other implementations it should be simply passed through the
      // wire -- it's needed for decoder.
      int payload_type,

      // Vector with opaque encoded data.
      std::vector<uint8_t> encoded_data);

  ~MediaTransportEncodedAudioFrame();
  MediaTransportEncodedAudioFrame(const MediaTransportEncodedAudioFrame&);
  MediaTransportEncodedAudioFrame& operator=(
      const MediaTransportEncodedAudioFrame& other);
  MediaTransportEncodedAudioFrame& operator=(
      MediaTransportEncodedAudioFrame&& other);
  MediaTransportEncodedAudioFrame(MediaTransportEncodedAudioFrame&&);

  // Getters.
  int sampling_rate_hz() const { return sampling_rate_hz_; }
  int starting_sample_index() const { return starting_sample_index_; }
  int samples_per_channel() const { return samples_per_channel_; }
  int sequence_number() const { return sequence_number_; }

  int payload_type() const { return payload_type_; }
  FrameType frame_type() const { return frame_type_; }

  rtc::ArrayView<const uint8_t> encoded_data() const { return encoded_data_; }

 private:
  int sampling_rate_hz_;
  int starting_sample_index_;
  int samples_per_channel_;

  // TODO(sukhanov): Refactor NetEq so we don't need sequence number.
  // Having sample_index and samples_per_channel should be enough.
  int sequence_number_;

  FrameType frame_type_;

  int payload_type_;

  std::vector<uint8_t> encoded_data_;
};

// Interface for receiving encoded audio frames from MediaTransportInterface
// implementations.
class MediaTransportAudioSinkInterface {
 public:
  virtual ~MediaTransportAudioSinkInterface() = default;

  // Called when new encoded audio frame is received.
  virtual void OnData(uint64_t channel_id,
                      MediaTransportEncodedAudioFrame frame) = 0;
};

}  // namespace webrtc
#endif  // API_TRANSPORT_MEDIA_AUDIO_TRANSPORT_H_
