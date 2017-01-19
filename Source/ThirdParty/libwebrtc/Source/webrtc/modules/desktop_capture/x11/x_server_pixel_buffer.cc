/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/x11/x_server_pixel_buffer.h"

#include <assert.h>
#include <string.h>
#include <sys/shm.h>

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/x11/x_error_trap.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace {

// Returns the number of bits |mask| has to be shifted left so its last
// (most-significant) bit set becomes the most-significant bit of the word.
// When |mask| is 0 the function returns 31.
uint32_t MaskToShift(uint32_t mask) {
  int shift = 0;
  if ((mask & 0xffff0000u) == 0) {
    mask <<= 16;
    shift += 16;
  }
  if ((mask & 0xff000000u) == 0) {
    mask <<= 8;
    shift += 8;
  }
  if ((mask & 0xf0000000u) == 0) {
    mask <<= 4;
    shift += 4;
  }
  if ((mask & 0xc0000000u) == 0) {
    mask <<= 2;
    shift += 2;
  }
  if ((mask & 0x80000000u) == 0)
    shift += 1;

  return shift;
}

// Returns true if |image| is in RGB format.
bool IsXImageRGBFormat(XImage* image) {
  return image->bits_per_pixel == 32 &&
      image->red_mask == 0xff0000 &&
      image->green_mask == 0xff00 &&
      image->blue_mask == 0xff;
}

}  // namespace

