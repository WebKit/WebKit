/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_CODEC_H_
#define API_VIDEO_CODECS_VIDEO_CODEC_H_

#include <string>

#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {

// The VideoCodec class represents an old defacto-apis, which we're migrating
// away from slowly.

// Video codec
enum class VideoCodecComplexity {
  kComplexityNormal = 0,
  kComplexityHigh = 1,
  kComplexityHigher = 2,
  kComplexityMax = 3
};

// VP8 specific
struct VideoCodecVP8 {
  bool operator==(const VideoCodecVP8& other) const;
  bool operator!=(const VideoCodecVP8& other) const {
    return !(*this == other);
  }
  VideoCodecComplexity complexity;
  unsigned char numberOfTemporalLayers;
  bool denoisingOn;
  bool automaticResizeOn;
  bool frameDroppingOn;
  int keyFrameInterval;
};

enum class InterLayerPredMode {
  kOn,       // Allow inter-layer prediction for all frames.
             // Frame of low spatial layer can be used for
             // prediction of next spatial layer frame.
  kOff,      // Encoder produces independent spatial layers.
  kOnKeyPic  // Allow inter-layer prediction only for frames
             // within key picture.
};

// VP9 specific.
struct VideoCodecVP9 {
  bool operator==(const VideoCodecVP9& other) const;
  bool operator!=(const VideoCodecVP9& other) const {
    return !(*this == other);
  }
  VideoCodecComplexity complexity;
  unsigned char numberOfTemporalLayers;
  bool denoisingOn;
  bool frameDroppingOn;
  int keyFrameInterval;
  bool adaptiveQpMode;
  bool automaticResizeOn;
  unsigned char numberOfSpatialLayers;
  bool flexibleMode;
  InterLayerPredMode interLayerPred;
};

// H264 specific.
struct VideoCodecH264 {
  bool operator==(const VideoCodecH264& other) const;
  bool operator!=(const VideoCodecH264& other) const {
    return !(*this == other);
  }
  bool frameDroppingOn;
  int keyFrameInterval;
  // These are NULL/0 if not externally negotiated.
  const uint8_t* spsData;
  size_t spsLen;
  const uint8_t* ppsData;
  size_t ppsLen;
  H264::Profile profile;
};

// Translates from name of codec to codec type and vice versa.
const char* CodecTypeToPayloadString(VideoCodecType type);
VideoCodecType PayloadStringToCodecType(const std::string& name);

union VideoCodecUnion {
  VideoCodecVP8 VP8;
  VideoCodecVP9 VP9;
  VideoCodecH264 H264;
};

enum class VideoCodecMode { kRealtimeVideo, kScreensharing };

// Common video codec properties
class VideoCodec {
 public:
  VideoCodec();

  // Public variables. TODO(hta): Make them private with accessors.
  VideoCodecType codecType;
  unsigned char plType;

  // TODO(nisse): Change to int, for consistency.
  uint16_t width;
  uint16_t height;

  unsigned int startBitrate;   // kilobits/sec.
  unsigned int maxBitrate;     // kilobits/sec.
  unsigned int minBitrate;     // kilobits/sec.
  unsigned int targetBitrate;  // kilobits/sec.

  uint32_t maxFramerate;

  // This enables/disables encoding and sending when there aren't multiple
  // simulcast streams,by allocating 0 bitrate if inactive.
  bool active;

  unsigned int qpMax;
  unsigned char numberOfSimulcastStreams;
  SimulcastStream simulcastStream[kMaxSimulcastStreams];
  SpatialLayer spatialLayers[kMaxSpatialLayers];

  VideoCodecMode mode;
  bool expect_encode_from_texture;

  // Timing frames configuration. There is delay of delay_ms between two
  // consequent timing frames, excluding outliers. Frame is always made a
  // timing frame if it's at least outlier_ratio in percent of "ideal" average
  // frame given bitrate and framerate, i.e. if it's bigger than
  // |outlier_ratio / 100.0 * bitrate_bps / fps| in bits. This way, timing
  // frames will not be sent too often usually. Yet large frames will always
  // have timing information for debug purposes because they are more likely to
  // cause extra delays.
  struct TimingFrameTriggerThresholds {
    int64_t delay_ms;
    uint16_t outlier_ratio_percent;
  } timing_frame_thresholds;

  bool operator==(const VideoCodec& other) const = delete;
  bool operator!=(const VideoCodec& other) const = delete;

  // Accessors for codec specific information.
  // There is a const version of each that returns a reference,
  // and a non-const version that returns a pointer, in order
  // to allow modification of the parameters.
  VideoCodecVP8* VP8();
  const VideoCodecVP8& VP8() const;
  VideoCodecVP9* VP9();
  const VideoCodecVP9& VP9() const;
  VideoCodecH264* H264();
  const VideoCodecH264& H264() const;

 private:
  // TODO(hta): Consider replacing the union with a pointer type.
  // This will allow removing the VideoCodec* types from this file.
  VideoCodecUnion codec_specific_;
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_VIDEO_CODEC_H_
