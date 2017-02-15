/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * WebRTC's wrapper to libyuv.
 */

#ifndef WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_
#define WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_

#include <stdio.h>
#include <vector>

#include "webrtc/base/export.h"
#include "webrtc/common_types.h"  // RawVideoTypes.
#include "webrtc/common_video/rotation.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_frame.h"

namespace webrtc {

// Supported video types.
enum VideoType {
  kUnknown,
  kI420,
  kIYUV,
  kRGB24,
  kABGR,
  kARGB,
  kARGB4444,
  kRGB565,
  kARGB1555,
  kYUY2,
  kYV12,
  kUYVY,
  kMJPG,
  kNV21,
  kNV12,
  kBGRA,
};

// This is the max PSNR value our algorithms can return.
const double kPerfectPSNR = 48.0f;

// Conversion between the RawVideoType and the LibYuv videoType.
// TODO(wu): Consolidate types into one type throughout WebRtc.
VideoType RawVideoTypeToCommonVideoVideoType(RawVideoType type);

// Calculate the required buffer size.
// Input:
//   - type         :The type of the designated video frame.
//   - width        :frame width in pixels.
//   - height       :frame height in pixels.
// Return value:    :The required size in bytes to accommodate the specified
//                   video frame.
size_t CalcBufferSize(VideoType type, int width, int height);

// TODO(mikhal): Add unit test for these two functions and determine location.
// Print VideoFrame to file
// Input:
//    - frame       : Reference to video frame.
//    - file        : pointer to file object. It is assumed that the file is
//                    already open for writing.
// Return value: 0 if OK, < 0 otherwise.
int PrintVideoFrame(const VideoFrame& frame, FILE* file);
int PrintVideoFrame(const VideoFrameBuffer& frame, FILE* file);

// Extract buffer from VideoFrame or VideoFrameBuffer (consecutive
// planes, no stride)
// Input:
//   - frame       : Reference to video frame.
//   - size        : pointer to the size of the allocated buffer. If size is
//                   insufficient, an error will be returned.
//   - buffer      : Pointer to buffer
// Return value: length of buffer if OK, < 0 otherwise.
int ExtractBuffer(const rtc::scoped_refptr<VideoFrameBuffer>& input_frame,
                  size_t size,
                  uint8_t* buffer);
int ExtractBuffer(const VideoFrame& input_frame, size_t size, uint8_t* buffer);
// Convert To I420
// Input:
//   - src_video_type   : Type of input video.
//   - src_frame        : Pointer to a source frame.
//   - crop_x/crop_y    : Starting positions for cropping (0 for no crop).
//   - src_width        : src width in pixels.
//   - src_height       : src height in pixels.
//   - sample_size      : Required only for the parsing of MJPG (set to 0 else).
//   - rotate           : Rotation mode of output image.
// Output:
//   - dst_buffer       : Reference to a destination frame buffer.
// Return value: 0 if OK, < 0 otherwise.

// TODO(nisse): Delete this wrapper, and let users call libyuv directly. Most
// calls pass |src_video_type| == kI420, and should use libyuv::I420Copy. The
// only exception at the time of this writing is
// VideoCaptureImpl::IncomingFrame, which still needs libyuv::ConvertToI420.
WEBRTC_EXPORT int ConvertToI420(VideoType src_video_type,
                  const uint8_t* src_frame,
                  int crop_x,
                  int crop_y,
                  int src_width,
                  int src_height,
                  size_t sample_size,
                  VideoRotation rotation,
                  I420Buffer* dst_buffer);

// Convert From I420
// Input:
//   - src_frame        : Reference to a source frame.
//   - dst_video_type   : Type of output video.
//   - dst_sample_size  : Required only for the parsing of MJPG.
//   - dst_frame        : Pointer to a destination frame.
// Return value: 0 if OK, < 0 otherwise.
// It is assumed that source and destination have equal height.
int ConvertFromI420(const VideoFrame& src_frame,
                    VideoType dst_video_type,
                    int dst_sample_size,
                    uint8_t* dst_frame);

// Compute PSNR for an I420 frame (all planes).
// Returns the PSNR in decibel, to a maximum of kInfinitePSNR.
double I420PSNR(const VideoFrame* ref_frame, const VideoFrame* test_frame);
double I420PSNR(const VideoFrameBuffer& ref_buffer,
                const VideoFrameBuffer& test_buffer);

// Compute SSIM for an I420 frame (all planes).
double I420SSIM(const VideoFrame* ref_frame, const VideoFrame* test_frame);
double I420SSIM(const VideoFrameBuffer& ref_buffer,
                const VideoFrameBuffer& test_buffer);

// Helper function for scaling NV12 to NV12.
void NV12Scale(std::vector<uint8_t>* tmp_buffer,
               const uint8_t* src_y, int src_stride_y,
               const uint8_t* src_uv, int src_stride_uv,
               int src_width, int src_height,
               uint8_t* dst_y, int dst_stride_y,
               uint8_t* dst_uv, int dst_stride_uv,
               int dst_width, int dst_height);

// Helper class for directly converting and scaling NV12 to I420. The Y-plane
// will be scaled directly to the I420 destination, which makes this faster
// than separate NV12->I420 + I420->I420 scaling.
class NV12ToI420Scaler {
 public:
  void NV12ToI420Scale(const uint8_t* src_y, int src_stride_y,
                       const uint8_t* src_uv, int src_stride_uv,
                       int src_width, int src_height,
                       uint8_t* dst_y, int dst_stride_y,
                       uint8_t* dst_u, int dst_stride_u,
                       uint8_t* dst_v, int dst_stride_v,
                       int dst_width, int dst_height);
 private:
  std::vector<uint8_t> tmp_uv_planes_;
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_VIDEO_LIBYUV_INCLUDE_WEBRTC_LIBYUV_H_
