/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_VIDEO_CODEC_SETTINGS_H_
#define TEST_VIDEO_CODEC_SETTINGS_H_

#include "api/video_codecs/video_encoder.h"

namespace webrtc {
namespace test {

const uint16_t kTestWidth = 352;
const uint16_t kTestHeight = 288;
const uint32_t kTestFrameRate = 30;
const unsigned int kTestMinBitrateKbps = 30;
const unsigned int kTestStartBitrateKbps = 300;
const uint8_t kTestPayloadType = 100;
const int64_t kTestTimingFramesDelayMs = 200;
const uint16_t kTestOutlierFrameSizePercent = 250;

static void CodecSettings(VideoCodecType codec_type, VideoCodec* settings) {
  memset(settings, 0, sizeof(VideoCodec));
  switch (codec_type) {
    case kVideoCodecVP8:
      strncpy(settings->plName, "VP8", 4);
      settings->codecType = kVideoCodecVP8;
      // 96 to 127 dynamic payload types for video codecs.
      settings->plType = kTestPayloadType;
      settings->startBitrate = kTestStartBitrateKbps;
      settings->minBitrate = kTestMinBitrateKbps;
      settings->maxBitrate = 0;
      settings->maxFramerate = kTestFrameRate;
      settings->width = kTestWidth;
      settings->height = kTestHeight;
      settings->numberOfSimulcastStreams = 0;
      settings->qpMax = 56;
      settings->timing_frame_thresholds = {
          kTestTimingFramesDelayMs, kTestOutlierFrameSizePercent,
      };
      *(settings->VP8()) = VideoEncoder::GetDefaultVp8Settings();
      return;
    case kVideoCodecVP9:
      strncpy(settings->plName, "VP9", 4);
      settings->codecType = kVideoCodecVP9;
      // 96 to 127 dynamic payload types for video codecs.
      settings->plType = kTestPayloadType;
      settings->startBitrate = 100;
      settings->minBitrate = kTestMinBitrateKbps;
      settings->maxBitrate = 0;
      settings->maxFramerate = kTestFrameRate;
      settings->width = kTestWidth;
      settings->height = kTestHeight;
      settings->numberOfSimulcastStreams = 0;
      settings->qpMax = 56;
      settings->timing_frame_thresholds = {
          kTestTimingFramesDelayMs, kTestOutlierFrameSizePercent,
      };
      *(settings->VP9()) = VideoEncoder::GetDefaultVp9Settings();
      return;
    case kVideoCodecH264:
      strncpy(settings->plName, "H264", 5);
      settings->codecType = kVideoCodecH264;
      // 96 to 127 dynamic payload types for video codecs.
      settings->plType = kTestPayloadType;
      settings->startBitrate = kTestStartBitrateKbps;
      settings->minBitrate = kTestMinBitrateKbps;
      settings->maxBitrate = 0;
      settings->maxFramerate = kTestFrameRate;
      settings->width = kTestWidth;
      settings->height = kTestHeight;
      settings->numberOfSimulcastStreams = 0;
      settings->qpMax = 56;
      settings->timing_frame_thresholds = {
          kTestTimingFramesDelayMs, kTestOutlierFrameSizePercent,
      };
      *(settings->H264()) = VideoEncoder::GetDefaultH264Settings();
      return;
    case kVideoCodecI420:
      strncpy(settings->plName, "I420", 5);
      settings->codecType = kVideoCodecI420;
      // 96 to 127 dynamic payload types for video codecs.
      settings->plType = kTestPayloadType;
      // Bitrate needed for this size and framerate.
      settings->startBitrate =
          3 * kTestWidth * kTestHeight * 8 * kTestFrameRate / 1000 / 2;
      settings->maxBitrate = settings->startBitrate;
      settings->maxFramerate = kTestFrameRate;
      settings->width = kTestWidth;
      settings->height = kTestHeight;
      settings->minBitrate = kTestMinBitrateKbps;
      settings->numberOfSimulcastStreams = 0;
      return;
    case kVideoCodecRED:
    case kVideoCodecULPFEC:
    case kVideoCodecFlexfec:
    case kVideoCodecStereo:
    case kVideoCodecGeneric:
    case kVideoCodecUnknown:
      RTC_NOTREACHED();
      return;
  }
}
}  // namespace test
}  // namespace webrtc

#endif  // TEST_VIDEO_CODEC_SETTINGS_H_
