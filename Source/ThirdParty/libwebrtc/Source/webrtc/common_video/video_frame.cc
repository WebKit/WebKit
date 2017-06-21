/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/include/video_frame.h"

#include <string.h>

#include <algorithm>  // swap

#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"

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
    case kVideoCodecUnknown:
      return 0;
  }
  RTC_NOTREACHED();
  return 0;
}

}  // namespace webrtc
