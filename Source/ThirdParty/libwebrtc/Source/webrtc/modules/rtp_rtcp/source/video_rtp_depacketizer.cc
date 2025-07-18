/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"

#include <stddef.h>
#include <stdint.h>

#include <cstring>

#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "rtc_base/checks.h"

namespace webrtc {

scoped_refptr<EncodedImageBuffer> VideoRtpDepacketizer::AssembleFrame(
    ArrayView<const ArrayView<const uint8_t>> rtp_payloads) {
  size_t frame_size = 0;
  for (ArrayView<const uint8_t> payload : rtp_payloads) {
    frame_size += payload.size();
  }

  scoped_refptr<EncodedImageBuffer> bitstream =
      EncodedImageBuffer::Create(frame_size);

  uint8_t* write_at = bitstream->data();
  for (ArrayView<const uint8_t> payload : rtp_payloads) {
    memcpy(write_at, payload.data(), payload.size());
    write_at += payload.size();
  }
  RTC_DCHECK_EQ(write_at - bitstream->data(), bitstream->size());
  return bitstream;
}

}  // namespace webrtc
