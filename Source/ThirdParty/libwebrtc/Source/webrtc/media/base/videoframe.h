/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(nisse): Deprecated, replace cricket::VideoFrame with
// webrtc::VideoFrame everywhere, then delete this file. See
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5682.

#ifndef WEBRTC_MEDIA_BASE_VIDEOFRAME_H_
#define WEBRTC_MEDIA_BASE_VIDEOFRAME_H_

// TODO(nisse): Some applications expect that including this file
// implies an include of logging.h. So keep for compatibility, until
// this file can be deleted.
#include "webrtc/base/logging.h"

#include "webrtc/api/video/video_frame.h"

// TODO(nisse): Similarly, some applications expect that including this file
// implies a forward declaration of rtc::Thread.
namespace rtc {
class Thread;
}  // namespace rtc

namespace cricket {

using VideoFrame = webrtc::VideoFrame;

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_VIDEOFRAME_H_
