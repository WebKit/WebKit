/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * video_processing_defines.h
 * This header file includes the definitions used in the video processor module
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_INCLUDE_VIDEO_PROCESSING_DEFINES_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_INCLUDE_VIDEO_PROCESSING_DEFINES_H_

#include "webrtc/typedefs.h"

namespace webrtc {

// Error codes
#define VPM_OK 0
#define VPM_GENERAL_ERROR -1
#define VPM_MEMORY -2
#define VPM_PARAMETER_ERROR -3
#define VPM_SCALE_ERROR -4
#define VPM_UNINITIALIZED -5
#define VPM_UNIMPLEMENTED -6

enum VideoFrameResampling {
  kNoRescaling,    // Disables rescaling.
  kFastRescaling,  // Point filter.
  kBiLinear,       // Bi-linear interpolation.
  kBox,            // Box inteprolation.
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_INCLUDE_VIDEO_PROCESSING_DEFINES_H_
