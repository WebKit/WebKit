/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_API_VIDEO_CAPTURER_H_
#define SDK_OBJC_NATIVE_API_VIDEO_CAPTURER_H_

#import "base/RTCVideoCapturer.h"

#include "api/mediastreaminterface.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> ObjCToNativeVideoCapturer(
    RTCVideoCapturer* objc_video_capturer,
    rtc::Thread* signaling_thread,
    rtc::Thread* worker_thread);

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_API_VIDEO_CAPTURER_H_