namespace webrtc {

XServerPixelBuffer::XServerPixelBuffer() {}

XServerPixelBuffer::~XServerPixelBuffer() {
  Release();
}

void XServerPixelBuffer::Release() {
  if (x_image_) {
    XDestroyImage(x_image_);
    x_image_ = NULL;
  }
  if (shm_pixmap_) {
    XFreePixmap(display_, shm_pixmap_);
    shm_pixmap_ = 0;
  }
  if (shm_gc_) {
    XFreeGC(display_, shm_gc_);
    shm_gc_ = NULL;
  }
  if (shm_segment_info_) {
    if (shm_segment_info_->shmaddr != reinterpret_cast<char*>(-1))
      shmdt(shm_segment_info_->shmaddr);
    if (shm_segment_info_->shmid != -1)
      shmctl(shm_segment_info_->shmid, IPC_RMID, 0);
    delete shm_segment_info_;
    shm_segment_info_ = NULL;
  }
  window_ = 0;
}

bool XServerPixelBuffer::Init(Display* display, Window window) {
  Release();
  display_ = display;

  XWindowAttributes attributes;
  {
    XErrorTrap error_trap(display_);
    if (!XGetWindowAttributes(display_, window, &attributes) ||
        error_trap.GetLastErrorAndDisable() != 0) {
      return false;
    }
  }

  window_size_ = DesktopSize(attributes.width, attributes.height);
  window_ = window;
  InitShm(attributes);

  return true;
}

void XServerPixelBuffer::InitShm(const XWindowAttributes& attributes) {
  Visual* default_visual = attributes.visual;
  int default_depth = attributes.depth;

  int major, minor;
  Bool have_pixmaps;
  if (!XShmQueryVersion(display_, &major, &minor, &have_pixmaps)) {
    // Shared memory not supported. CaptureRect will use the XImage API instead.
    return;
  }

  bool using_shm = false;
  shm_segment_info_ = new XShmSegmentInfo;
  shm_segment_info_->shmid = -1;
  shm_segment_info_->shmaddr = reinterpret_cast<char*>(-1);
  shm_segment_info_->readOnly = False;
  x_image_ = XShmCreateImage(display_, default_visual, default_depth, ZPixmap,
                             0, shm_segment_info_, window_size_.width(),
                             window_size_.height());
  if (x_image_) {
    shm_segment_info_->shmid = shmget(
        IPC_PRIVATE, x_image_->bytes_per_line * x_image_->height,
        IPC_CREAT | 0600);
    if (shm_segment_info_->shmid != -1) {
      shm_segment_info_->shmaddr = x_image_->data =
          reinterpret_cast<char*>(shmat(shm_segment_info_->shmid, 0, 0));
      if (x_image_->data != reinterpret_cast<char*>(-1)) {
        XErrorTrap error_trap(display_);
        using_shm = XShmAttach(display_, shm_segment_info_);
        XSync(display_, False);
        if (error_trap.GetLastErrorAndDisable() != 0)
          using_shm = false;
        if (using_shm) {
          LOG(LS_VERBOSE) << "Using X shared memory segment "
                          << shm_segment_info_->shmid;
        }
      }
    } else {
      LOG(LS_WARNING) << "Failed to get shared memory segment. "
                      "Performance may be degraded.";
    }
  }

  if (!using_shm) {
    LOG(LS_WARNING) << "Not using shared memory. Performance may be degraded.";
    Release();
    return;
  }

  if (have_pixmaps)
    have_pixmaps = InitPixmaps(default_depth);

  shmctl(shm_segment_info_->shmid, IPC_RMID, 0);
  shm_segment_info_->shmid = -1;

  LOG(LS_VERBOSE) << "Using X shared memory extension v"
                  << major << "." << minor
                  << " with" << (have_pixmaps ? "" : "out") << " pixmaps.";
}

bool XServerPixelBuffer::InitPixmaps(int depth) {
  if (XShmPixmapFormat(display_) != ZPixmap)
    return false;

  {
    XErrorTrap error_trap(display_);
    shm_pixmap_ = XShmCreatePixmap(display_, window_,
                                   shm_segment_info_->shmaddr,
                                   shm_segment_info_,
                                   window_size_.width(),
                                   window_size_.height(), depth);
    XSync(display_, False);
    if (error_trap.GetLastErrorAndDisable() != 0) {
      // |shm_pixmap_| is not not valid because the request was not processed
      // by the X Server, so zero it.
      shm_pixmap_ = 0;
      return false;
    }
  }

  {
    XErrorTrap error_trap(display_);
    XGCValues shm_gc_values;
    shm_gc_values.subwindow_mode = IncludeInferiors;
    shm_gc_values.graphics_exposures = False;
    shm_gc_ = XCreateGC(display_, window_,
                        GCSubwindowMode | GCGraphicsExposures,
                        &shm_gc_values);
    XSync(display_, False);
    if (error_trap.GetLastErrorAndDisable() != 0) {
      XFreePixmap(display_, shm_pixmap_);
      shm_pixmap_ = 0;
      shm_gc_ = 0;  // See shm_pixmap_ comment above.
      return false;
    }
  }

  return true;
}

bool XServerPixelBuffer::IsWindowValid() const {
  XWindowAttributes attributes;
  {
    XErrorTrap error_trap(display_);
    if (!XGetWindowAttributes(display_, window_, &attributes) ||
        error_trap.GetLastErrorAndDisable() != 0) {
      return false;
    }
  }
  return true;
}

void XServerPixelBuffer::Synchronize() {
  if (shm_segment_info_ && !shm_pixmap_) {
    // XShmGetImage can fail if the display is being reconfigured.
    XErrorTrap error_trap(display_);
    // XShmGetImage fails if the window is partially out of screen.
    xshm_get_image_succeeded_ =
        XShmGetImage(display_, window_, x_image_, 0, 0, AllPlanes);
  }
}

bool XServerPixelBuffer::CaptureRect(const DesktopRect& rect,
                                     DesktopFrame* frame) {
  assert(rect.right() <= window_size_.width());
  assert(rect.bottom() <= window_size_.height());

  uint8_t* data;

  if (shm_segment_info_ && (shm_pixmap_ || xshm_get_image_succeeded_)) {
    if (shm_pixmap_) {
      XCopyArea(display_, window_, shm_pixmap_, shm_gc_,
                rect.left(), rect.top(), rect.width(), rect.height(),
                rect.left(), rect.top());
      XSync(display_, False);
    }
    data = reinterpret_cast<uint8_t*>(x_image_->data) +
        rect.top() * x_image_->bytes_per_line +
        rect.left() * x_image_->bits_per_pixel / 8;
  } else {
    if (x_image_)
      XDestroyImage(x_image_);
    x_image_ = XGetImage(display_, window_, rect.left(), rect.top(),
                         rect.width(), rect.height(), AllPlanes, ZPixmap);
    if (!x_image_)
      return false;

    data = reinterpret_cast<uint8_t*>(x_image_->data);
  }

  if (IsXImageRGBFormat(x_image_)) {
    FastBlit(data, rect, frame);
  } else {
    SlowBlit(data, rect, frame);
  }

  return true;
}

void XServerPixelBuffer::FastBlit(uint8_t* image,
                                  const DesktopRect& rect,
                                  DesktopFrame* frame) {
  uint8_t* src_pos = image;
  int src_stride = x_image_->bytes_per_line;
  int dst_x = rect.left(), dst_y = rect.top();

  uint8_t* dst_pos = frame->data() + frame->stride() * dst_y;
  dst_pos += dst_x * DesktopFrame::kBytesPerPixel;

  int height = rect.height();
  int row_bytes = rect.width() * DesktopFrame::kBytesPerPixel;
  for (int y = 0; y < height; ++y) {
    memcpy(dst_pos, src_pos, row_bytes);
    src_pos += src_stride;
    dst_pos += frame->stride();
  }
}

void XServerPixelBuffer::SlowBlit(uint8_t* image,
                                  const DesktopRect& rect,
                                  DesktopFrame* frame) {
  int src_stride = x_image_->bytes_per_line;
  int dst_x = rect.left(), dst_y = rect.top();
  int width = rect.width(), height = rect.height();

  uint32_t red_mask = x_image_->red_mask;
  uint32_t green_mask = x_image_->red_mask;
  uint32_t blue_mask = x_image_->blue_mask;

  uint32_t red_shift = MaskToShift(red_mask);
  uint32_t green_shift = MaskToShift(green_mask);
  uint32_t blue_shift = MaskToShift(blue_mask);

  int bits_per_pixel = x_image_->bits_per_pixel;

  uint8_t* dst_pos = frame->data() + frame->stride() * dst_y;
  uint8_t* src_pos = image;
  dst_pos += dst_x * DesktopFrame::kBytesPerPixel;
  // TODO(hclam): Optimize, perhaps using MMX code or by converting to
  // YUV directly.
  // TODO(sergeyu): This code doesn't handle XImage byte order properly and
  // won't work with 24bpp images. Fix it.
  for (int y = 0; y < height; y++) {
    uint32_t* dst_pos_32 = reinterpret_cast<uint32_t*>(dst_pos);
    uint32_t* src_pos_32 = reinterpret_cast<uint32_t*>(src_pos);
    uint16_t* src_pos_16 = reinterpret_cast<uint16_t*>(src_pos);
    for (int x = 0; x < width; x++) {
      // Dereference through an appropriately-aligned pointer.
      uint32_t pixel;
      if (bits_per_pixel == 32) {
        pixel = src_pos_32[x];
      } else if (bits_per_pixel == 16) {
        pixel = src_pos_16[x];
      } else {
        pixel = src_pos[x];
      }
      uint32_t r = (pixel & red_mask) << red_shift;
      uint32_t g = (pixel & green_mask) << green_shift;
      uint32_t b = (pixel & blue_mask) << blue_shift;
      // Write as 32-bit RGB.
      dst_pos_32[x] = ((r >> 8) & 0xff0000) | ((g >> 16) & 0xff00) |
          ((b >> 24) & 0xff);
    }
    dst_pos += frame->stride();
    src_pos += src_stride;
  }
}

}  // namespace webrtc
