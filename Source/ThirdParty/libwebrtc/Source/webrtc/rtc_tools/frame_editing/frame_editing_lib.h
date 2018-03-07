/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_FRAME_EDITING_FRAME_EDITING_LIB_H_
#define RTC_TOOLS_FRAME_EDITING_FRAME_EDITING_LIB_H_

#include <string>

namespace webrtc {

// Frame numbering starts at 1. The set of frames to be processed includes the
// frame with the number: first_frame_to_process and last_frame_to_process.
// Interval specifies with what interval frames should be cut or kept.
// Example 1:
// If one clip has 10 frames (1 to 10), and you specify
// first_frame_to_process = 4, last_frame_to_process = 7 and interval = -1,
// then you will get a clip that contains frame 1, 2, 3, 8, 9 and 10.
// Example 2:
// I you specify first_frame_to_process = 1, last_frame_to_process = 10 and
// interval = -4, then you will get a clip that contains frame 1, 5, 9.
// Example 3:
// If you specify first_frame_to_process = 4, last_frame_to_process = 7 and
// interval = 2, then you will get a clip that contains frame
// 1, 2, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9 and 10.
// No interpolation is done when up-sampling.

int EditFrames(const std::string& in_path, int width, int height,
                int first_frame_to_process, int interval,
                int last_frame_to_process, const std::string& out_path);
}  // namespace webrtc

#endif  // RTC_TOOLS_FRAME_EDITING_FRAME_EDITING_LIB_H_
