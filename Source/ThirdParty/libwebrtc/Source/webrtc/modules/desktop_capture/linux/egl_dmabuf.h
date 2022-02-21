/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_EGL_DMABUF_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_EGL_DMABUF_H_

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <gbm.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "modules/desktop_capture/desktop_geometry.h"

namespace webrtc {

class EglDmaBuf {
 public:
  struct EGLStruct {
    std::vector<std::string> extensions;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
  };

  EglDmaBuf();
  ~EglDmaBuf();

  std::unique_ptr<uint8_t[]> ImageFromDmaBuf(const DesktopSize& size,
                                             uint32_t format,
                                             uint32_t n_planes,
                                             const int32_t* fds,
                                             const uint32_t* strides,
                                             const uint32_t* offsets,
                                             uint64_t modifiers);
  std::vector<uint64_t> QueryDmaBufModifiers(uint32_t format);

  bool IsEglInitialized() const { return egl_initialized_; }

 private:
  bool egl_initialized_ = false;
  int32_t drm_fd_ = -1;               // for GBM buffer mmap
  gbm_device* gbm_device_ = nullptr;  // for passed GBM buffer retrieval

  EGLStruct egl_;

  absl::optional<std::string> GetRenderNode();
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_EGL_DMABUF_H_
