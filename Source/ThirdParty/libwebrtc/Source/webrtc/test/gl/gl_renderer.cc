/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/gl/gl_renderer.h"

#include <string.h>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

GlRenderer::GlRenderer()
    : is_init_(false), buffer_(NULL), width_(0), height_(0) {}

void GlRenderer::Init() {
  RTC_DCHECK(!is_init_);
  is_init_ = true;

  glGenTextures(1, &texture_);
}

void GlRenderer::Destroy() {
  if (!is_init_) {
    return;
  }

  is_init_ = false;

  delete[] buffer_;
  buffer_ = NULL;

  glDeleteTextures(1, &texture_);
}

void GlRenderer::ResizeViewport(size_t width, size_t height) {
  // TODO(pbos): Aspect ratio, letterbox the video.
  glViewport(0, 0, width, height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glOrtho(0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f);
  glMatrixMode(GL_MODELVIEW);
}

void GlRenderer::ResizeVideo(size_t width, size_t height) {
  RTC_DCHECK(is_init_);
  width_ = width;
  height_ = height;

  buffer_size_ = width * height * 4;  // BGRA

  delete[] buffer_;
  buffer_ = new uint8_t[buffer_size_];
  RTC_DCHECK(buffer_);
  memset(buffer_, 0, buffer_size_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGRA,
               GL_UNSIGNED_INT_8_8_8_8, static_cast<GLvoid*>(buffer_));
}

void GlRenderer::OnFrame(const webrtc::VideoFrame& frame) {
  RTC_DCHECK(is_init_);

  if (static_cast<size_t>(frame.width()) != width_ ||
      static_cast<size_t>(frame.height()) != height_) {
    ResizeVideo(frame.width(), frame.height());
  }

  webrtc::ConvertFromI420(frame, VideoType::kBGRA, 0, buffer_);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_BGRA,
                  GL_UNSIGNED_INT_8_8_8_8, buffer_);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glLoadIdentity();

  glBegin(GL_QUADS);
  {
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(0.0f, 1.0f, 0.0f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 0.0f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);
  }
  glEnd();

  glBindTexture(GL_TEXTURE_2D, 0);
  glFlush();
}
}  // namespace test
}  // namespace webrtc
