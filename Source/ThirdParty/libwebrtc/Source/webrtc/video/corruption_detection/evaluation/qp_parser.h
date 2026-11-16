/*
 *  Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_QP_PARSER_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_QP_PARSER_H_

#include "test/qp_parser_for_test.h"

namespace webrtc {

// TODO(bugs.webrtc.org/496266459): Remove when downstream usage is gone.
using GenericQpParser [[deprecated("Use QpParserForTest instead.")]] =
    QpParserForTest;

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_QP_PARSER_H_
