/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/qp_parser.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/video_coding/utility/vp8_header_parser.h"

namespace webrtc {

bool QpParser::GetQp(const VCMEncodedFrame& frame, int* qp) {
  switch (frame.CodecSpecific()->codecType) {
    case kVideoCodecVP8:
      // QP range: [0, 127].
      return vp8::GetQp(frame.Buffer(), frame.Length(), qp);
    default:
      return false;
  }
}

}  // namespace webrtc
