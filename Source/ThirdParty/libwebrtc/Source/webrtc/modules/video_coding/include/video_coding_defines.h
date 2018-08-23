/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_DEFINES_H_
#define MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_DEFINES_H_

#include <string>
#include <vector>

#include "api/video/video_frame.h"
// For EncodedImage
#include "common_video/include/video_frame.h"
#include "modules/include/module_common_types.h"

namespace webrtc {

// Error codes
#define VCM_FRAME_NOT_READY 3
#define VCM_MISSING_CALLBACK 1
#define VCM_OK 0
#define VCM_GENERAL_ERROR -1
#define VCM_LEVEL_EXCEEDED -2
#define VCM_MEMORY -3
#define VCM_PARAMETER_ERROR -4
#define VCM_UNKNOWN_PAYLOAD -5
#define VCM_CODEC_ERROR -6
#define VCM_UNINITIALIZED -7
#define VCM_NO_CODEC_REGISTERED -8
#define VCM_JITTER_BUFFER_ERROR -9
#define VCM_OLD_PACKET_ERROR -10
#define VCM_NO_FRAME_DECODED -11
#define VCM_NOT_IMPLEMENTED -20

enum {
  // Timing frames settings. Timing frames are sent every
  // |kDefaultTimingFramesDelayMs|, or if the frame is at least
  // |kDefaultOutliserFrameSizePercent| in size of average frame.
  kDefaultTimingFramesDelayMs = 200,
  kDefaultOutlierFrameSizePercent = 250,
  // Maximum number of frames for what we store encode start timing information.
  kMaxEncodeStartTimeListSize = 50,
};

enum VCMVideoProtection {
  kProtectionNone,
  kProtectionNack,
  kProtectionFEC,
  kProtectionNackFEC,
};

enum VCMTemporalDecimation {
  kBitrateOverUseDecimation,
};

struct VCMFrameCount {
  uint32_t numKeyFrames;
  uint32_t numDeltaFrames;
};

// Callback class used for passing decoded frames which are ready to be
// rendered.
class VCMReceiveCallback {
 public:
  virtual int32_t FrameToRender(VideoFrame& videoFrame,  // NOLINT
                                absl::optional<uint8_t> qp,
                                VideoContentType content_type) = 0;

  virtual int32_t ReceivedDecodedReferenceFrame(const uint64_t pictureId);
  // Called when the current receive codec changes.
  virtual void OnIncomingPayloadType(int payload_type);
  virtual void OnDecoderImplementationName(const char* implementation_name);

 protected:
  virtual ~VCMReceiveCallback() {}
};

// Callback class used for informing the user of the incoming bit rate and frame
// rate.
class VCMReceiveStatisticsCallback {
 public:
  virtual void OnReceiveRatesUpdated(uint32_t bitRate, uint32_t frameRate) = 0;
  virtual void OnCompleteFrame(bool is_keyframe,
                               size_t size_bytes,
                               VideoContentType content_type) = 0;
  virtual void OnDiscardedPacketsUpdated(int discarded_packets) = 0;
  virtual void OnFrameCountsUpdated(const FrameCounts& frame_counts) = 0;
  virtual void OnFrameBufferTimingsUpdated(int decode_ms,
                                           int max_decode_ms,
                                           int current_delay_ms,
                                           int target_delay_ms,
                                           int jitter_buffer_ms,
                                           int min_playout_delay_ms,
                                           int render_delay_ms) = 0;

  virtual void OnTimingFrameInfoUpdated(const TimingFrameInfo& info) = 0;

 protected:
  virtual ~VCMReceiveStatisticsCallback() {}
};

// Callback class used for telling the user about what frame type needed to
// continue decoding.
// Typically a key frame when the stream has been corrupted in some way.
class VCMFrameTypeCallback {
 public:
  virtual int32_t RequestKeyFrame() = 0;

 protected:
  virtual ~VCMFrameTypeCallback() {}
};

// Callback class used for telling the user about which packet sequence numbers
// are currently
// missing and need to be resent.
// TODO(philipel): Deprecate VCMPacketRequestCallback
//                 and use NackSender instead.
class VCMPacketRequestCallback {
 public:
  virtual int32_t ResendPackets(const uint16_t* sequenceNumbers,
                                uint16_t length) = 0;

 protected:
  virtual ~VCMPacketRequestCallback() {}
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_DEFINES_H_
