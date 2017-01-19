/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Implementation of GtkVideoRenderer

#include "webrtc/media/devices/gtkvideorenderer.h"
#include "webrtc/video_frame.h"

#include <gdk/gdk.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "libyuv/convert_argb.h"

namespace cricket {

class ScopedGdkLock {
 public:
  ScopedGdkLock() {
    gdk_threads_enter();
  }

  ~ScopedGdkLock() {
    gdk_threads_leave();
  }
};

GtkVideoRenderer::GtkVideoRenderer(int x, int y)
    : window_(NULL),
      draw_area_(NULL),
      initial_x_(x),
      initial_y_(y),
      width_(0),
      height_(0) {
  g_type_init();
  // g_thread_init API is deprecated since glib 2.31.0, see release note:
  // http://mail.gnome.org/archives/gnome-announce-list/2011-October/msg00041.html
#if !GLIB_CHECK_VERSION(2, 31, 0)
  g_thread_init(NULL);
#endif
  gdk_threads_init();
}

GtkVideoRenderer::~GtkVideoRenderer() {
  if (window_) {
    ScopedGdkLock lock;
    gtk_widget_destroy(window_);
    // Run the Gtk main loop to tear down the window.
    Pump();
  }
  // Don't need to destroy draw_area_ because it is not top-level, so it is
  // implicitly destroyed by the above.
}

bool GtkVideoRenderer::SetSize(int width, int height) {
  ScopedGdkLock lock;

  // If the dimension is the same, no-op.
  if (width_ == width && height_ == height) {
    return true;
  }

  // For the first frame, initialize the GTK window
  if ((!window_ && !Initialize(width, height)) || IsClosed()) {
    return false;
  }

  image_.reset(new uint8_t[width * height * 4]);
  gtk_widget_set_size_request(draw_area_, width, height);

  width_ = width;
  height_ = height;
  return true;
}

void GtkVideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame) {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer(
      webrtc::I420Buffer::Rotate(video_frame.video_frame_buffer(),
                                 video_frame.rotation()));

  // Need to set size as the frame might be rotated.
  if (!SetSize(buffer->width(), buffer->height())) {
    return;
  }

  // convert I420 frame to ABGR format, which is accepted by GTK
  libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
                     buffer->DataU(), buffer->StrideU(),
                     buffer->DataV(), buffer->StrideV(),
                     image_.get(), buffer->width() * 4,
                     buffer->width(), buffer->height());

  ScopedGdkLock lock;

  if (IsClosed()) {
    return;
  }

  // draw the ABGR image
  gdk_draw_rgb_32_image(draw_area_->window,
                        draw_area_->style->fg_gc[GTK_STATE_NORMAL],
                        0,
                        0,
                        buffer->width(),
                        buffer->height(),
                        GDK_RGB_DITHER_MAX,
                        image_.get(),
                        buffer->width() * 4);

  // Run the Gtk main loop to refresh the window.
  Pump();
}

bool GtkVideoRenderer::Initialize(int width, int height) {
  gtk_init(NULL, NULL);
  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  draw_area_ = gtk_drawing_area_new();
  if (!window_ || !draw_area_) {
    return false;
  }

  gtk_window_set_position(GTK_WINDOW(window_), GTK_WIN_POS_CENTER);
  gtk_window_set_title(GTK_WINDOW(window_), "Video Renderer");
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_widget_set_size_request(draw_area_, width, height);
  gtk_container_add(GTK_CONTAINER(window_), draw_area_);
  gtk_widget_show_all(window_);
  gtk_window_move(GTK_WINDOW(window_), initial_x_, initial_y_);

  image_.reset(new uint8_t[width * height * 4]);
  return true;
}

void GtkVideoRenderer::Pump() {
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }
}

bool GtkVideoRenderer::IsClosed() const {
  if (!window_) {
    // Not initialized yet, so hasn't been closed.
    return false;
  }

  if (!GTK_IS_WINDOW(window_) || !GTK_IS_DRAWING_AREA(draw_area_)) {
    return true;
  }

  return false;
}

}  // namespace cricket
