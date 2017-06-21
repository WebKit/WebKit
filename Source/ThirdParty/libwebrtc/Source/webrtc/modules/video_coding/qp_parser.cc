/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/qp_parser.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/video_coding/utility/vp8_header_parser.h"
#include "webrtc/modules/video_coding/utility/vp9_uncompressed_header_parser.h"

namespace webrtc {

bool QpParser::GetQp(const VCMEncodedFrame&, int*) {
  return false;
}

}  // namespace webrtc
