/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_QP_PARSER_H_
#define MODULES_VIDEO_CODING_QP_PARSER_H_

#include "modules/video_coding/encoded_frame.h"

namespace webrtc {

class QpParser {
 public:
  QpParser() {}
  ~QpParser() {}

  // Parses an encoded |frame| and extracts the |qp|.
  // Returns true on success, false otherwise.
  bool GetQp(const VCMEncodedFrame& frame, int* qp);
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_QP_PARSER_H_
