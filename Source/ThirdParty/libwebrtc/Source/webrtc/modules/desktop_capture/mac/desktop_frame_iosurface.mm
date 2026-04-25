/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/desktop_frame_iosurface.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

// static
std::unique_ptr<DesktopFrameIOSurface> DesktopFrameIOSurface::Wrap(
    webrtc::ScopedCFTypeRef<IOSurfaceRef> io_surface, CGRect rect) {
  if (!io_surface) {
    return nullptr;
  }

  IOSurfaceIncrementUseCount(io_surface.get());
  IOReturn status =
      IOSurfaceLock(io_surface.get(), kIOSurfaceLockReadOnly, nullptr);
  if (status != kIOReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to lock the IOSurface with status " << status;
    IOSurfaceDecrementUseCount(io_surface.get());
    return nullptr;
  }

  // Verify that the image has 32-bit depth.
  int bytes_per_pixel = IOSurfaceGetBytesPerElement(io_surface.get());
  if (bytes_per_pixel != DesktopFrame::kBytesPerPixel) {
    RTC_LOG(LS_ERROR) << "CGDisplayStream handler returned IOSurface with "
                      << (8 * bytes_per_pixel)
                      << " bits per pixel. Only 32-bit depth is supported.";
    IOSurfaceUnlock(io_surface.get(), kIOSurfaceLockReadOnly, nullptr);
    IOSurfaceDecrementUseCount(io_surface.get());
    return nullptr;
  }

  const size_t surface_width = IOSurfaceGetWidth(io_surface.get());
  const size_t surface_height = IOSurfaceGetHeight(io_surface.get());
  const int32_t stride =
      checked_cast<int32_t>(IOSurfaceGetBytesPerRow(io_surface.get()));
  uint8_t* const data =
      static_cast<uint8_t*>(IOSurfaceGetBaseAddress(io_surface.get()));
  int32_t width = checked_cast<int32_t>(surface_width);
  int32_t height = checked_cast<int32_t>(surface_height);
  ptrdiff_t offset = 0;
  ptrdiff_t offset_columns = 0;
  ptrdiff_t offset_rows = 0;
  if (rect.size.width > 0 && rect.size.height > 0) {
    width = checked_cast<int32_t>(std::floor(rect.size.width));
    height = checked_cast<int32_t>(std::floor(rect.size.height));
    offset_columns = checked_cast<ptrdiff_t>(std::ceil(rect.origin.x));
    offset_rows = checked_cast<ptrdiff_t>(std::ceil(rect.origin.y));
    offset = stride * offset_rows + bytes_per_pixel * offset_columns;
  }

  RTC_LOG(LS_VERBOSE) << "DesktopFrameIOSurface wrapping IOSurface with size "
                      << surface_width << "x" << surface_height
                      << ". Cropping to (" << offset_columns << ","
                      << offset_rows << "; " << width << "x" << height
                      << "). Stride=" << stride / bytes_per_pixel
                      << ", buffer-offset-px=" << offset / bytes_per_pixel
                      << ", buffer-offset-bytes=" << offset;

  RTC_CHECK_GE(surface_width, offset_columns + width);
  RTC_CHECK_GE(surface_height, offset_rows + height);
  RTC_CHECK_GE(offset, 0);
  RTC_CHECK_LE(offset + ((height - 1) * stride) + (width * bytes_per_pixel) - 1,
               IOSurfaceGetAllocSize(io_surface.get()));

  return std::unique_ptr<DesktopFrameIOSurface>(new DesktopFrameIOSurface(
      io_surface, data + offset, width, height, stride));
}

DesktopFrameIOSurface::DesktopFrameIOSurface(
    webrtc::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    uint8_t* data,
    int32_t width,
    int32_t height,
    int32_t stride)
    : DesktopFrame(DesktopSize(width, height), stride, data, nullptr),
      io_surface_(io_surface) {
  RTC_DCHECK(io_surface_);
}

DesktopFrameIOSurface::~DesktopFrameIOSurface() {
  IOSurfaceUnlock(io_surface_.get(), kIOSurfaceLockReadOnly, nullptr);
  IOSurfaceDecrementUseCount(io_surface_.get());
}

}  // namespace webrtc
