/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_FRAME_MARKING_H_
#define API_VIDEO_VIDEO_FRAME_MARKING_H_

namespace webrtc {

struct FrameMarking {
  bool start_of_frame;
  bool end_of_frame;
  bool independent_frame;
  bool discardable_frame;
  bool base_layer_sync;
  uint8_t temporal_id;
  uint8_t layer_id;
  uint8_t tl0_pic_idx;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_FRAME_MARKING_H_
