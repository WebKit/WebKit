/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "modules/video_capture/linux/video_capture_v4l2.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_impl.h"
#include "modules/video_capture/video_capture_options.h"
#include "system_wrappers/include/clock.h"

#if defined(WEBRTC_USE_PIPEWIRE)
#include "modules/video_capture/linux/video_capture_pipewire.h"
#endif

namespace webrtc {
namespace videocapturemodule {
scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(
    Clock* clock,
    const char* deviceUniqueId) {
  auto implementation = make_ref_counted<VideoCaptureModuleV4L2>(clock);

  if (implementation->Init(deviceUniqueId) != 0)
    return nullptr;

  return implementation;
}

scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(
    Clock* clock,
    VideoCaptureOptions* options,
    const char* deviceUniqueId) {
#if defined(WEBRTC_USE_PIPEWIRE)
  if (options->allow_pipewire()) {
    auto implementation =
        webrtc::make_ref_counted<VideoCaptureModulePipeWire>(clock, options);

    if (implementation->Init(deviceUniqueId) == 0)
      return implementation;
  }
#endif
  if (options->allow_v4l2()) {
    auto implementation = make_ref_counted<VideoCaptureModuleV4L2>(clock);

    if (implementation->Init(deviceUniqueId) == 0)
      return implementation;
  }
  return nullptr;
}
}  // namespace videocapturemodule
}  // namespace webrtc
