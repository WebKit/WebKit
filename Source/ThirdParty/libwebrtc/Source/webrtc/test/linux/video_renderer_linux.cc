/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/video_renderer.h"

#include "test/linux/glx_renderer.h"

namespace webrtc {
namespace test {

VideoRenderer* VideoRenderer::CreatePlatformRenderer(const char* window_title,
                                                     size_t width,
                                                     size_t height) {
  GlxRenderer* glx_renderer = GlxRenderer::Create(window_title, width, height);
  if (glx_renderer != NULL) {
    return glx_renderer;
  }
  return NULL;
}
}  // namespace test
}  // namespace webrtc
