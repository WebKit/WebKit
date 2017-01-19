/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(nisse): Deprecated, replace cricket::WebRtcVideoFrame with
// webrtc::VideoFrame everywhere, then delete this file. See
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5682.

#ifndef WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOFRAME_H_
#define WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOFRAME_H_

#include <memory>

#include "webrtc/base/buffer.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/include/video_frame_buffer.h"
#include "webrtc/media/base/videoframe.h"

namespace cricket {

using WebRtcVideoFrame = webrtc::VideoFrame;

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_WEBRTCVIDEOFRAME_H_
