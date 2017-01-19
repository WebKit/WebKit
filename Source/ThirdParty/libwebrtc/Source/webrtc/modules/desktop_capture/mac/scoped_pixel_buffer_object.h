/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_SCOPED_PIXEL_BUFFER_OBJECT_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_SCOPED_PIXEL_BUFFER_OBJECT_H_

#include <OpenGL/CGLMacro.h>
#include <OpenGL/OpenGL.h>

#include "webrtc/base/constructormagic.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ScopedPixelBufferObject {
 public:
  ScopedPixelBufferObject();
  ~ScopedPixelBufferObject();

  bool Init(CGLContextObj cgl_context, int size_in_bytes);
  void Release();

  GLuint get() const { return pixel_buffer_object_; }

 private:
  CGLContextObj cgl_context_;
  GLuint pixel_buffer_object_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScopedPixelBufferObject);
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_DESKTOP_CAPTURE_SCOPED_PIXEL_BUFFER_OBJECT_H_
