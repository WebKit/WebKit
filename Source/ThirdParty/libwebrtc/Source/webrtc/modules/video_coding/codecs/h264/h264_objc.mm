/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"

#if defined(WEBRTC_IOS)
#import <UIKit/UIKit.h>
#endif

namespace webrtc {

bool IsH264CodecSupportedObjC() {
#if defined(WEBRTC_VIDEO_TOOLBOX_SUPPORTED) && \
    defined(WEBRTC_IOS)
  // Supported on iOS8+.
  return [[[UIDevice currentDevice] systemVersion] doubleValue] >= 8.0;
#else
  // TODO(tkchin): Support OS/X once we stop mixing libstdc++ and libc++ on
  // OSX 10.9.
  return false;
#endif
}

}  // namespace webrtc
