/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/utils.h"

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame_buffer.h"

namespace webrtc {
namespace {

constexpr char kPayloadNameVp8[] = "VP8";
constexpr char kPayloadNameVp9[] = "VP9";
constexpr char kPayloadNameAv1[] = "AV1";
constexpr char kPayloadNameH264[] = "H264";
constexpr char kPayloadNameH265[] = "H265";
constexpr char kPayloadNameGeneric[] = "Generic";

}  // namespace

// Returns the `VideoCodecType` corresponding to the given `codec_name`.
// The `codec_name` does not need to "exactly" match the namings
// `kPayloadNameXXX`. For example, "VP8", "vp8" and "libvxp_vp8" are all
// valid, and will return the `kVideoCodecVP8`.
// I.e. it does the best to match a codec name to a `VideoCodecType`.
VideoCodecType GetVideoCodecType(absl::string_view codec_name) {
  if (absl::StrContainsIgnoreCase(codec_name, kPayloadNameVp8))
    return kVideoCodecVP8;
  if (absl::StrContainsIgnoreCase(codec_name, kPayloadNameVp9))
    return kVideoCodecVP9;
  if (absl::StrContainsIgnoreCase(codec_name, kPayloadNameAv1))
    return kVideoCodecAV1;
  if (absl::StrContainsIgnoreCase(codec_name, kPayloadNameH264))
    return kVideoCodecH264;
  if (absl::StrContainsIgnoreCase(codec_name, kPayloadNameH265))
    return kVideoCodecH265;
  if (absl::StrContainsIgnoreCase(codec_name, kPayloadNameGeneric))
    return kVideoCodecGeneric;
  RTC_FATAL() << "Codec name " << codec_name << " is not supported.";
}

// Creates a new buffer and copies the pixel data. While the copying is done,
// the type of the buffer is changed from `I420BufferInterface` to `I420Buffer`.
// Observe also that the padding bytes are removed.
scoped_refptr<I420Buffer> GetAsI420Buffer(
    const scoped_refptr<I420BufferInterface> i420_buffer_interface) {
  // Note: `I420Buffer::Copy` removes padding bytes.
  // I.e. if the input is to the left the output will be as to the right.
  // +------+--+      +------+
  // |      |  |      |      |
  // |  Y   |P |  --> |  Y   |
  // |      |  |      |      |
  // +------+--+      +------+
  scoped_refptr<I420Buffer> frame_as_i420_buffer =
      I420Buffer::Copy(*i420_buffer_interface);
  RTC_DCHECK_EQ(frame_as_i420_buffer->StrideY(), frame_as_i420_buffer->width());
  return frame_as_i420_buffer;
}

}  // namespace webrtc
