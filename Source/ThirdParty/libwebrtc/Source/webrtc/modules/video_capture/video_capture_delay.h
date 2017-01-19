/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_DELAY_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_DELAY_H_

namespace webrtc
{
namespace videocapturemodule
{

struct DelayValue
{
    int32_t width;
    int32_t height;
    int32_t delay;
};

enum { NoOfDelayValues = 40 };
struct DelayValues
{
    char * deviceName;
    char* productId;
    DelayValue delayValues[NoOfDelayValues];
};

}  // namespace videocapturemodule
}  // namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_DELAY_H_
