/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/include/video_frame.h"

#include <string.h>

#include <algorithm>  // swap

#include "rtc_base/bind.h"
#include "rtc_base/checks.h"

namespace webrtc {

// FFmpeg's decoder, used by H264DecoderImpl, requires up to 8 bytes padding due
// to optimized bitstream readers. See avcodec_decode_video2.
const size_t EncodedImage::kBufferPaddingBytesH264 = 8;

size_t EncodedImage::GetBufferPaddingBytes(VideoCodecType codec_type) {
  switch (codec_type) {
    case kVideoCodecVP8:
    case kVideoCodecVP9:
      return 0;
    case kVideoCodecH264:
      return kBufferPaddingBytesH264;
    case kVideoCodecI420:
    case kVideoCodecRED:
    case kVideoCodecULPFEC:
    case kVideoCodecFlexfec:
    case kVideoCodecGeneric:
    case kVideoCodecStereo:
    case kVideoCodecUnknown:
      return 0;
  }
  RTC_NOTREACHED();
  return 0;
}

EncodedImage::EncodedImage() : EncodedImage(nullptr, 0, 0) {}

EncodedImage::EncodedImage(uint8_t* buffer, size_t length, size_t size)
      : _buffer(buffer), _length(length), _size(size) {}

void EncodedImage::SetEncodeTime(int64_t encode_start_ms,
                                 int64_t encode_finish_ms) {
  timing_.encode_start_ms = encode_start_ms;
  timing_.encode_finish_ms = encode_finish_ms;
}
}  // namespace webrtc
